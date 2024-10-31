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

#ifndef JYCACHEFS_SDK_LIBJYCACHEFS_LIBJYCACHEFS_H_
#define JYCACHEFS_SDK_LIBJYCACHEFS_LIBJYCACHEFS_H_

#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef __cplusplus

#include <string>
#include <sstream>
#include <memory>

//#include "jycachefs/src/client/vfs/config.h"
//#include "jycachefs/src/client/vfs/vfs.h"
//
//using ::jycachefs::client::vfs::Configure;
//using ::jycachefs::client::vfs::VFS;
//#include "client/include/s3fs_lib.h"

typedef struct {
//    std::shared_ptr<Configure> cfg;
//    std::shared_ptr<VFS> vfs;
} jycachefs_mount_t;

#endif  // __cplusplus

// Must be synchronized with DirStream if changed
typedef struct {
    uint64_t ino;
    uint64_t fh;
    uint64_t offset;
} dir_stream_t;

typedef struct {
    struct stat stat;
    char name[256];
} dirent_t;

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((visibility("default")))
uintptr_t jycachefs_create();

__attribute__((visibility("default")))
void jycachefs_release(uintptr_t instance_ptr);

// NOTE: instance_ptr is the pointer of jycachefs_mount_t instance.
__attribute__((visibility("default")))
void jycachefs_conf_set(uintptr_t instance_ptr,
                      const char* key,
                      const char* value);

__attribute__((visibility("default")))
int jycachefs_mount(uintptr_t instance_ptr,
                  const char* fsname,
                  const char* mountpoint);

__attribute__((visibility("default")))
int jycachefs_umonut(uintptr_t instance_ptr,
                   const char* fsname,
                   const char* mountpoint);

// directory
__attribute__((visibility("default")))
int jycachefs_mkdir(uintptr_t instance_ptr, const char* path, uint16_t mode);

__attribute__((visibility("default")))
int jycachefs_mkdirs(uintptr_t instance_ptr, const char* path, uint16_t mode);

__attribute__((visibility("default")))
int jycachefs_rmdir(uintptr_t instance_ptr, const char* path);

__attribute__((visibility("default")))
int jycachefs_opendir(uintptr_t instance_ptr,
                    const char* path,
                    dir_stream_t* dir_stream);

__attribute__((visibility("default")))
ssize_t jycachefs_readdir(uintptr_t instance_ptr,
                        dir_stream_t* dir_stream,
                        dirent_t* dirent);

__attribute__((visibility("default")))
int jycachefs_closedir(uintptr_t instance_ptr, dir_stream_t* dir_stream);

__attribute__((visibility("default")))
int jycachefs_listdir(uintptr_t instance_ptr, const char* path, dirent_t* dirent, int *dir_num);

// file
__attribute__((visibility("default")))
int jycachefs_open(uintptr_t instance_ptr,
                 const char* path,
                 uint32_t flags,
                 uint16_t mode);

__attribute__((visibility("default")))
int jycachefs_lseek(uintptr_t instance_ptr,
                  int fd,
                  uint64_t offset,
                  int whence);

__attribute__((visibility("default")))
ssize_t jycachefs_read(uintptr_t instance_ptr,
                     int fd,
                     char* buffer,
                     size_t count);

__attribute__((visibility("default")))
ssize_t jycachefs_write(uintptr_t instance_ptr,
                      int fd,
                      char* buffer,
                      size_t count);

__attribute__((visibility("default")))
int jycachefs_fsync(uintptr_t instance_ptr, int fd);

__attribute__((visibility("default")))
int jycachefs_close(uintptr_t instance_ptr, int fd);

__attribute__((visibility("default")))
int jycachefs_unlink(uintptr_t instance_ptr, const char* path);

// others
__attribute__((visibility("default")))
int jycachefs_statfs(uintptr_t instance_ptr, struct statvfs* statvfs);

__attribute__((visibility("default")))
int jycachefs_lstat(uintptr_t instance_ptr, const char* path, struct stat* stat);

__attribute__((visibility("default")))
int jycachefs_fstat(uintptr_t instance_ptr, int fd, struct stat* stat);

__attribute__((visibility("default")))
int jycachefs_setattr(uintptr_t instance_ptr,
                    const char* path,
                    struct stat* stat,
                    int to_set);

__attribute__((visibility("default")))
int jycachefs_chmod(uintptr_t instance_ptr, const char* path, uint16_t mode);

__attribute__((visibility("default")))
int jycachefs_chown(uintptr_t instance_ptr,
                  const char* path,
                  uint32_t uid,
                  uint32_t gid);

__attribute__((visibility("default")))
int jycachefs_rename(uintptr_t instance_ptr,
                   const char* oldpath,
                   const char* newpath);
#ifdef __cplusplus
}
#endif

#endif  // JYCACHEFS_SDK_LIBJYCACHEFS_LIBJYCACHEFS_H_
