#pragma once

#include<cerrno>
#include<cstddef>

class Allocator {
public:
    virtual ~Allocator() {}
    virtual char* Allocate(size_t bytes) = 0;
    virtual char* AllocateAligned(size_t bytes, size_t huge_page_size = 0) = 0;
    virtual size_t BlockSize() const = 0;
};