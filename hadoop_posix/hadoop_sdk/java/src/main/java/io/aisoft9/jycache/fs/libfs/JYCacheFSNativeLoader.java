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

import java.net.URL;
import java.net.URI;
import java.net.URLConnection;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.attribute.BasicFileAttributes; 
import java.nio.file.StandardCopyOption;


import java.util.Date;
import java.util.Arrays;
import org.apache.commons.io.FileUtils;
import org.apache.commons.compress.archivers.tar.TarArchiveInputStream;
import org.apache.commons.compress.archivers.ArchiveEntry;
import org.apache.commons.compress.utils.IOUtils;

public class JYCacheFSNativeLoader {
    boolean initialized = false;
    private static final JYCacheFSNativeLoader instance = new JYCacheFSNativeLoader();

    private static final String TMP_DIR = "/tmp";
    private static final String JYCACHEFS_LIBRARY_PATH = "/tmp/libjycachefs";
    private static final String RESOURCE_TAR_NAME = "libjycachefs.tar";
    private static final String RESOURCE_TAR_MD5SUM_NAME = "libjycachefs.tar.md5sum";
    private static final String JNI_LIBRARY_NAME = "libjycachefs_jni.so";

    private JYCacheFSNativeLoader() {}

    public static JYCacheFSNativeLoader getInstance() {
        return instance;
    }

    public long getJarModifiedTime() throws IOException {
        URL location = JYCacheFSNativeLoader.class.getProtectionDomain().getCodeSource().getLocation();
        URLConnection conn = location.openConnection();
        return conn.getLastModified();
    }

    public void descompress(InputStream in, String dest) throws IOException {
        File dir = new File(dest);
        if (!dir.exists()) {
            dir.mkdirs();
        }else{
            FileUtils.deleteDirectory(dir);
            dir.mkdirs();
        }

        ArchiveEntry entry;
        TarArchiveInputStream reader = new TarArchiveInputStream(in);
        while ((entry = reader.getNextTarEntry()) != null) {
            if (entry.isDirectory()) {
                continue;
            }

            String path = TMP_DIR + File.separator + entry.getName();
            System.err.println("make the new path.... " + path);
            File file = new File(path);
            IOUtils.copy(reader, new FileOutputStream(file));
        }
        reader.close();
    }

        /*
	public void descompress(InputStream in, String dest) throws IOException {
		long timestamp = System.currentTimeMillis();
		String newDest = dest + "_" + timestamp;
		File newDir = new File(newDest);
        System.err.println("the new dir: " + newDest);
		if (!newDir.exists()) {
			newDir.mkdirs();
		}

		TarArchiveInputStream reader = new TarArchiveInputStream(in);
		ArchiveEntry entry;
		while ((entry = reader.getNextTarEntry()) != null) {
			if (entry.isDirectory()) {
				continue;
			}

            int lastIndexOfSlash = entry.getName().lastIndexOf(File.separator);
            String fileName = entry.getName().substring(lastIndexOfSlash + 1);
			String filePath = newDest + File.separator + fileName;
            System.err.println("the copy filepath: " + filePath);
			File file = new File(filePath);
			IOUtils.copy(reader, new FileOutputStream(file));
		}
		reader.close();

		boolean isSame = compareDirectories(dest, newDest);

        try {
            if (isSame) {
                System.err.println("old files are the same with new...");
                FileUtils.deleteDirectory(newDir);
                //newDir.delete();
            } else {
                System.err.println("old files are not same with new... newDir: " + newDest + "  olddir: " + dest);
                File oldDir = new File(dest);
                FileUtils.deleteDirectory(oldDir);
                FileUtils.moveDirectory(newDir, oldDir);
                System.err.println("finish move dir..");
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
            
            // try {
            //    Path newdir = Paths.get(newDest);
            //    Path olddir = Paths.get(dest);
            //    Files.move(newdir, olddir, StandardCopyOption.REPLACE_EXISTING);
            //    System.out.println("Directory renamed successfully.");
            // } catch (IOException e) {
            //    e.printStackTrace();
            // }
	}
        */

	private boolean compareDirectories(String dir1, String dir2) throws IOException {
        Path path1 = Paths.get(dir1);
        Path path2 = Paths.get(dir2);
        if (Files.exists(path1) == false || Files.exists(path2) == false) {
            return false;
        }
		File d1 = new File(dir1);
		File d2 = new File(dir2);

		File[] files1 = d1.listFiles();
		File[] files2 = d2.listFiles();

		if (files1.length != files2.length) {
			return false;
		}

		for (File file : files1) {
			File otherFile = new File(d2, file.getName());
			if (!compareFiles(file, otherFile)) {
				return false;
			}
		}

		return true;
	}

	private boolean compareFiles(File file1, File file2) throws IOException {
        // System.out.println("cmp file: " + file1.getName() + ": " +  file1.length() + " with file2:" + file2.getName() + ": " + file2.length());
		if ((file1.length() != 0  && file2.length() != 0) && (file1.length() != file2.length())) {
		    return false;
		}

		try (FileInputStream fis1 = new FileInputStream(file1);
			 FileInputStream fis2 = new FileInputStream(file2)) {
			byte[] buffer1 = new byte[1024];
			byte[] buffer2 = new byte[1024];
			int bytesRead1, bytesRead2;

			while ((bytesRead1 = fis1.read(buffer1)) != -1) {
				bytesRead2 = fis2.read(buffer2, 0, bytesRead1);
				if (bytesRead2 < bytesRead1) {
					return false;
				}
				if (!java.util.Arrays.equals(buffer1, buffer2)) {
                                        // System.out.println("buf1: " + Arrays.toString(buffer1) + " buf2: " + Arrays.toString(buffer2));
					return false;
				}
			}
		}

		return true;
	}

    public void loadJniLibrary() throws IOException {
        File libFile = new File(JYCACHEFS_LIBRARY_PATH, JNI_LIBRARY_NAME);
        System.load(libFile.getAbsolutePath());
    }

    public synchronized void loadLibrary() throws IOException {
        if (initialized) {
            return;
        }

        InputStream md5sumReader = JYCacheFSNativeLoader.class.getResourceAsStream("/" + RESOURCE_TAR_MD5SUM_NAME);
        if (md5sumReader == null){
            throw new IOException("Cannot get resource " + RESOURCE_TAR_MD5SUM_NAME + " from Jar file.");
        }
        
        byte[] jycachefsTarMD5Sum = new byte[1024];
        md5sumReader.read(jycachefsTarMD5Sum);
        md5sumReader.close();

        File oldTarMD5SumFile = new File(JYCACHEFS_LIBRARY_PATH + "/" + RESOURCE_TAR_MD5SUM_NAME);
        
        if (oldTarMD5SumFile.exists()){
            InputStream oldTarMD5SumStream = new FileInputStream(oldTarMD5SumFile);
            byte[] oldTarMD5Sum = new byte[1024];
            oldTarMD5SumStream.read(oldTarMD5Sum);
            oldTarMD5SumStream.close();

            if (Arrays.equals(oldTarMD5Sum, jycachefsTarMD5Sum)) {
                loadJniLibrary();
                initialized = true;
                System.out.println("Same jar load jycachefs library success!");
                return;
            }else{
                System.out.println("Different MD5Sum Jar!");
            }
        }

        InputStream reader = JYCacheFSNativeLoader.class.getResourceAsStream("/" + RESOURCE_TAR_NAME);
        if (reader == null) {
            throw new IOException("Cannot get resource " + RESOURCE_TAR_NAME + " from Jar file.");
        }

        descompress(reader, JYCACHEFS_LIBRARY_PATH);
        reader.close();

        md5sumReader = JYCacheFSNativeLoader.class.getResourceAsStream("/" + RESOURCE_TAR_MD5SUM_NAME);
        if (md5sumReader == null){
             throw new IOException("Cannot get resource " + RESOURCE_TAR_MD5SUM_NAME + " from Jar file.");
         }
        
        File md5sumNew = new File(JYCACHEFS_LIBRARY_PATH + "/" + RESOURCE_TAR_MD5SUM_NAME);
        IOUtils.copy(md5sumReader, new FileOutputStream(md5sumNew));
        md5sumReader.close();

        loadJniLibrary();
        initialized = true;
        System.out.println("New jar load jycachefs library success!");
    }

}
