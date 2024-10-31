#!/usr/bin/env bash

set -e

g_root="${PWD}"

build_jni_library() {
    # 1) generate jni native header file
    local maven_repo="${HOME}/.m2/repository"
    cd "${g_root}/hadoop_sdk/java/src/main/java"
    echo ${maven_repo}/org/apache/commons/commons-compress/1.24.0/commons-compress-1.24.0.jar
    echo ${maven_repo}/commons-io/commons-io/2.11.0/commons-io-2.11.0.jar
    javac -cp "${maven_repo}/org/apache/commons/commons-compress/1.24.0/commons-compress-1.24.0.jar:${maven_repo}/commons-io/commons-io/2.11.0/commons-io-2.11.0.jar":. \
        -h \
        "${g_root}/hadoop_sdk/java/native/" \
        io/aisoft9/jycache/fs/libfs/JYCacheFSMount.java

    # 2) build libjycachefs_jni.so
    cd "${g_root}"
    build_ops=()
    
    if [[ $(uname -i) == 'aarch64' || $(uname -m) == 'aarch64' ]]; then
        build_ops+=("--define compile_base_flags=base_flags_arm64 --define compile_flags=gcc_arm64 --copt=-fno-gcse --copt=-fno-cse-follow-jumps --copt=-fno-move-loop-invariants")
    fi
    bazel build \
        ${build_ops[@]} \
        --config=gcc7-later \
        --compilation_mode=opt \
        --copt -g \
        --copt -fvisibility=hidden \
        --copt -DUNINSTALL_SIGSEGV=1 \
        --copt -DCLIENT_CONF_PATH="\"${g_root}/jycachefs/conf/client.conf\"" \
        --action_env=BAZEL_LINKLIBS='-l%:libstdc++.a' \
        --action_env=BAZEL_LINKOPTS="-static-libstdc++ -lm -Wl,-exclude-libs,ALL -L${LIBRARY_PATH}" \
        //hadoop_sdk/java/native:jycachefs_jni
}

copy_jni_library() {
    local dest="${g_root}/hadoop_sdk/java/native/build/libjycachefs"
    rm -rf  "${dest}"
    mkdir -p "${dest}"

    # 1) copy the dependencies of the libjycachefs_jni.so
    local jni_so="$(realpath "${g_root}/bazel-bin/hadoop_sdk/java/native/libjycachefs_jni.so")"
    ldd "${jni_so}" | grep '=>' | awk '{ print $1, $3 }' | while read -r line;
    do
        name=$(echo "${line}" | cut -d ' ' -f1)
        path=$(echo "${line}" | cut -d ' ' -f2)
        sudo cp "$(realpath "${path}")" "${dest}/${name}"
    done

    # 2) copy libjycachefs_jni.so
    sudo cp "${jni_so}" "${dest}"

    # 3) achive libjycachefs
    cd "${g_root}/hadoop_sdk/java/native/build/"
    tar -cf libjycachefs.tar libjycachefs
    md5sum libjycachefs.tar > libjycachefs.tar.md5sum
    rm -rf libjycachefs
}

build_hadoop_jar() {
    # build jycachefs-hadoop-1.0-SNAPSHOT.jar
    cd "${g_root}/hadoop_sdk/java"
    mvn clean
    mvn package
}

download_hadoop_jar() {
    cd "${g_root}/hadoop_sdk/java"
    mvn clean
    mvn compile
}

display_output() {
    local src="${g_root}/hadoop_sdk/java/target/jycachefs-hadoop-1.0-SNAPSHOT.jar"
    local dest="${g_root}/hadoop_sdk/output";
    rm -rf "${dest}"
    mkdir -p "${dest}"
    cp "${src}" "${dest}"

    echo -e "\nBuild Hadoop SDK success => ${dest}/jycachefs-hadoop-1.0-SNAPSHOT.jar"
}

set_default_gcc() {
    if [ -z $GCC_10_DIR ]; then
        GCC_10_DIR=/usr/local/gcc-10.2.0
    fi

    if [ -d $GCC_10_DIR ]; then
	gcc_installed_dir=$GCC_10_DIR
    else
        gcc_installed_dir=""
    fi

    if [ -z "$gcc_installed_dir" ]; then
        echo "ERROR: GCC_10_DIR not set or exist!"
        exit 1
    fi

    export PATH=$gcc_installed_dir/bin:$PATH
    export LIBRARY_PATH=$gcc_installed_dir/lib64:$LIBRARY_PATH
    echo $PATH
    echo $LIBRARY_PATH
}

main() {
#    source "util/basic.sh"
   set_default_gcc 

    download_hadoop_jar
    build_jni_library
    copy_jni_library
    build_hadoop_jar
    display_output
}

main
