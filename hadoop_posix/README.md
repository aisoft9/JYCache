## 简介

此项目为JYCacheFS 提供hadoop支持，支持hadoop的应用，可以通过hadoop接口在JYCacheFS上存取数据。



## 部署

### 前置要求

1. 已经获取到此项目的发行包：jycachefs-hadoop-1.0-SNAPSHOT.jar
2. 已经部署并启动JYCacheFS，挂载路径：/home/jycachefs/mnt/

### 安装

1. 复制 jycachefs-hadoop-1.0-SNAPSHOT.jar 包到相应hadoop程序的目录下

   ```bash
   cp jycachefs-hadoop-1.0-SNAPSHOT.jar /opt/hadoop/share/hadoop/common/lib/
   cp jycachefs-hadoop-1.0-SNAPSHOT.jar /opt/hadoop/share/hadoop/yarn/lib/
   cp jycachefs-hadoop-1.0-SNAPSHOT.jar /opt/spark/jars/
   ```

2. 修改hadoop的配置文件(/opt/hadoop/etc/hadoop/core-site.xml)

   ```xml
   <?xml version="1.0" encoding="UTF-8"?>
   <?xml-stylesheet type="text/xsl" href="configuration.xsl"?>
   <!--
     Licensed under the Apache License, Version 2.0 (the "License");
     you may not use this file except in compliance with the License.
     You may obtain a copy of the License at
   
       http://www.apache.org/licenses/LICENSE-2.0
   
     Unless required by applicable law or agreed to in writing, software
     distributed under the License is distributed on an "AS IS" BASIS,
     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     See the License for the specific language governing permissions and
     limitations under the License. See accompanying LICENSE file.
   -->
   
   <!-- Put site-specific property overrides in this file. -->
   
   <configuration>
       <property>
           <name>fs.jycachefs.impl</name>
           <value>io.aisoft9.jycache.fs.hadoop.JYCacheFileSystem</value>
       </property>
       <property>
           <name>fs.AbstractFileSystem.jycachefs.impl</name>
           <value>io.aisoft9.jycache.fs.hadoop.JYCacheFS</value>
       </property>
       <property>
           <name>jycachefs.name</name>
           <value>jycachefs</value>
       </property>
       <property>
           <name>fs.defaultFS</name>
           <value>jycachefs://jycachefs</value>
       </property>
   </configuration>
   ```

3. 在相应应用中修改文件的访问路径uri为: jycachefs://jycachefs

### 验证

1. 查看是否能通过hadoop接口正常访问JYCacheFS

   ```bash
   hadoop fs -ls /
   ```



## 构建

构建jycachefs-hadoop-1.0-SNAPSHOT.jar, 若已获取jar包，则无需关注此项。

### 前置要求

1. 已安装 gcc-10，bazel， maven 等编译构建所需软件。

### 启动构建

1. 若GCC-10不在默认路径(/usr/local/gcc-10.2.0)，则需要手动指定路径。执行构建命令：

   ```bash
   cd hadoop_posix
   GCC_10_DIR=/home/programs/gcc-10.2.0/ bash util/sdk.sh
   ```

### 构建完成

1. 构建完成，jar包生成的路径：

   ```bash
   cd hadoop_posix
   ls -al hadoop_sdk/output/jycachefs-hadoop-1.0-SNAPSHOT.jar
   ```

   