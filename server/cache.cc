
#include "cache.h"

#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <pwd.h>
#include <sstream>
#include <system_error>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace instrew {

// Cache implementation heavily inspired by MESA:
// https://github.com/mesa3d/mesa/blob/main/src/util/disk_cache_os.c

namespace {

struct HexBuffer {
    const uint8_t* buf;
    size_t size;
    friend std::ostream& operator<<(std::ostream& os, HexBuffer const& self) {
        os << std::hex << std::setfill('0');
        for (size_t i = 0; i != self.size; i++)
            os << std::setw(2) << static_cast<int>(self.buf[i]);
        return os << std::dec;
    }
};

static ssize_t write_full(int fd, const char* buf, size_t nbytes) {
    size_t total_written = 0;
    const char* buf_cp = buf;
    while (total_written < nbytes) {
        ssize_t bytes_written = write(fd, buf_cp + total_written, nbytes - total_written);
        if (bytes_written < 0)
            return bytes_written;
        if (bytes_written == 0)
            return -EIO;
        total_written += bytes_written;
    }
    return total_written;
}

} // end namespace

Cache::Cache(const InstrewConfig& instrew_cfg) {
    allow_read = false;
    allow_write = false; // default to no cache -- it's not critical.
    if (!instrew_cfg.cache)
        return;
    if (geteuid() != getuid())
        return;
    if (instrew_cfg.cachedir != "") {
        path = instrew_cfg.cachedir;
    } else {
        passwd* pw = getpwuid(getuid());
        path = pw->pw_dir;
        path /= ".cache";
        path /= "instrew";
    }
    std::error_code ec;
    // F*ck C++ creates this with 0777, but 0755 would be better.
    // TODO: fix cache dir permissions
    std::filesystem::create_directories(path, ec);
    if (ec) {
        std::cerr << "unable to create cache directory, disabling cache" << std::endl;
        return;
    }
    if (instrew_cfg.cachemode == "rw") {
        allow_read = true;
        allow_write = true;
    } else if (instrew_cfg.cachemode == "r") {
        allow_read = true;
    } else if (instrew_cfg.cachemode == "w") {
        allow_write = true;
    } else {
        // Well... keep allow_read and allow_write as false.
        std::cerr << "invalid cache mode, disabling cache" << std::endl;
    }
    verbose = instrew_cfg.cacheverbose;
}

Cache::~Cache() {
}

std::filesystem::path Cache::FileName(const uint8_t* hash, std::string suffix) {
    std::stringstream fn;
    fn << HexBuffer{hash, HASH_SIZE} << suffix;
    return path / fn.str();
}

std::pair<int,size_t> Cache::Get(const uint8_t* hash) {
    if (!allow_read)
        return std::make_pair(-1, 0);

    std::filesystem::path cachefile = FileName(hash);
    int fd = open(cachefile.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd == -1)
        return std::make_pair(-1, 0);
    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        close(fd);
        return std::make_pair(-1, 0);
    }
    if (verbose)
        std::cerr << "hitting " << cachefile << "\n";
    return std::make_pair(fd, sb.st_size);
}

void Cache::Put(const uint8_t* hash, size_t bufsz, const char* buf) {
    if (!allow_write)
        return;

    std::filesystem::path cachefile = FileName(hash, ".tmp");
    std::string cachefile_tmp = cachefile; // copy
    cachefile.replace_extension("");

    struct flock lock{};

    int fd_real = -1;
    int fd = open(cachefile_tmp.c_str(), O_WRONLY | O_CLOEXEC | O_CREAT, 0644);
    if (fd == -1)
        goto close_fds;

    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    if (fcntl(fd, F_SETLK, &lock) < 0)
        goto close_fds;

    fd_real = open(cachefile.c_str(), O_RDONLY, O_CLOEXEC);
    if (fd_real >= 0) { // someone else got it already, so nothing to do.
        unlink(cachefile_tmp.c_str());
        goto close_fds;
    }
    if (write_full(fd, buf, bufsz) < 0) {
        unlink(cachefile_tmp.c_str());
        goto close_fds;
    }
    if (verbose)
        std::cerr << "writing to " << cachefile << "\n";
    if (rename(cachefile_tmp.c_str(), cachefile.c_str()) < 0) {
        unlink(cachefile_tmp.c_str());
        goto close_fds;
    }

close_fds:
    if (fd_real != -1)
        close(fd_real);
    if (fd != -1)
        close(fd);
}

} // namespace instrew
