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
#include <sys/syscall.h>

#include "hadoop_sdk/libjycachefs/libjycachefs.h"
#include "jycachefs_ipc/include/posix_op.h"

//using ::jycachefs::client::filesystem::JYCACHEFS_ERROR;
//using ::jycachefs::client::filesystem::SysErr;
//using ::jycachefs::client::vfs::DirStream;
//using ::jycachefs::client::vfs::DirEntry;

//static jycachefs_mount_t* get_instance(uintptr_t instance_ptr) {
//}

//static void stream_cast(DirStream* stream, dir_stream_t* dir_stream) {
//}

#define SYSCALL_ARGS_NUM 6

static void get_intercept_path(const char *path, char *intercept_path, size_t dest_max_len){
    const char *intercept_prefix = "/testdir";
    
    if (strncmp(path, intercept_prefix, strlen(intercept_prefix)) == 0){
        strncpy(intercept_path, path, dest_max_len);
        return;
    }

    strncpy(intercept_path, intercept_prefix, dest_max_len);
    strncat(intercept_path, path, dest_max_len);
    return;
}

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
    ThreadInit();
    printf("mount %s end, mountpoint: %s\n", fsname, mount_point);
    return 0;
}

int jycachefs_umonut(uintptr_t instance_ptr,
                   const char* fsname,
                   const char* mountpoint) {
    printf("umount %s\n", fsname);
    printf("umount %s end\n", fsname);
    return 0;
}

int jycachefs_mkdir(uintptr_t instance_ptr, const char* path, uint16_t mode) {
    printf("mkdir %s, mode: %o\n", path, (int)mode);

    if(strncmp(path, "/", 1) != 0){
        printf("mkdir not start with / ! path: %s\n", path);
        return -1;
    }

    char intercept_path[PATH_MAX] = {0};
    get_intercept_path(path, intercept_path, sizeof(intercept_path) - 1);

    long args[SYSCALL_ARGS_NUM] = {0};
    long result = 0;
    args[0] = (long)intercept_path;
    args[1] = (long) mode;
    PosixOpMkDir(args, &result);

    printf("mkdir %s => %s, mode: %o, ret: %d\n", path, intercept_path, (int)mode, (int)result);
    return -(result);
}

int jycachefs_mkdirs(uintptr_t instance_ptr, const char* path, uint16_t mode) {
    printf("mkdirs %s, mode: %o\n", path, (int)mode);

    if(strncmp(path, "/", 1) != 0){
        printf("mkdirs not start with / ! path: %s\n", path);
        return -1;
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

    if(strncmp(path, "/", 1) != 0 || strcmp(path, "/") == 0){
        printf("rm not start with / or is / ! path: %s\n", path);
        return -1;
    }

    long args[SYSCALL_ARGS_NUM] = {0};
    long result = 0;
    char intercept_path[PATH_MAX];
    get_intercept_path(path, intercept_path, sizeof(intercept_path) - 1);
    
    args[0] = -1;
    args[1] = (long) intercept_path;
    args[2] = (long) AT_REMOVEDIR;
    PosixOpUnlinkat(args, &result);
    printf("rmdir %s => %s, ret: %d\n", path, intercept_path, (int)result);
    return (int)result;
}

int jycachefs_getdents64(uintptr_t instance_ptr, int fd, dirent_t *dirs, size_t max_count) {
    printf("getdents64 %d\n", fd);

    const size_t max_read_count = 1024;

    if (max_count < max_read_count) {
        printf("ERROR: getdents buf too small! count: %lld, max_read_count: %lld\n",
               (long long int) max_count, (long long int) max_read_count);
        return -1;
    }

    long args[SYSCALL_ARGS_NUM] = {0};
    long result = 0;

    char read_dirs[max_read_count * sizeof(dirent64)] = {0};

    args[0] = (long) fd;
    args[1] = (long) read_dirs;
    args[2] = (long) sizeof(read_dirs);

    PosixOpGetdents64(args, &result);
    printf("getdents64 %d, ret: %d\n", fd, (int) result);

    if (result < 0){
        return result;
    }

    if (result % sizeof(dirent64) != 0) {
        printf("ERROR: getdents read not full! ret: %d, dirent64 size: %d\n",
               (int) result, (int) sizeof(dirent64));
        return -1;
    }

    int dir_num = 0;

    for (int pos = 0; pos < result; ){
        dirent64 *src_dir = (dirent64 *)(read_dirs + pos);
        dirent_t *dest_dir = dirs + dir_num;
        strncpy(dest_dir->name, src_dir->d_name, sizeof (dest_dir->name));
        printf("getdents64 %d, read: %s\n", fd, dest_dir->name);
        dest_dir->stat.st_ino = src_dir->d_ino;
        pos += src_dir->d_reclen;
        dir_num += 1;
    }

    return dir_num;
}

int jycachefs_open(uintptr_t instance_ptr,
                 const char* path,
                 uint32_t flags,
                 uint16_t mode) {
    printf("open path: %s, flags: %o, mode: %o\n", path, flags, mode);

    long args[SYSCALL_ARGS_NUM] = {0};
    long result = 0;
    char intercept_path[PATH_MAX] = {0};
    get_intercept_path(path, intercept_path, sizeof(intercept_path) - 1);

    args[0] = (long) intercept_path;
    args[1] = (long) flags;
    args[2] = (long) mode;

    PosixOpOpen(args, &result);
    printf("open path: %s => %s, ret: %d, flags: %o, mode: %o\n",
           path, intercept_path, (int)result, flags, (int)mode);

    return result;
}

int jycachefs_lseek(uintptr_t instance_ptr,
                  int fd,
                  uint64_t offset,
                  int whence) {
    printf("lseek fd: %d, offset: %lld, whence: %d\n", fd, (long long int)offset, whence);

    long args[SYSCALL_ARGS_NUM] = {0};
    long result = 0;
    args[0] = (long) fd;
    args[1] = (long) offset;
    args[2] = (long) whence;

    errno = 0;
    PosixOpLseek(args, &result);
    printf("lseek fd: %d, ret: %ld\n", fd, result);

    if (result < 0){
        if (errno != 0){
            result = errno;
        }else{
            result = -result;
        }
    }else{
        result = 0;
    }
    return (int)result;
}

ssize_t jycachefs_read(uintptr_t instance_ptr,
                     int fd,
                     char* buffer,
                     size_t count) {
    // printf("read %d, count: %lld\n", fd, (long long int)count);

    long args[SYSCALL_ARGS_NUM] = {0};
    long result = 0;
    args[0] = (long) fd;
    args[1] = (long) buffer;
    args[2] = (long) count;

    PosixOpRead(args, &result);
    // printf("read %d, ret: %lld\n", fd, (long long int)result);

    return (ssize_t)result;
}

ssize_t jycachefs_write(uintptr_t instance_ptr,
                      int fd,
                      char* buffer,
                      size_t count) {
    // printf("write %d, count: %lld\n", fd, (long long int)count);

    long args[SYSCALL_ARGS_NUM] = {0};
    long result = 0;

    args[0] = (long) fd;
    args[1] = (long) buffer;
    args[2] = (long) count;

    PosixOpWrite(args, &result);
    // printf("write: %d, ret: %lld\n", fd, (long long int)result);

    return (ssize_t)result;
}

int jycachefs_fsync(uintptr_t instance_ptr, int fd) {
    printf("fsync %d\n", fd);

    long args[SYSCALL_ARGS_NUM] = {0};
    long result = 0;

    args[0] = (long) fd;

    PosixOpFsync(args, &result);
    printf("fsync %d, ret: %ld\n", fd, result);

    return (int) result;
}

int jycachefs_close(uintptr_t instance_ptr, int fd) {
    printf("close %d\n", fd);

    long args[SYSCALL_ARGS_NUM] = {0};
    long result = 0;

    args[0] = (long) fd;

    PosixOpClose(args, &result);
    printf("close %d, ret: %ld\n", fd, result);

    return (int)result;
}

int jycachefs_unlink(uintptr_t instance_ptr, const char* path) {
    printf("unlink %s\n", path);

    long args[SYSCALL_ARGS_NUM];
    long result;

    char intercept_path[PATH_MAX] = {0};
    get_intercept_path(path, intercept_path, sizeof(intercept_path) - 1);

    args[0] = (long) intercept_path;

    PosixOpUnlink(args, &result);
    printf("unlink %s => %s, ret: %ld\n", path, intercept_path, result);

    return result;
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
    printf("lstat %s\n", path);

    long args[SYSCALL_ARGS_NUM] = {0};
    long result = 0;

    char intercept_path[PATH_MAX] = {0};
    get_intercept_path(path, intercept_path, sizeof(intercept_path) - 1);

    args[0] = (long) intercept_path;
    args[1] = (long) file_stat;

    PosixOpLstat(args, &result);
    printf("lstat %s => %s, ret: %ld\n", path, intercept_path, result);

    return (int)-result;
}

int jycachefs_fstat(uintptr_t instance_ptr, int fd, struct stat* stat) {
    printf("fstat %d\n", fd);

    long args[SYSCALL_ARGS_NUM];
    long result = 0;

    args[0] = (long) fd;
    args[1] = (long) stat;

    PosixOpFstat(args, &result);
    printf("fstat %d, ret: %ld\n", fd, result);

    return (int)-result;
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

    char old_intercept_path[PATH_MAX] = {0};
    char new_intercept_path[PATH_MAX] = {0};
    get_intercept_path(oldpath, old_intercept_path, sizeof(old_intercept_path) - 1);
    get_intercept_path(newpath, new_intercept_path, sizeof(new_intercept_path) - 1);

    long args[SYSCALL_ARGS_NUM] = {0};
    long result = 0;

    args[1] = (long) old_intercept_path;
    args[3] = (long) new_intercept_path;

    PosixOpRenameat(args, &result);
    printf("rename %s => %s -> %s => %s, ret: %ld\n",
           oldpath, old_intercept_path, newpath, new_intercept_path, result);

    return (int)result;
}
