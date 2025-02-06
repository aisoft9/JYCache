#!/usr/bin/env bash
if [ ! -d "./thirdparties" ]; then
    case $(uname -m) in
        x86_64)
            wget https://madstorage.s3.cn-north-1.jdcloud-oss.com/JYCache_Dendepency_x64.tgz
            md5=`md5sum JYCache_Dendepency_x64.tgz | awk {'print $1'}`
            if [ "$md5" != "48f67dd9b7bcb1b2bdd6be9f2283b714" ]; then
                echo 'JYCache_Dendepency version inconsistency!'
                exit 1
            fi
            tar -zxvf JYCache_Dendepency_x64.tgz
        ;;
        aarch64)
            wget https://madstorage.s3.cn-north-1.jdcloud-oss.com/JYCache_Dendepency_arm64_v1.0.1.tgz
            md5=`md5sum JYCache_Dendepency_arm64_v1.0.1.tgz | awk {'print $1'}`
            if [ "$md5" != "1de6694d30a5113bed34de7a8e2abe70" ]; then
                echo 'JYCache_Dendepency version inconsistency!'
                exit 1
            fi
            tar -zxvf JYCache_Dendepency_arm64_v1.0.1.tgz
        ;;
    esac
fi

mkdir -p build && cd build
cmake .. && cmake --build . -j 16
