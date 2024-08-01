#include <iostream>
#include <cstring>
#include "spdlog/spdlog.h"

#include "s3fs_filesystem.h"
#include "s3fs_lib.h"

namespace intercept {
namespace filesystem {

S3fsFileSystem::S3fsFileSystem() {

}

S3fsFileSystem::~S3fsFileSystem() {
    s3fs_global_uninit();
}


void S3fsFileSystem::Init() {
    s3fs_global_init();
}

void S3fsFileSystem::Shutdown() {
    std::cout << "S3fsFileSystem::Shutdown" << std::endl;
}

int S3fsFileSystem::Open(const char* path, int flags, int mode) {
    // std::cout << "S3fsFileSystem::Open: " << path << std::endl;
    spdlog::info("S3fsFileSystem::Open:{}", path);
    return posix_s3fs_open(path, flags, mode);
}

ssize_t S3fsFileSystem::MultiRead(int fd, void* buf, size_t count) {
    intercept::common::Timer timer("server S3fsFileSystem::MultiRead");
    int numThreads = intercept::common::Configure::getInstance().getConfig("opThreadnum") == ""  ?
        1 : atoi(intercept::common::Configure::getInstance().getConfig("opThreadnum").c_str());
    size_t partSize = count / numThreads; // Part size for each thread
    size_t remaining = count % numThreads; // Remaining part

    std::vector<std::thread> threads;
    char* charBuf = static_cast<char*>(buf);

    std::atomic<ssize_t> totalBytesRead(0); // Atomic variable to accumulate bytes read
    std::mutex readMutex; // Mutex to protect shared variable

    for (size_t i = 0; i < numThreads; ++i) {
        size_t offset = i * partSize;
        size_t size = (i == numThreads - 1) ? (partSize + remaining) : partSize;
        threads.emplace_back([=, &totalBytesRead, &readMutex]() {
            ssize_t bytesRead = posix_s3fs_multiread(fd, charBuf + offset, size, offset);
            spdlog::debug("S3fsFileSystem::MultiRead, fd: {}, offset: {}, size: {}, bytesRead: {}", fd, offset, size, bytesRead);
            std::lock_guard<std::mutex> lock(readMutex);
            totalBytesRead += bytesRead;
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    posix_s3fs_lseek(fd, totalBytesRead.load(), SEEK_CUR);
    spdlog::info("S3fsFileSystem::MultiRead, read bytes: {}", totalBytesRead.load());
    return totalBytesRead.load(); // Return the total bytes read
}

ssize_t S3fsFileSystem::Read(int fd, void* buf, size_t count) {
    // std::cout << "S3fsFileSystem::Read: " << fd << std::endl;
    spdlog::debug("S3fsFileSystem::Read, fd: {}, count: {}", fd, count);
    return posix_s3fs_read(fd, buf, count);
}

ssize_t S3fsFileSystem::MultiWrite(int fd, const void* buf, size_t count) {
    intercept::common::Timer timer("server S3fsFileSystem::MultiWrite");
    int numThreads = intercept::common::Configure::getInstance().getConfig("opThreadnum") == ""  ?
        1 : atoi(intercept::common::Configure::getInstance().getConfig("opThreadnum").c_str());
    size_t partSize = count / numThreads; // Part size for each thread
    size_t remaining = count % numThreads; // Remaining part

    std::vector<std::thread> threads;
    const char* charBuf = static_cast<const char*>(buf);

    std::atomic<ssize_t> totalBytesWrite(0); // Atomic variable to accumulate bytes write
    std::mutex writeMutex; // Mutex to protect shared variable

    for (size_t i = 0; i < numThreads; ++i) {
        size_t offset = i * partSize;
        size_t size = (i == numThreads - 1) ? (partSize + remaining) : partSize;
        threads.emplace_back([=, &totalBytesWrite, &writeMutex]() {
            ssize_t bytesWrite = posix_s3fs_multiwrite(fd, charBuf + offset, size, offset);
            spdlog::debug("finish S3fsFileSystem::Multiwrite, fd: {}, offset: {}, size: {}, bytesRead: {}", fd, offset, size, bytesWrite);
            std::lock_guard<std::mutex> lock(writeMutex);
            totalBytesWrite += bytesWrite;
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    posix_s3fs_lseek(fd, totalBytesWrite.load(), SEEK_CUR);
    spdlog::debug("S3fsFileSystem::Multiwrite, multiwrite bytes: {}", totalBytesWrite.load());
    return totalBytesWrite.load(); // Return the total bytes write
}

ssize_t S3fsFileSystem::Write(int fd, const void* buf, size_t count) {
    // std::cout << "S3fsFileSystem::Write: " << fd << std::endl;
    spdlog::debug("S3fsFileSystem::Write, fd: {}, count: {}", fd, count);
    return posix_s3fs_write(fd, buf, count);
}
int S3fsFileSystem::Close(int fd) {
    //std::cout << "S3fsFileSystem::Close: " << fd << std::endl;
    spdlog::info("S3fsFileSystem::Close, fd: {}", fd);
    return posix_s3fs_close(fd);
}

off_t S3fsFileSystem::Lseek(int fd, off_t offset, int whence) {
    //std::cout << "S3fsFileSystem::Lseek: " << fd << std::endl;
    spdlog::debug("S3fsFileSystem::Lseek, fd: {}, offset: {}, whence: {}", fd, offset, whence);
    return posix_s3fs_lseek(fd, offset, whence);
}

int S3fsFileSystem::Stat(const char* path, struct stat* statbuf) {
    // std::cout << "S3fsFileSystem::Stat: " << path << std::endl;
    spdlog::info("S3fsFileSystem::Stat, path: {}", path);
    int ret = posix_s3fs_stat(path, statbuf);
    return ret;
}

int S3fsFileSystem::Fstat(int fd, struct stat* statbuf) {
    // std::cout << "S3fsFileSystem::Fstat: " << fd << std::endl;
    spdlog::info("S3fsFileSystem::Stat, fd: {}", fd);
    int ret = posix_s3fs_fstat(fd, statbuf);
    return ret;
}

int S3fsFileSystem::Fsync(int fd) {
    // std::cout << "S3fsFileSystem::Fsync: " << fd << std::endl;
    spdlog::info("S3fsFileSystem::Fsync, fd: {} no implement....", fd);
    return 0;
}

int S3fsFileSystem::Ftruncate(int fd, off_t length) {
   //  std::cout << "S3fsFileSystem::Ftruncate: " << fd << " " << length << std::endl;
    spdlog::info("S3fsFileSystem::Ftruncate, fd: {}  length: {} no implement...", fd, length);
    return 0;
}

int S3fsFileSystem::Unlink(const char* path) {
    // std::cout << "S3fsFileSystem::Unlink: " << path << std::endl;
    spdlog::info("S3fsFileSystem::Unlink, path: {}", path);
    return posix_s3fs_unlink(path);
}
int S3fsFileSystem::Mkdir(const char* path, mode_t mode) {
    // std::cout << "S3fsFileSystem::Mkdir: " << path << " " << mode << std::endl;
    spdlog::info("S3fsFileSystem::Mkdir, path: {}  mode: {}", path, mode);
    return posix_s3fs_mkdir(path, mode);
}

int S3fsFileSystem::Opendir(const char* path, DirStream* dirstream) {
    int ret = posix_s3fs_opendir(path, (S3DirStream*)dirstream);
    // std::cout << "S3fsFileSystem::Opendir: " << path << std::endl;
    spdlog::info("S3fsFileSystem::Opendir path: {}  ret {}", path, ret);
    return 0;
}

int S3fsFileSystem::Getdents(DirStream* dirstream, char* contents, size_t maxread, ssize_t* realbytes) {
    //std::cout << "S3fsFileSystem::Getdents: " << dirstream  << " " << maxread << " " << realbytes << std::endl;
    int ret = posix_s3fs_getdents((S3DirStream*)dirstream, contents, maxread, realbytes);
    spdlog::info("S3fsFileSystem::Getdents, maxread: {}, realbytes: {}", maxread, *realbytes);
    return ret;
}

int S3fsFileSystem::Closedir(DirStream* dirstream) {
    // std::cout << "S3fsFileSystem::Closedir: " << dirstream << std::endl;
    int ret = posix_s3fs_closedir((S3DirStream*)dirstream);
    spdlog::info("S3fsFileSystem::Closedir, ret: {}", ret);
    return ret;
}

int S3fsFileSystem::Rmdir(const char* path) {
    std::cout << "S3fsFileSystem::Rmdir: " << path << std::endl;
    return 0;
}

int S3fsFileSystem::Rename(const char* from, const char* to) {
    std::cout << "S3fsFileSystem::Rename: " << from << " to " << to << std::endl;
    return 0;
}

int S3fsFileSystem::Link(const char* oldpath, const char* newpath) {
    throw std::runtime_error("未实现");
}

int S3fsFileSystem::Symlink(const char* oldpath, const char* newpath) {
    throw std::runtime_error("未实现");
}

int S3fsFileSystem::Readlink(const char* path, char* buf, size_t bufsize) {
    throw std::runtime_error("未实现");
}

int S3fsFileSystem::Chmod(const char* path, mode_t mode) {
    throw std::runtime_error("未实现");
}

int S3fsFileSystem::Chown(const char* path, uid_t uid, gid_t gid) {
    throw std::runtime_error("未实现");
}

int S3fsFileSystem::Truncate(const char* path, off_t length) {
    std::cout << "S3fsFileSystem::Truncate" << std::endl;
    return 0;
}

int S3fsFileSystem::Utime(const char* path, const struct utimbuf* ubuf) {
    throw std::runtime_error("未实现");
}


std::string S3fsFileSystem::NormalizePath(const std::string& path) {
    throw std::runtime_error("未实现");
}

} // namespace filesystem
} // namespace intercept
