#include <iostream>
#include <cstring>

#include "dummy_filesystem.h" 

namespace intercept {
namespace filesystem{

std::size_t g_size = 10240000000;
char* DummyFileSystem::memory_ = nullptr;

DummyFileSystem::DummyFileSystem()
{
    if (memory_ == nullptr) {
        memory_ = new char[g_size];
        //memset(memory_, 'j', g_size);
        std::cout << "Memory allocated for shared_memory" << std::endl;
    }
    std::cout << "DummyFileSystem created" << std::endl;
}

DummyFileSystem::~DummyFileSystem()
{
    std::cout << "DummyFileSystem destroyed, copy num: " << copynum_ << std::endl;
    if (memory_ != nullptr) {
        delete[] memory_;
        memory_ = nullptr;
        std::cout << "Memory deallocated for shared_memory" << std::endl;
    }
}

void DummyFileSystem::Init() {
    std::cout << "DummyFileSystem Init" << std::endl;
}

void DummyFileSystem::Shutdown() {
    std::cout << "DummyFileSystem Shutdown" << std::endl;
}

int DummyFileSystem::Open(const char* path, int flags, int mode) {
    fd_.fetch_add(1);
    std::cout << "DummyFileSystem Open: " << path << "  ret: " << fd_.load() << std::endl;

    return fd_.load();
}

char buffer[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
ssize_t DummyFileSystem::Read(int fd, void* buf, size_t count) {
    // std::cout << "DummyFileSystem Read: " << fd << std::endl;
    offset_ += count;
    if (offset_ > g_size - count) {
        // std::cout << "begin offset_: " << offset_ << "  g_size: "<< g_size << ", count: " << count << std::endl;
        offset_ = offset_ % (g_size - 10000000);
        // std::cout << "after offset_: " << offset_ << std::endl;
    }
    if (offset_ < (g_size - 10000000)) {
        memcpy((char*)buf, memory_ + offset_, count);
        // memcpy((char*)buf, buffer, count);
    } else {
        memcpy((char*)buf, memory_ + 128, count);
        // memcpy((char*)buf, buffer, count);
    }
    copynum_++;
    return count;
}

ssize_t DummyFileSystem::Write(int fd, const void* buf, size_t count) {
    std::cout << "DummyFileSystem Write: " << fd << ", count: " << count << std::endl;
    //memcpy(memory_ + offset_, buf, count);
    return count;
}

int DummyFileSystem::Close(int fd) {
    std::cout << "DummyFileSystem Close: " << fd << "   ,copynum_ :" <<  copynum_  << std::endl;
    return 0;
}


off_t DummyFileSystem::Lseek(int fd, off_t offset, int whence) {
    std::cout << "DummyFileSystem Lseek: " << fd << std::endl;
    
    if (offset_ > g_size - 10000000) {
        offset_ = offset_ % (g_size-10000000);
    } else {
        offset_ = offset;
    }
    return 0;
}

int DummyFileSystem::Stat(const char* path, struct stat* buf) {
    buf->st_ino = 111111;
    std::cout << "DummyFileSystem Stat: " << path << std::endl;
    return 0;
}

int DummyFileSystem::Fstat(int fd, struct stat* buf) {
    std::cout << "DummyFileSystem Fstat: " << fd << std::endl;
    return 0;
}

int DummyFileSystem::Fsync(int fd) {
    std::cout << "DummyFileSystem Fsync: " << fd << std::endl;
    return 0;
}

int DummyFileSystem::Ftruncate(int fd, off_t length) {
    std::cout << "DummyFileSystem Ftruncate: " << fd << std::endl;
    return 0;
}


int DummyFileSystem::Unlink(const char* path) {
    std::cout << "DummyFileSystem Unlink: " << path << std::endl;
    return 0;
}


int DummyFileSystem::Mkdir(const char* path, mode_t mode) {
    std::cout << "DummyFileSystem Mkdir: " << path << std::endl;
    return 0;
}

int DummyFileSystem::Opendir(const char* path, DirStream* dirstream) {
    std::cout << "DummyFileSystem Opendir: " << path << std::endl;
    return 0;
}

int  DummyFileSystem::Getdents(DirStream* dirstream, char* contents, size_t maxread, ssize_t* realbytes) {
    std::cout << "DummyFileSystem getdentes: "  << std::endl;
    return 0;
}


int DummyFileSystem::Closedir(DirStream* dirstream) {
    std::cout << "DummyFileSystem Closedir: " << std::endl;
    return 0;
}

int DummyFileSystem::Rmdir(const char* path) {
    std::cout << "DummyFileSystem Rmdir: " << path << std::endl;
    return 0;
}

int DummyFileSystem::Rename(const char* oldpath, const char* newpath) {
    std::cout << "DummyFileSystem Rename: " << oldpath << " to " << newpath << std::endl;
    return 0;
}

int DummyFileSystem::Link(const char* oldpath, const char* newpath) {
    std::cout << "DummyFileSystem Link: " << oldpath << " to " << newpath << std::endl;
    return 0;
}

int DummyFileSystem::Symlink(const char* oldpath, const char* newpath) {
    std::cout << "DummyFileSystem Symlink: " << oldpath << std::endl;
    return 0;
}  

int DummyFileSystem::Readlink(const char* path, char* buf, size_t bufsize) {
    throw std::runtime_error("未实现");
}

int DummyFileSystem::Chmod(const char* path, mode_t mode) {
    throw std::runtime_error("未实现");
}

int DummyFileSystem::Chown(const char* path, uid_t uid, gid_t gid) {
    throw std::runtime_error("未实现");
}

int DummyFileSystem::Truncate(const char* path, off_t length) {
    return 0;
}

int DummyFileSystem::Utime(const char* path, const struct utimbuf* ubuf) {
    throw std::runtime_error("未实现");
}



std::string DummyFileSystem::NormalizePath(const std::string& path) {
    throw std::runtime_error("未实现");
}

} // namespace intercept
} // namespace filesystem
