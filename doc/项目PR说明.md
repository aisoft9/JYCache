# 简略说明
### 修改：
1. 将Local WriteCache的写入限流机制结合到当前项目中
2. 接入基于LinUCB算法的ReadCache & WriteCache动态调整策略
   
### 变更内容：
1. 修改JYCache/local_cache文件夹下的config.h & config.cpp & write_cache.h &write_cache.cpp & read_cache.h & read_cache.cpp & page_cache.h & page_cache.cpp，新增throttle.h & throttle.cpp
2. 修改JYCache/s3fs文件夹下的hybridcache_accessor_4_s3fs.h & hybridcache_accessor_4_s3fs.cpp

### 测试结果：
1. 对于写入限流机制，本地测试未发现问题。
2. 接入LinUCB算法后，在小规模数据集上测试效果较优。

### 注意事项：
1. sh build.sh && sh install.sh后需要更新JYCache_Env/conf/newcache.conf，根据自身需要开启/关闭限流机制以及LinUCB调整策略，具体选项如/conf_spec/newcache.conf_spce所示

### 使用方法
1. JYCache路径下
```bash
    # 1.从源码构建
    sh build.sh 
    # 2.系统安装
    sh install.sh
    # 3.开启LinUCB服务器端（不开启LinUCB调整可跳过此项）
    cd LinUCBSchedule && python3 server.py 
 ```

2. JYCache/JYCache_Env路径下
```bash
# 1.开启minio
    cd ./minio && sh start.sh && cd ..
# 2.修改配置文件
    vim conf/newcache.conf
    # 下为新增：
    WriteCacheConfig.EnableThrottle=0 
    EnableResize = 1
    EnableLinUCB = 1
    # 下为修改
    UseGlobalCache=0    #不开启全局缓存
# 3.启动s3fs
    sh start_s3fs.sh
 ```

# 详细介绍

## 1.Local WriteCache写入限流机制
### 1.1 框架设计

JYCache是一款面向个人使用、大模型训练推理等多种场景，适配大容量对象存储等多种底层存储形态，高性能、易扩展的分布式缓存存储系统。JYCache在同一时刻支持多个进程同时对不同文件进行写入操作，但受限于内存读写带宽的限制，以及出于防止恶意攻击、滥用或过度消耗资源的目的，此处增添了Local WriteCache层的写入限流机制。该机制运行流程如下：

![](image\LocalWriteCache_throttle.png)

写入限流机制使用文件名称作为区分标识符，即同一个文件在一定时间间隔内可写入的字节数目是有限的，将令牌视同于带宽资源。当一个写入任务（即客户端）发起Put请求时会传递目标文件名称和该次写入字节长度等参数。WriteCache在接收到这些信息时运行流程如下：

1. 该文件为首次写入：
   1. 是->为其建立与令牌桶的映射，分配一定带宽资源。
   2. 否->查找文件对应令牌桶。
2. 从令牌桶中消耗该次写入长度的令牌数目，记录因获取令牌而消耗的时间BlockTime
3. 执行真正的写入操作。

同时在WriteCache被初始化时新增线程throttling_thread_，用于对当前每个文件的写入带宽进行调节。每隔0.1s启动一次。此处认为BlockTime越长则说明这段时间内该文件发出的写入请求越多/饥饿情况越严重，此时应当为其分配更多写入带宽资源；反之亦然。因此使用各个文件的BlockTime/AvgBlockTime * TotalBandWidth作为下一轮次的写入带宽，出于公平性考虑，同时限制每个文件的写入带宽最高不能超过总带宽的50%，不能低于总带宽的10%。详细介绍见[限流工具实现及使用说明](https://epr023ri66.feishu.cn/docx/MfbOdxoLboII2Ex7MfyccKjXnEg)

### 1.2 参数说明

只有单个任务时默认调节令牌桶的rate()和burst()为649651540：实际运行日志中[WriteCache]Put统计为len:131072，res:0，writePagecnt:2，time:0.201659ms，即约每秒写入649651540Bytes/s

因此文件初次写入时设置rate()=32768000,burst()=65536000,capacity=32768000

### 1.3 使用说明
修改JYCache_Env/conf/newcache.conf中的WriteCacheConfig.EnableThrottle=1即可。

相关代码实现位于local_cache/throttle.h&throttle.cpp下，同时修改了local_cache/write_cache.h&write_cache.cpp中写入、删除相关内容。

### 1.4 效果展示

在开启写入限流时，短时间内发起超量写入请求的文件将被阻塞一定时间后方能继续下一次写入。

![](image\result_throttle.png)

## 2.基于强化学习的Pool Resize设计

### 2.1 框架设计
应用程序在不同的运行阶段有着不同的访存模式，造成了不断变化的读写性能需求。然而，虽然读写缓冲池的大小对读写性能有着直接影响，但是现有缓存系统中的读写缓冲池的大小固定，无法跟随应用的性能需求而动态改变。为了解决这个问题，提出了 Pool Resize 设计，该设计收集当前缓冲池的配置信息和性能参数等信息，然后通过 socket 发送该信息给强化学习模型 OLUCB 生成新的缓冲池配置信息并对缓冲池大小进行改变。此设计通过实时检测和调整不同缓冲池大小的策略更好的满足了应用需求，提高了应用的存储性能，同时减少了资源空闲率。

![](image\OLUCB_frame.png)

整体框架采用 CS 架构实现，其中 Server 端运行 OLUCB 模型负责接收当前配置信息并计算新配置，Client 端是运行在缓存系统中的一个线程负责接收新配置并进行资源调整。

在s3fs中，资源调整线程为LinUCBThread，运行周期与程序的生存周期一致。每隔resizeInterval_段时间该线程会与Server端进行一次交互，Client向Server端发送信息如下：

```bash
"WritePool:"writeCacheSize_/fixSize; // 当前WritePool大小
"ReadPool:"readCacheSize_/fixSize;// 当前ReadPool大小
"WritePool:"writeThroughput; // 写入吞吐量
"ReadPool:"readThroughput;// 读取吞吐量
"WritePool:"writeCount_; // Put函数调用次数
"ReadPool:"readCount_;// Get函数调用次数
```

- 1~2行：当前WritePool和ReadPool各自占据的资源单位数目，以256M为1个资源单位。
- 3~4行：在这段时间间隔内的读写吞吐量。吞吐量计算方式如下，相关变量在Put/Get函数内部调用时累积统计
  - writeThroughput = (double)writeByteAcc_/(double)writeTimeAcc_/1024 / 1024;
  - readThroughput = (double)readByteAcc_/(double)readTimeAcc_/ 1024 / 1024;
- 5~6行：Put()函数和Get()函数的调用次数，作为context信息发给Server

Server端使用强化学习模型 OLUCB进行单目标调优，计算出下一轮次中WritePool和ReadPool应当获取的资源单位数目发送给Client。Client在根据发送来的信息调节Pool时流程如下：
1. 先进行缩池操作，对需要缩小的池计算当前剩余空间freeSize。如果当前剩余空间减去保留空间reserveSize的值小于要缩小的空间的值，则说明其最多可以减少freeSize - reverseSize大小的空间，将其对齐到256M以避免因为四舍五入导致总资源数量减少，进行缩池操作。
2. 进行增池操作，使用上一步实际缩小的容量作为当前可增加的容量，防止超出上限。

### 2.2 参数说明
- Server端IP 地址为 127.0.0.1，端口号为 2333
- 模型说明：[OLUCB单目标调度算法](https://epr023ri66.feishu.cn/docx/KfsddCGbLoZjf0xgSOqcw0V8nZb)
- fixSize：资源分配单位，= 1024 * 1024 * 256，即256M
- reserveSize：保留单位，用于防止Resize时出现某个Pool为0的情况，= 1024 * 1024 * 512，即512M
- resizeInterval_：Resize的时间间隔，设置为5s

### 2.3 使用说明
1. 修改JYCache_Env/conf/newcache.conf中的WriteCacheConfig.EnableLinUCB=1，开启s3fs；
2. 运行Server.py文件
3. 开始运行。

### 2.4 效果展示

![](image\result_LinUCB.png)