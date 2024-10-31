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

// #include <boost/filesystem/path.hpp>

#include "hadoop_sdk/libjycachefs/libjycachefs.h"
// #include "s3fs/include/s3fs_lib.h"

//using ::jycachefs::client::filesystem::JYCACHEFS_ERROR;
//using ::jycachefs::client::filesystem::SysErr;
//using ::jycachefs::client::vfs::DirStream;
//using ::jycachefs::client::vfs::DirEntry;

//static jycachefs_mount_t* get_instance(uintptr_t instance_ptr) {
//    return reinterpret_cast<jycachefs_mount_t*>(instance_ptr);
//}

//static void stream_cast(DirStream* stream, dir_stream_t* dir_stream) {
//    dir_stream->fh = stream->fh;
//    dir_stream->ino = stream->ino;
//    dir_stream->offset = stream->offset;
//}

static char sg_mountpoint[PATH_MAX] = {0};
static int sg_mountpoint_len = 0;


int get_posix_path(const char *path_under_mount, char *posix_path){
    char full_path[PATH_MAX * 2];
    snprintf(full_path, PATH_MAX * 2, "%s%s", sg_mountpoint, path_under_mount);
    
    if (strlen(full_path) >= PATH_MAX){
        printf("get posix path too long! full path: %s\n", full_path);
        return -1;
    }

    strcpy(posix_path, full_path);
    return 0;
}


uintptr_t jycachefs_create() {
//    auto mount = new jycachefs_mount_t();
//    mount->cfg = Configure::Default();
//    return reinterpret_cast<uintptr_t>(mount);
    printf("jycachefs create\n");
    return  static_cast<uintptr_t>(0x12345678);
}

void jycachefs_release(uintptr_t instance_ptr) {
//    auto mount = get_instance(instance_ptr);
//    delete mount;
//    mount = nullptr;
    printf("jycachefs release\n");
    return ;
}

void jycachefs_conf_set(uintptr_t instance_ptr,
                      const char* key,
                      const char* value) {
//    auto mount = get_instance(instance_ptr);
//    return mount->cfg->Set(key, value);
    printf("conf set ...\n");
    return;
}

int jycachefs_mount(uintptr_t instance_ptr,
                  const char* fsname,
                  const char* mount_point) {
//    auto mount = get_instance(instance_ptr);
//    mount->vfs = std::make_shared<VFS>(mount->cfg);
//    auto rc = mount->vfs->Mount(fsname, mountpoint);
//    return SysErr(rc);
    printf("mount %s, mountpoint: %s\n", fsname, mount_point);
    const char *default_mountpoint = "/home/jycachefs/mnt/";
    strcpy(sg_mountpoint, default_mountpoint);
    sg_mountpoint_len = strlen(sg_mountpoint);
    printf("mount %s end, mountpoint: %s, len: %d\n", fsname, sg_mountpoint, sg_mountpoint_len);
    return 0;
}

int jycachefs_umonut(uintptr_t instance_ptr,
                   const char* fsname,
                   const char* mountpoint) {
//    auto mount = get_instance(instance_ptr);
//    auto rc = mount->vfs->Umount(fsname, mountpoint);
//    return SysErr(rc);
    printf("umount %s\n", fsname);
    
    if(mountpoint){
        sg_mountpoint[0] = '\0';
        sg_mountpoint_len = 0;
    }

    printf("umount %s end\n", fsname);
    return 0;
}

int jycachefs_mkdir(uintptr_t instance_ptr, const char* path, uint16_t mode) {
//    auto mount = get_instance(instance_ptr);
//    auto rc = mount->vfs->MkDir(path, mode);
//    return SysErr(rc);
    printf("mkdir %s, mode: %o\n", path, (int)mode);

    if(strcmp(path, "/") == 0){
        return 0;
    }

    char posix_path[PATH_MAX];
    posix_path[0] = '\0';
    int ret = 0;

    ret = get_posix_path(path, posix_path);

    if( ret != 0){
        return ret;
    }

    mode_t old_umask = umask(0);
    ret = mkdir(posix_path, mode);
    umask(old_umask);

    if (ret != 0){
        ret = errno;
    }

    printf("mkdir %s, posix path: %s, mode: %o, ret: %d\n", path, posix_path, (int)mode, ret);
    return ret;
}

int jycachefs_mkdirs(uintptr_t instance_ptr, const char* path, uint16_t mode) {
//    auto mount = get_instance(instance_ptr);
//    auto rc = mount->vfs->MkDirs(path, mode);
//    return SysErr(rc);
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
//    auto mount = get_instance(instance_ptr);
//    auto rc = mount->vfs->RmDir(path);
//    return SysErr(rc);
    printf("rmdir %s\n", path);

    char posix_path[PATH_MAX];
    posix_path[0] = '\0';
    int ret = 0;

    ret = get_posix_path(path, posix_path);

    if (ret != 0){
        return ret;
    }

    ret = rmdir(posix_path);

    if (ret != 0){
        ret = errno;
    }

    printf("rmdir %s -- %s, ret: %d\n", path, posix_path, ret);
    return ret;
}

int jycachefs_opendir(uintptr_t instance_ptr,
                    const char* path,
                    dir_stream_t* dir_stream) {
//    DirStream stream;
//    auto mount = get_instance(instance_ptr);
//    auto rc = mount->vfs->OpenDir(path, &stream);
//    if (rc == JYCACHEFS_ERROR::OK) {
//        stream_cast(&stream, dir_stream);
//    }
//    return SysErr(rc);
    printf("opendir %s\n", path);
    
    int ret = 0;
    char posix_path[PATH_MAX];
    posix_path[0] = '\0';

    ret = get_posix_path(path, posix_path);

    if (ret != 0) {
        return ret;
    }

    DIR *d = opendir(posix_path);

    if (d == NULL) {
        ret = errno;
    }else{
        ret = 0;
        dir_stream->fh = (uint64_t) d;
    }

    printf("opendir %s -- %s, ret: %d, dir stream: %p\n", path, posix_path, ret, dir_stream);

    return ret;
}

ssize_t jycachefs_readdir(uintptr_t instance_ptr,
                        dir_stream_t* dir_stream,
                        dirent_t* dir) {
//    DirEntry dirEntry;
//    auto mount = get_instance(instance_ptr);
//    DirStream* stream = reinterpret_cast<DirStream*>(dir_stream);
//    auto rc = mount->vfs->ReadDir(stream, &dirEntry);
    printf("readdir: %p\n", dir_stream);

    DIR *d = (DIR *) dir_stream->fh;
    dirent *entry = NULL;
    int ret = 0;

    do {
        ret = 0;
        errno = 0;

        entry = readdir(d);

        if (entry == NULL){
            ret = errno;
            printf("readdir %p failed or end! ret: %d\n", dir_stream, ret);
            return ret;
        }

        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
            continue;
        }
        break;

    }while(true);
   
    strcpy(dir->name, entry->d_name);
    dir->stat.st_ino = entry->d_ino;
    ret = 1;

    printf("readdir %p, dir: %s\n", dir_stream, entry->d_name);
    return ret;
}

int jycachefs_closedir(uintptr_t instance_ptr, dir_stream_t* dir_stream) {
//    DirStream* stream = reinterpret_cast<DirStream*>(dir_stream);
//    auto mount = get_instance(instance_ptr);
//    auto rc = mount->vfs->CloseDir(stream);
//    return SysErr(rc);
    printf("close dir: %p\n", dir_stream);

    DIR *d = (DIR *)dir_stream->fh;
    int ret = 0;

    ret = closedir(d);

    if (ret != 0){
        ret = errno;
    }

    printf("cloese dir: %p, ret: %d\n", dir_stream, ret);
    return ret;
}


int jycachefs_open(uintptr_t instance_ptr,
                 const char* path,
                 uint32_t flags,
                 uint16_t mode) {
//    JYCACHEFS_ERROR rc;
//    auto mount = get_instance(instance_ptr);
//    if (flags & O_CREAT) {
//        rc = mount->vfs->Create(path, mode);
//        if (rc != JYCACHEFS_ERROR::OK) {
//            return SysErr(rc);
//        }
//    }
//
//    uint64_t fd = 0;
//    rc = mount->vfs->Open(path, flags, mode, &fd);
//    if (rc != JYCACHEFS_ERROR::OK) {
//        return SysErr(rc);
//    }
//    return static_cast<int>(fd);
    printf("open path: %s, flags: %o, mode: %o\n", path, flags, mode);
    // auto ret = posix_s3fs_open(path, flags, mode);
    char posix_path[PATH_MAX];
    posix_path[0] = '\0';
    int ret = 0;

    ret = get_posix_path(path, posix_path);

    if (ret != 0){
        return ret;
    }

    ret = open(posix_path, flags, mode);

    if (ret < 0){
        ret = -errno;
    }

    printf("open path: %s -- %s, ret: %d, flags: %o, mode: %o\n", path, posix_path, ret, flags, mode);
    return ret;
}

int jycachefs_lseek(uintptr_t instance_ptr,
                  int fd,
                  uint64_t offset,
                  int whence) {
//    auto mount = get_instance(instance_ptr);
//    auto rc = mount->vfs->LSeek(fd, offset, whence);
//    return SysErr(rc);
    printf("lseek fd: %d, offset: %lld, whence: %d\n", fd, (long long int)offset, whence);
    auto ret = lseek(fd, offset, whence);

    if (ret == -1){
        ret = -errno;
    }else{
        ret = 0;
    }

    printf("lseek fd: %d, ret: %lld\n", fd, (long long int)ret);
    return ret;
}

ssize_t jycachefs_read(uintptr_t instance_ptr,
                     int fd,
                     char* buffer,
                     size_t count) {
//    size_t nread = 0;
//    auto mount = get_instance(instance_ptr);
//    auto rc = mount->vfs->Read(fd, buffer, count, &nread);
//    if (rc != JYCACHEFS_ERROR::OK) {
//        return SysErr(rc);
//    }
//    return nread;

    printf("read %d, count: %lld\n", fd, (long long int)count);

    auto ret = read(fd, buffer, count);

    printf("read %d, ret: %lld\n", fd, (long long int)ret);
    return ret;
}

ssize_t jycachefs_write(uintptr_t instance_ptr,
                      int fd,
                      char* buffer,
                      size_t count) {
//    size_t nwritten;
//    auto mount = get_instance(instance_ptr);
//    auto rc = mount->vfs->Write(fd, buffer, count, &nwritten);
//    if (rc != JYCACHEFS_ERROR::OK) {
//        return SysErr(rc);
//    }
//    return nwritten;
    printf("write %d, count: %lld\n", fd, (long long int)count);

    auto ret = write(fd, buffer, count);

    printf("write: %d, ret: %lld\n", fd, (long long int)ret);
    return ret;
}

int jycachefs_fsync(uintptr_t instance_ptr, int fd) {
//    auto mount = get_instance(instance_ptr);
//    auto rc = mount->vfs->FSync(fd);
//    return SysErr(rc);
    int ret = fsync(fd);

    if(ret != 0){
        ret = errno;
    }

    printf("fsync %d, ret: %d\n", fd, ret);
    return ret;
}

int jycachefs_close(uintptr_t instance_ptr, int fd) {
//    auto mount = get_instance(instance_ptr);
//    auto rc = mount->vfs->Close(fd);
//    return SysErr(rc);
    printf("close %d\n", fd);
    
    auto ret = close(fd);

    if (ret != 0){
        ret = errno;
    }

    printf("close %d, ret: %d\n", fd, ret);
    return ret;
}

int jycachefs_unlink(uintptr_t instance_ptr, const char* path) {
//    auto mount = get_instance(instance_ptr);
//    auto rc = mount->vfs->Unlink(path);
//    return SysErr(rc);
    printf("unlink %s\n", path);

    char posix_path[PATH_MAX];
    posix_path[0] = '\0';
    int ret = 0;

    ret = get_posix_path(path, posix_path);

    if (ret != 0){
        return ret;
    }

    ret = unlink(posix_path);
    
    if (ret != 0){
        ret = errno;
    }

    printf("unlink %s -- %s, ret: %d\n", path, posix_path, ret);
    return ret;
}

int jycachefs_statfs(uintptr_t instance_ptr,
                   struct statvfs* statvfs) {
//    auto mount = get_instance(instance_ptr);
//    auto rc = mount->vfs->StatFs(statvfs);
//    return SysErr(rc);
    printf("statfs to do!\n");
    statvfs->f_frsize = 512;
    statvfs->f_blocks = (long)10 * 1024 * 1024 * 1024;
    statvfs->f_bavail = (long)10 * 1024 * 1024 * 1024;

    statvfs->f_files = 1024 * 1024;
    statvfs->f_ffree = 1024 * 1024 * 1024;

    statvfs->f_fsid = 128;

    statvfs->f_flag = 0;
    statvfs->f_namemax = 256;
    return 0;
}

int jycachefs_lstat(uintptr_t instance_ptr, const char* path, struct stat* file_stat) {
//    auto mount = get_instance(instance_ptr);
//    auto rc = mount->vfs->LStat(path, stat);
//    return SysErr(rc);
    printf("lstat %s\n", path);

    char posix_path[PATH_MAX];
    posix_path[0] = '\0';
    int ret = 0;

    ret = get_posix_path(path, posix_path);

    if(ret == 0){
        ret = stat(posix_path, file_stat);

        if(ret != 0){
            ret = errno;
        }
    }

    printf("lstat %s -- %s, ret: %d\n", path, posix_path, ret);
    return ret;

//    memset(stat, 0, sizeof(struct stat));
//    stat->st_ino = 1;  //  inode number
//    stat->st_mode = 040775;  // permission mode
//    stat->st_nlink = 7;  // number of links
//    stat->st_uid = 0;  // user ID of owner
//    stat->st_gid = 0;  // group ID of owner
//    stat->st_size = 146;  // total size, in bytes
//    stat->st_rdev = 801;  // device ID (if special file)
//    stat->st_atim.tv_sec = 0;  // time of last access
//    stat->st_atim.tv_nsec = 0;
//    stat->st_mtim.tv_sec = 0;  // time of last modification
//    stat->st_mtim.tv_nsec = 0;
//    stat->st_ctim.tv_sec = 0;  // time of last status change
//    stat->st_ctim.tv_nsec = 0;
//    stat->st_blksize = 512;  // blocksize for file system I/O
//    stat->st_blocks = 0;  // number of 512B blocks allocated
//    return 0;
}

int jycachefs_fstat(uintptr_t instance_ptr, int fd, struct stat* stat) {
    printf("fstat %d\n", fd);
    auto ret = fstat(fd, stat);

    if (ret != 0){
        ret = errno;
    }

    printf("fstat %d, ret: %d\n", fd, ret);
    return ret;
//    memset(stat, 0, sizeof(struct stat));
//    stat->st_size = 7;
//    return 0;
}

int jycachefs_setattr(uintptr_t instance_ptr,
                    const char* path,
                    struct stat* stat,
                    int to_set) {
//    auto mount = get_instance(instance_ptr);
//    auto rc = mount->vfs->SetAttr(path, stat, to_set);
//    return SysErr(rc);
    printf("setattr to do\n");
    return 0;
}

int jycachefs_chmod(uintptr_t instance_ptr, const char* path, uint16_t mode) {
//    auto mount = get_instance(instance_ptr);
//    auto rc = mount->vfs->Chmod(path, mode);
//    return SysErr(rc);
    printf("chmode to do\n");
    return 0;
}

int jycachefs_chown(uintptr_t instance_ptr,
                  const char* path,
                  uint32_t uid,
                  uint32_t gid) {
//    auto mount = get_instance(instance_ptr);
//    auto rc = mount->vfs->Chown(path, uid, gid);
//    return SysErr(rc);
    printf("chown to do\n");
    return 0;
}

int jycachefs_rename(uintptr_t instance_ptr,
                   const char* oldpath,
                   const char* newpath) {
//    auto mount = get_instance(instance_ptr);
//    auto rc = mount->vfs->Rename(oldpath, newpath);
//    return SysErr(rc);
    printf("rename %s -> %s\n", oldpath, newpath);

    int ret = 0;
    char old_posix_path[PATH_MAX];
    old_posix_path[0] = '\0';
    char new_posix_path[PATH_MAX];
    new_posix_path[0] = '\0';

    ret = get_posix_path(oldpath, old_posix_path);

    if (ret != 0){
        return ret;
    }

    ret = get_posix_path(newpath, new_posix_path);

    if (ret != 0){
        return ret;
    }

    ret = rename(old_posix_path, new_posix_path);
    
    if (ret != 0){
        ret = errno;
    }
 
    printf("rename %s -- %s -> %s -- %s, ret: %d\n", oldpath, old_posix_path, newpath, new_posix_path, ret);
    return ret; 
}
