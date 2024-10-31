# Copyright (C) 2024 Jingli Chen (Wine93), # Inc.

root= $(shell echo $${PWD%/jycachefs/sdk/libjycachefs/examples})
so_path?= $(root)/bazel-bin/jycachefs/sdk/libjycachefs
hrd_opt?= -I/jycache
build_opt?= -L$(so_path)
link_opt?= -Wl,-rpath=$(so_path) -Wall
flags?= $(hrd_opt) $(build_opt) $(link_opt)
targets= mkdir rmdir ls touch read write unlink append rename stat statfs fstat chmod
build_copt= 

host_arch := $(shell uname -m | \
    sed -e s/i.86/x86/ \
        -e s/sun4u/sparc64/ \
        -e s/arm.*/arm/ \
        -e s/sa110/arm/ \
        -e s/ppc64/powerpc/ \
        -e s/ppc/powerpc/ \
        -e s/macppc/powerpc/\
        -e s/sh.*/sh/)
ifeq ($(host_arch), aarch64)
    build_copt= --define compile_base_flags=base_flags_arm64 --define compile_flags=gcc_arm64 --copt=-fno-gcse --copt=-fno-cse-follow-jumps --copt=-fno-move-loop-invariants
endif


libjycachefs:
	mkdir -p bin
	bazel build \
        --config=gcc7-later \
        --compilation_mode=opt \
        --copt -g \
        --copt -DUNINSTALL_SIGSEGV=1 \
        --copt -DCLIENT_CONF_PATH="\"$(root)/jycachefs/conf/client.conf\"" \
        $(build_copt) \
        //jycachefs/sdk/java/native:jycachefs_jni

$(targets): libjycachefs
	gcc ${flags} $@.c -o bin/$@ -ljycachefs

all: $(targets)


