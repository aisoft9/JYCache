#ifndef S3FS_S3FS_LIB_H_
#define S3FS_S3FS_LIB_H_

#ifdef S3FS_MALLOC_TRIM
#ifdef HAVE_MALLOC_TRIM
#include <malloc.h>
#define S3FS_MALLOCTRIM(pad)    malloc_trim(pad)
#else   // HAVE_MALLOC_TRIM
#define S3FS_MALLOCTRIM(pad)
#endif  // HAVE_MALLOC_TRIM
#else   // S3FS_MALLOC_TRIM
#define S3FS_MALLOCTRIM(pad)
#endif  // S3FS_MALLOC_TRIM


//-------------------------------------------------------------------
// posix interface functions
//-------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

struct S3DirStream;

void s3fs_global_init();

void s3fs_global_uninit();

int posix_s3fs_create(const char* _path, int flags, mode_t mode);

int posix_s3fs_open(const char* _path, int flags, mode_t mode);

int posix_s3fs_multiread(int fd, void* buf, size_t size, off_t file_offset);

int posix_s3fs_read(int fd, void* buf, size_t size);

int posix_s3fs_multiwrite(int fd, const void* buf, size_t size, off_t file_offset);

int posix_s3fs_write(int fd, const void* buf, size_t size);

off_t posix_s3fs_lseek(int fd, off_t offset, int whence);

int posix_s3fs_close(int fd);

int posix_s3fs_stat(const char* _path, struct stat* stbuf);

int posix_s3fs_fstat(int fd, struct stat* stbuf) ;

int posix_s3fs_mkdir(const char* _path, mode_t mode);

int posix_s3fs_opendir(const char* _path, S3DirStream* dirstream);

int posix_s3fs_getdents(S3DirStream* dirstream, char* contents, size_t maxread, ssize_t* realbytes);

int posix_s3fs_closedir(S3DirStream* dirstream);

int posix_s3fs_unlink(const char* _path);

#ifdef __cplusplus
}
#endif

#endif // S3FS_S3FS_LIB_H_