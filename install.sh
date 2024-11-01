#!/usr/bin/env bash

if [ ! -d "./JYCache_Env" ]; then
    case $(uname -m) in
        x86_64)
            wget https://madstorage.s3.cn-north-1.jdcloud-oss.com/JYCache_Env_x64_20241101.tgz
            md5=`md5sum JYCache_Env_x64_20241101.tgz | awk {'print $1'}`
            if [ "$md5" != "4c685f0465bbe08191c0308889b59826" ]; then
                echo 'JYCache_Env version inconsistency!'
                exit 1
            fi
            tar -zxvf JYCache_Env_x64_20241101.tgz
        ;;
        aarch64)
            wget https://madstorage.s3.cn-north-1.jdcloud-oss.com/JYCache_Env_arm64_20241101.tgz
            md5=`md5sum JYCache_Env_arm64_20241101.tgz | awk {'print $1'}`
            if [ "$md5" != "4fcb8fec4217869e66747cd7841de8dc" ]; then
                echo 'JYCache_Env version inconsistency!'
                exit 1
            fi
            tar -zxvf JYCache_Env_arm64_20241101.tgz
        ;;
    esac
fi

cp ./build/intercept/intercept_server JYCache_Env/
cp ./build/intercept/libintercept_client.so JYCache_Env/
cp ./build/global_cache/madfs_gc JYCache_Env/
cp ./build/global_cache/madfs_global_server JYCache_Env/
cp ./build/bin/s3fs JYCache_Env/
