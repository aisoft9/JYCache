#include "filesystem/abstract_filesystem.h"
#ifndef CLIENT_BUILD
#include "filesystem/curve_filesystem.h"
#endif
#include "iox/signal_watcher.hpp"
#include "iceoryx_wrapper.h"

#include "iceoryx_posh/mepoo/chunk_header.hpp"

namespace intercept {
namespace middleware {

using intercept::internal::PosixOpReqRes;
using intercept::internal::PosixOpRequest;
using intercept::internal::PosixOpResponse;
using intercept::internal::PosixOpType;

using intercept::internal::OpenRequestData;
using intercept::internal::OpenResponseData;
using intercept::internal::ReadRequestData;
using intercept::internal::ReadResponseData;
using intercept::internal::WriteRequestData;
using intercept::internal::WriteResponseData;
using intercept::internal::CloseRequestData;
using intercept::internal::CloseResponseData;
using intercept::internal::StatRequestData;
using intercept::internal::StatResponseData;
using intercept::internal::FstatRequestData;
using intercept::internal::FstatResponseData;
using intercept::internal::FsyncRequestData;
using intercept::internal::FsyncResponseData;
using intercept::internal::LseekRequestData;
using intercept::internal::LseekResponseData;
using intercept::internal::MkdirRequestData;
using intercept::internal::MkdirResponseData;
using intercept::internal::OpendirRequestData;
using intercept::internal::OpendirResponseData;
using intercept::internal::GetdentsRequestData;
using intercept::internal::GetdentsResponseData;
using intercept::internal::ClosedirRequestData;
using intercept::internal::ClosedirResponseData;
using intercept::internal::UnlinkRequestData;
using intercept::internal::UnlinkResponseData;
using intercept::internal::RenameRequestData;
using intercept::internal::RenameResponseData;
using intercept::internal::TruncateRequestData;
using intercept::internal::TruncateResponseData;
using intercept::internal::TerminalRequestData;
using intercept::internal::TerminalResponseData;

std::shared_ptr<intercept::filesystem::AbstractFileSystem> ReqResMiddlewareWrapper::fileSystem_ = nullptr;

IceoryxWrapper::IceoryxWrapper(const ServiceMetaInfo& info) : 
    ReqResMiddlewareWrapper(info){
}

IceoryxWrapper::~IceoryxWrapper() {
    Shutdown();
}

void IceoryxWrapper::Init() {

}
void IceoryxWrapper::InitClient() {
    // 创建client
    iox::capro::IdString_t service(iox::TruncateToCapacity, 
            info_.service.c_str(), info_.service.length());
    iox::capro::IdString_t instance(iox::TruncateToCapacity, 
            info_.instance.c_str(), info_.instance.length());
    iox::capro::IdString_t event(iox::TruncateToCapacity, 
            info_.event.c_str(), info_.event.length());
    
    client_.reset(new iox::popo::UntypedClient({service, instance, event}));
    spdlog::info("client init, service: {}, instance: {}, event: {}",
        info_.service, info_.instance, info_.event);
}

void IceoryxWrapper::InitServer() {
    // 创建server
    ReqResMiddlewareWrapper::InitServer();
     iox::capro::IdString_t service(iox::TruncateToCapacity, 
            info_.service.c_str(), info_.service.length());
    iox::capro::IdString_t instance(iox::TruncateToCapacity, 
            info_.instance.c_str(), info_.instance.length());
    iox::capro::IdString_t event(iox::TruncateToCapacity, 
            info_.event.c_str(), info_.event.length());
    server_.reset(new iox::popo::UntypedServer({service, instance, event}));
    // std::cout << "server init, service: " << info_.service << ", instance: " << info_.instance << ", event: " << info_.event << std::endl;
    spdlog::info("IceoryxWrapper::InitServer, server: {}, instance: {}, event: {} ", info_.service, info_.instance, info_.event);
}

void IceoryxWrapper::InitDummyServer() {
    iox::capro::IdString_t service(iox::TruncateToCapacity, 
            info_.service.c_str(), info_.service.length());
    iox::capro::IdString_t instance(iox::TruncateToCapacity, 
            info_.instance.c_str(), info_.instance.length());
    iox::capro::IdString_t event(iox::TruncateToCapacity, 
            info_.event.c_str(), info_.event.length());
    server_.reset(new iox::popo::UntypedServer({service, instance, event}));
    // std::cout << "server init, service: " << info_.service << ", instance: " << info_.instance << ", event: " << info_.event << std::endl;
    spdlog::info("IceoryxWrapper::InitDummyServer, server: {}, instance: {}, event: {} ", info_.service, info_.instance, info_.event);
}

void IceoryxWrapper::Shutdown() {
    spdlog::info("shutdown IceoryxWrapper");
    if (servicetype_ == ServiceType::SERVER) {
        spdlog::info("stop the server....");
        // StopServer();
    } else if (servicetype_ == ServiceType::CLIENT) {
        StopClient();
        spdlog::info("stop the client....");
    } else if (servicetype_ == ServiceType::DUMMYSERVER) {
        spdlog::info("stop the dummyserver, do nothing");
    } else {
        spdlog::info("unknown service type : {}", (int)servicetype_);
    }
}

void IceoryxWrapper::StartServer() {
    // 启动server
    if (server_.get() == nullptr) {
        std::cerr << "server is nullptr" << std::endl;
        return;
    }
    spdlog::info("enter IceoryxWrapper::StartServer, bgein OnResponse");
    running_ = true;
    OnResponse();
    spdlog::info("enter IceoryxWrapper::StartServer, end OnResponse");
}

// 暂时没有调用
void IceoryxWrapper::StartClient() {
    // 启动client
    InitClient();
}

void IceoryxWrapper::StopServer() {
    kill(getpid(), SIGINT);
    running_ = false;
}

void IceoryxWrapper::StopClient() {
    intercept::internal::TerminalOpReqRes terminal;
    spdlog::info("wait stop client, service: {}, instance: {}, event: {}， client count: {}",
    info_.service, info_.instance, info_.event, client_.use_count());
    OnRequest(terminal);
}

// client: 这里组织请求并处理返回的响应
void IceoryxWrapper::OnRequest(PosixOpReqRes& reqRes) {
    // 上游用户侧需要调用
    // 假设我们直接将请求的响应数据复制回响应对象
    int reqsize = reqRes.GetRequestSize();
    int alignsize = reqRes.GetRequestAlignSize();
    int64_t expectedResponseSequenceId = requestSequenceId_;
    
    {
    // intercept::common::Timer timer("client request");
    client_->loan(reqsize, alignsize)
                .and_then([&](auto& requestPayload) {

                    auto requestHeader = iox::popo::RequestHeader::fromPayload(requestPayload);
                                        requestHeader->setSequenceId(requestSequenceId_);
                    expectedResponseSequenceId = requestSequenceId_;
                    requestSequenceId_ += 1;
                    char* request = static_cast<char*>(requestPayload);
                    
                    const iox::mepoo::ChunkHeader * chunkHeader = iox::mepoo::ChunkHeader::fromUserPayload(requestPayload);
                    spdlog::info("to loan chunk in client, head info, chunksize: {}", chunkHeader->chunkSize());

                    reqRes.CopyRequestDataToBuf((void*)request);
                    client_->send(request).or_else(
                        [&](auto& error) { std::cout << "Could not send Request! Error: " << error << std::endl; });
            })
            .or_else([](auto& error) { std::cout << "Could not allocate Request! Error: " << error << std::endl; });
    
    }
    //! [take response]
    {
    // intercept::common::Timer timer("client response");
    bool hasReceivedResponse{false};
    do{
        client_->take().and_then([&](const auto& responsePayload) {
            auto responseHeader = iox::popo::ResponseHeader::fromPayload(responsePayload);
            if (responseHeader->getSequenceId() == expectedResponseSequenceId)
            {
                const iox::mepoo::ChunkHeader * chunkHeader = iox::mepoo::ChunkHeader::fromUserPayload(responsePayload);
                spdlog::info("to release chunk in client, head info, type: {} typestr: {} , chunksize: {}", int(reqRes.GetOpType()), TypeToStr(reqRes.GetOpType()), chunkHeader->chunkSize());

                reqRes.SetResponse((void*)responsePayload);

                client_->releaseResponse(responsePayload);
                // sleep(10);
                const iox::mepoo::ChunkHeader* nowheader = iox::mepoo::ChunkHeader::fromUserPayload(responsePayload);
                if (nowheader == nullptr) {
                    spdlog::error("the chunkheader is nullptr!!!!");
                }
                spdlog::info("chunkheader info, chunksize {}", nowheader->chunkSize());
                // std::cout << "Got Response with expected sequence ID! -> continue" << std::endl;
            }
            else
            {
                spdlog::error("Got Response with outdated sequence ID! Expected = {}; Actual = {} ! -> skip",
                            expectedResponseSequenceId, responseHeader->getSequenceId());
            }
            hasReceivedResponse = true;
        });
    } while (!hasReceivedResponse);
    }

}

// server: 这里获取、处理请求并返回响应结果
void IceoryxWrapper::OnResponse() {
    auto lastRequestTime = std::chrono::steady_clock::now(); // 初始化上一次处理请求的时间戳
    int intervalSeconds = intercept::common::Configure::getInstance().getConfig("waitRequestMaxSeconds") == "" ? 5 : std::stoi(intercept::common::Configure::getInstance().getConfig("waitRequestMaxSeconds"));
    int trynumber = 0;
    int getnum = 0;
    int missnum = 0;
    
    std::chrono::steady_clock::duration totalDuration = std::chrono::steady_clock::duration::zero(); // 总耗时
    while (!iox::hasTerminationRequested() && running_) {
        trynumber++;
        if(trynumber > 2000000) {
            // ! 注意的判断可能会导致某些连接过早被中断，使得client无法正常响应
            auto now = std::chrono::steady_clock::now(); // 获取当前时间
            if (now - lastRequestTime > std::chrono::seconds(intervalSeconds)) { // 检查是否超过n秒无请求处理
                spdlog::info("No request handled in the last {}  seconds. Exiting loop.", intervalSeconds);
                break;
            }
        }
        server_->take().and_then([&](auto& requestPayload) {
            auto begintime = std::chrono::steady_clock::now();
            auto request = static_cast<const PosixOpRequest*>(requestPayload);
            // std::cout << "request type: " << (int)request->opType << std::endl;
            switch (request->opType) {
                case PosixOpType::OPEN:
                    HandleOpenRequest(requestPayload);
                    break;
                case PosixOpType::READ:
                    HandleReadRequest(requestPayload);
                    break;
                case PosixOpType::WRITE:
                    HandleWriteRequest(requestPayload);
                    break;
                case PosixOpType::CLOSE:
                    HandleCloseRequest(requestPayload);
                    break;
                case PosixOpType::STAT:
                    HandleStatRequest(requestPayload);
                    break;
                case PosixOpType::FSTAT:
                    HandleFstatRequest(requestPayload);
                    break;
                case PosixOpType::FSYNC:
                    HandleFsyncRequest(requestPayload);
                    break;
                case PosixOpType::LSEEK:
                    HandleLseekRequest(requestPayload);
                    break;
                case PosixOpType::MKDIR:
                    HandleMkdirRequest(requestPayload);
                    break;
                case PosixOpType::UNLINK:
                    HandleUnlinkRequest(requestPayload);
                    break;
                case PosixOpType::OPENDIR:
                    HandleOpendirRequest(requestPayload);
                    break;
                case PosixOpType::GETDENTS:
                    HandleGetdentsRequest(requestPayload);
                    break;
                case PosixOpType::CLOSEDIR:
                    HandleClosedirRequest(requestPayload);
                    break;
                case PosixOpType::RENAME:
                    HandleRenameRequest(requestPayload);
                    break;
                case PosixOpType::TRUNCATE:
                    HandleTruncateRequest(requestPayload);
                    break;
                case PosixOpType::TERMINAL:
                    HandleTerminalRequest(requestPayload);
                    break;
                default:
                    spdlog::error("Unsupported request type: {}", (int)request->opType);
                    break;
            }
            
            // 更新最后处理请求的时间戳
            lastRequestTime = std::chrono::steady_clock::now();
            trynumber = 0; // 归零
            getnum++;
            totalDuration +=  (lastRequestTime - begintime);
        }
        );
        // TODO: 如果不sleep 获取不到数据 待排查
        // sleep(1);
    }
    std::cout << "exit Server OnResponse... " << info_.service << " " << info_.instance << "  " << info_.event << std::endl;

    // if (getnum > 0) {
    //     std::cout << "total request time: " << totalDuration.count() << " , average time : " << totalDuration.count()/ getnum << std::endl;
    // }
}

void IceoryxWrapper::HandleOpenRequest(const auto& requestPayload) {
    auto request = static_cast<const OpenRequestData*>(requestPayload);
    spdlog::info("Open file request, path: {}, flags: {}, mode: {}", request->path, request->flags, request->mode);
    // 这里可以调用posix open函数
    auto requestHeader = iox::popo::RequestHeader::fromPayload(requestPayload);
    server_->loan(requestHeader, sizeof(OpenResponseData), alignof(OpenResponseData))
            .and_then([&](auto& responsePayload) {
                const iox::mepoo::ChunkHeader * chunkHeader = iox::mepoo::ChunkHeader::fromUserPayload(requestPayload);
                spdlog::info("to loan chunk in server open , head info, chunksize: {}", chunkHeader->chunkSize());  

                auto response = static_cast<OpenResponseData*>(responsePayload);
                response->opType = request->opType;
                response->fd = fileSystem_->Open(request->path, request->flags, request->mode);
                server_->send(responsePayload).or_else(
                        [&](auto& error) { std::cout << "Could not send Response! Error: " << error << std::endl; });
                spdlog::info("open response info, the type: {}, the fd: {}", intercept::internal::TypeToStr(response->opType), response->fd );
            })
            .or_else(
            [&](auto& error) { std::cout << "Could not allocate Open Response! Error: " << error << std::endl; });

    const iox::mepoo::ChunkHeader * chunkHeader = iox::mepoo::ChunkHeader::fromUserPayload(requestPayload);
    spdlog::info("to release chunk in server open , head info, chunksize: {}", chunkHeader->chunkSize());
    server_->releaseRequest(request);
}

void IceoryxWrapper::HandleReadRequest(const auto& requestPayload) {
    auto request = static_cast<const ReadRequestData*>(requestPayload);
    auto requestHeader = iox::popo::RequestHeader::fromPayload(requestPayload);
    server_->loan(requestHeader, sizeof(ReadResponseData) + request->count, alignof(ReadResponseData))
            .and_then([&](auto& responsePayload) {
        auto response = static_cast<ReadResponseData*>(responsePayload);
        response->opType = request->opType;
        char* buf = (char*) response + sizeof(ReadResponseData);

        const iox::mepoo::ChunkHeader * chunkHeader = iox::mepoo::ChunkHeader::fromUserPayload(requestPayload);
        // spdlog::info("to loan chunk in server read , head info, chunksize: {} real size: {}", chunkHeader->chunkSize(), sizeof(ReadResponseData) + request->count);

        if (intercept::common::Configure::getInstance().getConfig("multiop") == "true"
        && request->count >= atol(intercept::common::Configure::getInstance().getConfig("blocksize").c_str())) {
            response->length = fileSystem_->MultiRead(request->fd, buf, request->count);
        } else {
            response->length = fileSystem_->Read(request->fd, buf, request->count);
        }
        server_->send(responsePayload).or_else([&](auto& error){ std::cout << "Could not send Response for Read! Error: " << error << std::endl;});
        spdlog::debug("read response, fd: {}, count: {}, read response info, the type: {}, the length: {}",
                        request->fd, request->count, intercept::internal::TypeToStr(response->opType), response->length);
    }).or_else(
            [&](auto& error) { std::cout << "Could not allocate Read Response! Error: " << error << std::endl; });
    
    const iox::mepoo::ChunkHeader * chunkHeader = iox::mepoo::ChunkHeader::fromUserPayload(requestPayload);
                // spdlog::info("to release chunk in server read , head info, chunksize: {}", chunkHeader->chunkSize());

    server_->releaseRequest(request);
    
}

void IceoryxWrapper::HandleWriteRequest(const auto& requestPayload) {
    spdlog::debug("handle one write request");
    auto request = static_cast<const WriteRequestData*>(requestPayload);
    auto requestHeader = iox::popo::RequestHeader::fromPayload(requestPayload);
    server_->loan(requestHeader, sizeof(WriteResponseData), alignof(WriteResponseData))
            .and_then([&](auto& responsePayload) {
        auto response = static_cast<WriteResponseData*>(responsePayload);
        response->opType = request->opType;
        if (intercept::common::Configure::getInstance().getConfig("multiop") == "true"
        && request->count >= atol(intercept::common::Configure::getInstance().getConfig("blocksize").c_str())) {
            response->length = fileSystem_->MultiWrite(request->fd, request->content, request->count); 
        } else {
            response->length = fileSystem_->Write(request->fd, request->content, request->count);
        }
        
        server_->send(responsePayload).or_else([&](auto& error){ std::cout << "Could not send Response for Write! Error: " << error << std::endl;});
        spdlog::debug("write response, fd: {}, count: {}, write response info, the type: {}, the length: {}",
                        request->fd, request->count, intercept::internal::TypeToStr(response->opType), response->length);
    }).or_else(
            [&](auto& error) { std::cout << "Could not allocate Write Response! Error: " << error << std::endl; });
    server_->releaseRequest(request);
}

void IceoryxWrapper::HandleCloseRequest(const auto& requestPayload) {
    auto request = static_cast<const CloseRequestData*>(requestPayload);
    spdlog::info("close request, fd: {}", request->fd);
    auto requestHeader = iox::popo::RequestHeader::fromPayload(requestPayload);
    server_->loan(requestHeader, sizeof(CloseResponseData), alignof(CloseResponseData))
            .and_then([&](auto& responsePayload) {
        
        const iox::mepoo::ChunkHeader * chunkHeader = iox::mepoo::ChunkHeader::fromUserPayload(requestPayload);
        spdlog::info("to loan chunk in server close , head info, chunksize: {}", chunkHeader->chunkSize());

        auto response = static_cast<CloseResponseData*>(responsePayload);
        response->opType = request->opType;
        response->ret = fileSystem_->Close(request->fd);
        spdlog::info("finish close, fd: {}", request->fd);
        server_->send(responsePayload).or_else([&](auto& error){ std::cout << "Could not send Response for Close! Error: " << error << std::endl;});

        spdlog::info("close response info, the type: {}, the ret: {}", intercept::internal::TypeToStr(response->opType), response->ret);
    }).or_else(
            [&](auto& error) { std::cout << "Could not allocate Write Response! Error: " << error << std::endl; });
    
    const iox::mepoo::ChunkHeader * chunkHeader = iox::mepoo::ChunkHeader::fromUserPayload(requestPayload);
    spdlog::info("to release chunk in server close , head info, chunksize: {}", chunkHeader->chunkSize());

    server_->releaseRequest(request);
}

void IceoryxWrapper::HandleFsyncRequest(const auto& requestPayload) {
    auto request = static_cast<const FsyncRequestData*>(requestPayload);
    spdlog::info("fsync reqeust, fd: {}", request->fd);
    auto requestHeader = iox::popo::RequestHeader::fromPayload(requestPayload);
    server_->loan(requestHeader, sizeof(FsyncResponseData), alignof(FsyncResponseData))
            .and_then([&](auto& responsePayload) {
        auto response = static_cast<FsyncResponseData*>(responsePayload);
        response->opType = request->opType;
        response->ret = fileSystem_->Fsync(request->fd);
        server_->send(responsePayload).or_else([&](auto& error){ std::cout << "Could not send Response for Stat! Error: " << error << std::endl;});
        spdlog::info("fsync response info, ret: {}", response->ret);
    }).or_else(
            [&](auto& error) { std::cout << "Could not allocate Write Response! Error: " << error << std::endl; });
    server_->releaseRequest(request);
}

void IceoryxWrapper::HandleStatRequest(const auto& requestPayload) {
    auto request = static_cast<const StatRequestData*>(requestPayload);
    spdlog::info("stat request, pathname: {}", request->path);
    auto requestHeader = iox::popo::RequestHeader::fromPayload(requestPayload);
    server_->loan(requestHeader, sizeof(StatResponseData), alignof(StatResponseData))
            .and_then([&](auto& responsePayload) {
        auto response = static_cast<StatResponseData*>(responsePayload);
        response->opType = request->opType;
        response->ret = fileSystem_->Stat(request->path, &(response->fileStat));
        server_->send(responsePayload).or_else([&](auto& error){ std::cout << "Could not send Response for Stat! Error: " << error << std::endl;});
        spdlog::info("stat response info, the ino: {}, size: {}, the ret: {}", 
                    (int)response->fileStat.st_ino, response->fileStat.st_size, response->ret);
    }).or_else(
            [&](auto& error) { std::cout << "Could not allocate Write Response! Error: " << error << std::endl; });
    server_->releaseRequest(request);
}

void IceoryxWrapper::HandleFstatRequest(const auto& requestPayload) {
    auto request = static_cast<const FstatRequestData*>(requestPayload);
    spdlog::info("fstat request, fd: {}", request->fd);
    auto requestHeader = iox::popo::RequestHeader::fromPayload(requestPayload);
    server_->loan(requestHeader, sizeof(FstatResponseData), alignof(FstatResponseData))
            .and_then([&](auto& responsePayload) {
        auto response = static_cast<FstatResponseData*>(responsePayload);
        response->opType = request->opType;
        response->ret = fileSystem_->Fstat(request->fd, &(response->fileStat));
        server_->send(responsePayload).or_else([&](auto& error){ std::cout << "Could not send Response for Stat! Error: " << error << std::endl;});
        spdlog::info("fstat response info, the ino: {}, size: {}, the ret: {}",
                     (int)response->fileStat.st_ino, response->fileStat.st_size, response->ret);

    }).or_else(
            [&](auto& error) { std::cout << "Could not allocate Write Response! Error: " << error << std::endl; });
    server_->releaseRequest(request);
}

void IceoryxWrapper::HandleLseekRequest(const auto& requestPayload) {
    auto request = static_cast<const LseekRequestData*>(requestPayload);
    spdlog::debug("lseek request, fd: {}, offset: {}", request->fd, request->offset);
    auto requestHeader = iox::popo::RequestHeader::fromPayload(requestPayload);
    server_->loan(requestHeader, sizeof(LseekResponseData), alignof(LseekResponseData))
            .and_then([&](auto& responsePayload) {
        auto response = static_cast<LseekResponseData*>(responsePayload);
        response->opType = request->opType;
        response->ret = fileSystem_->Lseek(request->fd, request->offset, request->whence);
        server_->send(responsePayload).or_else([&](auto& error){ std::cout << "Could not send Response for Stat! Error: " << error << std::endl;});
        spdlog::debug("lseek response, ret: {}", response->ret);
    }).or_else(
            [&](auto& error) { std::cout << "Could not allocate Write Response! Error: " << error << std::endl; });
    server_->releaseRequest(request);
}

void IceoryxWrapper::HandleMkdirRequest(const auto& requestPayload) {
    auto request = static_cast<const MkdirRequestData*>(requestPayload);
    spdlog::info("mkdir request, pathname: {}, mode: {}", request->path, request->mode);
    auto requestHeader = iox::popo::RequestHeader::fromPayload(requestPayload);
    server_->loan(requestHeader, sizeof(MkdirResponseData), alignof(MkdirResponseData))
            .and_then([&](auto& responsePayload) {
        auto response = static_cast<MkdirResponseData*>(responsePayload);
        response->opType = request->opType;
        response->ret = fileSystem_->Mkdir(request->path, request->mode);
        server_->send(responsePayload).or_else([&](auto& error){ std::cout << "Could not send Response for Stat! Error: " << error << std::endl;});
        spdlog::info("mkdir resposne, ret: {}", response->ret);
    }).or_else(
            [&](auto& error) { std::cout << "Could not allocate Write Response! Error: " << error << std::endl; });
    server_->releaseRequest(request);
}

void IceoryxWrapper::HandleGetdentsRequest(const auto& requestPayload) {
    auto request = static_cast<const GetdentsRequestData*>(requestPayload);
    int maxread = request->maxread;
    maxread = 200; // 暂时读取目录下的200个文件，否则分配会失败
    spdlog::info("getdents request, fd: {}, the info: {}", request->dirinfo.fh, request->dirinfo.ino);
    auto requestHeader = iox::popo::RequestHeader::fromPayload(requestPayload);
    server_->loan(requestHeader, sizeof(GetdentsResponseData) + maxread * sizeof(dirent64), alignof(GetdentsResponseData))
            .and_then([&](auto& responsePayload) {
        auto response = static_cast<GetdentsResponseData*>(responsePayload);
        response->opType = request->opType;
        auto req = const_cast<GetdentsRequestData*>(request);
        response->ret = fileSystem_->Getdents(&req->dirinfo, response->contents, maxread, &response->realbytes);
        response->dirinfo = req->dirinfo;
        server_->send(responsePayload).or_else([&](auto& error){ std::cout << "Could not send Response for Stat! Error: " << error << std::endl;});
        spdlog::info("getdents response, ret: {}, thre realbytes: {}, the offset: {}",
                    response->ret, response->realbytes, response->dirinfo.offset);
    }).or_else(
            [&](auto& error) { std::cout << "Could not allocate Write Response! Error: " << error << std::endl; });
    server_->releaseRequest(request);
}

void IceoryxWrapper::HandleOpendirRequest(const auto&requestPayload) {
    auto request = static_cast<const OpendirRequestData*>(requestPayload);
    spdlog::info("opendir request, path: {}", request->path);
    auto requestHeader = iox::popo::RequestHeader::fromPayload(requestPayload);
    server_->loan(requestHeader, sizeof(OpendirResponseData), alignof(OpendirResponseData))
            .and_then([&](auto& responsePayload) {
                auto response = static_cast<OpendirResponseData*>(responsePayload);
                response->opType = request->opType;
                response->ret = fileSystem_->Opendir(request->path, &response->dirStream);
                server_->send(responsePayload).or_else(
                        [&](auto& error) { std::cout << "Could not send Response! Error: " << error << std::endl; });
                spdlog::info("opendir response, the type: {}, the fd: {}", TypeToStr(response->opType), response->dirStream.fh);
            })
            .or_else(
            [&](auto& error) { std::cout << "Could not allocate Open Response! Error: " << error << std::endl; });
    server_->releaseRequest(request);
}

void IceoryxWrapper::HandleClosedirRequest(const auto& requestPayload) {
    auto request = static_cast<const ClosedirRequestData*>(requestPayload);
    spdlog::info("closedir requset, fd: {}", request->dirstream.fh);
    auto requestHeader = iox::popo::RequestHeader::fromPayload(requestPayload);
    server_->loan(requestHeader, sizeof(ClosedirResponseData), alignof(ClosedirResponseData))
            .and_then([&](auto& responsePayload) {
        auto response = static_cast<ClosedirResponseData*>(responsePayload);
        response->opType = request->opType;
        response->ret = fileSystem_->Closedir(const_cast<intercept::common::DirStream*>(&request->dirstream));
        server_->send(responsePayload).or_else([&](auto& error){ std::cout << "Could not send Response for Closedir! Error: " << error << std::endl;});
        spdlog::info("closedir response, the type: {}, the ret: {}", TypeToStr(response->opType), response->ret );
    }).or_else(
            [&](auto& error) { std::cout << "Could not allocate Write Response! Error: " << error << std::endl; });
    server_->releaseRequest(request);
}


void IceoryxWrapper::HandleUnlinkRequest(const auto& requestPayload) {
    auto request = static_cast<const UnlinkRequestData*>(requestPayload);
    spdlog::info("unlink reqeust, pathname: {}", request->path);
    auto requestHeader = iox::popo::RequestHeader::fromPayload(requestPayload);
    server_->loan(requestHeader, sizeof(UnlinkResponseData), alignof(UnlinkResponseData))
            .and_then([&](auto& responsePayload) {
        auto response = static_cast<UnlinkResponseData*>(responsePayload);
        response->opType = request->opType;
        response->ret = fileSystem_->Unlink(request->path);
        server_->send(responsePayload).or_else([&](auto& error){ std::cout << "Could not send Response for Stat! Error: " << error << std::endl;});
        spdlog::info("unlink response, ret: ", response->ret);
    }).or_else(
            [&](auto& error) { std::cout << "Could not allocate Write Response! Error: " << error << std::endl; });
    server_->releaseRequest(request);
}

void IceoryxWrapper::HandleRenameRequest(const auto& requestPayload) {
    auto request = static_cast<const RenameRequestData*>(requestPayload);
    spdlog::info("rename request, oldpath: {}, newpath: {}", request->oldpath, request->newpath);
    auto requestHeader = iox::popo::RequestHeader::fromPayload(requestPayload);
    server_->loan(requestHeader, sizeof(RenameResponseData), alignof(RenameResponseData))
            .and_then([&](auto& responsePayload) {
        auto response = static_cast<RenameResponseData*>(responsePayload);
        response->opType = request->opType;
        response->ret = fileSystem_->Rename(request->oldpath, request->newpath);
        server_->send(responsePayload).or_else([&](auto& error){ std::cout << "Could not send Response for Stat! Error: " << error << std::endl;});
        spdlog::info("rename response, ret: {}", response->ret);
    }).or_else(
            [&](auto& error) { std::cout << "Could not allocate Write Response! Error: " << error << std::endl; });
    server_->releaseRequest(request);
}

void IceoryxWrapper::HandleTruncateRequest(const auto& requestPayload) {
    
    auto request = static_cast<const TruncateRequestData*>(requestPayload);
    spdlog::info("truncate request, path: {}, length: {}", request->path, request->length);
    auto requestHeader = iox::popo::RequestHeader::fromPayload(requestPayload);
    server_->loan(requestHeader, sizeof(TruncateResponseData), alignof(TruncateResponseData))
            .and_then([&](auto& responsePayload) {
        auto response = static_cast<TruncateResponseData*>(responsePayload);
        
        response->opType = request->opType;
        response->ret = fileSystem_->Truncate(request->path, request->length);
        server_->send(responsePayload).or_else([&](auto& error){ std::cout << "Could not send Response for Stat! Error: " << error << std::endl;});
        spdlog::info("truncate response, ret: {}", response->ret);
    }).or_else(
            [&](auto& error) { std::cout << "Could not allocate Write Response! Error: " << error << std::endl; });
    server_->releaseRequest(request);
}

void IceoryxWrapper::HandleTerminalRequest(const auto& requestPayload) {
    
    auto request = static_cast<const TerminalRequestData*>(requestPayload);
    spdlog::info("terminal request.");
    auto requestHeader = iox::popo::RequestHeader::fromPayload(requestPayload);
    server_->loan(requestHeader, sizeof(TerminalResponseData), alignof(TerminalResponseData))
            .and_then([&](auto& responsePayload) {

        auto response = static_cast<TerminalResponseData*>(responsePayload);
        
        response->opType = request->opType;
        response->ret = 0;
        running_ = false; // 终结退出
        server_->send(responsePayload).or_else([&](auto& error){ std::cout << "Could not send Response for Terminal! Error: " << error << std::endl;});

        const iox::mepoo::ChunkHeader * chunkHeader = iox::mepoo::ChunkHeader::fromUserPayload(responsePayload);
        spdlog::info("terminal response, ret: {}, pid: {}, tid: {}, loan chunk  chunksize: {}", 
                        response->ret, (unsigned int) getpid(), (unsigned int) pthread_self(), chunkHeader->chunkSize());
        sleep(0.1);
    }).or_else(
            [&](auto& error) { std::cout << "Could not allocate Terminal Response! Error: " << error << std::endl; });

    const iox::mepoo::ChunkHeader * chunkHeader = iox::mepoo::ChunkHeader::fromUserPayload(requestPayload);
    spdlog::info("to release chunk in server terminal , head info, chunksize: {}", chunkHeader->chunkSize());
    server_->releaseRequest(request);
}

} // namespace middleware
} // namespace intercept


int test() {
    // std::string servicename = "MyService";
    // std::unique_ptr<ReqResMiddlewareWrapper> middleware = std::make_unique<IceoryxWrapper>(servicename);
    // AddClientService(servicename);
    // WriteOpReqRes writeReqRes(1, "data".data(), 4, 0);
    // int ret = middleware->OnRequest(writeReqRes);
    // const auto& response = middleware->GetResponse(writeRequest);
    // if (response.result >= 0) {
    //     std::cout << "Write operation successful!" << std::endl;
    // } else {
    //     std::cout << "Write operation failed with error code: " << response.result << std::endl;
    // }
    return 0;
}
