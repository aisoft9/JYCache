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

import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.AbstractFileSystem;
import org.apache.hadoop.fs.DelegateToFileSystem;
import io.aisoft9.jycache.fs.flink.JYCacheFileSystemFactory;

import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;

/**
 * The JYCacheFS implementation of AbstractFileSystem.
 * This impl delegates to the old FileSystem
 */
public class JYCacheFS extends DelegateToFileSystem {
  /**
   * This constructor has the signature needed by
   * {@link AbstractFileSystem#createFileSystem(URI, Configuration)}.
   *
   * @param theUri which must be that of localFs
   * @param conf
   * @throws IOException
   * @throws URISyntaxException
   */
    JYCacheFS(final URI theUri, final Configuration conf) throws IOException,
        URISyntaxException {
        super(theUri, new JYCacheFileSystem(conf), conf, JYCacheFileSystemFactory.SCHEME, false);
        System.out.println("JYCacheFS new...");
    }
}
