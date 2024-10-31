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
 * Created Date: 2023-08-08
 * Author: Jingli Chen (Wine93)
 */

#include <vector>
#include <utility>

#include <cstring>
#include <string>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include "absl/cleanup/cleanup.h"
#include "hadoop_sdk/libjycachefs/libjycachefs.h"
#include "hadoop_sdk/java/native/io_aisoft9_jycache_fs_libfs_JYCacheFSMount.h"

/* Cached field IDs for io.aisoft9.jycache.fs.JYCacheStat */
static jfieldID jycachestat_mode_fid;
static jfieldID jycachestat_uid_fid;
static jfieldID jycachestat_gid_fid;
static jfieldID jycachestat_size_fid;
static jfieldID jycachestat_blksize_fid;
static jfieldID jycachestat_blocks_fid;
static jfieldID jycachestat_a_time_fid;
static jfieldID jycachestat_m_time_fid;
static jfieldID jycachestat_is_file_fid;
static jfieldID jycachestat_is_directory_fid;
static jfieldID jycachestat_is_symlink_fid;

/* Cached field IDs for io.aisoft9.jycache.fs.JYCacheStatVFS */
static jfieldID jycachestatvfs_bsize_fid;
static jfieldID jycachestatvfs_frsize_fid;
static jfieldID jycachestatvfs_blocks_fid;
static jfieldID jycachestatvfs_bavail_fid;
static jfieldID jycachestatvfs_files_fid;
static jfieldID jycachestatvfs_fsid_fid;
static jfieldID jycachestatvfs_namemax_fid;

/*
 * Setup cached field IDs
 */
static void setup_field_ids(JNIEnv* env) {
    jclass jycachestat_cls;
    jclass jycachestatvfs_cls;

/*
 * Get a fieldID from a class with a specific type
 *
 * clz: jclass
 * field: field in clz
 * type: integer, long, etc..
 *
 * This macro assumes some naming convention that is used
 * only in this file:
 *
 *   GETFID(jycachestat, mode, I) gets translated into
 *     jycachestat_mode_fid = env->GetFieldID(jycachestat_cls, "mode", "I");
 */
#define GETFID(clz, field, type) do { \
    clz ## _ ## field ## _fid = env->GetFieldID(clz ## _cls, #field, #type); \
    if (!clz ## _ ## field ## _fid) \
        return; \
    } while (0)

    /* Cache JYCacheStat fields */

    jycachestat_cls = env->FindClass("io/aisoft9/jycache/fs/libfs/JYCacheFSStat");
    if (!jycachestat_cls) {
        return;
    }

    GETFID(jycachestat, mode, I);
    GETFID(jycachestat, uid, I);
    GETFID(jycachestat, gid, I);
    GETFID(jycachestat, size, J);
    GETFID(jycachestat, blksize, J);
    GETFID(jycachestat, blocks, J);
    GETFID(jycachestat, a_time, J);
    GETFID(jycachestat, m_time, J);
    GETFID(jycachestat, is_file, Z);
    GETFID(jycachestat, is_directory, Z);
    GETFID(jycachestat, is_symlink, Z);

    /* Cache JYCacheStatVFS fields */

    jycachestatvfs_cls =
        env->FindClass("io/aisoft9/jycache/fs/libfs/JYCacheFSStatVFS");
    if (!jycachestatvfs_cls) {
        return;
    }

    GETFID(jycachestatvfs, bsize, J);
    GETFID(jycachestatvfs, frsize, J);
    GETFID(jycachestatvfs, blocks, J);
    GETFID(jycachestatvfs, bavail, J);
    GETFID(jycachestatvfs, files, J);
    GETFID(jycachestatvfs, fsid, J);
    GETFID(jycachestatvfs, namemax, J);

#undef GETFID
}

static void fill_jycachestat(JNIEnv* env,
                           jobject j_jycachestat,
                           struct stat* stat) {
    env->SetIntField(j_jycachestat, jycachestat_mode_fid, stat->st_mode);
    env->SetIntField(j_jycachestat, jycachestat_uid_fid, stat->st_uid);
    env->SetIntField(j_jycachestat, jycachestat_gid_fid, stat->st_gid);
    env->SetLongField(j_jycachestat, jycachestat_size_fid, stat->st_size);
    env->SetLongField(j_jycachestat, jycachestat_blksize_fid, stat->st_blksize);
    env->SetLongField(j_jycachestat, jycachestat_blocks_fid, stat->st_blocks);

    // mtime
    uint64_t time = stat->st_mtim.tv_sec;
    time *= 1000;
    time += stat->st_mtim.tv_nsec / 1000000;
    env->SetLongField(j_jycachestat, jycachestat_m_time_fid, time);

    // atime
    time = stat->st_atim.tv_sec;
    time *= 1000;
    time += stat->st_atim.tv_nsec / 1000000;
    env->SetLongField(j_jycachestat, jycachestat_a_time_fid, time);

    env->SetBooleanField(j_jycachestat, jycachestat_is_file_fid,
        S_ISREG(stat->st_mode) ? JNI_TRUE : JNI_FALSE);

    env->SetBooleanField(j_jycachestat, jycachestat_is_directory_fid,
        S_ISDIR(stat->st_mode) ? JNI_TRUE : JNI_FALSE);

    env->SetBooleanField(j_jycachestat, jycachestat_is_symlink_fid,
        S_ISLNK(stat->st_mode) ? JNI_TRUE : JNI_FALSE);
}

static void fill_jycachestatvfs(JNIEnv* env,
                              jobject j_jycachestatvfs,
                              struct statvfs st) {
    env->SetLongField(j_jycachestatvfs, jycachestatvfs_bsize_fid, st.f_bsize);
    env->SetLongField(j_jycachestatvfs, jycachestatvfs_frsize_fid, st.f_frsize);
    env->SetLongField(j_jycachestatvfs, jycachestatvfs_blocks_fid, st.f_blocks);
    env->SetLongField(j_jycachestatvfs, jycachestatvfs_bavail_fid, st.f_bavail);
    env->SetLongField(j_jycachestatvfs, jycachestatvfs_files_fid, st.f_files);
    env->SetLongField(j_jycachestatvfs, jycachestatvfs_fsid_fid, st.f_fsid);
    env->SetLongField(j_jycachestatvfs, jycachestatvfs_namemax_fid, st.f_namemax);
}

/* Map io_aisoft9_jycache_fs_libfs_JYCacheFSMount_O_* open flags to values in libc */
static inline uint32_t fixup_open_flags(jint jflags) {
    uint32_t flags = 0;

#define FIXUP_OPEN_FLAG(name) \
    if (jflags & io_aisoft9_jycache_fs_libfs_JYCacheFSMount_##name) \
        flags |= name;

    FIXUP_OPEN_FLAG(O_RDONLY)
    FIXUP_OPEN_FLAG(O_RDWR)
    FIXUP_OPEN_FLAG(O_APPEND)
    FIXUP_OPEN_FLAG(O_CREAT)
    FIXUP_OPEN_FLAG(O_TRUNC)
    FIXUP_OPEN_FLAG(O_EXCL)
    FIXUP_OPEN_FLAG(O_WRONLY)
    FIXUP_OPEN_FLAG(O_DIRECTORY)

#undef FIXUP_OPEN_FLAG

    return flags;
}

#define JYCACHEFS_SETATTR_MODE       (1 << 0)
#define JYCACHEFS_SETATTR_UID        (1 << 1)
#define JYCACHEFS_SETATTR_GID        (1 << 2)
#define JYCACHEFS_SETATTR_SIZE       (1 << 3)
#define JYCACHEFS_SETATTR_ATIME      (1 << 4)
#define JYCACHEFS_SETATTR_MTIME      (1 << 5)
#define JYCACHEFS_SETATTR_ATIME_NOW  (1 << 7)
#define JYCACHEFS_SETATTR_MTIME_NOW  (1 << 8)
#define JYCACHEFS_SETATTR_CTIME      (1 << 10)

/* Map JAVA_SETATTR_* to values in jycache lib */
static inline int fixup_attr_mask(jint jmask) {
    int mask = 0;

#define FIXUP_ATTR_MASK(name) \
    if (jmask & io_aisoft9_jycache_fs_libfs_JYCacheFSMount_##name) \
        mask |= JYCACHEFS_##name;

    FIXUP_ATTR_MASK(SETATTR_MODE)
    FIXUP_ATTR_MASK(SETATTR_UID)
    FIXUP_ATTR_MASK(SETATTR_GID)
    FIXUP_ATTR_MASK(SETATTR_MTIME)
    FIXUP_ATTR_MASK(SETATTR_ATIME)

#undef FIXUP_ATTR_MASK
    return mask;
}

/*
 * Exception throwing helper. Adapted from Apache Hadoop header
 * org_apache_hadoop.h by adding the do {} while (0) construct.
 */
#define THROW(env, exception_name, message) \
    do { \
        jclass ecls = env->FindClass(exception_name); \
        if (ecls) { \
            int ret = env->ThrowNew(ecls, message); \
            if (ret < 0) { \
                printf("(JYCacheFS) Fatal Error\n"); \
            } \
            env->DeleteLocalRef(ecls); \
        } \
    } while (0)

static void handle_error(JNIEnv* env, int rc) {
    switch (rc) {
        case ENOENT:
            THROW(env, "java/io/FileNotFoundException", "");
            return;
        case EEXIST:
            THROW(env, "org/apache/hadoop/fs/FileAlreadyExistsException", "");
            return;
        case ENOTDIR:
            THROW(env, "org/apache/hadoop/fs/ParentNotDirectoryException", "");
            return;
        default:
            break;
    }

    THROW(env, "java/io/IOException", strerror(rc));
}

// nativeJYCacheFSCreate: jycachefs_create
JNIEXPORT jlong
JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSCreate
    (JNIEnv* env, jobject) {
    setup_field_ids(env);
    uintptr_t instance = jycachefs_create();
    return reinterpret_cast<uint64_t>(instance);
}

// nativeJYCacheFSRelease: jycachefs_release
JNIEXPORT void
JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSRelease
    (JNIEnv* env, jobject, jlong j_instance) {
    uintptr_t instance = static_cast<uintptr_t>(j_instance);
    return jycachefs_release(instance);
}

// nativeJYCacheFSConfSet: jycachefs_conf_set
JNIEXPORT void
JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSConfSet
    (JNIEnv* env, jclass, jlong j_instance, jstring j_key, jstring j_value) {
    uintptr_t instance = static_cast<uintptr_t>(j_instance);
    const char* key = env->GetStringUTFChars(j_key, NULL);
    const char* value = env->GetStringUTFChars(j_value, NULL);
    auto defer = absl::MakeCleanup([&]() {
        env->ReleaseStringUTFChars(j_key, key);
        env->ReleaseStringUTFChars(j_value, value);
    });

    return jycachefs_conf_set(instance, key, value);
}

// nativeJYCacheFSMount: jycachefs_mount
JNIEXPORT jint
JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSMount
    (JNIEnv* env, jclass, jlong j_instance,
     jstring j_fsname, jstring j_mountpoint) {
    uintptr_t instance = static_cast<uintptr_t>(j_instance);
    const char* fsname = env->GetStringUTFChars(j_fsname, NULL);
    const char* mountpoint = env->GetStringUTFChars(j_mountpoint, NULL);
    auto defer = absl::MakeCleanup([&]() {
        env->ReleaseStringUTFChars(j_fsname, fsname);
        env->ReleaseStringUTFChars(j_mountpoint, mountpoint);
    });

    int rc = jycachefs_mount(instance, fsname, mountpoint);
    if (rc != 0) {
        handle_error(env, rc);
    }
    return rc;
}

// nativeJYCacheFSUmount: jycachefs_umount
JNIEXPORT jint
JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSUmount
    (JNIEnv* env, jclass, jlong j_instance,
     jstring j_fsname, jstring j_mountpoint) {
    uintptr_t instance = static_cast<uintptr_t>(j_instance);
    const char* fsname = env->GetStringUTFChars(j_fsname, NULL);
    const char* mountpoint = env->GetStringUTFChars(j_mountpoint, NULL);
    auto defer = absl::MakeCleanup([&]() {
        env->ReleaseStringUTFChars(j_fsname, fsname);
        env->ReleaseStringUTFChars(j_mountpoint, mountpoint);
    });

    int rc = jycachefs_umonut(instance, fsname, mountpoint);
    if (rc != 0) {
        handle_error(env, rc);
    }
    return rc;
}

// nativeJYCacheFSMkDirs: jycachefs_mkdirs
JNIEXPORT jint
JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSMkDirs
  (JNIEnv* env, jclass, jlong j_instance, jstring j_path, jint j_mode) {
    uintptr_t instance = static_cast<uintptr_t>(j_instance);
    const char* path = env->GetStringUTFChars(j_path, NULL);
    uint16_t mode = static_cast<uint16_t>(j_mode);
    auto defer = absl::MakeCleanup([&]() {
        env->ReleaseStringUTFChars(j_path, path);
    });

    int rc = jycachefs_mkdirs(instance, path, mode);
    if (rc == EEXIST) {
        rc = 0;
    } else if (rc != 0) {
        handle_error(env, rc);
    }
    return rc;
}

// nativeJYCacheFSRmDir: jycachefs_rmdir
JNIEXPORT jint
JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSRmDir
    (JNIEnv* env, jclass, jlong j_instance, jstring j_path) {
    uintptr_t instance = static_cast<uintptr_t>(j_instance);
    const char* path = env->GetStringUTFChars(j_path, NULL);
    auto defer = absl::MakeCleanup([&]() {
        env->ReleaseStringUTFChars(j_path, path);
    });

    int rc = jycachefs_rmdir(instance, path);
    if (rc != 0) {
        handle_error(env, rc);
    }
    return rc;
}

// nativeJYCacheFSListDir: jycachefs_opendir/jycachefs_readdir/jycachefs_closedir
JNIEXPORT jobjectArray
JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSListDir
    (JNIEnv* env, jclass, jlong j_instance, jstring j_path) {
    uintptr_t instance = static_cast<uintptr_t>(j_instance);
    const char* path = env->GetStringUTFChars(j_path, NULL);
    auto defer = absl::MakeCleanup([&]() {
        env->ReleaseStringUTFChars(j_path, path);
    });

    // jycachefs_opendir
    dir_stream_t dir_stream;
    auto rc = jycachefs_opendir(instance, path, &dir_stream);
    if (rc != 0) {
        handle_error(env, rc);
        return NULL;
    }

    // jycachefs_readdir
    std::vector<dirent_t> dirents;
    dirent_t dirent;
    for ( ;; ) {
        ssize_t n = jycachefs_readdir(instance, &dir_stream, &dirent);
        if (n < 0) {
            handle_error(env, rc);
            return NULL;
        } else if (n == 0) {
            break;
        }
        dirents.push_back(dirent);
    }

    // closedir
    rc = jycachefs_closedir(instance, &dir_stream);
    if (rc != 0) {
        handle_error(env, rc);
        return NULL;
    }

    // extract entry name
    jobjectArray j_names = env->NewObjectArray(
        dirents.size(), env->FindClass("java/lang/String"), NULL);

    for (int i = 0; i < dirents.size(); i++) {
        jstring j_name = env->NewStringUTF(dirents[i].name);
        env->SetObjectArrayElement(j_names, i, j_name);
        env->DeleteLocalRef(j_name);
    }
    return j_names;
}

// nativeJYCacheFSOpen: jycachefs_open
JNIEXPORT jint
JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSOpen
    (JNIEnv* env, jclass,
     jlong j_instance, jstring j_path, jint j_flags, jint j_mode) {
    uintptr_t instance = static_cast<uintptr_t>(j_instance);
    const char* path = env->GetStringUTFChars(j_path, NULL);
    uint32_t flags = fixup_open_flags(j_flags);
    uint16_t mode = static_cast<uint16_t>(j_mode);
    auto defer = absl::MakeCleanup([&]() {
        env->ReleaseStringUTFChars(j_path, path);
    });

    int fd = jycachefs_open(instance, path, flags, mode);
    if (fd < 0) {
        handle_error(env, fd);
    }
    return fd;
}

// nativeJYCacheFSLSeek: jycachefs_lseek
JNIEXPORT jlong
JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSLSeek
    (JNIEnv* env, jclass,
     jlong j_instance, jint j_fd, jlong j_offset, jint j_whence) {
    uintptr_t instance = static_cast<uintptr_t>(j_instance);
    int fd = static_cast<int>(j_fd);
    uint64_t offset = static_cast<uint64_t>(j_offset);

    int whence;
    switch (j_whence) {
    case io_aisoft9_jycache_fs_libfs_JYCacheFSMount_SEEK_SET:
        whence = SEEK_SET;
        break;
    case io_aisoft9_jycache_fs_libfs_JYCacheFSMount_SEEK_CUR:
        whence = SEEK_CUR;
        break;
    case io_aisoft9_jycache_fs_libfs_JYCacheFSMount_SEEK_END:
        whence = SEEK_END;
        break;
    default:
        return -1;
    }

    int rc = jycachefs_lseek(instance, fd, offset, whence);
    if (rc != 0) {
        handle_error(env, rc);
    }
    return rc;
}

// nativieJYCacheFSRead: jycachefs_read
JNIEXPORT jlong
JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativieJYCacheFSRead
    (JNIEnv* env, jclass, jlong j_instance, jint j_fd,
     jbyteArray j_buffer, jlong j_size, jlong j_offset) {
    uintptr_t instance = static_cast<uintptr_t>(j_instance);
    int fd = static_cast<int>(j_fd);
    jbyte* c_buffer = env->GetByteArrayElements(j_buffer, NULL);
    char* buffer = reinterpret_cast<char*>(c_buffer);
    size_t count = static_cast<size_t>(j_size);
    auto defer = absl::MakeCleanup([&]() {
        env->ReleaseByteArrayElements(j_buffer, c_buffer, 0);
    });

    ssize_t n = jycachefs_read(instance, fd, buffer, count);
    if (n < 0) {
        handle_error(env, n);
    }
    return static_cast<jlong>(n);
}

// nativieJYCacheFSWrite: jycachefs_write
JNIEXPORT jlong
JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativieJYCacheFSWrite
    (JNIEnv* env, jclass, jlong j_instance, jint j_fd,
     jbyteArray j_buffer, jlong j_size, jlong j_offset) {
    uintptr_t instance = static_cast<uintptr_t>(j_instance);
    int fd = static_cast<int>(j_fd);
    jbyte* c_buffer = env->GetByteArrayElements(j_buffer, NULL);
    char* buffer = reinterpret_cast<char*>(c_buffer);
    size_t count = static_cast<size_t>(j_size);
    auto defer = absl::MakeCleanup([&]() {
        env->ReleaseByteArrayElements(j_buffer, c_buffer, 0);
    });

    ssize_t n = jycachefs_write(instance, fd, buffer, count);
    if (n < 0) {
        handle_error(env, n);
    }
    return static_cast<jlong>(n);
}

// nativeJYCacheFSFSync: jycachefs_fsync
JNIEXPORT jint
JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSFSync
    (JNIEnv* env, jclass, jlong j_instance, jint j_fd) {
    uintptr_t instance = static_cast<uintptr_t>(j_instance);
    int fd = static_cast<int>(j_fd);

    int rc = jycachefs_fsync(instance, fd);
    if (rc != 0) {
        handle_error(env, rc);
    }
    return rc;
}

// nativeJYCacheFSClose: jycachefs_close
JNIEXPORT jint
JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSClose
    (JNIEnv* env, jclass, jlong j_instance, jint j_fd) {
    uintptr_t instance = static_cast<uintptr_t>(j_instance);
    int fd = static_cast<int>(j_fd);

    int rc = jycachefs_close(instance, fd);
    if (rc != 0) {
        handle_error(env, rc);
    }
    return rc;
}

// nativeJYCacheFSUnlink: jycachefs_unlink
JNIEXPORT jint
JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSUnlink
    (JNIEnv* env, jclass, jlong j_instance, jstring j_path) {
    uintptr_t instance = static_cast<uintptr_t>(j_instance);
    const char* path = env->GetStringUTFChars(j_path, NULL);
    auto defer = absl::MakeCleanup([&]() {
        env->ReleaseStringUTFChars(j_path, path);
    });

    int rc = jycachefs_unlink(instance, path);
    if (rc != 0) {
        handle_error(env, rc);
    }
    return rc;
}

// nativeJYCacheFSStatFs: jycachefs_statfs
JNIEXPORT jint
JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSStatFs
    (JNIEnv* env, jclass,
     jlong j_instance, jobject j_jycachestatvfs) {
    uintptr_t instance = static_cast<uintptr_t>(j_instance);

    struct statvfs statvfs;
    int rc = jycachefs_statfs(instance, &statvfs);
    if (rc != 0) {
        handle_error(env, rc);
        return rc;
    }

    fill_jycachestatvfs(env, j_jycachestatvfs, statvfs);
    return rc;
}

// nativeJYCacheFSLstat: jycachefs_lstat
JNIEXPORT jint
JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSLstat
    (JNIEnv* env, jclass,
     jlong j_instance, jstring j_path, jobject j_jycachestat) {
    uintptr_t instance = static_cast<uintptr_t>(j_instance);
    const char* path = env->GetStringUTFChars(j_path, NULL);
    auto defer = absl::MakeCleanup([&]() {
        env->ReleaseStringUTFChars(j_path, path);
    });

    // jycachefs_lstat
    struct stat stat;
    auto rc = jycachefs_lstat(instance, path, &stat);
    if (rc != 0) {
        handle_error(env, rc);
        return rc;
    }

    fill_jycachestat(env, j_jycachestat, &stat);
    return rc;
}

// nativeJYCacheFSFStat: jycachefs_fstat
JNIEXPORT jint
JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSFStat
    (JNIEnv* env, jclass, jlong j_instance, jint j_fd, jobject j_jycachestat) {
    uintptr_t instance = static_cast<uintptr_t>(j_instance);
    int fd = static_cast<int>(j_fd);

    // jycachefs_fstat
    struct stat stat;
    auto rc = jycachefs_fstat(instance, fd, &stat);
    if (rc != 0) {
        handle_error(env, rc);
        return rc;
    }

    fill_jycachestat(env, j_jycachestat, &stat);
    return rc;
}

// nativeJYCacheFSSetAttr: jycachefs_setattr
JNIEXPORT jint
JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSSetAttr
    (JNIEnv* env, jclass,
     jlong j_instance, jstring j_path, jobject j_jycachestat, jint j_mask) {
    uintptr_t instance = static_cast<uintptr_t>(j_instance);
    const char* path = env->GetStringUTFChars(j_path, NULL);
    int to_set = fixup_attr_mask(j_mask);
    auto defer = absl::MakeCleanup([&]() {
        env->ReleaseStringUTFChars(j_path, path);
    });

    struct stat stat;
    memset(&stat, 0, sizeof(stat));
    stat.st_mode = env->GetIntField(j_jycachestat, jycachestat_mode_fid);
    stat.st_uid = env->GetIntField(j_jycachestat, jycachestat_uid_fid);
    stat.st_gid = env->GetIntField(j_jycachestat, jycachestat_gid_fid);
    uint64_t mtime_msec = env->GetLongField(j_jycachestat, jycachestat_m_time_fid);
    uint64_t atime_msec = env->GetLongField(j_jycachestat, jycachestat_a_time_fid);
    stat.st_mtim.tv_sec = mtime_msec / 1000;
    stat.st_mtim.tv_nsec = (mtime_msec % 1000) * 1000000;
    stat.st_atim.tv_sec = atime_msec / 1000;
    stat.st_atim.tv_nsec = (atime_msec % 1000) * 1000000;

    int rc = jycachefs_setattr(instance, path, &stat, to_set);
    if (rc != 0) {
        handle_error(env, rc);
    }
    return rc;
}

// nativeJYCacheFSChmod: jycachefs_chmod
JNIEXPORT jint
JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSChmod
    (JNIEnv* env, jclass, jlong j_instance, jstring j_path, jint j_mode) {
    uintptr_t instance = static_cast<uintptr_t>(j_instance);
    uint16_t mode = static_cast<uint16_t>(j_mode);
    const char* path = env->GetStringUTFChars(j_path, NULL);
    auto defer = absl::MakeCleanup([&]() {
        env->ReleaseStringUTFChars(j_path, path);
    });

    int rc = jycachefs_chmod(instance, path, mode);
    if (rc != 0) {
        handle_error(env, rc);
    }
    return rc;
}

// nativeJYCacheFSChown: jycachefs_chown
JNIEXPORT jint
JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSChown
  (JNIEnv* env, jclass,
   jlong j_instance, jstring j_path, jint j_uid, jint j_gid) {
    uintptr_t instance = static_cast<uintptr_t>(j_instance);
    uint32_t uid = static_cast<uint32_t>(j_uid);
    uint32_t gid = static_cast<uint32_t>(j_gid);
    const char* path = env->GetStringUTFChars(j_path, NULL);
    auto defer = absl::MakeCleanup([&]() {
        env->ReleaseStringUTFChars(j_path, path);
    });

    int rc = jycachefs_chown(instance, path, uid, gid);
    if (rc != 0) {
        handle_error(env, rc);
    }
    return rc;
}

// nativeJYCacheFSRename: jycachefs_rename
JNIEXPORT jint
JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSRename
    (JNIEnv* env, jclass, jlong j_instance, jstring j_src, jstring j_dst) {
    uintptr_t instance = static_cast<uintptr_t>(j_instance);
    const char* src = env->GetStringUTFChars(j_src, NULL);
    const char* dst = env->GetStringUTFChars(j_dst, NULL);
    auto defer = absl::MakeCleanup([&]() {
        env->ReleaseStringUTFChars(j_src, src);
        env->ReleaseStringUTFChars(j_dst, dst);
    });

    int rc = jycachefs_rename(instance, src, dst);
    if (rc != 0) {
        handle_error(env, rc);
    }
    return rc;
}
