#ifndef ABSTRACT_FILESYSTEM_H
#define ABSTRACT_FILESYSTEM_H

#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>

#include "common/common.h"

namespace intercept {
namespace filesystem {

using intercept::common::DirStream;
class AbstractFileSystem {
public:
    virtual ~AbstractFileSystem() {}
    virtual void Init() = 0;
    virtual void Shutdown() = 0;
    virtual int Open(const char* path, int flags, int mode) = 0;
    virtual ssize_t Read(int fd, void* buf, size_t count) = 0;
    virtual ssize_t Write(int fd, const void* buf, size_t count) = 0;
    virtual int Close(int fd) = 0;
    virtual off_t Lseek(int fd, off_t offset, int whence) = 0;
    virtual int Stat(const char* path, struct stat* st) = 0;
    virtual int Fstat(int fd, struct stat* st) = 0;
    virtual int Fsync(int fd) = 0;
    virtual int Truncate(const char* path, off_t length) = 0;
    virtual int Ftruncate(int fd, off_t length) = 0;
    virtual int Unlink(const char* path) = 0;
    virtual int Mkdir(const char* path, mode_t mode) = 0;
    virtual int Opendir(const char* path, DirStream* dirstream) = 0;
    virtual int Getdents(DirStream* dirstream, char* contents, size_t maxread, ssize_t* realbytes) = 0;
    virtual int Closedir(DirStream* dirstream) = 0;
    virtual int Rmdir(const char* path) = 0;
    virtual int Chmod(const char* path, mode_t mode) = 0;
    virtual int Chown(const char* path, uid_t owner, gid_t group) = 0;
    virtual int Rename(const char* oldpath, const char* newpath) = 0;
    virtual int Link(const char* oldpath, const char* newpath) = 0;
    virtual int Symlink(const char* oldpath, const char* newpath) = 0;
    virtual int Readlink(const char* path, char* buf, size_t bufsize) = 0;
    virtual int Utime(const char* path, const struct utimbuf* times) = 0;

    virtual ssize_t MultiRead(int fd, void* buf, size_t count) {}
    virtual ssize_t MultiWrite(int fd, const void* buf, size_t count) {}

protected:
    virtual std::string NormalizePath(const std::string& path) = 0;
};

} // namespace filesystem
} // namespace intercept



#endif // ABSTRACT_FILESYSTEM_H
