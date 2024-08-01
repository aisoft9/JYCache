#!/usr/bin/env bash
if [ ! -d "./thirdparties" ]; then
    wget https://madstorage.s3.cn-north-1.jdcloud-oss.com/JYCache_Dendepency_x64.tgz
    md5=`md5sum JYCache_Dendepency_x64.tgz | awk {'print $1'}`
    if [ "$md5" != "48f67dd9b7bcb1b2bdd6be9f2283b714" ]; then
    　　echo 'JYCache_Dendepency version inconsistency!'
        exit 1
    fi
    tar -zxvf JYCache_Dendepency_x64.tgz
fi

mkdir -p build && cd build
cmake .. && cmake --build . -j 16
