#pragma once

#include "include/db.hpp"
#include <assert.h>

class Iterator {

public:
    Iterator(){
        cleanup_.function = nullptr;
        cleanup_.next = nullptr;
    }
    virtual ~Iterator(){
        if (cleanup_.function != nullptr) {
            (*cleanup_.function)(cleanup_.arg1, cleanup_.arg2);
            for (auto c = cleanup_.next; c != nullptr; ) {
                (*c->function)(c->arg1, c->arg2);
                auto next = c->next;
                delete c;
                c = next;
            }
        }
    }

    virtual bool Valid() const = 0;
    virtual void SeekToFirst() = 0;
    virtual void SeekToLast() = 0;
    virtual void Seek(const Slice& target) = 0;
    virtual void Next() = 0;
    virtual void Prev() = 0;
    virtual Slice key() = 0;
    virtual Slice value() = 0;
    virtual Status status() const = 0;

    typedef void (*CleanupFunction)(void* arg1, void* arg2);
    void RegisterCleanup(CleanupFunction func, void* arg1, void* arg2){
        assert(func != nullptr);
        Cleanup* c;
        if (cleanup_.function == nullptr) {
            c = &cleanup_;
        } else {
            c = new Cleanup;
            c->next = cleanup_.next;
            cleanup_.next = c;
        }
    }

private:
    struct Cleanup {
        CleanupFunction function;
        void* arg1;
        void* arg2;
        Cleanup* next;
    };
    Cleanup cleanup_;

    // No copying allowed
    Iterator(const Iterator&);
    void operator=(const Iterator&);
};

class EmptyIterator : public Iterator {
public:
    explicit EmptyIterator(const Status& s) : status_(s) { }
    virtual bool Valid() const { return false; }
    virtual void Seek(const Slice& target) { }
    virtual void SeekToFirst() { }
    virtual void SeekToLast() { }
    virtual void Next() { assert(false); }
    virtual void Prev() { assert(false); }
    Slice key() const { assert(false); return {}; }
    Slice value() const { assert(false); return {}; }
    virtual Status status() const { return status_; }
private:
    Status status_;
};