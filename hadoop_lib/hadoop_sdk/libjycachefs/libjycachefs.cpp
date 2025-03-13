/*
 *  Copyright (c) 2023 # Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/*
 * Project: JYCache
 * Created Date: 2023-07-12
 * Author: Jingli Chen (Wine93)
 */

//#include "jycachefs/src/client/filesystem/error.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <memory.h>
#include <dirent.h>
#include <limits.h>
#include <fcntl.h>

#include "hadoop_sdk/libjycachefs/libjycachefs.h"
#include "s3fs/include/s3fs_lib.h"

//using ::jycachefs::client::filesystem::JYCACHEFS_ERROR;
//using ::jycachefs::client::filesystem::SysErr;
//using ::jycachefs::client::vfs::DirStream;
//using ::jycachefs::client::vfs::DirEntry;

//static jycachefs_mount_t* get_instance(uintptr_t instance_ptr) {
//}

//static void stream_cast(DirStream* stream, dir_stream_t* dir_stream) {
//}

uintptr_t jycachefs_create() {
    printf("jycachefs create\n");
    return  static_cast<uintptr_t>(0x12345678);
}

void jycachefs_release(uintptr_t instance_ptr) {
    printf("jycachefs release\n");
    return ;
}

void jycachefs_conf_set(uintptr_t instance_ptr,
                      const char* key,
                      const char* value) {
    printf("conf set ...\n");
    return;
}

int jycachefs_mount(uintptr_t instance_ptr,
                  const char* fsname,
                  const char* mount_point) {
    printf("mount %s, mountpoint: %s\n", fsname, mount_point);
    s3fs_global_init();
    printf("mount %s end, mountpoint: %s\n", fsname, mount_point);
    return 0;
}

int jycachefs_umonut(uintptr_t instance_ptr,
                   const char* fsname,
                   const char* mountpoint) {
    printf("umount %s\n", fsname);
    
    s3fs_global_uninit();
    // s3fs_stop();

    printf("umount %s end\n", fsname);
    return 0;
}

int jycachefs_mkdir(uintptr_t instance_ptr, const char* path, uint16_t mode) {
    printf("mkdir %s, mode: %o\n", path, (int)mode);

    if(strcmp(path, "/") == 0){
        return 0;
    }

    int ret = 0;

    mode_t old_umask = umask(0);
    ret = posix_s3fs_mkdir(path, mode);
    umask(old_umask);

    printf("mkdir %s, mode: %o, ret: %d\n", path, (int)mode, ret);
    return -ret;
}

int jycachefs_mkdirs(uintptr_t instance_ptr, const char* path, uint16_t mode) {
    printf("mkdirs %s, mode: %o\n", path, (int)mode);

    if(strcmp(path, "/") == 0){
        return 0;
    }

    char *p = strdup(path);

    if (!p){
        printf("mkdirs %s, errno: %d\n", path, errno);
        return errno;
    }

    char *sep = strchr(p+1, '/');
    while(sep != NULL)
    {
        *sep = '\0';
        
        auto ret = jycachefs_mkdir(instance_ptr, p, mode);

        if (ret != 0 && ret != EEXIST){
            printf("mkdirs %s, ret: %d, mode: %o\n", p, ret, (int)mode);
            free(p);
            return ret;
        }

        *sep = '/';
        sep = strchr(sep+1, '/');
    }

    auto ret = jycachefs_mkdir(instance_ptr, p, mode);
    printf("mkdirs %s, ret: %d, mode: %o\n", p, ret, (int)mode);
    free(p);
    return ret;
}

int jycachefs_rmdir(uintptr_t instance_ptr, const char* path) {
    printf("rmdir %s\n", path);
    
    if(strcmp(path, "/") == 0){
        return 0;
    }

    int ret = posix_s3fs_rmdir(path);

    printf("rmdir %s, ret: %d\n", path, ret);
    return ret;
}

int jycachefs_opendir(uintptr_t instance_ptr,
                    const char* path,
                    dir_stream_t* dir_stream) {
    printf("opendir %s\n", path);
    
    int ret = posix_s3fs_opendir(path, (S3DirStream *)dir_stream);
    printf("opendir %s, ret: %d, dir stream: %p\n", path, ret, dir_stream);

    if (ret < 0){
        return ret;
    }

    auto fd_nouse = ret;
    return 0;
}

ssize_t jycachefs_readdir(uintptr_t instance_ptr,
                        dir_stream_t* dir_stream,
                        dirent_t* dirent) {
    printf("readdir: %p\n", dir_stream);

    char buf[sizeof(struct dirent64)];
    ssize_t real_bytes = 0;

    auto ret = posix_s3fs_getdents((S3DirStream *)dir_stream, buf, 1, &real_bytes);

    printf("readdir %p, ret: %d, real bytes: %lld, stream: \n", dir_stream, ret, (long long int) real_bytes);

    if (ret != 0){
        return -1;
    }

    if (real_bytes == 0){
        return 0;
    }

    dirent64 *d = (dirent64 *) buf;
    memcpy(dirent->name, d->d_name, sizeof(dirent->name));
    dirent->name[sizeof(dirent->name) - 1] = '\0';
    memset((char *)&(dirent->stat), '0', sizeof(dirent->stat));

    printf("readdir %p, dir: %s\n", dir_stream, dirent->name);
    return 1;
}

int jycachefs_closedir(uintptr_t instance_ptr, dir_stream_t* dir_stream) {
    printf("close dir: %p\n", dir_stream);

    int ret = posix_s3fs_closedir((S3DirStream*) dir_stream);

    printf("cloese dir: %p, ret: %d\n", dir_stream, ret);
    return ret;
}

int jycachefs_listdir(uintptr_t instance_ptr, const char* path, dirent_t* dirent, int *dir_num){
    printf("listdir %s\n", path);
    dir_stream_t dir_stream;

    auto open_ret = posix_s3fs_opendir(path, (S3DirStream *)&dir_stream);

    if (open_ret < 0){
        printf("listdir open %s, ret: %d\n", path, open_ret);
        return open_ret;
    }

    const int max_dir_num = 10240;
    int max_dir_size = sizeof(dirent64) * max_dir_num;
    char *buf = (char *) malloc(max_dir_size);

    if (!buf){
        auto ignore_ret = posix_s3fs_closedir((S3DirStream *)&dir_stream);
        return ENOMEM;
    }

    ssize_t real_bytes = 0;
    auto getdents_ret = posix_s3fs_getdents((S3DirStream *)&dir_stream, buf, max_dir_num, &real_bytes);

    auto close_ret = posix_s3fs_closedir((S3DirStream *)&dir_stream);

    if (getdents_ret != 0){
        free(buf);
        printf("listdir getdents %s, ret: %d\n", path, getdents_ret);
        return -1;
    }

    if (real_bytes > max_dir_size){
        free(buf);
        printf("listdir getdents %s, real bytes: %lld, max dir size: %d\n", path, (long long int)real_bytes, max_dir_size);
        abort();
        return ERANGE;
    }

    *dir_num = 0;

    for(int pos = 0; pos < real_bytes; ){
        dirent64 *s3_dir = (dirent64 *)(buf + pos);
        dirent_t *dir = dirent + (*dir_num);
        memcpy(dir->name, s3_dir->d_name, sizeof(dir->name));
        dir->stat.st_ino = s3_dir->d_ino;
        pos += s3_dir->d_reclen;
        (*dir_num) += 1;
    }

    free(buf);

    if (close_ret != 0){
        printf("listdir closedir %s, ret: %d\n", path, close_ret);
        return close_ret;
    }

    printf("listdir %s: num: %d\n", path, *dir_num);
    return 0;
}

int jycachefs_open(uintptr_t instance_ptr,
                 const char* path,
                 uint32_t flags,
                 uint16_t mode) {
    printf("open path: %s, flags: %o, mode: %o\n", path, flags, mode);

    int ret = posix_s3fs_open(path, flags, mode);

    printf("open path: %s, ret: %d, flags: %o, mode: %o\n", path, ret, flags, (int)mode);
    return ret;
}

int jycachefs_lseek(uintptr_t instance_ptr,
                  int fd,
                  uint64_t offset,
                  int whence) {
    printf("lseek fd: %d, offset: %lld, whence: %d\n", fd, (long long int)offset, whence);

    int ret = posix_s3fs_lseek(fd, offset, whence);
    printf("lseek fd: %d, ret: %d\n", fd, ret);

    if (ret < 0){
        if (errno != 0){
            ret = errno;
        }else{
            ret = -ret;
        }
    }else{
        ret = 0;
    }
    return ret;
}

ssize_t jycachefs_read(uintptr_t instance_ptr,
                     int fd,
                     char* buffer,
                     size_t count) {
    printf("read %d, count: %lld\n", fd, (long long int)count);

    int ret = posix_s3fs_read(fd, buffer, count);

    printf("read %d, ret: %lld\n", fd, (long long int)ret);
    return (ssize_t)ret;
}

ssize_t jycachefs_write(uintptr_t instance_ptr,
                      int fd,
                      char* buffer,
                      size_t count) {
    printf("write %d, count: %lld\n", fd, (long long int)count);

    auto ret = posix_s3fs_write(fd, buffer, count);

    printf("write: %d, ret: %lld\n", fd, (long long int)ret);
    return (ssize_t)ret;
}

int jycachefs_fsync(uintptr_t instance_ptr, int fd) {
    printf("fsync to do%d\n", fd);
    return 0;
}

int jycachefs_close(uintptr_t instance_ptr, int fd) {
    printf("close %d\n", fd);

    int ret = posix_s3fs_close(fd);

    printf("close %d, ret: %d\n", fd, ret);
    return ret;
}

int jycachefs_unlink(uintptr_t instance_ptr, const char* path) {
    printf("unlink %s\n", path);

    int ret = posix_s3fs_unlink(path);

    printf("unlink %s, ret: %d\n", path, ret);
    return ret;
}

int jycachefs_statfs(uintptr_t instance_ptr,
                   struct statvfs* statvfs) {
    printf("statfs to do!\n");
    statvfs->f_frsize = 512;
    statvfs->f_blocks = (long)100 * 1024 * 1024 * 1024;
    statvfs->f_bavail = (long)100 * 1024 * 1024 * 1024;

    statvfs->f_files = 1024 * 1024 * 1024;
    statvfs->f_ffree = 1024 * 1024 * 1024;

    statvfs->f_fsid = 128;

    statvfs->f_flag = 0;
    statvfs->f_namemax = 256;
    return 0;
}

int jycachefs_lstat(uintptr_t instance_ptr, const char* path, struct stat* file_stat) {
    if (!path || !file_stat){
        abort();
    }    

    printf("lstat %s\n", path);

    int ret = posix_s3fs_stat(path, file_stat);

    printf("lstat %s, ret: %d\n", path, ret);
    return -ret;
}

int jycachefs_fstat(uintptr_t instance_ptr, int fd, struct stat* stat) {
    printf("fstat %d\n", fd);

    int  ret = posix_s3fs_fstat(fd, stat);

    printf("fstat %d, ret: %d\n", fd, ret);
    return -ret;
}

int jycachefs_setattr(uintptr_t instance_ptr,
                    const char* path,
                    struct stat* stat,
                    int to_set) {
    printf("setattr to do\n");
    return 0;
}

int jycachefs_chmod(uintptr_t instance_ptr, const char* path, uint16_t mode) {
    printf("chmode to do\n");
    return 0;
}

int jycachefs_chown(uintptr_t instance_ptr,
                  const char* path,
                  uint32_t uid,
                  uint32_t gid) {
    printf("chown to do\n");
    return 0;
}

int jycachefs_rename(uintptr_t instance_ptr,
                   const char* oldpath,
                   const char* newpath) {
    printf("rename %s -> %s\n", oldpath, newpath);

    int ret = posix_s3fs_rename(oldpath, newpath);

    printf("rename %s -> %s, ret: %d\n", oldpath, newpath, ret);
    return ret; 
}
