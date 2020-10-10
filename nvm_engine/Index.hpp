#pragma once

#include "include/db.hpp"

#include <iostream>
#include <memory>
#include <deque>

#include "Iterator.hpp"

using entry_key_t = uint64_t;

struct KeyAndMeta{
    entry_key_t key;
    std::shared_ptr<IndexMeta> meta;
};


class IndexMeta {
public:
    uint32_t offset;
    uint32_t size;
    uint16_t file_number;

    IndexMeta() : offset(0), size(0), file_number(0) {}
    IndexMeta(uint32_t offset, uint16_t size, uint16_t file_number) : \
        offset(offset), size(size), file_number(file_number) {}
};

class Index {
public:
    Index() = default;
    virtual ~Index() = default;
    virtual void Insert(const uint32_t& key, IndexMeta meta) = 0;
    virtual IndexMeta* Get(std::deque<KeyAndMeta>& queue) = 0;
    virtual Iterator* NewIterator() = 0;
};

Index* CreateBtreeIndex();