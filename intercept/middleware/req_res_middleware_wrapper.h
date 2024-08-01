#pragma once
#include <memory>

#include "internal/posix_op_req_res.h"
#include "internal/metainfo.h"

namespace intercept {
namespace filesystem {
    class AbstractFileSystem; // Forward declaration
}
}

namespace intercept
{
namespace middleware 
{
using intercept::internal::ServiceMetaInfo;
using intercept::internal::PosixOpReqRes;

enum class ServiceType {
    CLIENT = 0,
    SERVER = 1,
    DUMMYSERVER = 2,
};

class ReqResMiddlewareWrapper {
public:
    ReqResMiddlewareWrapper()  {
        spdlog::info("construct  ReqResMiddlewareWrapper");
    }

    ReqResMiddlewareWrapper(ServiceMetaInfo info) : info_(info) {
        spdlog::info("construct  ReqResMiddlewareWrapper");

    }

    virtual ~ReqResMiddlewareWrapper() {
        spdlog::info("deconstruct  ReqResMiddlewareWrapper");

    }

    virtual void Init();

    virtual void InitClient();

    virtual void InitServer();

    virtual void SetServiceType(ServiceType type) {
        servicetype_ = type;
    }

    virtual void InitDummyServer() {}
    
    virtual void StartServer() = 0;

    virtual void StartClient() = 0;

    virtual void StopServer() = 0;

    virtual void StopClient() = 0;

    // 对外request接口
    virtual void OnRequest(PosixOpReqRes& reqRes) = 0;

    // 对外response接口
    virtual void OnResponse() = 0;

    virtual void Shutdown() = 0;

    virtual ServiceMetaInfo GetServiceMetaInfo() = 0;

protected:
    static  std::shared_ptr<intercept::filesystem::AbstractFileSystem> fileSystem_;
    ServiceMetaInfo info_;
    ServiceType servicetype_;
};

} // namespace middleware
} // namespace intercept

