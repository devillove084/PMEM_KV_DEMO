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
    kEncodingRawUnCompressed = 0x01,
    kEncodingPtrCompressed = 0x02,
    kEncodingPtrUnCompressed = 0x03,
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

