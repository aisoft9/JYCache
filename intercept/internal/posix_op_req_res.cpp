#include <sys/stat.h>
#include <sys/mman.h>
#include <sched.h>
#include <cstring>
#include <iostream>
// #include <immintrin.h>

#include "posix_op_req_res.h"

namespace intercept {
namespace internal {
std::string TypeToStr(PosixOpType opType) {
    switch(opType) {
        case PosixOpType::OPEN: return "OPEN";
        case PosixOpType::WRITE: return "WRITE";
        case PosixOpType::READ: return "READ";
        case PosixOpType::ACCESS: return "ACCESS";
        case PosixOpType::CLOSE: return "CLOSE";
        case PosixOpType::FSYNC: return "FSYNC";
        case PosixOpType::TRUNCATE: return "TRUNCATE";
        case PosixOpType::FTRUNCATE: return "FTRUNCATE";
        case PosixOpType::FUTIMES: return "FUTIMES";
        case PosixOpType::LSEEK: return "LSEEK";
        case PosixOpType::MKDIR: return "MKDIR";
        case PosixOpType::MKNOD: return "MKNOD";
        case PosixOpType::OPENDIR: return "OPENDIR";
        case PosixOpType::READDIR: return "READDIR";
        case PosixOpType::GETDENTS: return "GETDENTS";
        case PosixOpType::CLOSEDIR: return "CLOSEDIR";
        case PosixOpType::RENAME: return "RENAME";
        case PosixOpType::STAT: return "STAT";
        case PosixOpType::FSTAT: return "FSTAT";
        case PosixOpType::UNLINK: return "UNLINK";
        case PosixOpType::UTIMES: return "UTIMES";
        case PosixOpType::TERMINAL: return "TERMINAL";
        // ... 其他操作类型
        default: return "UNKNOWN";
    }
}


// PosixOpReqRes 类的实现
PosixOpReqRes::PosixOpReqRes(PosixOpType opType) : opType_(opType) {}

PosixOpReqRes::PosixOpReqRes(const long* args, long* result) {}

void PosixOpReqRes::SetOpType(PosixOpType type) { opType_ = type; }

PosixOpType PosixOpReqRes::GetOpType() const { return opType_; }


// ------------------------------open---------------------------
OpenOpReqRes::OpenOpReqRes(const char* path, int flags, mode_t mode)
    : PosixOpReqRes(PosixOpType::OPEN) {
    strcpy(requestData_.path, path);
    requestData_.flags = flags;
    requestData_.mode = mode;
    requestData_.opType = opType_;
}

OpenOpReqRes::OpenOpReqRes(const long *args, long *result) : PosixOpReqRes(PosixOpType::OPEN) {
    strcpy(requestData_.path, reinterpret_cast<const char*>(args[0]));
    requestData_.flags = static_cast<int>(args[1]);
    requestData_.mode = static_cast<mode_t>(args[2]);
    requestData_.opType = opType_;
}

OpenOpReqRes::~OpenOpReqRes() {

}

void OpenOpReqRes::CopyRequestDataToBuf(void* buf) {
    // 将请求数据复制到缓冲区
    memcpy(buf, &requestData_.opType, sizeof(requestData_.opType));
    
    memcpy(buf + sizeof(requestData_.opType), requestData_.path,  sizeof(requestData_.path));

    memcpy(buf + sizeof(requestData_.opType) + sizeof(requestData_.path), &requestData_.flags, sizeof(requestData_.flags));

    memcpy(buf + sizeof(requestData_.opType) + sizeof(requestData_.path) + sizeof(requestData_.flags), &requestData_.mode, sizeof(requestData_.mode));
    return;
}

int OpenOpReqRes::GetRequestSize() {
    return sizeof(OpenRequestData);
}

int OpenOpReqRes::GetRequestAlignSize() {
    return alignof(OpenRequestData);
}

int OpenOpReqRes::GetResponseSize() {
    return sizeof(OpenResponseData);
}

int OpenOpReqRes::GetResponseAlignSize() {
    return alignof(OpenResponseData);
}

// 将response转化为Response
PosixOpResponse& OpenOpReqRes::GetResponse() {
    return responseData_;
}

void OpenOpReqRes::SetResponse(void* response) {
    OpenResponseData
        *responseData = reinterpret_cast<OpenResponseData*>(response);
    responseData_.opType = responseData->opType;
    responseData_.fd = responseData->fd;
    spdlog::info("OpenOpReqRes::SetResponse: fd ={}",responseData_.fd);
}


// ------------------------------read----------------------------
ReadOpReqRes::ReadOpReqRes(int fd, void* buf, size_t count)
    : PosixOpReqRes(PosixOpType::READ) {
    // for request
    requestData_.fd = fd;
    requestData_.count = count;
    requestData_.opType = opType_;
    // for response
    responseData_.buf = buf;
}

ReadOpReqRes::ReadOpReqRes(const long *args, long *result): PosixOpReqRes(PosixOpType::READ) {
    requestData_.opType = opType_;
    // for reqeust
    requestData_.fd = static_cast<int>(args[0]);
    requestData_.count = static_cast<size_t>(args[2]);
    // for response
    responseData_.buf = reinterpret_cast<void*>(args[1]);
}

ReadOpReqRes::~ReadOpReqRes() {
    // 析构函数
}

void ReadOpReqRes::CopyRequestDataToBuf(void* buf) {
    memcpy(buf, &requestData_, sizeof(requestData_));
}

int ReadOpReqRes::GetRequestSize() {
    return sizeof(requestData_);
}

int ReadOpReqRes::GetRequestAlignSize() {
    return alignof(ReadRequestData);
}

int ReadOpReqRes::GetResponseSize() {
    // 响应数据大小 结构体大小+需要的长度
    return sizeof(responseData_) + requestData_.count;
}

int ReadOpReqRes::GetResponseAlignSize() {
    return alignof(ReadResponseData);
}

PosixOpResponse& ReadOpReqRes::GetResponse() {
    return responseData_;
}

void ReadOpReqRes::SetResponse(void* response) {
    ReadResponseData* responseData = static_cast<ReadResponseData*>(response);
    responseData_.opType = responseData->opType;
    responseData_.ret = responseData->ret;
    responseData_.length = responseData->length;
    if (intercept::common::Configure::getInstance().getConfig("multiop") == "true"
        && responseData_.length >= atol(intercept::common::Configure::getInstance().getConfig("blocksize").c_str())) {
        SetResponseMultithreads(response);
    } else {
        if (responseData_.length > 0 && responseData_.buf != nullptr) {
            intercept::common::Timer timer("client ReadOpReqRes::SetResponse time ");
            memcpy(responseData_.buf, responseData->content, responseData->length);
            //std::cout << "the read response, the length: " << responseData->length << " , the buf: " << (char*)responseData_.buf << std::endl;
        } else {
            spdlog::debug("the length: {}, the buf maybe nullptr", responseData_.length);
        }
    }
    
}

void initialize_memory(char* ptr, size_t size) {
    // 通过访问每个页面确保内存已分配
    for (size_t i = 0; i < size; i += sysconf(_SC_PAGESIZE)) {
        ptr[i] = 0;
    }
    ptr[size - 1] = 0; // 访问最后一个字节确保全部内存已分配
}
void ReadOpReqRes::SetResponseMultithreads(void* response) {
    ReadResponseData* responseData = static_cast<ReadResponseData*>(response);
    responseData_.opType = responseData->opType;
    responseData_.ret = responseData->ret;
    responseData_.length = responseData->length;

    if (responseData_.length > 0 && responseData_.buf != nullptr) {
        intercept::common::Timer timer("client ReadOpReqRes::SetResponseMultithreads time ");
        // Determine the number of threads to use (for example, 4)
        int numThreads = intercept::common::Configure::getInstance().getConfig("opThreadnum") == ""  ?
        1 : atoi(intercept::common::Configure::getInstance().getConfig("opThreadnum").c_str());
        size_t chunkSize = responseData_.length / numThreads;
        size_t remainder = responseData_.length % numThreads;
        auto copyChunk = [](char* dest, const char* src, size_t len) {
            
            // initialize_memory(dest, len);
            // mlock(dest, len);
            // memmove(dest, src, len);
            memcpy(dest, src, len);
            
            // size_t i = 0;
            // // 处理前面未对齐的部分
            // while (i < len && reinterpret_cast<uintptr_t>(dest + i) % 32 != 0) {
            //     dest[i] = src[i];
            //     i++;
            // }
            
            // // 处理对齐的中间部分
            // for (; i + 31 < len; i += 32) {
            //     __m256i data = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i));
            //     _mm256_storeu_si256(reinterpret_cast<__m256i*>(dest + i), data);
            // }
            
            // // 处理末尾未对齐的部分
            // for (; i < len; ++i) {
            //     dest[i] = src[i];
            // }
            // munlock(dest, len);
        };

        std::vector<std::thread> threads;
        std::vector<int> numaNode1Cores = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 43, 45, 47, 49, 51, 53, 55, 57, 59, 61, 63, 65, 67, 69, 71, 73, 75, 77, 79, 81, 83, 85, 87, 89, 91, 93, 95};

        for (int i = 0; i < numThreads; ++i) {
            size_t startIdx = i * chunkSize;
            size_t len = (i == numThreads - 1) ? (chunkSize + remainder) : chunkSize;

            // threads.emplace_back([&, startIdx, len, i]() {
            // cpu_set_t cpuset;
            // CPU_ZERO(&cpuset);
            // CPU_SET(numaNode1Cores[i % numaNode1Cores.size()], &cpuset);
            // sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

            // copyChunk(static_cast<char*>(responseData_.buf) + startIdx, responseData->content + startIdx, len);
            // });
            threads.emplace_back(copyChunk, static_cast<char*>(responseData_.buf) + startIdx, responseData->content + startIdx, len);
        }
        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }
        spdlog::debug("the read response, the length: {}" ,responseData_.length);
    } else {
        spdlog::debug("the length: {}, the buf maybe nullptr", responseData_.length);
    }
}

// void ReadOpReqRes::SetResponse(void* response) {
//     ReadResponseData* responseData = static_cast<ReadResponseData*>(response);
//     responseData_.opType = responseData->opType;
//     responseData_.ret = responseData->ret;
//     responseData_.length = responseData->length;

//     if (responseData_.length > 0 && responseData_.buf != nullptr) {
        
//         int numThreads = intercept::common::Configure::getInstance().getConfig("opThreadnum").empty() ?
//             1 : std::stoi(intercept::common::Configure::getInstance().getConfig("opThreadnum"));
//         size_t chunkSize = responseData_.length / numThreads;
//         size_t remainder = responseData_.length % numThreads;
        
//         if (intercept::common::Configure::getInstance().getConfig("multiop") == "true")  {
//             intercept::common::Timer timer("client ReadOpReqRes::SetResponse time ");
//             auto copyChunk = [](char* dest, const char* src, size_t len) {
//             memcpy(dest, src, len);
//             };
//             std::atomic<int> tasksRemaining(numThreads);
//             auto tasksMutex = std::make_shared<std::mutex>();
//             auto tasksCondition = std::make_shared<std::condition_variable>();

//             for (int i = 0; i < numThreads; ++i) {
//                 size_t startIdx = i * chunkSize;
//                 size_t len = (i == numThreads - 1) ? (chunkSize + remainder) : chunkSize;
//                 threadPool_.enqueue([=, &tasksRemaining, tasksMutex, tasksCondition]() {
//                     copyChunk(static_cast<char*>(responseData_.buf) + startIdx, responseData->content + startIdx, len);
//                     if (--tasksRemaining == 0) {
//                         std::unique_lock<std::mutex> lock(*tasksMutex);
//                         tasksCondition->notify_all();
//                     }
//                 });
//             }

//             {
//                 std::unique_lock<std::mutex> lock(*tasksMutex);
//                 tasksCondition->wait(lock, [&tasksRemaining] { return tasksRemaining.load() == 0; });
//             }

//         } else {
//             memcpy(responseData_.buf, responseData->content, responseData->length);
//         }
       
//         spdlog::debug("The read response, length: {}", responseData_.length);
//     } else {
//         spdlog::debug("The length: {}, the buffer may be nullptr", responseData_.length);
//     }
// }


// -----------------------------write-------------------------
WriteOpReqRes::WriteOpReqRes(int fd, void* buf, size_t count) 
            : PosixOpReqRes(PosixOpType::WRITE) {
    requestData_.opType = opType_;
    requestData_.fd = fd;
    requestData_.buf = buf;
    requestData_.count = count;
}

WriteOpReqRes::WriteOpReqRes(const long *args, long *result) 
            : PosixOpReqRes(PosixOpType::WRITE) {
    // 从参数中初始化
    requestData_.opType = opType_;
    requestData_.fd = static_cast<int>(args[0]);
    requestData_.buf = reinterpret_cast<void*>(args[1]);
    requestData_.count = static_cast<size_t>(args[2]);
}

WriteOpReqRes::~WriteOpReqRes() {
    // 析构函数
}

void WriteOpReqRes::CopyRequestDataToBuf(void* buf) {
    // 元信息
    memcpy(buf, &requestData_, sizeof(requestData_));
    // 数据
    if (intercept::common::Configure::getInstance().getConfig("multiop") == "true" &&
       requestData_.count >= atoi(intercept::common::Configure::getInstance().getConfig("blocksize").c_str()) ) {
        int numThreads = intercept::common::Configure::getInstance().getConfig("opThreadnum") == ""  ?
        1 : atoi(intercept::common::Configure::getInstance().getConfig("opThreadnum").c_str());
        CopyRequestDataToBufMultithread((char*)buf + sizeof(requestData_), requestData_.buf, requestData_.count, numThreads);
    } else {
        memcpy((char*)buf + sizeof(requestData_), requestData_.buf, requestData_.count);
    }
}

void WriteOpReqRes::CopyRequestDataToBufMultithread(void* dest, const void* src, size_t count, int numThreads) {
    size_t chunkSize = count / numThreads;
    size_t remainder = count % numThreads;
    intercept::common::Timer timer("client WriteOpReqRes::CopyRequestDataToBufMultithread time:");
    auto copyChunk = [](char* dest, const char* src, size_t len) {
            memcpy(dest, src, len);
    };
    spdlog::info("copy request with multithread for writing, chunksize: {}, remainder: {}", chunkSize, remainder);
    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        size_t startIdx = i * chunkSize;
        size_t len = (i == numThreads - 1) ? (chunkSize + remainder) : chunkSize;
        threads.emplace_back(copyChunk, static_cast<char*>(dest + startIdx), static_cast<const char*>(src + startIdx), len);
    }

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

int WriteOpReqRes::GetRequestSize() {
    return sizeof(requestData_) + requestData_.count;
}

int WriteOpReqRes::GetRequestAlignSize() {
    return alignof(WriteRequestData);
}

int WriteOpReqRes::GetResponseSize() {
    return sizeof(WriteResponseData);
}

int WriteOpReqRes::GetResponseAlignSize() {
    return alignof(WriteResponseData);
}

PosixOpResponse& WriteOpReqRes::GetResponse() {
    return responseData_;
}

void WriteOpReqRes::SetResponse(void* response) {
    WriteResponseData* responseData = static_cast<WriteResponseData*>(response);
    responseData_.opType = responseData->opType;
    responseData_.ret = responseData->ret;
    responseData_.length = responseData->length;
    // std::cout << "write response, optype: " << (int)responseData_.opType << " , ret: " << responseData_.ret << " , length: " << responseData_.length << std::endl; 
}

// -----------------------close-------------------------------
CloseOpReqRes::CloseOpReqRes(int fd) : PosixOpReqRes(PosixOpType::CLOSE) {
    requestData_.opType = PosixOpType::CLOSE;
    requestData_.fd = fd;
}

CloseOpReqRes::CloseOpReqRes(const long* args, long* result) : PosixOpReqRes(PosixOpType::CLOSE) {
    requestData_.opType = PosixOpType::CLOSE;
    requestData_.fd = static_cast<int>(args[0]);
}

CloseOpReqRes::~CloseOpReqRes() {
    // 析构函数
}

void CloseOpReqRes::CopyRequestDataToBuf(void* buf) {
    memcpy(buf, &requestData_, sizeof(requestData_));
}

int CloseOpReqRes::GetRequestSize() {
    return sizeof(requestData_);
}

int CloseOpReqRes::GetRequestAlignSize() {
    return alignof(CloseRequestData);
}

int CloseOpReqRes::GetResponseSize() {
    return sizeof(CloseResponseData);
}

int CloseOpReqRes::GetResponseAlignSize() {
    return alignof(CloseResponseData);
}

PosixOpResponse& CloseOpReqRes::GetResponse() {
    return responseData_;
}

void CloseOpReqRes::SetResponse(void* response) {
    CloseResponseData* responseData = static_cast<CloseResponseData*>(response);
    responseData_.opType = responseData->opType;
    responseData_.ret = responseData->ret;
}

// ------------------------fysnc-------------------------------------
FsyncOpReqRes::FsyncOpReqRes(int fd) {
    requestData_.opType = PosixOpType::FSYNC;
    requestData_.fd = fd;
}

FsyncOpReqRes::FsyncOpReqRes(const long* args, long* result) {
    requestData_.opType = PosixOpType::FSYNC;
    requestData_.fd = static_cast<int>(args[0]);
}

FsyncOpReqRes::~FsyncOpReqRes() {
    
}
void FsyncOpReqRes::CopyRequestDataToBuf(void* buf) {
    memcpy(buf, &requestData_, sizeof(requestData_));
}

int FsyncOpReqRes::GetRequestSize() {
    return sizeof(requestData_);
}

int FsyncOpReqRes::GetRequestAlignSize() {
    return alignof(FsyncRequestData);
}

int FsyncOpReqRes::GetResponseSize() {
    return sizeof(FsyncResponseData);
}

int FsyncOpReqRes::GetResponseAlignSize() {
    return alignof(FsyncResponseData);
}

PosixOpResponse& FsyncOpReqRes::GetResponse() {
    return responseData_;
}

void FsyncOpReqRes::SetResponse(void* response) {
    FsyncResponseData* responseData = static_cast<FsyncResponseData*>(response);
    responseData_.opType = responseData->opType;
    responseData_.ret = responseData->ret;
}

// -------------------stat---------------------------
StatOpReqRes::StatOpReqRes(const char* path, struct stat* st) 
    : PosixOpReqRes(PosixOpType::STAT){
    requestData_.opType = opType_;
    strncpy(requestData_.path, path, strlen(path));
    requestData_.path[strlen(path)] = '\0';
    responseData_.st = st;
    spdlog::debug("StatOpReqRes, the type: {}, the path: {}", TypeToStr(requestData_.opType), requestData_.path);
}

StatOpReqRes::StatOpReqRes(const long* args, long* result) 
    : PosixOpReqRes(PosixOpType::STAT){
    requestData_.opType = opType_;
    strncpy(requestData_.path, (const char*)args[1], strlen((const char*)args[1]));
    requestData_.path[strlen((const char*)args[1])] = '\0';
    responseData_.st = (struct stat*)(args[1]);
    spdlog::debug("StatOpReqRes, the type: {}, the path: {}", TypeToStr(requestData_.opType), requestData_.path ); 
}
StatOpReqRes::~StatOpReqRes() {
}

void StatOpReqRes::CopyRequestDataToBuf(void* buf) {
    memcpy(buf, &requestData_, sizeof(requestData_));
}

int StatOpReqRes::GetRequestSize() {
    return sizeof(requestData_);
}

int StatOpReqRes::GetRequestAlignSize() {
    return alignof(StatRequestData);
}

int StatOpReqRes::GetResponseSize() {
    return sizeof(StatResponseData);
}

int StatOpReqRes::GetResponseAlignSize() {
    return alignof(StatResponseData);
}

PosixOpResponse& StatOpReqRes::GetResponse() {
    return responseData_;
}

void StatOpReqRes::SetResponse(void* response) {
    StatResponseData* responseData = static_cast<StatResponseData*>(response);
    responseData_.opType = responseData->opType;
    responseData_.ret = responseData->ret;
    memcpy(responseData_.st, &responseData->fileStat, sizeof(struct stat));
}

// --------------------------------fstat--------------------------------------

FstatOpReqRes::FstatOpReqRes(int fd, struct stat* st) 
    : PosixOpReqRes(PosixOpType::FSTAT){
    requestData_.opType = opType_;
    requestData_.fd = fd;
    responseData_.st = st;
    spdlog::debug("FstatOpReqRes, the type: {}, the fd: {}", TypeToStr(requestData_.opType), requestData_.fd );

}

FstatOpReqRes::FstatOpReqRes(const long* args, long* result) 
    : PosixOpReqRes(PosixOpType::FSTAT){
    requestData_.opType = opType_;
    requestData_.fd = (int)args[0];
    responseData_.st = (struct stat*)(args[1]);
    spdlog::debug("FstatOpReqRes, the type: {}, the fd: {}", TypeToStr(requestData_.opType), requestData_.fd);

}
FstatOpReqRes::~FstatOpReqRes() {
}

void FstatOpReqRes::CopyRequestDataToBuf(void* buf) {
    memcpy(buf, &requestData_, sizeof(requestData_));
}

int FstatOpReqRes::GetRequestSize() {
    return sizeof(requestData_);
}

int FstatOpReqRes::GetRequestAlignSize() {
    return alignof(FstatRequestData);
}

int FstatOpReqRes::GetResponseSize() {
    return sizeof(FstatResponseData);
}

int FstatOpReqRes::GetResponseAlignSize() {
    return alignof(FstatResponseData);
}

PosixOpResponse& FstatOpReqRes::GetResponse() {
    return responseData_;
}

void FstatOpReqRes::SetResponse(void* response) {
    FstatResponseData* responseData = static_cast<FstatResponseData*>(response);
    responseData_.opType = responseData->opType;
    responseData_.ret = responseData->ret;
    memcpy(responseData_.st, &responseData->fileStat, sizeof(struct stat));
}

// --------------------------------lseek---------------------------------------
LseekOpReqRes::LseekOpReqRes(int fd, uint64_t offset, int whence) 
    : PosixOpReqRes(PosixOpType::LSEEK){
    requestData_.opType = opType_;
    requestData_.fd = fd;
    requestData_.offset = offset;
    requestData_.whence = whence;
}

LseekOpReqRes::LseekOpReqRes(const long* args, long* result) 
    : PosixOpReqRes(PosixOpType::LSEEK){
    requestData_.opType = opType_;
    requestData_.fd = (int)args[0];
    requestData_.offset = (off_t)args[1];
    requestData_.whence = (int)args[2];
}

LseekOpReqRes::~LseekOpReqRes() {
}

void LseekOpReqRes::CopyRequestDataToBuf(void* buf) {
    memcpy(buf, &requestData_, sizeof(requestData_));
}

int LseekOpReqRes::GetRequestSize() {
    return sizeof(requestData_);
}

int LseekOpReqRes::GetRequestAlignSize() {
    return alignof(LseekRequestData);
}

int LseekOpReqRes::GetResponseSize() {
    return sizeof(LseekResponseData);
}

int LseekOpReqRes::GetResponseAlignSize() {
    return alignof(LseekResponseData);
}

PosixOpResponse& LseekOpReqRes::GetResponse() {
    return responseData_;
}

void LseekOpReqRes::SetResponse(void* response) {
    LseekResponseData* responseData = static_cast<LseekResponseData*>(response);
    responseData_.opType = responseData->opType;
    responseData_.ret = responseData->ret;
}

// -------------------------------mkdir----------------------------------------
MkdirOpReqRes::MkdirOpReqRes(const char* path, mode_t mode) 
    : PosixOpReqRes(PosixOpType::MKDIR){
    requestData_.opType = opType_;
    strncpy(requestData_.path, path, strlen(path));
    requestData_.path[strlen(path)] = '\0';
    requestData_.mode = mode;
}

MkdirOpReqRes::MkdirOpReqRes(const long* args, long* result) 
    : PosixOpReqRes(PosixOpType::MKDIR){
    requestData_.opType = opType_;
    strncpy(requestData_.path, (const char*)args[0], strlen((const char*)args[0]));
    requestData_.path[strlen((const char*)args[0])] = '\0';
    requestData_.mode = (mode_t)args[1];
}

MkdirOpReqRes::~MkdirOpReqRes() {
}

void MkdirOpReqRes::CopyRequestDataToBuf(void* buf) {
    memcpy(buf, &requestData_, sizeof(requestData_));
}

int MkdirOpReqRes::GetRequestSize() {
    return sizeof(requestData_);
}

int MkdirOpReqRes::GetRequestAlignSize() {
    return alignof(MkdirRequestData);
}

int MkdirOpReqRes::GetResponseSize() {
    return sizeof(MkdirResponseData);
}

int MkdirOpReqRes::GetResponseAlignSize() {
    return alignof(MkdirResponseData);
}

PosixOpResponse& MkdirOpReqRes::GetResponse() {
    return responseData_;
}
void MkdirOpReqRes::SetResponse(void* response) {
    MkdirResponseData* responseData = static_cast<MkdirResponseData*>(response);
    responseData_.opType = responseData->opType;
    responseData_.ret = responseData->ret;
}

// -------------------------------opendir---------------------------------------
OpendirOpReqRes::OpendirOpReqRes(const char* path) 
    : PosixOpReqRes(PosixOpType::OPENDIR){
    requestData_.opType = opType_;
    strncpy(requestData_.path, path, strlen(path));
    requestData_.path[strlen(path)] = '\0';
}

OpendirOpReqRes::OpendirOpReqRes(const long* args, long* result) {
    requestData_.opType = opType_;
    strncpy(requestData_.path, (const char*)args[0], strlen((const char*)args[0]));
    requestData_.path[strlen((const char*)args[0])] = '\0';
}

OpendirOpReqRes::~OpendirOpReqRes() {
}

void OpendirOpReqRes::CopyRequestDataToBuf(void* buf) {
    memcpy(buf, &requestData_, sizeof(requestData_));
}

int OpendirOpReqRes::GetRequestSize() {
    return sizeof(requestData_);
}

int OpendirOpReqRes::GetRequestAlignSize() {
    return alignof(OpendirRequestData);
}

int OpendirOpReqRes::GetResponseSize() {
    return sizeof(OpendirResponseData);
}

int OpendirOpReqRes::GetResponseAlignSize() {
    return alignof(OpendirResponseData);
}

PosixOpResponse& OpendirOpReqRes::GetResponse() {
    return responseData_;
}

void OpendirOpReqRes::SetResponse(void* response) {
    OpendirResponseData* responseData = static_cast<OpendirResponseData*>(response);
    responseData_.opType = responseData->opType;
    responseData_.ret = responseData->ret;
    responseData_.dirStream = responseData->dirStream;
}

//------------------------------getdents---------------------------
GetdentsOpReqRes::GetdentsOpReqRes(DirStream dirinfo, char* data, size_t maxread)
    : PosixOpReqRes(PosixOpType::GETDENTS){
    requestData_.opType = opType_;
    requestData_.dirinfo = dirinfo;
    requestData_.maxread = maxread;

    responseData_.data = data;
}

GetdentsOpReqRes::GetdentsOpReqRes(const long* args, long* result) {
    requestData_.opType = opType_;
    // TODO
}

GetdentsOpReqRes::~GetdentsOpReqRes() {

}

void GetdentsOpReqRes::CopyRequestDataToBuf(void* buf) {
    memcpy(buf, &requestData_, sizeof(requestData_));
}

int GetdentsOpReqRes::GetRequestSize() {
    return sizeof(requestData_);
}

int GetdentsOpReqRes::GetRequestAlignSize() {
    return alignof(GetdentsRequestData);
}

int GetdentsOpReqRes::GetResponseSize() {
    return sizeof(GetdentsResponseData);
}

int GetdentsOpReqRes::GetResponseAlignSize() {
    return alignof(GetdentsResponseData);
}

PosixOpResponse& GetdentsOpReqRes::GetResponse() {
    return responseData_;
}

void GetdentsOpReqRes::SetResponse(void* response) {
    GetdentsResponseData* responseData = static_cast<GetdentsResponseData*>(response);
    responseData_.opType = responseData->opType;
    responseData_.dirinfo = responseData->dirinfo;
    responseData_.realbytes = responseData->realbytes;;
    responseData_.ret = responseData->ret;
    memcpy(responseData_.data, responseData->contents, responseData->realbytes);
}

// ------------------------------closedir-------------------------------
ClosedirOpReqRes::ClosedirOpReqRes(const DirStream& dirstream) 
    : PosixOpReqRes(PosixOpType::CLOSEDIR){
    requestData_.opType = opType_;
    requestData_.dirstream = dirstream;
}

ClosedirOpReqRes::ClosedirOpReqRes(const long* args, long* result) {
    requestData_.opType = opType_;
    // TODO
}

ClosedirOpReqRes::~ClosedirOpReqRes() {
}

void ClosedirOpReqRes::CopyRequestDataToBuf(void* buf) {
    memcpy(buf, &requestData_, sizeof(requestData_));
}

int ClosedirOpReqRes::GetRequestSize() {
    return sizeof(requestData_);
}

int ClosedirOpReqRes::GetRequestAlignSize() {
    return alignof(ClosedirRequestData);
}

int ClosedirOpReqRes::GetResponseSize() {
    return sizeof(ClosedirResponseData);
}

int ClosedirOpReqRes::GetResponseAlignSize() {
    return alignof(ClosedirResponseData);
}

PosixOpResponse& ClosedirOpReqRes::GetResponse() {
    return responseData_;
}

void ClosedirOpReqRes::SetResponse(void* response) {
    ClosedirResponseData* responseData = static_cast<ClosedirResponseData*>(response);
    responseData_.opType = responseData->opType;
    responseData_.ret = responseData->ret;
}

// -------------------------------unlink-------------------------------
UnlinkOpReqRes::UnlinkOpReqRes(const char* path) 
    : PosixOpReqRes(PosixOpType::UNLINK){
    requestData_.opType = opType_;
    strncpy(requestData_.path, path, strlen(path));
    requestData_.path[strlen(path)] = '\0';
}

UnlinkOpReqRes::UnlinkOpReqRes(const long* args, long* result) {
    requestData_.opType = opType_;
    strncpy(requestData_.path, (const char*)args[0], strlen((const char*)args[0]));
    requestData_.path[strlen((const char*)args[0])] = '\0';
}

UnlinkOpReqRes::~UnlinkOpReqRes() {
}

void UnlinkOpReqRes::CopyRequestDataToBuf(void* buf) {
    memcpy(buf, &requestData_, sizeof(requestData_));
}

int UnlinkOpReqRes::GetRequestSize() {
    return sizeof(requestData_);
}

int UnlinkOpReqRes::GetRequestAlignSize() {
    return alignof(UnlinkRequestData);
}

int UnlinkOpReqRes::GetResponseSize() {
    return sizeof(UnlinkResponseData);
}

int UnlinkOpReqRes::GetResponseAlignSize() {
    return alignof(UnlinkResponseData);
}

PosixOpResponse& UnlinkOpReqRes::GetResponse() {
    return responseData_;
}
void UnlinkOpReqRes::SetResponse(void* response) {
    UnlinkResponseData* responseData = static_cast<UnlinkResponseData*>(response);
    responseData_.opType = responseData->opType;
    responseData_.ret = responseData->ret;
}

// ------------------------------rename-------------------------------------
RenameOpReqRes::RenameOpReqRes(const char* oldpath, const char* newpath) 
    : PosixOpReqRes(PosixOpType::RENAME){
    requestData_.opType = opType_;
    strncpy(requestData_.oldpath, oldpath, strlen(oldpath));
    requestData_.oldpath[strlen(oldpath)] = '\0';
    strncpy(requestData_.newpath, newpath, strlen(newpath));
    requestData_.newpath[strlen(newpath)] = '\0';
}

RenameOpReqRes::RenameOpReqRes(const long* args, long* result) {
    requestData_.opType = opType_;
    strncpy(requestData_.oldpath, (const char*)args[0], strlen((const char*)args[0]));
    requestData_.oldpath[strlen((const char*)args[0])] = '\0';
    strncpy(requestData_.newpath, (const char*)args[1], strlen((const char*)args[1]));
    requestData_.newpath[strlen((const char*)args[1])] = '\0';
}

RenameOpReqRes::~RenameOpReqRes() {
}

void RenameOpReqRes::CopyRequestDataToBuf(void* buf) {
    memcpy(buf, &requestData_, sizeof(requestData_));
}

int RenameOpReqRes::GetRequestSize() {
    return sizeof(requestData_);
}

int RenameOpReqRes::GetRequestAlignSize() {
    return alignof(RenameRequestData);
}

int RenameOpReqRes::GetResponseSize() {
    return sizeof(RenameResponseData);
}

int RenameOpReqRes::GetResponseAlignSize() {
    return alignof(RenameResponseData);
}

PosixOpResponse& RenameOpReqRes::GetResponse() {
    return responseData_;
}
void RenameOpReqRes::SetResponse(void* response) {
    RenameResponseData* responseData = static_cast<RenameResponseData*>(response);
    responseData_.ret = responseData->ret;
    responseData_.opType = responseData->opType;
}

// -------------------------truncate---------------------------------
TruncateOpReqRes::TruncateOpReqRes(const char* path, off_t length) 
    : PosixOpReqRes(PosixOpType::TRUNCATE){
    requestData_.opType = opType_;
    strncpy(requestData_.path, path, strlen(path));
    requestData_.path[strlen(path)] = '\0';
    requestData_.length = length;
}

TruncateOpReqRes::TruncateOpReqRes(const long* args, long* result) {
    requestData_.opType = opType_;
    strncpy(requestData_.path, (const char*)args[0], strlen((const char*)args[0]));
    requestData_.path[strlen((const char*)args[0])] = '\0';
    requestData_.length = (off_t)args[1];
}

TruncateOpReqRes::~TruncateOpReqRes() {
}

void TruncateOpReqRes::CopyRequestDataToBuf(void* buf) {
    memcpy(buf, &requestData_, sizeof(requestData_));
}

int TruncateOpReqRes::GetRequestSize() {
    return sizeof(requestData_);
}

int TruncateOpReqRes::GetRequestAlignSize() {
    return alignof(TruncateRequestData);
}

int TruncateOpReqRes::GetResponseSize() {
    return sizeof(TruncateResponseData);
}

int TruncateOpReqRes::GetResponseAlignSize() {
    return alignof(TruncateResponseData);
}

PosixOpResponse& TruncateOpReqRes::GetResponse() {
    return responseData_;
}

void TruncateOpReqRes::SetResponse(void* response) {
    TruncateResponseData* responseData = static_cast<TruncateResponseData*>(response);
    responseData_.opType = responseData->opType;
    responseData_.ret = responseData->ret;
}

// -------------------------terminal---------------------------------
TerminalOpReqRes::TerminalOpReqRes() 
    : PosixOpReqRes(PosixOpType::TERMINAL){
    requestData_.opType = opType_;
}

void TerminalOpReqRes::CopyRequestDataToBuf(void* buf) {
    memcpy(buf, &requestData_, sizeof(requestData_));
}

int TerminalOpReqRes::GetRequestSize() {
    return sizeof(requestData_);
}

int TerminalOpReqRes::GetRequestAlignSize() {
    return alignof(TerminalRequestData);
}

int TerminalOpReqRes::GetResponseSize() {
    return sizeof(TerminalResponseData);
}

int TerminalOpReqRes::GetResponseAlignSize() {
    return alignof(TerminalResponseData);
}

PosixOpResponse& TerminalOpReqRes::GetResponse() {
    return responseData_;
}

void TerminalOpReqRes::SetResponse(void* response) {
    TerminalResponseData* responseData = static_cast<TerminalResponseData*>(response);
    responseData_.opType = responseData->opType;
    responseData_.ret = responseData->ret;
}

} // namespace internal
} // namespace intercept
