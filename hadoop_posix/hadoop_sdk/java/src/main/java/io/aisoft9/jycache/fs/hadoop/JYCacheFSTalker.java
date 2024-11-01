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
 * Created Date: 2023-08-01
 * Author: Media Bigdata
 */

package io.aisoft9.jycache.fs.hadoop;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.Path;
import io.aisoft9.jycache.fs.libfs.JYCacheFSMount;
import io.aisoft9.jycache.fs.libfs.JYCacheFSStat;
import io.aisoft9.jycache.fs.libfs.JYCacheFSStatVFS;

import java.util.UUID;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.net.URI;
import java.util.Map;

class JYCacheFSTalker extends JYCacheFSProto {
    private static final Log LOG = LogFactory.getLog(JYCacheFSTalker.class);
    private JYCacheFSMount mount;
    private String fsname = null;
    private String mountpoint = null;
    private boolean inited = false;

    private static final String PREFIX_KEY = "jycachefs";

    JYCacheFSTalker(Configuration conf, Log log) {
        mount = null;
    }

    private String tostr(Path path) {
        if (null == path) {
            return "/";
	    }
        return path.toUri().getPath();
    }

    private void loadCfg(Configuration conf) {
        Map<String,String> m = conf.getValByRegex("^" + PREFIX_KEY + "\\..*");
        for (Map.Entry<String,String> entry : m.entrySet()) {
            String key = entry.getKey();
            String value = entry.getValue();
            if (key.equals(PREFIX_KEY + ".name")) {
                fsname = value;
            } else {
                mount.confSet(key.substring(PREFIX_KEY.length() + 1), value);
            }
        }
    }
    @Override
    void initialize(URI uri, Configuration conf) throws IOException {
        LOG.info("Start initialize... the conf: " + conf);
        if (inited == false) {
            mount = new JYCacheFSMount();
            LOG.info("new JYCacheFSMount.....");
        }
        loadCfg(conf);
        LOG.info("finish load conf....");
        if (null == fsname || fsname.isEmpty()) {
            throw new IOException("jycachefs.name is not set");
        }
        if (inited == false) {
            mountpoint = UUID.randomUUID().toString();
            mount.mount(fsname, mountpoint);
            inited = true;
            LOG.info("mount the fs, fsname:" + fsname + " mountpoint: " + mountpoint);
        }
        LOG.warn("Initialization complete.");
    }

    @Override
    void shutdown() throws IOException {
        LOG.warn("Start shutdown...");
        if (inited) {
            //mount.umount(fsname, mountpoint);
            //mount = null;
            //inited = false;
            LOG.warn("have inited.... Shutdown complete.");
        } else {
            LOG.warn("no inited....Shundown nothing");
        }
    }

    @Override
    void mkdirs(Path path, int mode) throws IOException {
        mount.mkdirs(tostr(path), mode);
    }

    @Override
    void rmdir(Path path) throws IOException {
        mount.rmdir(tostr(path));
    }

    @Override
    String[] listdir(Path path) throws IOException {
        JYCacheFSStat stat = new JYCacheFSStat();
        try {
            mount.lstat(tostr(path), stat);
        } catch (FileNotFoundException e) {
            return null;
        }
        if (!stat.isDir()) {
            return null;
        }

        String[] files = mount.listdir(tostr(path));
        return files;
    }

    @Override
    int open(Path path, int flags, int mode) throws IOException {
        int fd = mount.open(tostr(path), flags, mode);
        return fd;
    }

    @Override
    long lseek(int fd, long offset, int whence) throws IOException {
        long newOffset = mount.lseek(fd, offset, whence);
        return newOffset;
    }

    @Override
    int write(int fd, byte[] buf, long size, long offset) throws IOException {
        int bytesWritten = mount.write(fd, buf, size, offset);
        return bytesWritten;
    }

    @Override
    int read(int fd, byte[] buf, long size, long offset) throws IOException {
        int bytesRead = mount.read(fd, buf, size, offset);
        return bytesRead;
    }

    @Override
    void fsync(int fd) throws IOException {
        mount.fsync(fd);
    }

    @Override
    void close(int fd) throws IOException {
        mount.close(fd);
    }

    @Override
    void unlink(Path path) throws IOException {
        mount.unlink(tostr(path));
    }

    @Override
    void statfs(Path path, JYCacheFSStatVFS stat) throws IOException {
        mount.statfs(tostr(path), stat);
    }

    @Override
    void lstat(Path path, JYCacheFSStat stat) throws IOException {
        mount.lstat(tostr(path), stat);
    }

    @Override
    void fstat(int fd, JYCacheFSStat stat) throws IOException {
        mount.fstat(fd, stat);
    }

    @Override
    void setattr(Path path, JYCacheFSStat stat, int mask) throws IOException {
        mount.setattr(tostr(path), stat, mask);
    }

    @Override
    void chmod(Path path, int mode) throws IOException {
        mount.chmod(tostr(path), mode);
    }

    @Override
    void chown(Path path, int uid, int gid) throws IOException {
        mount.chown(tostr(path), uid, gid);
    }

    @Override
    void rename(Path src, Path dst) throws IOException {
        mount.rename(tostr(src), tostr(dst));
    }
}
