#include "NvmEngine.hpp"

Status DB::CreateOrOpen(const std::string& name, DB** dbptr, FILE* log_file) {
    return NvmEngine::CreateOrOpen(name, dbptr);
}

DB::~DB() {}

Status NvmEngine::CreateOrOpen(const std::string& name, DB** dbptr) {
    return Ok;
}
Status NvmEngine::Get(const Slice& key, std::string* value) {
    return Ok;
}
Status NvmEngine::Set(const Slice& key, const Slice& value) {
    return Ok;
}

NvmEngine::~NvmEngine() {}
