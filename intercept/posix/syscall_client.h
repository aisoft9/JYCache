#ifndef CURVEFS_SRC_CLIENT_SYSCALL_CLIENT_
#define CURVEFS_SRC_CLIENT_SYSCALL_CLIENT_

#include <stddef.h>
#include <string.h>
#include <syscall.h>
#include <stdio.h>

#include "posix/libsyscall_intercept_hook_point.h"
//#include "syscall_interception.h"
#include "posix/posix_op.h"
#include <iostream>
#include <string>
#include <unordered_map>

// 拦截函数

static int hook(long syscallNumber,
                long arg0, long arg1,
                long arg2, long arg3,
                long arg4, long arg5,
                long* result) {
    
    long args[6] = {arg0, arg1, arg2, arg3, arg4, arg5};
    const struct syscall_desc* desc = GetSyscallDesc(syscallNumber, args);
    if (desc != nullptr) {
        int ret = desc->syscallFunction(args, result);
        //return 0; // 接管
        return ret;
    }
    
    return 1; // 如果不需要拦截，返回1 
}

// 初始化函数
static __attribute__((constructor)) void start(void) {
    InitSyscall();
    intercept_hook_point = &hook;
}

#endif



