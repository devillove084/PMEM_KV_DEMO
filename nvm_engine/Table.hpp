#pragma once

#include "include/db.hpp"
#include "Iterator.hpp"
#include "Index.hpp"
#include "Cache.hpp"

class Env;

struct TableHandle {
  typedef void (*CleanupFunction)(void* arg1, void* arg2);

  TableHandle() : table_(nullptr), func(nullptr), arg1(nullptr), arg2(nullptr) { }

  void RegisterCleanup(CleanupFunction arg, void* cache, void* handle) {
    func = arg;
    arg1 = cache;
    arg2 = handle;
  }

  ~TableHandle() {
    (*func)(arg1, arg2);
  }

  Table* table_;
  CleanupFunction func;
  void* arg1;
  void* arg2;
};

class RandomAccessFile {
public:
    RandomAccessFile() = default;
    virtual ~RandomAccessFile();
    virtual Status Read(uint64_t offset, size_t n, Slice* result, char* scratch) const = 0;

private:
    RandomAccessFile(const RandomAccessFile&);
    void operator=(const RandomAccessFile&);
};

class BlockHandle {
public:
    BlockHandle();
    BlockHandle(uint64_t size, uint64_t offset);

    // The offset of the block in the file.
    uint64_t offset() const { return offset_; }
    void set_offset(uint64_t offset) { offset_ = offset; }

    // The size of the stored block
    uint64_t size() const { return size_; }
    void set_size(uint64_t size) { size_ = size; }

    void EncodeTo(std::string* dst) const;
    Status DecodeFrom(Slice* input);

    // Maximum encoding length of a BlockHandle
    enum { kMaxEncodedLength = 10 + 10 };

private:
    uint64_t offset_;
    uint64_t size_;
};

class Footer {
public:
    Footer() = default;

    const BlockHandle& metaindex_handle() const { return metaindex_handle_; }
    void set_metaindex_handle(const BlockHandle& h) { metaindex_handle_ = h; }

    const BlockHandle& index_handle() const {
        return index_handle_;
    }
    void set_index_handle(const BlockHandle& h) {
        index_handle_ = h;
    }

    void EncodeTo(std::string* dst) const;
    Status DecodeFrom(Slice* input);

    enum {
        kEncodedLength = 2*BlockHandle::kMaxEncodedLength + 8
    };

private:
    BlockHandle metaindex_handle_;
    BlockHandle index_handle_;
};

class Table {
public:
    static Status Open(RandomAccessFile* file, uint64_t file_size, Table** table);
    ~Table();
    Iterator* NewIterator() const;
    Iterator* BlockIterator(const BlockHandle&);

private:
    struct Rep;
    Rep* rep_;

    explicit Table(Rep* rep){
        rep_ = rep;
    }
    static Iterator* BlockReader(void*, const Slice&);
    friend class TableCache;

    Status InternalGet(
        const Slice& key,
        void* arg,
        void (*handle_result)(void* arg, const Slice& k, const Slice& v));

    void ReadMeta(const Footer& footer);
    void ReadFilter(const Slice& filter_handle_value);

    Table(const Table&);
    void operator=(const Table&);
};

class TableCache {
public:
    TableCache(const std::string& dbanme, int entries);
    ~TableCache();

    Iterator* NewIterator(uint64_t file_number, uint64_t file_size, Table** tableptr = nullptr);

    Status Get(uint64_t file_number,
               uint64_t file_size,
               const Slice& k,
               void* arg,
               void (*handle_result)(void*, const Slice&, const Slice&));

    Status GetTable(uint64_t file_number, uint64_t, TableHandle* table_handle);

    void Evict(uint64_t file_number);

private:
    Env* const env_;
    const std::string dbname_;
    Cache* cache_;

    Status FindTable(uint64_t file_number, uint64_t file_size, Cache::Handle**);

};

struct TableAndFile {
  RandomAccessFile* file;
  Table* table;
};

inline static void DeleteEntry(const Slice& key, void* value) {
    TableAndFile* tf = reinterpret_cast<TableAndFile*>(value);
    delete tf->table;
    delete tf->file;
    delete tf;
}

inline static void UnrefEntry(void* arg1, void* arg2) {
    Cache* c = reinterpret_cast<Cache*>(arg1);
    Cache::Handle* h = reinterpret_cast<Cache::Handle*>(arg2);
    c->Release(h);
}




//TODO:
// table的详细实现