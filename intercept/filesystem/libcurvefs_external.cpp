/*
 *  Copyright (c) 2023 NetEase Inc.
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
 * Project: Curve
 * Created Date: 2023-07-12
 * Author: Jingli Chen (Wine93)
 */


#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/statvfs.h>

#include "libcurvefs_external.h"


uintptr_t curvefs_create() {return 0;}

void curvefs_load_config(uintptr_t instance_ptr,
                           const char* config_file) {}

void curvefs_release(uintptr_t instance_ptr) {}

// NOTE: instance_ptr is the pointer of curvefs_mount_t instance.
void curvefs_conf_set(uintptr_t instance_ptr,
                      const char* key,
                      const char* value) {}

int curvefs_mount(uintptr_t instance_ptr,
                  const char* fsname,
                  const char* mountpoint) {return 0;}

int curvefs_umonut(uintptr_t instance_ptr,
                   const char* fsname,
                   const char* mountpoint) {return 0;}

// directory
int curvefs_mkdir(uintptr_t instance_ptr, const char* path, uint16_t mode) {return 0;}

int curvefs_mkdirs(uintptr_t instance_ptr, const char* path, uint16_t mode) {return 0;}

int curvefs_rmdir(uintptr_t instance_ptr, const char* path) {return 0;}

int curvefs_opendir(uintptr_t instance_ptr,
                    const char* path,
                    dir_stream_t* dir_stream) {return 0;}

ssize_t curvefs_readdir(uintptr_t instance_ptr,
                        dir_stream_t* dir_stream,
                        dirent_t* dirent) {return 0;}

int curvefs_getdents(uintptr_t instance_ptr,
                      dir_stream_t* dir_stream,
                      char* data, size_t maxread, ssize_t* realbytes) {return 0;}

int curvefs_closedir(uintptr_t instance_ptr, dir_stream_t* dir_stream) {return 0;}

// file
int curvefs_open(uintptr_t instance_ptr,
                 const char* path,
                 uint32_t flags,
                 uint16_t mode) {return 0;}

int curvefs_lseek(uintptr_t instance_ptr,
                  int fd,
                  uint64_t offset,
                  int whence){return 0;}

ssize_t curvefs_read(uintptr_t instance_ptr,
                     int fd,
                     char* buffer,
                     size_t count) {return 0;}

ssize_t curvefs_write(uintptr_t instance_ptr,
                      int fd,
                      char* buffer,
                      size_t count) {return 0;}

int curvefs_fsync(uintptr_t instance_ptr, int fd) {return 0;}

int curvefs_close(uintptr_t instance_ptr, int fd) {return 0;}

int curvefs_unlink(uintptr_t instance_ptr, const char* path) {return 0;}

// others
int curvefs_statfs(uintptr_t instance_ptr, struct statvfs* statvfs) {return 0;}

int curvefs_lstat(uintptr_t instance_ptr, const char* path, struct stat* stat) {return 0;}

int curvefs_fstat(uintptr_t instance_ptr, int fd, struct stat* stat) {return 0;}

int curvefs_setattr(uintptr_t instance_ptr,
                    const char* path,
                    struct stat* stat,
                    int to_set) {return 0;}

int curvefs_chmod(uintptr_t instance_ptr, const char* path, uint16_t mode) {return 0;}

int curvefs_chown(uintptr_t instance_ptr,
                  const char* path,
                  uint32_t uid,
                  uint32_t gid) {return 0;}

int curvefs_rename(uintptr_t instance_ptr,
                   const char* oldpath,
                   const char* newpath) {return 0;}
