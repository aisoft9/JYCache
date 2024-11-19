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
import org.apache.hadoop.fs.*;
import org.apache.hadoop.fs.permission.FsPermission;
import org.apache.hadoop.util.Progressable;
import io.aisoft9.jycache.fs.libfs.JYCacheFSMount;
import io.aisoft9.jycache.fs.libfs.JYCacheFSStat;
import io.aisoft9.jycache.fs.libfs.JYCacheFSStatVFS;
import io.aisoft9.jycache.fs.hadoop.permission.Permission;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.OutputStream;
import java.net.URI;
import java.util.Map;

public class JYCacheFileSystem extends FileSystem {
    private static final Log LOG = LogFactory.getLog(JYCacheFileSystem.class);

    private URI uri;
    private Path workingDir;

    private static final Object lock = new Object();
    static private JYCacheFSProto jycache = null;
    static private Permission perm = null;

    //private JYCacheFSProto jycache = null;
    //private Permission perm = null;

    public JYCacheFileSystem() {
        LOG.warn("constructor  JYCacheFileSystem");
	System.err.println("JYCacheFilesystem print..");
	/*
	StackTraceElement[] stackTraceElements = Thread.currentThread().getStackTrace();
        for (StackTraceElement element : stackTraceElements) {
            System.err.println(element.getClassName() + "." + element.getMethodName() + "() at " + element.getFileName() + ":" + element.getLineNumber());
        }

	*/
    }

    public JYCacheFileSystem(Configuration conf) {
        setConf(conf);
        LOG.warn("constructor  JYCacheFileSystem with conf");
        System.err.println("JYCacheFilesystem print..");
        StackTraceElement[] stackTraceElements = Thread.currentThread().getStackTrace();
        /*
            for (StackTraceElement element : stackTraceElements) {
                System.err.println(element.getClassName() + "." + element.getMethodName() + "() at " + element.getFileName() + ":" + element.getLineNumber());
            }
        */
    }

    private Path makeAbsolute(Path path) {
        if (path.isAbsolute()) {
            return path;
        }
        return new Path(workingDir, path);
    }

    @Override
    public URI getUri() {
        return uri;
    }

    @Override
    public String getScheme() {
        return "hdfs";
    }

    @Override
    public void initialize(URI uri, Configuration conf) throws IOException {
        super.initialize(uri, conf);
	    synchronized(lock) {
			if (jycache == null) {
				jycache = new JYCacheFSTalker(conf, LOG);
				// LOG.warn("Current Process ID: {}, Current Thread ID: {}, create a JYCacheFSTalker....",
				//        System.getProperty("pid"), Thread.currentThread().getId());
				LOG.warn("create a JYCacheFSTalker....");
			}
			if (perm == null) {
				perm = new Permission();
			}
        perm.initialize(conf);
        jycache.initialize(uri, conf);
        setConf(conf);
        this.uri = URI.create(uri.getScheme() + "://" + uri.getAuthority());
        this.workingDir = getHomeDirectory();
        LOG.warn("the uri: " + this.uri + " the workingDir: " + this.workingDir + "  the conf: "+ conf);
        }	
    }

    @Override
    public FSDataInputStream open(Path path, int bufferSize) throws IOException {
        path = makeAbsolute(path);

        int fd = jycache.open(path, JYCacheFSMount.O_RDONLY, 0);

        JYCacheFSStat stat = new JYCacheFSStat();
        jycache.fstat(fd, stat);

        JYCacheFSInputStream istream = new JYCacheFSInputStream(getConf(), jycache, fd, stat.size, bufferSize);
        return new FSDataInputStream(istream);
    }

    @Override
    public void close() throws IOException {
        super.close();
        jycache.shutdown();
    }

    @Override
    public FSDataOutputStream append(Path path, int bufferSize, Progressable progress) throws IOException {
        path = makeAbsolute(path);

        if (progress != null) {
            progress.progress();
        }

        int fd = jycache.open(path, JYCacheFSMount.O_WRONLY | JYCacheFSMount.O_APPEND, 0);
        if (progress != null) {
            progress.progress();
        }

        JYCacheFSOutputStream ostream = new JYCacheFSOutputStream(getConf(), jycache, fd, bufferSize);
        return new FSDataOutputStream(ostream, statistics);
    }

    @Override
    public Path getWorkingDirectory() {
        return workingDir;
    }

    @Override
     public void setWorkingDirectory(Path dir) {
        workingDir = makeAbsolute(dir);
    }

    @Override
    public boolean mkdirs(Path path, FsPermission perms) throws IOException {
        path = makeAbsolute(path);
        jycache.mkdirs(path, (int) perms.toShort());
        return true;
    }

    @Override
    public boolean mkdirs(Path f) throws IOException {
        FsPermission perms = FsPermission.getDirDefault().applyUMask(FsPermission.getUMask(getConf()));;
        return mkdirs(f, perms);
    }

    @Override
    public FileStatus getFileStatus(Path path) throws IOException {
        path = makeAbsolute(path);

        JYCacheFSStat stat = new JYCacheFSStat();
        jycache.lstat(path, stat);
        String owner = perm.getUsername(stat.uid);;
        String group = perm.getGroupname(stat.gid);;

        FileStatus status = new FileStatus(
            stat.size, stat.isDir(), 1, stat.blksize,
            stat.m_time, stat.a_time,
            new FsPermission((short) stat.mode), owner, group,
            path.makeQualified(this));
        return status;
    }

    @Override
    public FileStatus[] listStatus(Path path) throws IOException {
        path = makeAbsolute(path);

        if (isFile(path)) {
            return new FileStatus[]{getFileStatus(path)};
        }

        String[] dirlist = jycache.listdir(path);
        if (dirlist != null) {
            FileStatus[] status = new FileStatus[dirlist.length];
            for (int i = 0; i < status.length; i++) {
                status[i] = getFileStatus(new Path(path, dirlist[i]));
            }
            return status;
        } else {
            throw new FileNotFoundException("File " + path + " does not exist.");
        }
    }

    @Override
    public void setPermission(Path path, FsPermission permission) throws IOException {
        path = makeAbsolute(path);
        jycache.chmod(path, permission.toShort());
    }

    @Override
    public void setTimes(Path path, long mtime, long atime) throws IOException {
        path = makeAbsolute(path);

        JYCacheFSStat stat = new JYCacheFSStat();

        int mask = 0;
        if (mtime != -1) {
            stat.m_time = mtime;
            mask |= JYCacheFSMount.SETATTR_MTIME;
        }

        if (atime != -1) {
            stat.a_time = atime;
            mask |= JYCacheFSMount.SETATTR_ATIME;
        }

        jycache.setattr(path, stat, mask);
    }

    @Override
    public FSDataOutputStream create(Path path, FsPermission permission, boolean overwrite, int bufferSize,
                                     short replication, long blockSize, Progressable progress) throws IOException {
        path = makeAbsolute(path);

        boolean exists = exists(path);

        if (progress != null) {
            progress.progress();
        }

        int flags = JYCacheFSMount.O_WRONLY | JYCacheFSMount.O_CREAT;

        if (exists) {
            if (overwrite) {
                flags |= JYCacheFSMount.O_TRUNC;
            } else {
                throw new FileAlreadyExistsException();
            }
        } else {
            Path parent = path.getParent();
            if (parent != null) {
                if (!mkdirs(parent)) {
                    throw new IOException("mkdirs failed for " + parent.toString());
                }
            }
        }

        if (progress != null) {
            progress.progress();
        }

        int fd = jycache.open(path, flags, (int) permission.toShort());

        if (progress != null) {
            progress.progress();
        }

        OutputStream ostream = new JYCacheFSOutputStream(getConf(), jycache, fd, bufferSize);
        return new FSDataOutputStream(ostream, statistics);
    }

    @Override
    public void setOwner(Path path, String username, String groupname) throws IOException {
        JYCacheFSStat stat = new JYCacheFSStat();
        jycache.lstat(path, stat);

        int uid = stat.uid;
        int gid = stat.gid;
        if (username != null) {
            uid = perm.getUid(username);
        }
        if (groupname != null) {
            gid = perm.getGid(groupname);
        }

        jycache.chown(path, uid, gid);
    }

    @Deprecated
    @Override
    public FSDataOutputStream createNonRecursive(Path path, FsPermission permission,
                                                 boolean overwrite,
                                                 int bufferSize, short replication, long blockSize,
                                                 Progressable progress) throws IOException {
        path = makeAbsolute(path);

        Path parent = path.getParent();

        if (parent != null) {
            JYCacheFSStat stat = new JYCacheFSStat();
            jycache.lstat(parent, stat);
            if (stat.isFile()) {
                throw new FileAlreadyExistsException(parent.toString());
            }
        }

        return this.create(path, permission, overwrite,
                bufferSize, replication, blockSize, progress);
    }

    @Override
    public boolean rename(Path src, Path dst) throws IOException {
        src = makeAbsolute(src);
        dst = makeAbsolute(dst);

        try {
            JYCacheFSStat stat = new JYCacheFSStat();
            jycache.lstat(dst, stat);
            if (stat.isDir()) {
                return rename(src, new Path(dst, src.getName()));
            }
            return false;
        } catch (FileNotFoundException e) {
        }

        try {
            jycache.rename(src, dst);
        } catch (FileNotFoundException e) {
            throw e;
        } catch (Exception e) {
            return false;
        }
        return true;
    }

    @Deprecated
    @Override
    public boolean delete(Path path) throws IOException {
        return delete(path, false);
    }

    @Override
    public boolean delete(Path path, boolean recursive) throws IOException {
        path = makeAbsolute(path);

        FileStatus status;
        try {
            status = getFileStatus(path);
        } catch (FileNotFoundException e) {
            return false;
        }

        if (status.isFile()) {
            jycache.unlink(path);
            return true;
        }

        FileStatus[] dirlist = listStatus(path);
        if (dirlist == null) {
            return false;
        }

        if (!recursive && dirlist.length > 0) {
            throw new IOException("Directory " + path.toString() + "is not empty.");
        }

        for (FileStatus fs : dirlist) {
            if (!delete(fs.getPath(), recursive)) {
                return false;
            }
        }

        jycache.rmdir(path);
        return true;
    }

    @Override
    public FsStatus getStatus(Path p) throws IOException {
        JYCacheFSStatVFS stat = new JYCacheFSStatVFS();
        jycache.statfs(p, stat);

        FsStatus status = new FsStatus(stat.bsize * stat.blocks,
                stat.bsize * (stat.blocks - stat.bavail),
                stat.bsize * stat.bavail);
        return status;
    }

    @Override
    public short getDefaultReplication() {
        return 1;
    }

    @Override
    public long getDefaultBlockSize() {
        return super.getDefaultBlockSize();
    }

    @Override
    protected int getDefaultPort() {
        return super.getDefaultPort();
    }

    @Override
    public String getCanonicalServiceName() {
        return null;
    }

  public static void main(String[] args) throws IOException, IllegalAccessException, InstantiationException, ClassNotFoundException {

    // Replace "jycachefs://my-jycachefs-cluster" with your actual JYCacheFS URI
    URI uri = URI.create("jycachefs://jycachefs");

    // You might need to configure your Hadoop configuration based on your environment
    Configuration conf = new Configuration();
	System.err.println("conf print.. the conf: " +  conf);
    for (Map.Entry<String, String> entry : conf) {
      System.out.println(entry.getKey() + " = " + entry.getValue());
    }


    // Load your JYCacheFileSystem implementation from the JAR
    Class<?> fsClass = Class.forName("io.aisoft9.jycache.fs.hadoop.JYCacheFileSystem");
    FileSystem fs = (FileSystem) fsClass.newInstance();

    // Initialize the JYCacheFileSystem with the URI and configuration
    fs.initialize(uri, conf);

    // Get the path to list (replace "/path/to/list" with your desired path)
    Path path = new Path("/");

    // List the contents of the directory
    FileStatus[] fileList = fs.listStatus(path);

    if (fileList != null) {
      System.out.println("Contents of directory: " + path);
      for (FileStatus file : fileList) {
        System.out.println(file.getPath() + " (isDirectory=" + file.isDirectory() + ")");
      }
    } else {
      System.out.println("Path not found: " + path);
    }

    // Close the filesystem
    fs.close();
  }
}
