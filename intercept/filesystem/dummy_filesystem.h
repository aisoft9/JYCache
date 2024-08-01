#ifndef DUMMY_FILESYSTEM_H
#define DUMMY_FILESYSTEM_H
#include <atomic>

#include "abstract_filesystem.h"
namespace intercept {
namespace filesystem {
class DummyFileSystem : public AbstractFileSystem {
public:
    DummyFileSystem();
    ~DummyFileSystem() override;
    void Init() override;
    void Shutdown() override;
    int Open(const char* path, int flags, int mode) override;
    ssize_t Read(int fd, void* buf, size_t count) override;
    ssize_t Write(int fd, const void* buf, size_t count) override;
    int Close(int fd) override;
    off_t Lseek(int fd, off_t offset, int whence) override;
    int Stat(const char* path, struct stat* st) override;
    int Fstat(int fd, struct stat* st) override;
    int Fsync(int fd) override;
    int Ftruncate(int fd, off_t length) override;
    int Unlink(const char* path) override;
    int Mkdir(const char* path, mode_t mode) override;
    int Opendir(const char* path, DirStream* dirstream);
    int Getdents(DirStream* dirstream, char* contents, size_t maxread, ssize_t* realbytes);
    int Closedir(DirStream* dirstream);
    int Rmdir(const char* path) override;
    int Rename(const char* from, const char* to) override;
    int Link(const char* from, const char* to) override;
    int Symlink(const char* from, const char* to) override;
    int Readlink(const char* path, char* buf, size_t bufsize) override;
    int Chmod(const char* path, mode_t mode) override;
    int Chown(const char* path, uid_t uid, gid_t gid) override;
    int Truncate(const char* path, off_t length) override;
    int Utime(const char* path, const struct utimbuf* times) override;
    

protected:
    std::string NormalizePath(const std::string& path) override;
    uintptr_t instance_;
    std::atomic<int> fd_ = 0;
    off_t offset_ = 0;
    long copynum_ = 0;
    static char* memory_;
};

} // namespace filesystem
} // namespace intercept
#endif // DUMMY_FILESYSTEM_H