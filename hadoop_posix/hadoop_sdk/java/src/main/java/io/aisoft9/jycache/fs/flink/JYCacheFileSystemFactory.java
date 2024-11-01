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

package io.aisoft9.jycache.fs.flink;

import io.aisoft9.jycache.fs.hadoop.JYCacheFileSystem;
import org.apache.flink.core.fs.FileSystem;
import org.apache.flink.core.fs.FileSystemFactory;
import org.apache.flink.runtime.fs.hdfs.HadoopFileSystem;
import org.apache.hadoop.conf.Configuration;

import java.io.IOException;
import java.net.URI;

public class JYCacheFileSystemFactory implements FileSystemFactory {
    private org.apache.hadoop.conf.Configuration conf = new Configuration();
    private static final String JYCACHE_FS_CONFIG_PREFIXES = "jycachefs.";
    private static final String FLINK_CONFIG_PREFIXES = "fs.";
    public static String SCHEME = "jycachefs";

    @Override
    public void configure(org.apache.flink.configuration.Configuration config) {
        config.keySet()
                .stream()
                .filter(key -> key.startsWith(JYCACHE_FS_CONFIG_PREFIXES) || key.startsWith(FLINK_CONFIG_PREFIXES))
                .forEach(key -> conf.set(key, config.getString(key, "")));
    }

    @Override
    public String getScheme() {
        return SCHEME;
    }

    @Override
    public FileSystem create(URI uri) throws IOException {
	System.out.println("JYCacheFileSystemFactory is : " + this);
	System.out.println("create filesystem in JYCacheFileSystemFactory");
        JYCacheFileSystem fs = new JYCacheFileSystem();
        fs.initialize(uri, conf);
        return new HadoopFileSystem(fs);
    }
}
