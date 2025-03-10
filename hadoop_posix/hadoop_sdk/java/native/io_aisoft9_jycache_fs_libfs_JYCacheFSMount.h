/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class io_aisoft9_jycache_fs_libfs_JYCacheFSMount */

#ifndef _Included_io_aisoft9_jycache_fs_libfs_JYCacheFSMount
#define _Included_io_aisoft9_jycache_fs_libfs_JYCacheFSMount
#ifdef __cplusplus
extern "C" {
#endif
#undef io_aisoft9_jycache_fs_libfs_JYCacheFSMount_O_RDONLY
#define io_aisoft9_jycache_fs_libfs_JYCacheFSMount_O_RDONLY 1L
#undef io_aisoft9_jycache_fs_libfs_JYCacheFSMount_O_RDWR
#define io_aisoft9_jycache_fs_libfs_JYCacheFSMount_O_RDWR 2L
#undef io_aisoft9_jycache_fs_libfs_JYCacheFSMount_O_APPEND
#define io_aisoft9_jycache_fs_libfs_JYCacheFSMount_O_APPEND 4L
#undef io_aisoft9_jycache_fs_libfs_JYCacheFSMount_O_CREAT
#define io_aisoft9_jycache_fs_libfs_JYCacheFSMount_O_CREAT 8L
#undef io_aisoft9_jycache_fs_libfs_JYCacheFSMount_O_TRUNC
#define io_aisoft9_jycache_fs_libfs_JYCacheFSMount_O_TRUNC 16L
#undef io_aisoft9_jycache_fs_libfs_JYCacheFSMount_O_EXCL
#define io_aisoft9_jycache_fs_libfs_JYCacheFSMount_O_EXCL 32L
#undef io_aisoft9_jycache_fs_libfs_JYCacheFSMount_O_WRONLY
#define io_aisoft9_jycache_fs_libfs_JYCacheFSMount_O_WRONLY 64L
#undef io_aisoft9_jycache_fs_libfs_JYCacheFSMount_O_DIRECTORY
#define io_aisoft9_jycache_fs_libfs_JYCacheFSMount_O_DIRECTORY 128L
#undef io_aisoft9_jycache_fs_libfs_JYCacheFSMount_SEEK_SET
#define io_aisoft9_jycache_fs_libfs_JYCacheFSMount_SEEK_SET 0L
#undef io_aisoft9_jycache_fs_libfs_JYCacheFSMount_SEEK_CUR
#define io_aisoft9_jycache_fs_libfs_JYCacheFSMount_SEEK_CUR 1L
#undef io_aisoft9_jycache_fs_libfs_JYCacheFSMount_SEEK_END
#define io_aisoft9_jycache_fs_libfs_JYCacheFSMount_SEEK_END 2L
#undef io_aisoft9_jycache_fs_libfs_JYCacheFSMount_SETATTR_MODE
#define io_aisoft9_jycache_fs_libfs_JYCacheFSMount_SETATTR_MODE 1L
#undef io_aisoft9_jycache_fs_libfs_JYCacheFSMount_SETATTR_UID
#define io_aisoft9_jycache_fs_libfs_JYCacheFSMount_SETATTR_UID 2L
#undef io_aisoft9_jycache_fs_libfs_JYCacheFSMount_SETATTR_GID
#define io_aisoft9_jycache_fs_libfs_JYCacheFSMount_SETATTR_GID 4L
#undef io_aisoft9_jycache_fs_libfs_JYCacheFSMount_SETATTR_MTIME
#define io_aisoft9_jycache_fs_libfs_JYCacheFSMount_SETATTR_MTIME 8L
#undef io_aisoft9_jycache_fs_libfs_JYCacheFSMount_SETATTR_ATIME
#define io_aisoft9_jycache_fs_libfs_JYCacheFSMount_SETATTR_ATIME 16L
/*
 * Class:     io_aisoft9_jycache_fs_libfs_JYCacheFSMount
 * Method:    nativeJYCacheFSCreate
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSCreate
  (JNIEnv *, jobject);

/*
 * Class:     io_aisoft9_jycache_fs_libfs_JYCacheFSMount
 * Method:    nativeJYCacheFSRelease
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSRelease
  (JNIEnv *, jobject, jlong);

/*
 * Class:     io_aisoft9_jycache_fs_libfs_JYCacheFSMount
 * Method:    nativeJYCacheFSConfSet
 * Signature: (JLjava/lang/String;Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSConfSet
  (JNIEnv *, jclass, jlong, jstring, jstring);

/*
 * Class:     io_aisoft9_jycache_fs_libfs_JYCacheFSMount
 * Method:    nativeJYCacheFSMount
 * Signature: (JLjava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSMount
  (JNIEnv *, jclass, jlong, jstring, jstring);

/*
 * Class:     io_aisoft9_jycache_fs_libfs_JYCacheFSMount
 * Method:    nativeJYCacheFSUmount
 * Signature: (JLjava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSUmount
  (JNIEnv *, jclass, jlong, jstring, jstring);

/*
 * Class:     io_aisoft9_jycache_fs_libfs_JYCacheFSMount
 * Method:    nativeJYCacheFSMkDirs
 * Signature: (JLjava/lang/String;I)I
 */
JNIEXPORT jint JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSMkDirs
  (JNIEnv *, jclass, jlong, jstring, jint);

/*
 * Class:     io_aisoft9_jycache_fs_libfs_JYCacheFSMount
 * Method:    nativeJYCacheFSRmDir
 * Signature: (JLjava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSRmDir
  (JNIEnv *, jclass, jlong, jstring);

/*
 * Class:     io_aisoft9_jycache_fs_libfs_JYCacheFSMount
 * Method:    nativeJYCacheFSListDir
 * Signature: (JLjava/lang/String;)[Ljava/lang/String;
 */
JNIEXPORT jobjectArray JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSListDir
  (JNIEnv *, jclass, jlong, jstring);

/*
 * Class:     io_aisoft9_jycache_fs_libfs_JYCacheFSMount
 * Method:    nativeJYCacheFSOpen
 * Signature: (JLjava/lang/String;II)I
 */
JNIEXPORT jint JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSOpen
  (JNIEnv *, jclass, jlong, jstring, jint, jint);

/*
 * Class:     io_aisoft9_jycache_fs_libfs_JYCacheFSMount
 * Method:    nativeJYCacheFSLSeek
 * Signature: (JIJI)J
 */
JNIEXPORT jlong JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSLSeek
  (JNIEnv *, jclass, jlong, jint, jlong, jint);

/*
 * Class:     io_aisoft9_jycache_fs_libfs_JYCacheFSMount
 * Method:    nativieJYCacheFSRead
 * Signature: (JI[BJJ)J
 */
JNIEXPORT jlong JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativieJYCacheFSRead
  (JNIEnv *, jclass, jlong, jint, jbyteArray, jlong, jlong);

/*
 * Class:     io_aisoft9_jycache_fs_libfs_JYCacheFSMount
 * Method:    nativieJYCacheFSWrite
 * Signature: (JI[BJJ)J
 */
JNIEXPORT jlong JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativieJYCacheFSWrite
  (JNIEnv *, jclass, jlong, jint, jbyteArray, jlong, jlong);

/*
 * Class:     io_aisoft9_jycache_fs_libfs_JYCacheFSMount
 * Method:    nativeJYCacheFSFSync
 * Signature: (JI)I
 */
JNIEXPORT jint JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSFSync
  (JNIEnv *, jclass, jlong, jint);

/*
 * Class:     io_aisoft9_jycache_fs_libfs_JYCacheFSMount
 * Method:    nativeJYCacheFSClose
 * Signature: (JI)I
 */
JNIEXPORT jint JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSClose
  (JNIEnv *, jclass, jlong, jint);

/*
 * Class:     io_aisoft9_jycache_fs_libfs_JYCacheFSMount
 * Method:    nativeJYCacheFSUnlink
 * Signature: (JLjava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSUnlink
  (JNIEnv *, jclass, jlong, jstring);

/*
 * Class:     io_aisoft9_jycache_fs_libfs_JYCacheFSMount
 * Method:    nativeJYCacheFSStatFs
 * Signature: (JLio/aisoft9/jycache/fs/libfs/JYCacheFSStatVFS;)I
 */
JNIEXPORT jint JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSStatFs
  (JNIEnv *, jclass, jlong, jobject);

/*
 * Class:     io_aisoft9_jycache_fs_libfs_JYCacheFSMount
 * Method:    nativeJYCacheFSLstat
 * Signature: (JLjava/lang/String;Lio/aisoft9/jycache/fs/libfs/JYCacheFSStat;)I
 */
JNIEXPORT jint JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSLstat
  (JNIEnv *, jclass, jlong, jstring, jobject);

/*
 * Class:     io_aisoft9_jycache_fs_libfs_JYCacheFSMount
 * Method:    nativeJYCacheFSFStat
 * Signature: (JILio/aisoft9/jycache/fs/libfs/JYCacheFSStat;)I
 */
JNIEXPORT jint JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSFStat
  (JNIEnv *, jclass, jlong, jint, jobject);

/*
 * Class:     io_aisoft9_jycache_fs_libfs_JYCacheFSMount
 * Method:    nativeJYCacheFSSetAttr
 * Signature: (JLjava/lang/String;Lio/aisoft9/jycache/fs/libfs/JYCacheFSStat;I)I
 */
JNIEXPORT jint JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSSetAttr
  (JNIEnv *, jclass, jlong, jstring, jobject, jint);

/*
 * Class:     io_aisoft9_jycache_fs_libfs_JYCacheFSMount
 * Method:    nativeJYCacheFSChmod
 * Signature: (JLjava/lang/String;I)I
 */
JNIEXPORT jint JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSChmod
  (JNIEnv *, jclass, jlong, jstring, jint);

/*
 * Class:     io_aisoft9_jycache_fs_libfs_JYCacheFSMount
 * Method:    nativeJYCacheFSChown
 * Signature: (JLjava/lang/String;II)I
 */
JNIEXPORT jint JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSChown
  (JNIEnv *, jclass, jlong, jstring, jint, jint);

/*
 * Class:     io_aisoft9_jycache_fs_libfs_JYCacheFSMount
 * Method:    nativeJYCacheFSRename
 * Signature: (JLjava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_io_aisoft9_jycache_fs_libfs_JYCacheFSMount_nativeJYCacheFSRename
  (JNIEnv *, jclass, jlong, jstring, jstring);

#ifdef __cplusplus
}
#endif
#endif
