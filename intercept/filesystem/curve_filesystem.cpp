#include <iostream>
#include <cstring>

#include "curve_filesystem.h"
#include "libcurvefs_external.h"

namespace intercept {
namespace filesystem {

#define POSIX_SET_ATTR_SIZE  (1 << 3)
CurveFileSystem::CurveFileSystem() {}
CurveFileSystem::~CurveFileSystem() {
    curvefs_release(instance_);
}

void CurveFileSystem::Init() {
    instance_ = curvefs_create();
    curvefs_load_config(instance_, "./curve_posix_client.conf");
    //curvefs_mount(instance_, "s3cy1", "/tmp/curvefs");
    curvefs_mount(instance_, "s3cy1", "/");
    std::cout << "finish curvefs create" << std::endl;
}

void CurveFileSystem::Shutdown() {

}

int CurveFileSystem::Open(const char* path, int flags, int mode) {
    std::cout << "open, the path: " << path << std::endl;
    int ret =  curvefs_open(instance_, path, flags, mode);
    // 注意，EEXIST为17， 那么当fd ret也是17时 是不是就会判断错误。
    if (ret ==  EEXIST) { // 不去创建
        ret = curvefs_open(instance_, path, flags & ~O_CREAT, mode);
    }
    //std::cout << "the path: " << path << " , the stat: " << tmp.st_size  << " , the time: " << tmp.st_mtime << std::endl;
    return ret;
}

ssize_t CurveFileSystem::Read(int fd, void* buf, size_t count) {
    int ret = curvefs_read(instance_, fd, (char*)buf, count);
    //int ret = count;
    //std::cout << "read, the fd: " << fd  << " the buf: " << (char*)buf << ", the count: " << count << ", the ret: " << ret << std::endl;
    return ret;
}

ssize_t CurveFileSystem::Write(int fd, const void* buf, size_t count) {
    int ret = curvefs_write(instance_, fd, (char*)buf, count);
    //int ret = count;
    //std::cout << "write, the fd: " << fd  << " the buf: " << (char*)buf << ", the count: " << count << ", the ret: " << ret << std::endl;
    return ret;
}

int CurveFileSystem::Close(int fd) {
    int ret = curvefs_close(instance_, fd);
    std::cout << "curve close, the fd: " << fd << std::endl;
    return ret;
}

off_t CurveFileSystem::Lseek(int fd, off_t offset, int whence) {
    int ret = curvefs_lseek(instance_, fd, offset, whence);
    std::cout << "curve lseek, the fd: " << fd << ", the offset: " << offset << ", the whence: " << whence << ", the ret: " << ret << std::endl;
    return ret;
}

int CurveFileSystem::Stat(const char* path, struct stat* st) {
    int ret = curvefs_lstat(instance_, path, st);
    return ret;
}

int CurveFileSystem::Fstat(int fd, struct stat* st) {
    int ret = curvefs_fstat(instance_, fd, st);
    return ret;
}

int CurveFileSystem::Fsync(int fd) {
    int ret = curvefs_fsync(instance_, fd);
    return ret;
}

int CurveFileSystem::Ftruncate(int fd, off_t length) {
    throw std::runtime_error("未实现");
}

int CurveFileSystem::Unlink(const char* path) {
    int ret = curvefs_unlink(instance_, path);
    std::cout << "unlink, the path: " << path << ", the ret: " << ret << std::endl;
    return ret;
}

int CurveFileSystem::Mkdir(const char* path, mode_t mode) {
    int ret = curvefs_mkdir(instance_, path, mode);
    std::cout << "mkdir, the path: " << path << ", the mode: " << mode << ", the ret: " << ret << std::endl;
    return ret;
}

int CurveFileSystem::Opendir(const char* path, DirStream* dirstream) {
    int ret = curvefs_opendir(instance_, path, (dir_stream_t*)dirstream);
    std::cout << "opendir, the path: " << path << ", the dirstream ino: " << dirstream->ino << ", the ret: " << ret << std::endl;
    return ret;
}

int CurveFileSystem::Getdents(DirStream* dirstream, char* contents, size_t maxread, ssize_t* realbytes) {
    int ret = curvefs_getdents(instance_, (dir_stream_t*)dirstream, contents, maxread, realbytes);
    std::cout << "getdents, the dirstream ino: " << dirstream->ino  << ", the maxread: " << maxread << ", the realbytes: " << realbytes << ", the ret: " << ret << std::endl;
    return ret;
}

int CurveFileSystem::Closedir(DirStream* dirstream) {
    int ret = curvefs_closedir(instance_, (dir_stream_t*)dirstream);;
    std::cout << "closedir, the fd: " << dirstream->fh << "  ino:" << dirstream->ino  << std::endl;
    return ret;
}

int CurveFileSystem::Rmdir(const char* path) {
    int ret = curvefs_rmdir(instance_, path);
    std::cout << "rmdir, the path: " << path << ", the ret: " << ret << std::endl;
    return ret;
}

int CurveFileSystem::Rename(const char* oldpath, const char* newpath) {
    int ret = curvefs_rename(instance_, oldpath, newpath);
    std::cout << "rename, the oldpath: " << oldpath << ", the newpath: " << newpath << ", the ret: " << ret << std::endl;
    return ret;

}
int CurveFileSystem::Link(const char* oldpath, const char* newpath) {
    throw std::runtime_error("未实现");
}

int CurveFileSystem::Symlink(const char* oldpath, const char* newpath) {
    throw std::runtime_error("未实现");
}

int CurveFileSystem::Readlink(const char* path, char* buf, size_t bufsize) {
    throw std::runtime_error("未实现");
}

int CurveFileSystem::Chmod(const char* path, mode_t mode) {
    throw std::runtime_error("未实现");
}

int CurveFileSystem::Chown(const char* path, uid_t uid, gid_t gid) {
    throw std::runtime_error("未实现");
}

int CurveFileSystem::Truncate(const char* path, off_t length) {
    struct stat attr;
    attr.st_size = length;
    int set = POSIX_SET_ATTR_SIZE ;
    int ret = curvefs_setattr(instance_, path, &attr, set);
    return ret;
}

int CurveFileSystem::Utime(const char* path, const struct utimbuf* ubuf) {
    throw std::runtime_error("未实现");
}



std::string CurveFileSystem::NormalizePath(const std::string& path) {
    throw std::runtime_error("未实现");
}


} // namespace filesystem
} // namespace intercept
