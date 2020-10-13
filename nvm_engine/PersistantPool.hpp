#pragma once

#include "include/db.hpp"

#include <functional>

#include <assert.h>
#include <atomic>
#include <string>
#include <vector>
#include <snappy.h>
#include <libpmem.h>
#include <libpmemobj.h>
struct KVSHdr {
    unsigned char encoding;
};

struct KVSRef {
    //指定插入值的编码
    struct KVSHdr hdr;

    unsigned int size;
    unsigned int pool_index;

    //pmem池中的偏移
    size_t off_in_pool;
};

enum ValueEncoding {
    kEncodingRawCompressed = 0x0,
    kEncodingRawUncompressed = 0x01,
    kEncodingPtrCompressed = 0x02,
    kEncodingPtrUncompressed = 0x03,
    kEncodingUnknown
};


struct Pool {
    PMEMobjpool* pool;
    uint64_t uuid_lo;
    size_t base_addr;

    Pool() : pool(nullptr) {

    }

    ~Pool() {
        if (pool) {
            pmemobj_close(pool);
        }
    }
};

struct PobjAction : pobj_action {
    unsigned int pool_index;
};

struct KVSRoot {
    size_t size;
};


static struct Pool* pools_ = nullptr;
static size_t pool_count_;
static std::atomic<size_t> next_pool_index_(0);

static size_t kvs_value_thres_ = 0;
static bool compress_value_ = false;
static size_t dcpmm_avail_size_min_ = 0;

static std::atomic<size_t> dcpmm_avail_size_(0);
static std::atomic<bool> dcpmm_is_avail_(true);

inline bool Snappy_Compress(const char* input,
                            size_t length, ::std::string* output) {
#ifdef SNAPPY
  output->resize(snappy::MaxCompressedLength(length));
  size_t outlen;
  snappy::RawCompress(input, length, &(*output)[0], &outlen);
  output->resize(outlen);
  return true;
#else
  (void)input;
  (void)length;
  (void)output;
  return false;
#endif
}

inline bool Snappy_GetUncompressedLength(const char* input, size_t length,
                                         size_t* result) {
#ifdef SNAPPY
  return snappy::GetUncompressedLength(input, length, result);
#else
  (void)input;
  (void)length;
  (void)result;
  return false;
#endif
}

inline bool Snappy_Uncompress(const char* input, size_t length, char* output) {
#ifdef SNAPPY
  return snappy::RawUncompress(input, length, output);
#else
  (void)input;
  (void)length;
  (void)output;
  return false;
#endif
}

int KVSOpen(const char* path, size_t size, size_t pool_count) {
    assert(!pools_);
    pools_ = new Pool[pool_count];
    pool_count_ = pool_count;

    size_t pool_size = size / pool_count;

    for (size_t i = 0; i < pool_count; ++i) {
        std::string pool_path(path);
        pool_path.append(".").append(std::to_string(i));

        PMEMoid root;
        KVSRoot *rootPtr;

        auto* pool = pmemobj_create(pool_path.data(), "store_value", pool_size, 0666);

        if (pool) {
            root = pmemobj_root(pool, sizeof(struct KVSRoot));
            rootPtr = (KVSRoot*)pmemobj_direct(root);
            rootPtr->size = pool_size;
            pmemobj_persist(pool, &(rootPtr->size), sizeof(rootPtr->size));
        }else {
            pool = pmemobj_open(pool_path.data(), "store_value");
            if (pool == nullptr) {
                delete[] pools_;
                pools_ = nullptr;
                return -EIO;
            }
            root = pmemobj_root(pool, sizeof(KVSRoot));
            rootPtr = (KVSRoot*)pmemobj_direct(root);
        }

        pools_[i].pool = pool;
        pools_[i].uuid_lo = root.pool_uuid_lo;
        pools_[i].base_addr = (size_t)pool;
    }
    dcpmm_avail_size_min_ = size / 10;
    return 0;
}

bool KVSEnabled() {
    return pools_ != nullptr;
}

void KVSCLose(){
    delete[] pools_;
    pools_ = nullptr;
}

ValueEncoding KVSGetEncoding(const void *ptr) {
    // 编码第一个byte
    auto* hdr = (KVSHdr*)ptr;
    return (ValueEncoding)(hdr->encoding);
}

inline static bool ReservePmem(size_t size, unsigned int* p_pool_index,
                                PMEMoid* p_oid, pobj_action** p_pact) {

    size_t pool_index = (next_pool_index_++) % pool_count_;
    size_t retry_loop = pool_count_;

    PMEMoid oid;
    auto* pact = new PobjAction;

    for (size_t i = 0; i < retry_loop; ++i) {
        auto* pool = pools_[pool_index].pool;
        oid = pmemobj_reserve(pool, pact, size, 0);
        if (!OID_IS_NULL(oid)) {
            *p_pool_index = pool_index;
            *p_oid = oid;
            pact->pool_index = pool_index;
            *p_pact = pact;
            return true;
        }
        pool_index++;
        if (pool_index >= pool_count_) {
            pool_index = 0;
        }
    }
    dcpmm_is_avail_ = false;
    delete pact;
    return false;
}

inline static bool KVSEncodeValue(const Slice& value, bool compress, KVSRef* ref, pobj_action** p_pact) {
    assert(pools_);
    if (!dcpmm_is_avail_) {
        return false;
    }

    if (!compress) {
        PMEMoid oid;
        if (!ReservePmem(sizeof(struct KVSHdr) + value.size(), &(ref->pool_index),
                        &oid, p_pact)) {
        return false;
        }
        void *buf = pmemobj_direct(oid);
        ref->hdr.encoding = 0x03;
        ref->size = value.size();
        assert((size_t)buf >= pools_[ref->pool_index].base_addr);
        ref->off_in_pool = (size_t)buf - pools_[ref->pool_index].base_addr;

        pmem_memcpy_nodrain(buf, &(ref->hdr), sizeof(ref->hdr));
        pmem_memcpy_persist((char*)buf + sizeof(ref->hdr),
                            value.data(), value.size());
    } else {
        char *compressed = new char[snappy::MaxCompressedLength(value.size())];
        size_t outsize;
        snappy::RawCompress(value.data(), value.size(), compressed, &outsize);
        PMEMoid oid;
        if (!ReservePmem(sizeof(struct KVSHdr) + outsize, &(ref->pool_index),
                        &oid, p_pact)) {
            delete[] compressed;
            return false;
        }
        void *buf = pmemobj_direct(oid);
        ref->hdr.encoding = 0x02;
        ref->size = outsize;
        assert((size_t)buf >= pools_[ref->pool_index].base_addr);
        ref->off_in_pool = (size_t)buf - pools_[ref->pool_index].base_addr;

        // Prefix the encoding type of value content.
        pmem_memcpy_nodrain(buf, &(ref->hdr), sizeof(ref->hdr));
        pmem_memcpy_persist((char*)buf + sizeof(ref->hdr),
                            compressed, outsize);
        delete[] compressed;
    }

    return true;
}

inline static void FreePmem(struct KVSRef* ref) {
    PMEMoid oid;
    oid.pool_uuid_lo = pools_[ref->pool_index].uuid_lo;
    oid.off = ref->off_in_pool;
    pmemobj_free(&oid);
    if (!dcpmm_is_avail_) {
        if ((dcpmm_avail_size_ += ref->size) > dcpmm_avail_size_min_) {
        dcpmm_avail_size_ = 0;
        dcpmm_is_avail_ = true;
        }
    }
}

void KVSDumpFromValueRef(const char* input,
                           std::function<void(const Slice& value)> add) {
    assert(pools_);
    auto* ref = (struct KVSRef*)input;
    if (ref->hdr.encoding == kEncodingPtrCompressed ||
        ref->hdr.encoding == kEncodingPtrUncompressed) {
        auto* hdr = (struct KVSHdr*)(pools_[ref->pool_index].base_addr
                                        + ref->off_in_pool);
        if (ref->hdr.encoding == kEncodingPtrCompressed) {
        hdr->encoding = kEncodingRawCompressed;
        }
        else {
        hdr->encoding = kEncodingRawUncompressed;
        }

        // Prefix encoding type of the value content.
        add(Slice((char*)hdr, ref->size + sizeof(struct KVSHdr)));

        FreePmem(ref);
    }
}

void KVSDecodeValueRef(const char* input, size_t size, std::string* dst) {
    assert(input);
    auto encoding = KVSGetEncoding(input);
    const char* src_data;
    size_t src_len;

    // if it is indirectly pointed, input is a KVSRef
    if (encoding == kEncodingPtrUncompressed ||
            encoding == kEncodingPtrCompressed) {
        assert(size == sizeof(struct KVSRef));
        auto* ref = (struct KVSRef*)input;
        // the data on DCPMM
        src_data = (char*)pools_[ref->pool_index].base_addr +
                    ref->off_in_pool + sizeof(struct KVSHdr);
        src_len = ref->size;
    }
    else {
        assert(encoding == kEncodingRawUncompressed ||
                encoding == kEncodingRawCompressed);
        assert(size >= sizeof(struct KVSHdr));
        src_data = input + sizeof(struct KVSHdr);
        src_len = size - sizeof(struct KVSHdr);
    }

    // if not compressed
    if (encoding == kEncodingRawUncompressed ||
        encoding == kEncodingPtrUncompressed) {
        dst->assign(src_data, src_len);
    }
    // else need to decompress
    else
    {
        assert(encoding == kEncodingRawCompressed ||
        encoding == kEncodingPtrCompressed);
        size_t dst_len;
        if (Snappy_GetUncompressedLength(src_data, src_len, &dst_len)) {
        char* tmp_buf = new char[dst_len];
        Snappy_Uncompress(src_data, src_len, tmp_buf);
        dst->assign(tmp_buf, dst_len);
        delete tmp_buf;
        } else {
        abort();
        }
    }
}

size_t KVSGetExtraValueSize(const Slice& value) {
    auto* ref = (struct KVSRef*)value.data();
    if (ref->hdr.encoding == kEncodingRawCompressed ||
        ref->hdr.encoding == kEncodingRawUncompressed) {
        return 0;
    }
    else {
        return (size_t)ref->size;
    }
}

void KVSFreeValue(const Slice& value) {
    auto* ref = (struct KVSRef*)value.data();
    if (ref && ref->hdr.encoding != kEncodingRawCompressed &&
        ref->hdr.encoding != kEncodingRawUncompressed) {
        FreePmem(ref);
    }
}

void KVSSetKVSValueThres(size_t thres) {
    kvs_value_thres_ = thres;
}

size_t KVSGetKVSValueThres() {
    return kvs_value_thres_;
}

void KVSSetCompressKnob(bool compress) {
    compress_value_ = compress;
}

bool KVSGetCompressKnob() {
    return compress_value_;
}

int KVSPublish(struct pobj_action** pact_array, size_t actvcnt) {
    assert(pools_);
    auto* pacts_of_pools = new std::vector<struct pobj_action>[pool_count_];
    for (size_t i = 0; i < actvcnt; i++) {
        auto* pact = (struct PobjAction*)pact_array[i];
        assert(pact->pool_index < pool_count_);
        pacts_of_pools[pact->pool_index].push_back(*((struct pobj_action*)pact));
        delete pact;
    }
    for (size_t i = 0; i < pool_count_; i++) {
        auto* pool = pools_[i].pool;
        auto& pacts = pacts_of_pools[i];
        auto* data = pacts.data();
        size_t count = pacts.size();
        size_t index = 0;
        while(index < count) {
        auto n = std::min(count - index, 32UL);
        pmemobj_publish(pool, data + index, n);
        index += n;
        }
        assert(index == count);
    }
    delete[] pacts_of_pools;

    return 0;
}