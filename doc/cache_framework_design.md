# 缓存系统设计

### 设计背景

在用户和数据服务器之间构建一套缓存系统，该缓存系统可以让用户以本地文件的形式透明且高效地访问数据服务器中的数据。其中，数据服务器的类型有对象存储、自建全局缓存等。以数据服务器为对象存储为例，用户可以通过fuse以本地文件形式访问存储在远端的对象，且远端的对象索引是用户可懂的。
![](https://www.osredm.com/jiuyuan/JYCache/tree/master/doc/image/system_purpose.png)

### 系统定位
该缓存系统支持多种数据源，包括S3对象存储、自建全局缓存等，故称为HybridCache。同时借助S3FS对fuse的支持，以及其在元数据管理方面的能力，实现fuse模式下的文件管理操作。HybridCache的定位如下图所示：
![](https://www.osredm.com/jiuyuan/JYCache/tree/master/doc/image/system_positioning.png)

### 系统架构
HybridCache架构如下图所示：
![](https://www.osredm.com/jiuyuan/JYCache/tree/master/doc/image/HybridCache_architecture.PNG)

1.写缓存模块

写缓存模块的定位是本地写缓存，写缓存中的key是文件的path，不理解远端数据源（对象存储和全局缓存等），从write->flush的过程由上层去做。

2.读缓存模块

读缓存模块的定位是文件（以远端数据源为对象存储为例）的只读缓存，读缓存中的key是对象的key。读缓存需要用到本地缓存，以及远端缓存（对象存储和全局缓存等）。

3.数据源访问组件

数据源访问组件负责和远端数据源进行交互，涉及数据的上传下载等。以Adaptor的形式支持多种数据源，包括对象存储和全局缓存等。

4.缓存管理组件

内存管理组件管理本地缓存，写缓存模块和读缓存模块中实际的本地缓存就是用的该组件。
在本地缓存中，我们直接将文件切分为固定大小的page（page大小可配置，下文以64KB为例），并使用CacheLib来维护这些page。page在CacheLib中以KV形式进行存储，其存储结构如下：
- key为 cacheKey_pageid。读写模块各自维护自己的本地缓存，cacheKey在写缓存模块中就是文件的path，在读缓存模块中就是S3上对象的key。pageid即为页号，通过offset/64KB计算得来。
- value的数据结构如下：
![](https://www.osredm.com/jiuyuan/JYCache/tree/master/doc/image/page_structure.jpg)

通过 cacheKey+offset+size 即可接操作指定文件中的特定page。page并发操作的安全性是通过CacheLib自身的机制以及page内的lock和新旧版号位来保证。

5.HybridCache访问组件

HybridCache访问组件定位在胶水层，要根据上层调用方的特性定制化实现，其内需要理解到上层调用方的逻辑。
