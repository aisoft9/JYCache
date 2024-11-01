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
 * Created Date: 2023-07-07
 * Author: Jingli Chen (Wine93)
 */
package io.aisoft9.jycache.fs.libfs;

import java.io.IOException;

public class JYCacheFSMount {
    // init
    private native long nativeJYCacheFSCreate();
    private native void nativeJYCacheFSRelease(long instancePtr);
    private static native void nativeJYCacheFSConfSet(long instancePtr, String key, String value);
    private static native int nativeJYCacheFSMount(long instancePtr, String fsname, String mountpoint);
    private static native int nativeJYCacheFSUmount(long instancePtr, String fsname, String mountpoint);
    // dir*
    private static native int nativeJYCacheFSMkDirs(long instancePtr, String path, int mode);
    private static native int nativeJYCacheFSRmDir(long instancePtr, String path);
    private static native String[] nativeJYCacheFSListDir(long instancePtr, String path);
    // file*
    private static native int nativeJYCacheFSOpen(long instancePtr, String path, int flags, int mode);
    private static native long nativeJYCacheFSLSeek(long instancePtr, int fd, long offset, int whence);
    private static native long nativieJYCacheFSRead(long instancePtr, int fd, byte[] buffer, long size, long offset);
    private static native long nativieJYCacheFSWrite(long instancePtr, int fd, byte[] buffer, long size, long offset);
    private static native int nativeJYCacheFSFSync(long instancePtr, int fd);
    private static native int nativeJYCacheFSClose(long instancePtr, int fd);
    private static native int nativeJYCacheFSUnlink(long instancePtr, String path);
    // others
    private static native int nativeJYCacheFSStatFs(long instancePtr, JYCacheFSStatVFS statvfs);
    private static native int nativeJYCacheFSLstat(long instancePtr, String path, JYCacheFSStat stat);
    private static native int nativeJYCacheFSFStat(long instancePtr, int fd, JYCacheFSStat stat);
    private static native int nativeJYCacheFSSetAttr(long instancePtr, String path, JYCacheFSStat stat, int mask);
    private static native int nativeJYCacheFSChmod(long instancePtr, String path, int mode);
    private static native int nativeJYCacheFSChown(long instancePtr, String path, int uid, int gid);
    private static native int nativeJYCacheFSRename(long instancePtr, String src, String dst);

    /*
     * Flags for open().
     *
     * Must be synchronized with JNI if changed.
     */
    public static final int O_RDONLY = 1;
    public static final int O_RDWR = 2;
    public static final int O_APPEND = 4;
    public static final int O_CREAT = 8;
    public static final int O_TRUNC = 16;
    public static final int O_EXCL = 32;
    public static final int O_WRONLY = 64;
    public static final int O_DIRECTORY = 128;

    /*
     * Whence flags for seek().
     *
     * Must be synchronized with JNI if changed.
     */
    public static final int SEEK_SET = 0;
    public static final int SEEK_CUR = 1;
    public static final int SEEK_END = 2;

    /*
     * Attribute flags for setattr().
     *
     * Must be synchronized with JNI if changed.
     */
    public static final int SETATTR_MODE = 1;
    public static final int SETATTR_UID = 2;
    public static final int SETATTR_GID = 4;
    public static final int SETATTR_MTIME = 8;
    public static final int SETATTR_ATIME = 16;

    private static final String JYCACHEFS_DEBUG_ENV_VAR = "JYCACHEFS_DEBUG";
    private static final String CLASS_NAME = "io.aisoft9.jycache.fs.JYCacheFSMount";

    private long instancePtr;

    private static void accessLog(String name, String... args) {
        String value = System.getenv(JYCACHEFS_DEBUG_ENV_VAR);
        if (!Boolean.valueOf(value)) {
            return;
        }

        String params = String.join(",", args);
        String message = String.format("%s.%s(%s)", CLASS_NAME, name, params);
        System.out.println(message);
    }

    static {
        accessLog("loadLibrary");
        try {
            System.out.println("before nativeloader to load library");
            System.out.println("before nativeload.. sleep...");
            Thread.sleep(1);
            JYCacheFSNativeLoader.getInstance().loadLibrary();
            System.out.println("after nativeloader to load library");
        } catch(Exception e) {}
    }

    protected void finalize() throws Throwable {
        accessLog("finalize");
    }

    public JYCacheFSMount() {
        accessLog("JYCacheMount");
        instancePtr = nativeJYCacheFSCreate();
    }

    public void confSet(String key, String value) {
        accessLog("confSet", key, value);
        nativeJYCacheFSConfSet(instancePtr, key, value);
    }

    public void mount(String fsname, String mountpoint) throws IOException {
        accessLog("mount");
        nativeJYCacheFSMount(instancePtr, fsname, mountpoint);
    }

    public void umount(String fsname, String mountpoint) throws IOException {
        accessLog("umount");
        nativeJYCacheFSUmount(instancePtr, fsname, mountpoint);
    }

    public void shutdown() throws IOException {
        accessLog("shutdown");
    }

    // directory*
    public void mkdirs(String path, int mode) throws IOException {
        accessLog("mkdirs", path.toString());
        nativeJYCacheFSMkDirs(instancePtr, path, mode);
    }

    public void rmdir(String path) throws IOException {
        accessLog("rmdir", path.toString());
        nativeJYCacheFSRmDir(instancePtr, path);
    }

    public String[] listdir(String path) throws IOException {
        accessLog("listdir", path.toString());
        return nativeJYCacheFSListDir(instancePtr, path);
    }

    // file*
    public int open(String path, int flags, int mode) throws IOException {
        accessLog("open", path.toString());
        return nativeJYCacheFSOpen(instancePtr, path, flags, mode);
    }

    public long lseek(int fd, long offset, int whence) throws IOException {
        accessLog("lseek", String.valueOf(fd), String.valueOf(offset), String.valueOf(whence));
        return nativeJYCacheFSLSeek(instancePtr, fd, offset, whence);
    }

    public int read(int fd, byte[] buf, long size, long offset) throws IOException {
        accessLog("read", String.valueOf(fd), String.valueOf(size), String.valueOf(size));
        long rc = nativieJYCacheFSRead(instancePtr, fd, buf, size, offset);
        return (int) rc;
    }

    public int write(int fd, byte[] buf, long size, long offset) throws IOException {
        accessLog("write", String.valueOf(fd), String.valueOf(size), String.valueOf(size));
        long rc = nativieJYCacheFSWrite(instancePtr, fd, buf, size, offset);
        return (int) rc;
    }

    public void fsync(int fd) throws IOException {
        accessLog("fsync", String.valueOf(fd));
        nativeJYCacheFSFSync(instancePtr, fd);
    }

    public void close(int fd) throws IOException {
        accessLog("close", String.valueOf(fd));
        nativeJYCacheFSClose(instancePtr, fd);
    }

    public void unlink(String path) throws IOException {
        accessLog("unlink", path.toString());
        nativeJYCacheFSUnlink(instancePtr, path);
    }

    // others
    public void statfs(String path, JYCacheFSStatVFS statvfs) throws IOException {
        accessLog("statfs", path.toString());
        nativeJYCacheFSStatFs(instancePtr, statvfs);
    }

    public void lstat(String path, JYCacheFSStat stat) throws IOException {
        accessLog("lstat", path.toString());
        nativeJYCacheFSLstat(instancePtr, path, stat);
    }

    public void fstat(int fd, JYCacheFSStat stat) throws IOException {
        accessLog("fstat", String.valueOf(fd));
        nativeJYCacheFSFStat(instancePtr, fd, stat);
    }

    public void setattr(String path, JYCacheFSStat stat, int mask) throws IOException {
        accessLog("setattr", path.toString());
        nativeJYCacheFSSetAttr(instancePtr, path, stat, mask);
    }

    public void chmod(String path, int mode) throws IOException {
        accessLog("chmod", path.toString());
        nativeJYCacheFSChmod(instancePtr, path, mode);
    }

    public void chown(String path, int uid, int gid) throws IOException {
        accessLog("chown", path.toString(), String.valueOf(uid), String.valueOf(gid));
        nativeJYCacheFSChown(instancePtr, path, uid, gid);
    }

    public void rename(String src, String dst) throws IOException {
        accessLog("rename", src.toString(), dst.toString());
        nativeJYCacheFSRename(instancePtr, src, dst);
    }
}
