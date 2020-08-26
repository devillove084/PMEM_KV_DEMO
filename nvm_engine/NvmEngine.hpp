#ifndef TAIR_CONTEST_KV_CONTEST_NVM_ENGINE_H_
#define TAIR_CONTEST_KV_CONTEST_NVM_ENGINE_H_

#include "include/db.hpp"

class NvmEngine : DB {
public:
    static Status CreateOrOpen(const std::string& name, DB** dbptr);
    Status Get(const Slice& key, std::string* value);
    Status Set(const Slice& key, const Slice& value);
    ~NvmEngine();
private:
};

#endif