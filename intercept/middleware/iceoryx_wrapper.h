#pragma once

#include "req_res_middleware_wrapper.h"

#include "iceoryx_posh/popo/untyped_server.hpp"
#include "iceoryx_posh/popo/untyped_client.hpp"

namespace intercept {
namespace filesystem {
    class AbstractFileSystem; // Forward declaration
}
}

namespace intercept {
namespace middleware {
    
class IceoryxWrapper : public ReqResMiddlewareWrapper {
public:
    explicit IceoryxWrapper(const ServiceMetaInfo& info);

    ~IceoryxWrapper() override;
    
    virtual void Init() override;

    virtual void InitClient() override;

    virtual void InitServer() override;

    virtual void InitDummyServer() override;
    
    virtual void StartServer();

    virtual void StartClient();

    virtual void StopServer() override;

    virtual void StopClient() override;

    virtual void OnRequest(PosixOpReqRes& reqRes) override;

    virtual void OnResponse() override;

    virtual void Shutdown() override;

    virtual ServiceMetaInfo GetServiceMetaInfo() override {return info_;}

private:
    void HandleOpenRequest(const auto& requestPayload);
    void HandleReadRequest(const auto& requestPayload);
    void HandleWriteRequest(const auto& requestPayload);
    void HandleCloseRequest(const auto& requestPayload);
    void HandleLseekRequest(const auto& requestPayload);
    void HandleFsyncRequest(const auto& requestPayload);
    void HandleStatRequest(const auto& requestPayload);
    void HandleFstatRequest(const auto& requestPayload);
    void HandleMkdirRequest(const auto& requestPayload);
    void HandleOpendirRequest(const auto& requestPayload);
    void HandleGetdentsRequest(const auto& requestPayload);
    void HandleClosedirRequest(const auto& requestPayload);
    void HandleUnlinkRequest(const auto& requestPayload);
    void HandleRenameRequest(const auto& requestPayload);
    void HandleTruncateRequest(const auto& requestPayload);
    void HandleTerminalRequest(const auto& requestPayload);

private:
    std::shared_ptr<iox::popo::UntypedServer> server_;
    
    std::shared_ptr<iox::popo::UntypedClient> client_;

    int64_t requestSequenceId_ = 0;
    bool running_ = false;
};


} // namespace middleware
} // namespace intercept
