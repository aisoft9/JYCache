#!/usr/bin/env bash
if [ ! -d "./JYCache_Env" ]; then
    wget https://madstorage.s3.cn-north-1.jdcloud-oss.com/JYCache_Env_x64.tgz
    md5=`md5sum JYCache_Env_x64.tgz | awk {'print $1'}`
    if [ "$md5" != "cd27e0db8b1fc33b88bf1c467ed012b8" ]; then
    　　echo 'JYCache_Env version inconsistency!'
        exit 1
    fi
    tar -zxvf JYCache_Env_x64.tgz
fi

cp ./build/intercept/intercept_server JYCache_Env/
cp ./build/intercept/libintercept_client.so JYCache_Env/
cp ./build/global_cache/madfs_gc JYCache_Env/
cp ./build/global_cache/madfs_global_server JYCache_Env/
cp ./build/bin/s3fs JYCache_Env/
