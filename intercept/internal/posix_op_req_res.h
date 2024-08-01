#pragma once 
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

#include "common/common.h"

namespace intercept {
namespace internal {
using intercept::common::DirStream;
// 操作类型枚举
enum class FileType {
    FILE = 0,
    DIR = 1,
};

enum class PosixOpType {
    OPEN = 0,
    WRITE,
    READ,
    ACCESS,
    CLOSE,
    FSYNC,
    TRUNCATE,
    FTRUNCATE,
    FUTIMES,
    LSEEK,
    MKDIR,
    MKNOD,
    OPENDIR,
    READDIR,
    GETDENTS,
    CLOSEDIR,
    RENAME,
    STAT,
    FSTAT,
    UNLINK,
    UTIMES,
    TERMINAL, // 程序退出时的操作
    // ... 其他操作类型
};

std::string TypeToStr(PosixOpType opType);

// 请求数据结构体
struct PosixOpRequest  {
     PosixOpType opType;
     //virtual ~PosixOpRequest() = default; // 添加虚析构函数使类变为多态
};

// 响应数据结构体
struct PosixOpResponse{
    PosixOpType opType;
    //virtual ~PosixOpResponse() = default; // 添加虚析构函数使类变为多态
};

// 请求/响应类
class PosixOpReqRes {
public:
    PosixOpReqRes() = default;

    PosixOpReqRes(PosixOpType opType);

    PosixOpReqRes(const long* args, long* result);

    virtual ~PosixOpReqRes() = default; // 添加虚析构函数使类变为多态

    void SetOpType(PosixOpType type);

    PosixOpType GetOpType() const;

    // virtual void Init() = 0;

    // virtual void Shutdown() = 0;

    // 设置和获取请求数据
    // virtual const PosixOpRequest& GetRequestData() const = 0;
    // virtual void SetRequestData(const PosixOpRequest& requestData) = 0;
    // virtual void SetRequestData(const long* args, long* result) = 0;

    // 复制请求数据到缓冲区
    virtual void CopyRequestDataToBuf(void* buf) = 0;

    // 获取请求大小
    virtual int GetRequestSize() = 0;
    virtual int GetRequestAlignSize() = 0;
    virtual int GetResponseSize() = 0;
    virtual int GetResponseAlignSize() = 0;

    // 设置和获取响应数据
    virtual PosixOpResponse& GetResponse() = 0;

    virtual void SetResponse(void* response) = 0;
    
protected:
    PosixOpType opType_;
};

// ---------------------------------open------------------------------------------------
struct OpenRequestData : PosixOpRequest {
    char path[200];
    int flags;
    mode_t mode;
};

struct OpenResponseData : PosixOpResponse {
    int fd;
};
class OpenOpReqRes : public PosixOpReqRes {
public:
    OpenOpReqRes(const char* path, int flags, mode_t mode);
    
    OpenOpReqRes(const long *args, long *result);

    ~OpenOpReqRes() override;

    // 复制请求数据到缓冲区
    virtual void CopyRequestDataToBuf(void* buf);

    // 获取请求大小
    int GetRequestSize() override;
    int GetRequestAlignSize() override;

    int GetResponseSize() override;
    int GetResponseAlignSize() override;

    // 获取和设置响应数据
    PosixOpResponse& GetResponse() override;
    void SetResponse(void* request) override;

private:
    OpenRequestData requestData_;
    OpenResponseData responseData_;
};


// --------------------------------------read----------------------------------------
struct ReadRequestData : PosixOpRequest {
    int fd;
    size_t count;
    // void* buf;
};

struct ReadResponseData : PosixOpResponse {
    int ret; // 返回值
    ssize_t length; // 返回长度
    void* buf; // 为上游保存数据的指针
    char content[0]; // server返回数据 
};

class ReadOpReqRes : public PosixOpReqRes {
public:
    ReadOpReqRes(int fd, void* buf, size_t count);
    ReadOpReqRes(const long *args, long *result);
    virtual ~ReadOpReqRes() override;

    virtual void CopyRequestDataToBuf(void* buf);

    virtual int GetRequestSize() override;
    virtual int GetRequestAlignSize() override;
    virtual int GetResponseSize() override;
    virtual int GetResponseAlignSize() override;

    virtual PosixOpResponse& GetResponse() override;
    void SetResponse(void* response) override;
    void SetResponseMultithreads(void* response);

private:
    ReadRequestData requestData_;
    ReadResponseData responseData_;
    // intercept::common::ThreadPool threadPool_;
};

// ---------------------------------write-------------------------------------------
struct WriteRequestData : PosixOpRequest {
    int fd;
    size_t count; // 要求长度
    void* buf;
    char content[0]; // 传输时保存数据
};

struct WriteResponseData : PosixOpResponse {
    int ret; // 返回值
    ssize_t length; // 返回长度
};

class WriteOpReqRes : public PosixOpReqRes {
public:
    WriteOpReqRes()
        : PosixOpReqRes(PosixOpType::WRITE) {}
    WriteOpReqRes(int fd, void* buf, size_t count);
    WriteOpReqRes(const long *args, long *result);
    ~WriteOpReqRes() override;

    virtual void CopyRequestDataToBuf(void* buf);
    void CopyRequestDataToBufMultithread(void* dest, const void* src, size_t count, int numThreads);

    virtual int GetRequestSize() override;
    virtual int GetRequestAlignSize() override;
    virtual int GetResponseSize() override;
    virtual int GetResponseAlignSize() override;

    virtual PosixOpResponse& GetResponse() override;
    void SetResponse(void* response) override;
    

private:
    WriteRequestData requestData_;
    WriteResponseData responseData_;
};

//-------------------------------------close---------------------------------------
struct CloseRequestData : PosixOpRequest {
    int fd;
};

struct CloseResponseData : PosixOpResponse {
    int ret; // 返回值
};

class CloseOpReqRes : public PosixOpReqRes {
public:
    CloseOpReqRes()
        : PosixOpReqRes(PosixOpType::CLOSE) {}
    CloseOpReqRes(int fd);
    CloseOpReqRes(const long *args, long *result);
    ~CloseOpReqRes() override;

    virtual void CopyRequestDataToBuf(void* buf);

    virtual int GetRequestSize() override;
    virtual int GetRequestAlignSize() override;
    virtual int GetResponseSize() override;
    virtual int GetResponseAlignSize() override;

    virtual PosixOpResponse& GetResponse() override;
    void SetResponse(void* response) override;

private:
    CloseRequestData requestData_;
    CloseResponseData responseData_;
};

// ----------------------------------------fsync-------------------------------
struct FsyncRequestData : PosixOpRequest {
    int fd;
};

struct FsyncResponseData : PosixOpResponse {
    int ret; // 返回值
};

class FsyncOpReqRes : public PosixOpReqRes {
public:
    FsyncOpReqRes()
        : PosixOpReqRes(PosixOpType::CLOSE) {}
    FsyncOpReqRes(int fd);
    FsyncOpReqRes(const long *args, long *result);
    ~FsyncOpReqRes() override;

    virtual void CopyRequestDataToBuf(void* buf);

    virtual int GetRequestSize() override;
    virtual int GetRequestAlignSize() override;
    virtual int GetResponseSize() override;
    virtual int GetResponseAlignSize() override;

    virtual PosixOpResponse& GetResponse() override;
    void SetResponse(void* response) override;

private:
    FsyncRequestData requestData_;
    FsyncResponseData responseData_;
};
// -----------------------------------stat----------------------------------------
struct StatRequestData : PosixOpRequest {
    char path[200];
};

struct StatResponseData : PosixOpResponse {
    int ret; // 返回值
    void* st; // 为上游保存数据的指针
    struct stat fileStat; // server返回数据 
};

class StatOpReqRes : public PosixOpReqRes {
public:
    StatOpReqRes()
        : PosixOpReqRes(PosixOpType::STAT) {}
    StatOpReqRes(const char *path, struct stat *st);
    StatOpReqRes(const long *args, long *result);
    ~StatOpReqRes() override;

    virtual void CopyRequestDataToBuf(void* buf);

    virtual int GetRequestSize() override;
    virtual int GetRequestAlignSize() override;
    virtual int GetResponseSize() override;
    virtual int GetResponseAlignSize() override;

    virtual PosixOpResponse& GetResponse() override;
    void SetResponse(void* response) override;
private:
    StatRequestData requestData_;
    StatResponseData responseData_;
};

// ----------------------------------fstat------------------------------------------
struct FstatRequestData : PosixOpRequest {
    int fd;
};

struct FstatResponseData : PosixOpResponse {
    int ret; // 返回值
    void* st; // 为上游保存数据的指针
    struct stat fileStat; // server返回数据 
};

class FstatOpReqRes : public PosixOpReqRes {
public:
    FstatOpReqRes()
        : PosixOpReqRes(PosixOpType::FSTAT) {}
    FstatOpReqRes(int fd, struct stat *st);
    FstatOpReqRes(const long *args, long *result);
    ~FstatOpReqRes() override;

    virtual void CopyRequestDataToBuf(void* buf);

    virtual int GetRequestSize() override;
    virtual int GetRequestAlignSize() override;
    virtual int GetResponseSize() override;
    virtual int GetResponseAlignSize() override;

    virtual PosixOpResponse& GetResponse() override;
    void SetResponse(void* response) override;
private:
    FstatRequestData requestData_;
    FstatResponseData responseData_;
};

// -----------------------------------lseek------------------------------------------
struct LseekRequestData : PosixOpRequest {
    int fd;
    uint64_t offset;
    int whence;
};

struct LseekResponseData : PosixOpResponse {
    off_t ret; // 返回值
};

class LseekOpReqRes : public PosixOpReqRes {
public:
    LseekOpReqRes()
        : PosixOpReqRes(PosixOpType::LSEEK) {}
        
    LseekOpReqRes(int fd, uint64_t offset, int whence);
    LseekOpReqRes(const long *args, long *result);
    ~LseekOpReqRes() override;

    virtual void CopyRequestDataToBuf(void* buf);

    virtual int GetRequestSize() override;
    virtual int GetRequestAlignSize() override;
    virtual int GetResponseSize() override;
    virtual int GetResponseAlignSize() override;

    virtual PosixOpResponse& GetResponse() override;
    void SetResponse(void* response) override;
private:
    LseekRequestData requestData_;
    LseekResponseData responseData_;
};

// ----------------------------------mkdir-----------------------------------------------
struct MkdirRequestData : PosixOpRequest {
    char path[200];
    mode_t mode;
};

struct MkdirResponseData : PosixOpResponse {
    int ret; // 返回值
};

class MkdirOpReqRes : public PosixOpReqRes {
public:
    MkdirOpReqRes()
        : PosixOpReqRes(PosixOpType::MKDIR) {}
    MkdirOpReqRes(const char *path, mode_t mode);
    MkdirOpReqRes(const long *args, long *result);

    ~MkdirOpReqRes() override;

    virtual void CopyRequestDataToBuf(void* buf);

    virtual int GetRequestSize() override;
    virtual int GetRequestAlignSize() override;
    virtual int GetResponseSize() override;
    virtual int GetResponseAlignSize() override;

    virtual PosixOpResponse& GetResponse() override;
    void SetResponse(void* response) override;
private:
    MkdirRequestData requestData_;
    MkdirResponseData responseData_;
};

// ----------------------------------opendir------------------------------------


struct OpendirRequestData : PosixOpRequest {
    char path[200];
};

struct OpendirResponseData : PosixOpResponse {
    int ret; // 返回值
    DIR* dir; // 上游保存dir的指针
    DirStream dirStream; // 保存server获取的结果
};

class OpendirOpReqRes : public PosixOpReqRes {
public:
    OpendirOpReqRes()
        : PosixOpReqRes(PosixOpType::OPENDIR) {}
    OpendirOpReqRes(const char *path);
    OpendirOpReqRes(const long *args, long *result);

    ~OpendirOpReqRes() override;

    virtual void CopyRequestDataToBuf(void* buf);

    virtual int GetRequestSize() override;
    virtual int GetRequestAlignSize() override;
    virtual int GetResponseSize() override;
    virtual int GetResponseAlignSize() override;

    virtual PosixOpResponse& GetResponse() override;
    void SetResponse(void* response) override;
private:
    OpendirRequestData requestData_;
    OpendirResponseData responseData_;
};

// ----------------------------------getdents------------------------

struct GetdentsRequestData : PosixOpRequest {
    DirStream dirinfo;
    size_t maxread;
};

struct GetdentsResponseData : PosixOpResponse {
    int ret; // 返回值
    DirStream dirinfo;
    ssize_t realbytes;
    char* data; // 上游数据指针
    char contents[0]; // 保存server获取的结果
};

class GetdentsOpReqRes : public PosixOpReqRes {
public:
    GetdentsOpReqRes()
        : PosixOpReqRes(PosixOpType::GETDENTS) {}
    GetdentsOpReqRes(DirStream dirinfo, char* data, size_t maxread);
    GetdentsOpReqRes(const long *args, long *result);

    ~GetdentsOpReqRes() override;

    virtual void CopyRequestDataToBuf(void* buf);

    virtual int GetRequestSize() override;
    virtual int GetRequestAlignSize() override;
    virtual int GetResponseSize() override;
    virtual int GetResponseAlignSize() override;

    virtual PosixOpResponse& GetResponse() override;
    void SetResponse(void* response) override;
private:
    GetdentsRequestData requestData_;
    GetdentsResponseData responseData_;
};

// ----------------------------------closedir------------------------------------
struct ClosedirRequestData : PosixOpRequest {
    DirStream dirstream;
};

struct ClosedirResponseData : PosixOpResponse {
    int ret; // 返回值
};

class ClosedirOpReqRes : public PosixOpReqRes {
public:
    ClosedirOpReqRes()
        : PosixOpReqRes(PosixOpType::CLOSEDIR) {}
    ClosedirOpReqRes(const DirStream& dirstream);
    ClosedirOpReqRes(const long *args, long *result);

    ~ClosedirOpReqRes() override;

    virtual void CopyRequestDataToBuf(void* buf);

    virtual int GetRequestSize() override;
    virtual int GetRequestAlignSize() override;
    virtual int GetResponseSize() override;
    virtual int GetResponseAlignSize() override;
    virtual PosixOpResponse& GetResponse() override;
    virtual void SetResponse(void* response) override;

private:
    ClosedirRequestData requestData_;
    ClosedirResponseData responseData_;
};

// -------------------------unlink-----------------------------------------
struct UnlinkRequestData : PosixOpRequest {
    char path[200];
};

struct UnlinkResponseData : PosixOpResponse {
    int ret; // 返回值
};

class UnlinkOpReqRes : public PosixOpReqRes {
public:
    UnlinkOpReqRes()
        : PosixOpReqRes(PosixOpType::UNLINK) {}
    UnlinkOpReqRes(const char *path);
    UnlinkOpReqRes(const long *args, long *result);
    ~UnlinkOpReqRes() override;

    virtual void CopyRequestDataToBuf(void* buf);

    virtual int GetRequestSize() override;
    virtual int GetRequestAlignSize() override;
    virtual int GetResponseSize() override;
    virtual int GetResponseAlignSize() override;

    virtual PosixOpResponse& GetResponse() override;
    void SetResponse(void* response) override;
private:
    UnlinkRequestData requestData_;
    UnlinkResponseData responseData_;
};

struct RenameRequestData : PosixOpRequest {
    char oldpath[200];
    char newpath[200];
};

struct RenameResponseData : PosixOpResponse {
    int ret; // 返回值
};

class RenameOpReqRes : public PosixOpReqRes {
public:
    RenameOpReqRes()
        : PosixOpReqRes(PosixOpType::RENAME) {}
    RenameOpReqRes(const char *oldpath, const char *newpath);
    RenameOpReqRes(const long *args, long *result);

    ~RenameOpReqRes() override;

    virtual void CopyRequestDataToBuf(void* buf);

    virtual int GetRequestSize() override;
    virtual int GetRequestAlignSize() override;
    virtual int GetResponseSize() override;
    virtual int GetResponseAlignSize() override;

    virtual PosixOpResponse& GetResponse() override;
    void SetResponse(void* response) override;
private:
    RenameRequestData requestData_;
    RenameResponseData responseData_;
};

// ----------------------truncate-----------------------------------------
class TruncateRequestData : public PosixOpRequest {
public:
    char path[200];
    off_t length;
};

class TruncateResponseData : public PosixOpResponse {
public:
    int ret; // 返回值
};

class TruncateOpReqRes : public PosixOpReqRes {
public:
    TruncateOpReqRes()
        : PosixOpReqRes(PosixOpType::TRUNCATE) {}
    TruncateOpReqRes(const char *path, off_t length);
    TruncateOpReqRes(const long *args, long *result);

    ~TruncateOpReqRes() override;

    virtual void CopyRequestDataToBuf(void* buf);

    virtual int GetRequestSize() override;
    virtual int GetRequestAlignSize() override;
    virtual int GetResponseSize() override;
    virtual int GetResponseAlignSize() override;

    virtual PosixOpResponse& GetResponse() override;
    void SetResponse(void* response) override;
private:
    TruncateRequestData requestData_;
    TruncateResponseData responseData_;
};

// ----------------------TERMINAL-------------------
class TerminalRequestData : public PosixOpRequest {
public:

};

class TerminalResponseData : public PosixOpResponse {
public:
    int ret; // 返回值
};

class TerminalOpReqRes : public PosixOpReqRes {
public:
    TerminalOpReqRes();
    ~TerminalOpReqRes() override {};

    virtual void CopyRequestDataToBuf(void* buf);

    virtual int GetRequestSize() override;
    virtual int GetRequestAlignSize() override;
    virtual int GetResponseSize() override ;
    virtual int GetResponseAlignSize() override;

    virtual PosixOpResponse& GetResponse() override;
    void SetResponse(void* response) override;;
private:
    TerminalRequestData requestData_;
    TerminalResponseData responseData_;
};

} // namespace internal
} // namespace intercept

