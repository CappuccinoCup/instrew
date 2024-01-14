
#ifndef _INSTREW_SERVER_CACHE_H
#define _INSTREW_SERVER_CACHE_H

#include <filesystem>
#include <utility>
#include <vector>

namespace instrew {

class Cache {
public:
    static constexpr size_t HASH_SIZE = 20;

    Cache();
    ~Cache();

    std::pair<int,size_t> Get(const uint8_t* hash); // returns fd (or -1) + size
    void Put(const uint8_t* hash, size_t bufsz, const char* buf);

private:
    std::filesystem::path FileName(const uint8_t* hash, std::string suffix = "");

    bool allow_read;
    bool allow_write;
    std::filesystem::path path;
};

} // namespace instrew

#endif
