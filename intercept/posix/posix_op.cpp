#include <cstdlib>
#include <cstring>
#include <stdio.h>
#include <syscall.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <cstring>
#include <memory>
#include <atomic>
#include <unistd.h>
#include <random>

#include "common/common.h"
#include "posix_op.h"
#include "middleware/iceoryx_wrapper.h"
#include "registry/client_server_registry.h"

using intercept::internal::FileType;
struct PosixInfo { 
    std::string fileName;
    FileType fileType;
    uint64_t fd;
    intercept::internal::DirStream dirinfo;
};

// key : 返回给上游的fd, value: 存储文件信息
std::unordered_map<int,  PosixInfo> g_fdtofile(10000);
// 以BEGIN_COUNTER为起始值在map中保存，避免fd从0开始与系统内部fd冲突
constexpr uint32_t BEGIN_COUNTER = 10000;
std::atomic<uint32_t> g_fdCounter(BEGIN_COUNTER);

std::chrono::steady_clock::duration totalDuration = std::chrono::steady_clock::duration::zero(); // 总耗时
int readnum = 0;

unsigned long g_processid = -1;
thread_local std::shared_ptr<intercept::middleware::ReqResMiddlewareWrapper> g_wrapper;
thread_local bool g_initflag = false;
std::mutex global_mutex;

thread_local struct ThreadCleanup {
    ThreadCleanup() {
        std::cout << "Thread cleanup object created\n";
    }

    ~ThreadCleanup() {
        std::cout << "Thread cleanup object destroyed\n";
    }
} cleanup;

struct syscall_desc table[1000] = {
    {0, 0, {argNone, argNone, argNone, argNone, argNone, argNone}}};

#define FUNC_NAME(name) PosixOp##name
#define REGISTER_CALL(sysname, funcname, ...)                                  \
    table[SYS_##sysname] = syscall_desc {                                      \
        #sysname, (FUNC_NAME(funcname)), { __VA_ARGS__, }                      \
    }


// ---------------------------init and unint---------------------------------- 



int ThreadInit() {
    // std::lock_guard<std::mutex> lock(global_mutex);
    if (g_initflag == true) {
        return 0;
    }
    std::stringstream ss;
    auto myid = std::this_thread::get_id();
    ss << myid;
    std::string threadid = ss.str();
    pthread_t tid = pthread_self();
    pid_t pid = getpid();
    if (g_processid == -1) {
        // 进程级初始化
        g_processid = (unsigned long)pid;
        GlobalInit();
    }
    spdlog::warn("thread init, processid: {}, threadid: {}, flag id: {}",
                    (unsigned long) pid, (unsigned long)tid, g_initflag);
    // sleep(10);
    
    intercept::internal::ServiceMetaInfo info;
    info.service = SERVICE_FLAG;
    info.instance = INTERCEPT_INSTANCE_FLAG;
    intercept::registry::ClientServerRegistry registry(ICEORYX, info);
    auto dummyserver = registry.CreateDummyServer();
    std::cout << "wait dummy server for client...." << std::endl;
    sleep(5);

    info = dummyserver->GetServiceMetaInfo();
    info.service = SERVICE_FLAG;
    info.instance = INTERCEPT_INSTANCE_FLAG;
    g_wrapper = registry.CreateClient(info);
    g_initflag = true;
    return 0;
}
int GlobalInit() {
    if (intercept::common::Configure::getInstance().loadConfig(intercept::common::CONFIG_FILE)) {
        std::cout << "Config file loaded : " << intercept::common::CONFIG_FILE << std::endl;
    } else {
        std::cout << "Config file not loaded:" << intercept::common::CONFIG_FILE <<  std::endl;
        return 0;
    }
    intercept::common::InitLog();

    constexpr char BASE_APP_NAME[] = "iox-intercept-client";
    std::string appNameWithRandom = BASE_APP_NAME + intercept::common::generateRandomSuffix();
    iox::string<iox::MAX_RUNTIME_NAME_LENGTH> appname(iox::TruncateToCapacity, appNameWithRandom.c_str(), appNameWithRandom.length());
    spdlog::info("create app name: {}", appNameWithRandom);
    iox::runtime::PoshRuntime::initRuntime(appname);
    return 0;
}

void UnInitPosixClient() {
}

// 初始化函数    
static __attribute__((constructor)) void Init(void) {
    printf("Library loaded: PID %d TID: %lu\n", getpid(), (unsigned long)pthread_self());
    //GlobalInit();
}

// 退出函数
static __attribute__((destructor)) void Clean(void) {   
    // std::cout << "readnum: " << readnum << " ,  total time : " << totalDuration.count() << " , average time : " <<  totalDuration.count() / readnum  << std::endl;
    pthread_t tid = pthread_self();
    pid_t pid = getpid();
    std::cout << "exit and kill, pid:" <<   (unsigned long)pid 
              << " threadid:" << (unsigned long) tid << std::endl;
    //kill(getpid(), SIGINT);
    //sleep(5); 
}

// ---------------------------posix func----------------------------------------------

// 判断字符串是否以指定挂载点开头
bool StartsWithMountPath(const char *str) {
    // 指定路径
    const std::string mountpath = "/testdir";
        //"/home/caiyi/shared_memory_code/iceoryx/iceoryx_examples/intercept/testdir";
    size_t prefixLen = mountpath.length();
    return strncmp(str, mountpath.c_str(), prefixLen) == 0;
}
    
std::string GetPath(const char* path) {
    return "";
}

// 获取相对路径
std::string GetRelativeFilePath(const std::string& fullPath) {
    size_t found = fullPath.find_last_of("/\\");
    return fullPath.substr(found+1);
}

// 判断路径是否有效
bool IsValidPath(arg_type type, long arg0, long arg1) {
    int fd = -1;
    switch (type) {
    case argFd:
        fd = (int)arg0;
        if (fd >= BEGIN_COUNTER &&
            (g_fdtofile.empty() == false && g_fdtofile.count(fd)) > 0) {
            return true;
        } else {
            return false;
        }
    case argCstr:
        if (StartsWithMountPath(reinterpret_cast<const char *>(arg0))) {
            return true;
        } else {
            // printf("cstr, not right filepath: %s\n", reinterpret_cast<const
            // char*>(arg0));
            return false;
        }
    case argAtfd:
        if (StartsWithMountPath(reinterpret_cast<const char *>(arg1)) ||
            (g_fdtofile.empty() == false && g_fdtofile.count((int)arg0)) > 0) {
            return true;
        } else {
            // printf("atfd, not right filepath: %s\n", reinterpret_cast<const
            // char*>(arg1));
            return false;
        }
    case arg_:
        return true;
    default:
        return false;
    }
}

// 判断系统调用是否需要拦截
bool ShouldInterceptSyscall(const struct syscall_desc *desc, const long *args) {
    return IsValidPath(desc->args[0], args[0], args[1]);
}

const struct syscall_desc *GetSyscallDesc(long syscallNumber,
                                          const long args[6]) {
    //char buffer[1024];
    if (syscallNumber < 0 ||
        static_cast<size_t>(syscallNumber) >=
            sizeof(table) / sizeof(table[0]) ||
        table[syscallNumber].name == NULL ||
        ShouldInterceptSyscall(&table[syscallNumber], args) == false) {
        return nullptr;
    }
    //sprintf(buffer, "right number:%ld, name:%s\n", syscallNumber, table[syscallNumber].name);
    //printSyscall(buffer);
    return table + syscallNumber;
}

uint32_t GetNextFileDescriptor() { return g_fdCounter.fetch_add(1); }

void InitSyscall() {
    #ifdef __aarch64__
    //REGISTER_CALL(access, Access, argCstr, argMode);
    REGISTER_CALL(faccessat, Faccessat, argAtfd, argCstr, argMode);
    //REGISTER_CALL(open, Open, argCstr, argOpenFlags, argMode);
    REGISTER_CALL(close, Close, argFd);
    REGISTER_CALL(openat, Openat, argAtfd, argCstr, argOpenFlags, argMode);
    //REGISTER_CALL(creat, Creat, argCstr, argMode);
    REGISTER_CALL(write, Write, argFd);
    REGISTER_CALL(read, Read, argFd);
    REGISTER_CALL(fsync, Fsync, argFd);
    REGISTER_CALL(lseek, Lseek, argFd);
    //REGISTER_CALL(stat, Stat, argCstr);
    // for fstatat
    REGISTER_CALL(newfstatat, Newfstatat, argAtfd, argCstr);
    REGISTER_CALL(fstat, Fstat, argFd);
    REGISTER_CALL(statx, Statx, argAtfd, argCstr);
    //REGISTER_CALL(lstat, Lstat, argCstr);
    //REGISTER_CALL(mkdir, MkDir, argCstr, argMode);
    REGISTER_CALL(mkdirat, MkDirat, argAtfd, argCstr, argMode);
    REGISTER_CALL(getdents64, Getdents64, argFd, argCstr, arg_);
    //REGISTER_CALL(unlink, Unlink, argCstr);
    REGISTER_CALL(unlinkat, Unlinkat, argAtfd, argCstr, argMode);
    //REGISTER_CALL(rmdir, Rmdir, argCstr);
    REGISTER_CALL(chdir, Chdir, argCstr);
    REGISTER_CALL(utimensat, Utimensat, argAtfd, argCstr);
    REGISTER_CALL(statfs, Statfs, argCstr);
    REGISTER_CALL(fstatfs, Fstatfs, argFd);

    REGISTER_CALL(truncate, Truncate, argCstr);
    REGISTER_CALL(ftruncate, Ftruncate, argFd);
    REGISTER_CALL(renameat, Renameat, argAtfd, argCstr);
    #else
    REGISTER_CALL(access, Access, argCstr, argMode);
    REGISTER_CALL(faccessat, Faccessat, argAtfd, argCstr, argMode);
    REGISTER_CALL(open, Open, argCstr, argOpenFlags, argMode);
    REGISTER_CALL(close, Close, argFd);
    REGISTER_CALL(openat, Openat, argAtfd, argCstr, argOpenFlags, argMode);
    REGISTER_CALL(creat, Creat, argCstr, argMode);
    REGISTER_CALL(write, Write, argFd);
    REGISTER_CALL(read, Read, argFd);
    REGISTER_CALL(fsync, Fsync, argFd);
    REGISTER_CALL(lseek, Lseek, argFd);
    REGISTER_CALL(stat, Stat, argCstr);
    // for fstatat
    REGISTER_CALL(newfstatat, Newfstatat, argAtfd, argCstr);
    REGISTER_CALL(fstat, Fstat, argFd);
    REGISTER_CALL(lstat, Lstat, argCstr);
    REGISTER_CALL(mkdir, MkDir, argCstr, argMode);
    REGISTER_CALL(getdents64, Getdents64, argFd, argCstr, arg_);
    REGISTER_CALL(unlink, Unlink, argCstr);
    REGISTER_CALL(unlinkat, Unlinkat, argAtfd, argCstr, argMode);
    REGISTER_CALL(rmdir, Rmdir, argCstr);
    REGISTER_CALL(chdir, Chdir, argCstr);
    REGISTER_CALL(utimensat, Utimensat, argAtfd, argCstr);
    REGISTER_CALL(statfs, Statfs, argCstr);
    REGISTER_CALL(fstatfs, Fstatfs, argFd);

    REGISTER_CALL(truncate, Truncate, argCstr);
    REGISTER_CALL(ftruncate, Ftruncate, argFd);
    REGISTER_CALL(rename, Rename, argCstr, argCstr);
    #endif
}

int PosixOpAccess(const long *args, long *result) {
    return 0;
}

int PosixOpFaccessat(const long *args, long *result) {
    return PosixOpAccess(args + 1, result);
}

int PosixOpOpen(const long *args, long *result) {
    ThreadInit();
    const char* path = (const char*)args[0];
    int flags = args[1];
    mode_t mode = args[2];

    if (flags & O_DIRECTORY) {
        intercept::internal::OpendirOpReqRes req(path);
         g_wrapper->OnRequest(req);
        const auto& openRes = static_cast<intercept::internal::OpendirResponseData&>  (req.GetResponse());
        // 向上游返回的fd
        *result = openRes.dirStream.fh + BEGIN_COUNTER;
        // 记录打开的fd
        PosixInfo info;
        info.fd = *result;
        info.dirinfo = openRes.dirStream;
        info.fileType = FileType::DIR;
        g_fdtofile[*result] = info;
        std::cout << "the opendir result fd is: " << *result <<  std::endl;
    } else {
        intercept::internal::OpenOpReqRes req(path, flags, mode);
         g_wrapper->OnRequest(req);
        const auto& openRes = static_cast<intercept::internal::OpenResponseData&>  (req.GetResponse());
        // 向上游返回的fd
        *result = openRes.fd + BEGIN_COUNTER;
        // 记录打开的fd
        PosixInfo info;
        info.fd = *result;
        info.fileType = FileType::FILE;
        info.fileName  = path;
        g_fdtofile[*result] = info;
        spdlog::info("the open result fd: {}, path: {}", *result, path);
    }
    return 0;
}

int PosixOpOpenat(const long *args, long *result) {
    return PosixOpOpen(args + 1, result);  // args[0] is dir fd, jump
}

int PosixOpCreat(const long *args, long *result) {
    return 0;
}

int PosixOpRead(const long *args, long *result) {
    ThreadInit();
    int fd = args[0] - BEGIN_COUNTER;
    char* buf = (char*)args[1];
    int count = args[2];
    const auto& info = g_fdtofile[fd];
    std::string timeinfo = "client read, count: " + std::to_string(count) + "  filename: " + info.fileName;
    intercept::common::Timer timer(timeinfo);

    intercept::internal::ReadOpReqRes readReq(fd, buf, count);
    //intercept::common::Timer timer("client OnRequest");
    g_wrapper->OnRequest(readReq);
    
    const auto& readRes = static_cast<intercept::internal::ReadResponseData&>  (readReq.GetResponse());
    *result = readRes.length;
    spdlog::debug("read fd: {}, length: {}", fd, readRes.length);
    return 0;
}

int PosixOpWrite(const long *args, long *result) {
    spdlog::debug("get write request...");
    ThreadInit();
    int fd = args[0] - BEGIN_COUNTER;
    char* writebuf = (char*)args[1];
    int count = args[2];
    std::string timeinfo = "client write, count: " + std::to_string(count);
    intercept::common::Timer timer(timeinfo);
    intercept::internal::WriteOpReqRes writeReq(fd, writebuf, count);
    g_wrapper->OnRequest(writeReq);
    const auto& writeRes = static_cast<intercept::internal::WriteResponseData&> (writeReq.GetResponse());
    *result = writeRes.length;
    spdlog::debug("write fd: {}, length: {}", fd, writeRes.length);
    return 0;
}
int PosixOpFsync(const long *args, long *result) {
    ThreadInit();
    int fd = args[0] - BEGIN_COUNTER;
    spdlog::info("begin fsync, fd: {}", fd);
    intercept::internal::FsyncOpReqRes fsyncReq(fd);
    g_wrapper->OnRequest(fsyncReq);
    const auto& fsyncRes = static_cast<intercept::internal::FsyncResponseData&> (fsyncReq.GetResponse());
    *result = fsyncRes.ret;
    spdlog::info("the fysnc result is: {}", *result);
    return 0;
}

int PosixOpLseek(const long *args, long *result) {
    ThreadInit();
    int fd = args[0] - BEGIN_COUNTER;
    long offset = args[1];
    int whence = args[2];
    intercept::internal::LseekOpReqRes lseekReq(fd, offset, whence);
    g_wrapper->OnRequest(lseekReq);
    const auto& lseekRes = static_cast<intercept::internal::LseekResponseData&> (lseekReq.GetResponse());
    *result = lseekRes.ret;
    // std::cout << "the lseek result is: " << *result << "  , the offset: "<<  offset << std::endl;
    spdlog::debug("lseek, fd: {}, offset: {}, whence: {}, result: {}", fd, offset, whence, *result);
    return 0;
}

int PosixOpStat(const long *args, long *result) {
    ThreadInit();
    spdlog::debug("it is opstat...");
    const char* filename = (const char*) args[0];
    struct stat* statbuf = (struct stat*) args[1];
    intercept::internal::StatOpReqRes statReq(filename, statbuf);
    g_wrapper->OnRequest(statReq);
    const auto& statRes = static_cast<intercept::internal::StatResponseData&> (statReq.GetResponse());
    // 向上游返回的fd
    *result = statRes.ret;
    spdlog::debug("the stat result fd: {}", *result);
    return 0;
}
int PosixOpNewfstatat(const long *args, long *result) {
    std::cout << "newfstatat" << std::endl;
    // TODO: 以args[0]为起点，找到args[1]路径
    int ret = 0;
    if (strlen((char*)args[1]) == 0) {
        // 空目录
        long newargs[4];
        newargs[0] = args[0];
        newargs[1] = args[2];
        return PosixOpFstat(newargs, result);
    }
    return PosixOpStat(args + 1, result);
}

int PosixOpLstat(const long *args, long *result) {
    std::cout << "call PosixOpLstat" << std::endl;
    return PosixOpStat(args, result);
}

int PosixOpFstat(const long *args, long *result) {
    ThreadInit();
    spdlog::debug("it is opfstat...");
    int fd =  args[0]  - BEGIN_COUNTER;
    struct stat* statbuf = (struct stat*) args[1];
    intercept::internal::FstatOpReqRes statReq(fd, statbuf);
    g_wrapper->OnRequest(statReq);
    const auto& statRes = static_cast<intercept::internal::FstatResponseData&> (statReq.GetResponse());
    // 向上游返回的fd
    *result = statRes.ret;
    spdlog::debug("the fstat result fd: {}, the stat ino: {}, size: {}",
        fd, statbuf->st_ino, statbuf->st_size);
    return 0;
}

int PosixOpFstat64(const long *args, long *result) {
    std::cout << "it is opfstat64" << std::endl;
    return 0;
}

int PosixOpStatx(const long *args, long *result) {
    ThreadInit();
    std::cout << "it is opstatx" << std::endl;
    const char* filename = (const char*) args[1];
    struct statx* fileStat = (struct statx*) args[4];
    struct stat tmpStat;
    intercept::internal::StatOpReqRes statReq(filename, &tmpStat);
    g_wrapper->OnRequest(statReq);
    const auto& statRes = static_cast<intercept::internal::StatResponseData&> (statReq.GetResponse());
    if (statRes.ret != 0 ) {
        std::cout << "get stat failed.." << std::endl;
    }
    
    *result = statRes.ret;
     // inode number
     fileStat->stx_ino = (uint64_t)tmpStat.st_ino;
 
     // total size, in bytes
     fileStat->stx_size = (uint64_t)tmpStat.st_size;
 
     // protection
     fileStat->stx_mode = (uint32_t)tmpStat.st_mode;
 
     // number of hard links
     fileStat->stx_nlink = (uint32_t)tmpStat.st_nlink;
 
     // user ID of owner
     fileStat->stx_uid = (uint32_t)tmpStat.st_uid;
 
     // group ID of owner
     fileStat->stx_gid = (uint32_t)tmpStat.st_gid;
 
     // last access time 
     fileStat->stx_atime.tv_sec = tmpStat.st_atim.tv_sec;
     fileStat->stx_atime.tv_nsec = tmpStat.st_atim.tv_nsec;
 
     // last modification time
     fileStat->stx_mtime.tv_sec = tmpStat.st_mtim.tv_sec;
     fileStat->stx_mtime.tv_nsec = tmpStat.st_mtim.tv_nsec;
 
     // last status change time
     fileStat->stx_ctime.tv_sec = tmpStat.st_ctim.tv_sec;
     fileStat->stx_ctime.tv_nsec = tmpStat.st_ctim.tv_nsec;
 
     // 示意性地为stx_attributes设置一个默认值，实际上这需要更具体的场景考虑
     fileStat->stx_attributes = 0;  // 假设没有额外的属性
 
     // stx_attributes_mask通常和stx_attributes一起使用，表示希望查询或设置哪些属性
     fileStat->stx_attributes_mask = 0;  // 示意性地设置，可能需要根据场景具体调整
    return 0;
}

int PosixOpClose(const long *args, long *result) {
    if (g_fdtofile.find((int)args[0]) == g_fdtofile.end()) {
        std::cout << "fd not found: " << args[0] << std::endl;
    }
    const auto& info = g_fdtofile[(int)args[0]];
    if (info.fileType == FileType::FILE) {
        int fd = args[0] - BEGIN_COUNTER;
        intercept::internal::CloseOpReqRes req(fd);
        spdlog::info("begin close, fd: {}", fd);
        g_wrapper->OnRequest(req);
        const auto& closeRes = static_cast<intercept::internal::CloseResponseData&>  (req.GetResponse());
        // 向上游返回的fd
        *result = closeRes.ret;
        spdlog::info("the close result, fd: {}", fd);
    } else if (info.fileType == FileType::DIR) {
        int fd = args[0] - BEGIN_COUNTER;
        intercept::internal::ClosedirOpReqRes req(info.dirinfo);
        g_wrapper->OnRequest(req);
        const auto& closeRes = static_cast<intercept::internal::CloseResponseData&>  (req.GetResponse());
        // 向上游返回的fd
        *result = closeRes.ret;
        std::cout << "the closedir result fd is: " << fd <<  std::endl;
    } else {
        std::cout << "unknown file type for close" << std::endl;
    }
    g_fdtofile.erase((int)args[0]);
    return 0;
}

int PosixOpMkDir(const long *args, long *result) {
    ThreadInit();
    const char* path = (const char*) args[0];
    mode_t mode = args[1];
    intercept::internal::MkdirOpReqRes req(path, mode);
    g_wrapper->OnRequest(req);
    const auto& mkdirRes = static_cast<intercept::internal::MkdirResponseData&>  (req.GetResponse());
    // 向上游返回的fd
    *result = mkdirRes.ret;
    std::cout << "the mkdir result fd is: " << *result <<  std::endl;
    return 0;
}

int PosixOpMkDirat(const long *args, long *result) {
    // 直接按照绝对路径处理
    return PosixOpMkDir(args + 1, result);
}

int PosixOpOpenDir(const long *args, long *result) {
    std::cout << "open dir....." << std::endl;
    return 0;
}

int PosixOpGetdents64(const long *args, long *result) {
    int fd = args[0] - BEGIN_COUNTER;
    char* data = (char*)args[1];
    size_t maxread = args[2];
    if (g_fdtofile.find(args[0]) == g_fdtofile.end()) {
        std::cout << "fd not found" << std::endl;
        *result = 0;
        return 0;
    }
    std::cout << "getdents request, fd: " << fd << " maxread: " << maxread << std::endl;
    PosixInfo& posixinfo = g_fdtofile[args[0]];
    intercept::internal::GetdentsOpReqRes req(posixinfo.dirinfo, data, maxread);
    g_wrapper->OnRequest(req);
    const auto& getdentsRes = static_cast<intercept::internal::GetdentsResponseData&> (req.GetResponse());
    posixinfo.dirinfo.offset = getdentsRes.dirinfo.offset;
    *result = getdentsRes.realbytes;
    std::cout << "the getdents result bytes:" << getdentsRes.realbytes << ", offset is: " << getdentsRes.dirinfo.offset <<  std::endl;
    return 0;
}

int PosixOpRmdir(const long *args, long *result) {
    std::cout << "rmdir, call thePosixOpUnlink " << std::endl;
    PosixOpUnlink(args, result);
    return 0;
}

int PosixOpChdir(const long *args, long *result) {
    return 0;
}

int PosixOpUnlink(const long *args, long *result) {
    const char* path = (const char*) args[0];
    intercept::internal::UnlinkOpReqRes req(path);
    g_wrapper->OnRequest(req);
    const auto& unlinkRes = static_cast<intercept::internal::UnlinkResponseData&>  (req.GetResponse());
    // 向上游返回的fd
    *result = unlinkRes.ret;
    std::cout << "the unlink path: " << path << " ,result fd is: " << *result <<  std::endl;
    return 0;
}

int PosixOpUnlinkat(const long *args, long *result) {
    const char *filename = (const char *)args[1];
    int flags = args[2];
    if (flags & AT_REMOVEDIR) { 
        // 删除目录
        std::cout << "unlinkat remove dir..." << std::endl;
        PosixOpRmdir(args + 1, result);
        return 0;
    }
    int ret = 0;
    // 暂不支持从指定位置开始删除
    ret = PosixOpUnlink(args + 1, result);
    std::cout << "unlinkat... ret: " << ret << std::endl;
    return ret;
}

int PosixOpUtimensat(const long* args, long *result) {
    int dirfd = args[0];
    return 0;
}

int PosixOpExitgroup(const long* args, long *result) {
    return 0;
}

int PosixOpStatfs(const long* args, long *result) {
    return 0; 
}

int PosixOpFstatfs(const long* args, long *result) {
    return 0; 
}

int PosixOpTruncate(const long* args, long *result) {
    const char* path = (const char*) args[0];
    off_t length = args[1];
    intercept::internal::TruncateOpReqRes req(path, length);
    g_wrapper->OnRequest(req);
    const auto& truncateRes = static_cast<intercept::internal::TruncateResponseData&>  (req.GetResponse());
    // 向上游返回的fd
    *result = truncateRes.ret;
    std::cout << "the truncate path: " << path << " ,result fd is: " << *result <<  std::endl;
    return 0;
}

int PosixOpFtruncate(const long* args, long *result) {
    return 0;
}

int PosixOpRename(const long *args, long *result) {
    return 0;
}

int PosixOpRenameat(const long *args, long *result) {
    // 假设都从根目录开始
    const char *oldpath = (const char *)args[1];
    const char* newpath = (const char*)args[3];
    intercept::internal::RenameOpReqRes req(oldpath, newpath);
    g_wrapper->OnRequest(req);
    const auto& renameRes = static_cast<intercept::internal::RenameResponseData&>  (req.GetResponse());
    // 向上游返回的fd
    *result = renameRes.ret;
    std::cout << "the rename path: " << oldpath << " ,result fd is: " << *result <<  std::endl;
    return 0;
}


