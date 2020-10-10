#pragma once

#include <cassert>
#include <cstdint>
#include <include/db.hpp>

class Cache {
public:
    Cache() = default;
    virtual ~Cache();
    struct Handle {};
    virtual Handle* Insert(const Slice& key, void* value, size_t charge,
                            void (*deleter)(const Slice& key, void* value)) = 0;

    virtual Handle* Lookup(const Slice& key) = 0;
    virtual void Release(Handle* handle) = 0;
    virtual void* Value(Handle* handle) = 0;
    virtual void Erase(const Slice& key) = 0;
    virtual uint64_t NewID() = 0;
    virtual void Prune() {}

    virtual size_t TotalCharge() const = 0;

private:
    void LRU_Remove(Handle* e);
    void LRU_Append(Handle* e);
    void Unref(Handle* e);

    struct Rep;
    Rep* rep_;

    Cache(const Cache&);
    void operator=(const Cache&);
};

struct LRUHandle {
    void* value;
    void (*deleter)(const Slice&, void* value);
    LRUHandle* next_hash;
    LRUHandle* next;
    LRUHandle* prev;
    size_t charge;
    uint64_t key_length;
    bool in_cache;
    uint32_t refs;
    uint32_t hash;
    char* key_data;

    Slice key() const {
        assert(next != this);
        return {key_data, key_length};
    }
};

class HandleTable {
public:
    HandleTable() : length_(0), elems_(0), list_(nullptr) { Resize(); }
    ~HandleTable() { delete[] list_; }

    LRUHandle* Lookup(const Slice& key, uint32_t hash) {
        return *FindPointer(key, hash);
    }

    LRUHandle* Insert(LRUHandle* h) {
        LRUHandle** ptr = FindPointer(h->key(), h->hash);
        LRUHandle* old = *ptr;
        h->next_hash = (old == nullptr ? nullptr : old->next_hash);
        *ptr = h;
        if (old == nullptr) {
        ++elems_;
        if (elems_ > length_) {
            Resize();
        }
        }
        return old;
    }

    LRUHandle* Remove(const Slice& key, uint32_t hash) {
        LRUHandle** ptr = FindPointer(key, hash);
        LRUHandle* result = *ptr;
        if (result != nullptr) {
        *ptr = result->next_hash;
        --elems_;
        }
        return result;
    }

private:
    uint32_t length_;
    uint32_t elems_;
    LRUHandle** list_;

    LRUHandle** FindPointer(const Slice& key, uint32_t hash) {
        LRUHandle** ptr = &list_[hash & (length_ - 1)];
        while (*ptr != nullptr &&
            ((*ptr)->hash != hash || !(const_cast<Slice&>(key) == (*ptr)->key()))) {

            ptr = &(*ptr)->next_hash;
        }
        return ptr;
    }

    void Resize() {
        uint32_t new_length = 4;
        while (new_length < elems_) {
        new_length *= 2;
        }
        LRUHandle** new_list = new LRUHandle*[new_length];
        memset(new_list, 0, sizeof(new_list[0]) * new_length);
        uint32_t count = 0;
        for (uint32_t i = 0; i < length_; i++) {
        LRUHandle* h = list_[i];
        while (h != nullptr) {
            LRUHandle* next = h->next_hash;
            uint32_t hash = h->hash;
            LRUHandle** ptr = &new_list[hash & (new_length - 1)];
            h->next_hash = *ptr;
            *ptr = h;
            h = next;
            count++;
        }
        }
        assert(elems_ == count);
        delete[] list_;
        list_ = new_list;
        length_ = new_length;
    }
};

//TODO: 实现LRU的函数部分