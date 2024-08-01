
#ifndef CURVEFS_SRC_CLIENT_CURVE_POSIX_OP_H_
#define CURVEFS_SRC_CLIENT_CURVE_POSIX_OP_H_

#include <string>
#include <unordered_map>

// #include "curvefs/src/client/filesystem/meta.h"
// using ::curvefs::client::filesystem::PosixFile;

// extern std::unordered_map<int, PosixFile*> g_fdtofile;

typedef int (*syscallFunction_t)(const long *args, long *result);

enum arg_type {
	argNone,
	argFd,
	argAtfd,
	argCstr,
	argOpenFlags,
	argMode,
	arg_ /* no special formatting implemented yet, print as hex number */
};

struct syscall_desc {
	const char *name;
    syscallFunction_t syscallFunction;
	enum arg_type args[6];
};

extern  struct syscall_desc table[1000];


bool ShouldInterceptSyscall(const struct syscall_desc* desc, const long* args); 

void InitSyscall();

const struct syscall_desc* GetSyscallDesc(long syscallNumber, const long args[6]);

bool StartsWithMountPath(const char* str);

int  GlobalInit();

void UnInitPosixClient();

#ifdef __cplusplus
extern "C" {
#endif


/**
 * The access() function is used to check the permissions of a file or directory.
 * 
 * @param args[0] const char* path The path name of the file or directory to be checked.
 * @param args[1] int: mode The mode specifies the desired permissions to be verified, and can be a combination of the following constants using bitwise OR:
 *             - R_OK: Check if the file or directory is readable.
 *             - W_OK: Check if the file or directory is writable.
 *             - X_OK: Check if the file or directory is executable.
 *             - F_OK: Check if the file or directory exists.
 * @return If the file or directory has the specified permissions (or exists), it returns 0. Otherwise, it returns -1 (with an errno error code set).
 */
int PosixOpAccess(const long *args, long *result);

/**
 * The faccessat() function is used to check the permissions of a file or directory relative to a specified directory file descriptor.
 * 
 * @param args[0] int: dirfd The file descriptor of the directory from which the path is relative.
 * @param args[1] const char* pathname The relative path name of the file or directory to be checked.
 * @param args[2] int The mode specifies the desired permissions to be verified, and can be a combination of the following constants using bitwise OR:
 *             - R_OK: Check if the file or directory is readable.
 *             - W_OK: Check if the file or directory is writable.
 *             - X_OK: Check if the file or directory is executable.
 *             - F_OK: Check if the file or directory exists.
 * @param args[3] int Flags for controlling how the function operates, such as AT_SYMLINK_NOFOLLOW to not follow symbolic links.
 * @return If the file or directory has the specified permissions (or exists), it returns 0. Otherwise, it returns -1 (with an errno error code set).
 */
int PosixOpFaccessat(const long *args, long *result);

/**
 * Open a file
 *
 * Opens the file specified by 'path' with the given 'flags'.
 * The 'flags' parameter provides information about the access mode
 * (read, write, read-write) and other options for opening the file.
 *
 * args[0]: path The path of the file to be opened
 * args[1]: flags The flags controlling the file open operation
 * args[2]: mode The mode for accessing file, only be used for creating new file
 * result:  The file descriptor on success, or -1 on failure with errno set
 */
int  PosixOpOpen(const long *args, long *result);

int  PosixOpOpenat(const long *args, long *result);

/**
 * Creates a new file or truncates an existing file.
 *
 * args[0] pathname The path to the file to be created.
 * args[1] mode The permissions to be set for the newly created file.
 *
 * result: On success, the file descriptor for the newly created file is returned.
 *         On error, -1 is returned, and errno is set appropriately.
 */
int  PosixOpCreat(const long *args, long *result);


/**
 * Read data from a file
 *
 * Reads up to 'count' bytes from the file associated with the file
 * descriptor 'fd' into the buffer pointed to by 'buf',
 * The actual number of bytes read is returned.
 *
 * args[0]: int fd: The file descriptor of the file to read from
 * args[1]: void* buf: The buffer to store the read data
 * args[2]: size_t count:  The maximum number of bytes to read
 * result: The number of bytes read on success, or -1 on failure with errno set
 */
int  PosixOpRead(const long *args, long *result);


/**
 * Read data from a file
 *
 * Reads up to 'count' bytes from the file associated with the file
 * descriptor 'fd' into the buffer pointed to by 'buf', starting at
 * the specified 'offset'. The actual number of bytes read is returned.
 *
 * args[0] int fd: The file descriptor of the file to read from
 * args[1] void* buf: The buffer to store the read data
 * args[2] size_t count: The maximum number of bytes to read
 * args[3] off_t offset: The offset within the file to start reading from
 * result: The number of bytes read on success, or -1 on failure with errno set
 */
int  PosixOpPread(const long *args, long *result);


/**
 * Write data to a file
 *
 * Writes up to 'count' bytes from the buffer pointed to by 'buf'
 * to the file associated with the file descriptor 'fd'. 
 * The actual number of bytes written is returned.
 *
 * args[0] int fd: The file descriptor of the file to write to
 * args[1] const void* buf: The buffer containing the data to be written
 * args[2] size_t count:  The number of bytes to write
 * result: The number of bytes written on success, or -1 on failure with errno set
 */
int  PosixOpWrite(const long *args, long *result);

/**
 * Write data to a file
 *
 * Writes up to 'count' bytes from the buffer pointed to by 'buf'
 * to the file associated with the file descriptor 'fd', starting at
 * the specified 'offset'. The actual number of bytes written is returned.
 *
 * args[0] int fd: The file descriptor of the file to write to
 * args[1] const void* buf: The buffer containing the data to be written
 * args[2] size_t count: The number of bytes to write
 * args[3] off_t offset: The offset within the file to start writing to
 * result: The number of bytes written on success, or -1 on failure with errno set
 */
int  PosixOpPwrite(const long *args, long *result);


/**
 * Sets the current read/write position of a file descriptor.
 *
 * args[0] int fd: The file descriptor representing the file.
 * args[1] off_t offset: The offset relative to the 'whence' position.
 * args[2] int whence: The reference position for calculating the offset:
 *               - SEEK_SET: Calculates from the beginning of the file.
 *               - SEEK_CUR: Calculates from the current position.
 *               - SEEK_END: Calculates from the end of the file.
 *
 * result The new offset of the file, or -1 if an error occurs.
 */
int PosixOpLseek(const long *args, long *result);

/**
 * Close a file
 * 
 * args[0] int fd: The file descriptor of the file to close
 * result: 0 on success, or -1 on failure with errno set
 */
int PosixOpClose(const long *args, long *result);

/**
 * Create a directory.
 *
 * args[0] const char* name: Name of the directory to create
 * args[1] mode_t mode: Mode with which to create the new directory
 * result: 0 on success, -1 on failure
 */
int PosixOpMkDir(const long *args, long *result);

/**
 * mkdirat - create a new directory relative to a directory file descriptor
 * @dirfd: the file descriptor of the base directory
 * @pathname: the pathname of the new directory to be created
 * @mode: the permissions to be set for the new directory
 *
 * Returns: 0 on success, or -1 on failure
 */
int PosixOpMkDirat(const long *args, long *result);

/**
 * Open a directory
 *
 * @args[0] const char* name:  dirname The path to the directory you want to open.
 *
 * @result: If successful, returns a pointer to a DIR structure that can be
 *         used for subsequent directory operations. If there's an error,
 *         it returns NULL, and you can use the errno variable to check the
 *         specific error.
 */
int PosixOpOpenDir(const long *args, long *result);

/**
 * Read directory entries from a directory file descriptor.
 *
 * @args[0]: fd File descriptor of the directory to read.
 * @args[1]: dirp Pointer to a buffer where the directory entries will be stored.
 * @args[2]: count The size of the buffer `dirp` in bytes.
 *
 * @result: realbytes, On success, returns the number of bytes read into the buffer `dirp`.
 *         On error, returns -1 and sets the appropriate errno.
 */
//ssize_t PosixOpGetdents64(int fd, struct linux_dirent64 *dirp, size_t count);
int PosixOpGetdents64(const long *args, long *result);

/**
 * Deletes a directory, which must be empty.
 *
 *
 * args[0] const char* name: Name of the directory to remove
 * result: 0 on success, -1 on failure
 */
int PosixOpRmdir(const long *args, long *result);

/**
	A function to change the current working directory of the calling process.
	@param args - A pointer to a null-terminated string specifying the path to the new working directory
	@param result - A pointer to an integer where the result of the operation will be stored.
				On successful completion, 0 will be returned. 
				In case of failure, a non-zero value is returned.
	@return - On successful completion, the function should return 0.
		If the function encounters an error, it will return -1 and set errno accordingly.
*/
int PosixOpChdir(const long *args, long *result);

/**
 * Rename a file
 *
 * Renames the file specified by 'oldpath' to 'newpath'.
 * If 'newpath' already exists, it should be replaced atomically.
 * If the target's inode's lookup count is non-zero, the file system
 * is expected to postpone any removal of the inode until the lookup
 * count reaches zero.s
 *
 * args[0] const char* oldpath: The path of the file to be renamed
 * args[1] const char* newpath: The new path of the file
 * result: 0 on success, or -1 on failure with errno set
 */
int PosixOpRename(const long *args, long *result);

/*
 * Renameat renames a file, moving it between directories if required.
 *
 * args[0] int olddirfd: The file descriptor of the directory containing the file to be renamed
 * args[1] const char* oldpath: The path of the file to be renamed
 * args[2] int newdirfd: The file descriptor of the directory containing the new path of the file
 * args[3] const char* newpath: The new path of the file
 * result: 0 on success, or -1 on failure with errno set
 *
*/
int PosixOpRenameat(const long *args, long *result);


/**
 * Get pathname attributes.
 *
 * args[0] const char* pathname: The path name
 * args[1] struct stat* attr: Pointer to struct stat to store the file attributes

 * result:  0 on success, -1 on failure
 */
int PosixOpStat(const long *args, long *result);

/**
 * Get file attributes.
 *
 * args[0] int fd: file descriptor
 * args[1]  struct stat* attr: Pointer to struct stat to store the file attributes

 * result: 0 on success, -1 on failure
 */
int PosixOpFstat(const long *args, long *result);

/**
 * Get file status relative to a directory file descriptor
 * args[0] int dirfd
 * args[1] pathname
 * args[2] struct stat* buf
 * args[3] flags :can either be 0, or include one or more of the following flags ORed:
 *         AT_EMPTY_PATH  AT_NO_AUTOMOUNT AT_SYMLINK_NOFOLLOW
*/
int  PosixOpNewfstatat(const long *args, long *result);

/**
 * Get file status information for a symbolic link or file.
 *
 * args[0] const char* pathname The path to the symbolic link or file.
 * args[1] struct stat* statbuf A pointer to a struct stat object where the file status
 *                information will be stored.
 *
 * result: On success, 0 is returned. On error, -1 is returned, and errno is
 *         set appropriately. If the symbolic link is encountered and the
 *         'pathname' argument refers to a symbolic link, then the 'statbuf'
 *         parameter will contain information about the link itself rather
 *         than the file it refers to.
 */
int PosixOpLstat(const long *args, long *result);

/*
   Obtain file status information.
   
   Parameters:
   - args[0] dirfd: A file descriptor referring to the directory in which the file resides. 
            Use AT_FDCWD to refer to the current working directory.
   - args[1] pathname: The path to the file whose status information is to be retrieved.
   - args[2] flags: Flags controlling the behavior of the call.
   - args[3] mask: Mask specifying which fields in the returned 'statx' structure should be populated.
   - args[4] statxbuf: Pointer to the 'statx' structure where the retrieved status information is stored.

   Return Value:
   - On success, returns 0. The 'statxbuf' structure contains the requested file status information.
   - On failure, returns -1 and sets errno to indicate the error.
*/
int PosixOpStatx(const long *args, long *result);

/**
 * Creates a symbolic link.
 *
 * args[0] const char*  target: The target file or directory that the symbolic link should point to.
 * args[1] const cahr* linkpath: The path and name of the symbolic link to be created.
 *
 * result: On success, 0 is returned. On error, -1 is returned, and errno is
 *         set appropriately.
 */
int PosixOpSymlink(const long *args, long *result);


/**
 * Create a hard link
 *
 * Creates a hard link between the file specified by 'oldpath'
 * and the 'newpath'.
 *
 * args[0] const char* oldpath: The path of the existing file
 * args[1] const char* newpath: The path of the new link to be created
 * result: 0 on success, or -1 on failure with errno set
 */
void PosixOpLink(const long *args, long *result);

/**
 * Deletes a file by removing its directory entry.
 *
 * args[0] const char* pathname: The path to the file to be deleted.
 *
 * result: On success, 0 is returned. On error, -1 is returned, and errno is
 *         set appropriately.
 */
int PosixOpUnlink(const long *args, long *result);

/*
 * Deletes a specified file or directory at a given path
 *
 * args[0] dirfd: A file descriptor representing the directory in which to perform the unlinkat operation.
 *          Typically, you can use AT_FDCWD to indicate the current working directory.
 *          This parameter specifies the base directory for the operation.
 * args[1] pathname: The path to the file to be removed. It can be either a relative or absolute path,
 *             depending on the setting of dirfd.
 * args[2] flags: An integer value used to control the behavior of the unlinkat operation.
 *          You can use flags to influence the operation. Common flags include 0 (default behavior)
 *          and AT_REMOVEDIR (to remove a directory instead of a file).

 * result: On success, returns 0, indicating the successful removal of the file or directory.
 *         On failure, returns -1 and sets the global variable errno to indicate the type of error.
 */
int PosixOpUnlinkat(const long *args, long *result);


/**
 * Synchronize the file data and metadata to disk.
 * 
 * arg[0] int fd The file descriptor associated with the file.
 * 
 * result: On success, the function should return 0. On error, it should
 *         return a negative value,
 */
int PosixOpFsync(const long* args, long *result);

/*
 * int utimensat(int dirfd, const char *pathname, const struct timespec *times, int flags);
 *
 * args[0] dirfd:The file descriptor of the directory containing the file or directory to be modified.
 *     If dirfd is AT_FDCWD, then the current working directory is used.
 *
 * args[1] pathname: The path to the file or directory to be modified.
 *
 * args[2] times: A pointer to a structure containing the new access and modification times for the file or directory.
 *     If times is NULL, then the current time is used for both times.
 *
 * args[3] flags: A bitwise OR of flags that modify the behavior of the call.
 *     See the `man utimensat` page for a list of supported flags.
 *
 * result: 0 on success; -1 on error, with errno set to the error number.
 */
int PosixOpUtimensat(const long* args, long *result);


/**
 * Terminate all threads in a process and exit.
 * 
 * This system call terminates all threads in the calling process and
 * causes the process to exit. The exit status of the process is
 * specified by the parameter "status".
 *
 *  args[0] status The exit status of the process.
 */
int PosixOpExitgroup(const long* args, long *result);


/**
 * statfs() - Get filesystem statistics
 *
 * @param args[0] path The path to the filesystem to query.
 * @param args[1] buf A pointer to a statfs structure to store the results.
 *
 * @return 0 on success, or a negative error code on failure.
 * 
 */
int PosixOpStatfs(const long* args, long *result);

/**
 * fstatfs() - Get filesystem statistics for a file descriptor
 *
 * @param args[0] fd The file descriptor of the filesystem to query.
 * @param args[1] buf A pointer to a statfs structure to store the results.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int PosixOpFstatfs(const long* args, long *result);

/**
 * @brief Truncate a file to the specified length.
 *
 * This function truncates the file specified by the given path to the specified
 * length. If the file is larger than the specified length, it is truncated to
 * the specified size; if it is smaller, it is extended and filled with zeros.
 *
 * @param args[0] path:  The path to the file to be truncated.
 * @param args[1] length:The desired length to which the file should be truncated.
 * 
 * @return On success, returns 0. On failure, returns -1, and sets errno to indicate
 * the error type.
 */
int PosixOpTruncate(const long* args, long *result);

/**
 * @brief Truncate a file opened with the specified file descriptor to the specified length.
 *
 * This function truncates the file associated with the given file descriptor to the
 * specified length. If the file is larger than the specified length, it is truncated;
 * if it is smaller, it is extended and filled with zeros.
 *
 * @param args[0] :fd     The file descriptor of the file to be truncated.
 * @param args[1]: length The desired length to which the file should be truncated.
 * 
 * @return On success, returns 0. On failure, returns -1, and sets errno to indicate
 * the error type.
 */
int PosixOpFtruncate(const long* args, long *result);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // CURVEFS_SRC_CLIENT_CURVE_POSIX_OP_H_

