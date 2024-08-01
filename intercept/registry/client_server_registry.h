#pragma once

#include "middleware/req_res_middleware_wrapper.h"
#include "discovery/iceoryx_discovery.h"
#include "discovery/discovery.h"

#define CREATE_FLAG  "create"
#define DESTROY_FLAG "destroy"
#define SERVER_FLAG "server"

namespace intercept {
namespace registry {

using intercept::middleware::ReqResMiddlewareWrapper;
using intercept::discovery::Discovery;
using intercept::internal::OpenOpReqRes;
using intercept::internal::ServiceMetaInfo;


class ClientServerRegistry {
    
public:
    // ...
    ClientServerRegistry(const std::string& middlewareType, const ServiceMetaInfo& info);
    ~ClientServerRegistry();
    // 创建临时的server，主要用于通过server创建数据交换的server
    std::shared_ptr<ReqResMiddlewareWrapper>  CreateDummyServer();
    void DestroyDummyServer();

    // 返回一个已经初始化的middleWrapper_;
    std::shared_ptr<ReqResMiddlewareWrapper> CreateClient(const ServiceMetaInfo& info);
    std::shared_ptr<ReqResMiddlewareWrapper>  CreateServer(const ServiceMetaInfo& info);

    // 在daemon端更新server
    void MonitorServers();

private:
    // 根据client传递的信息
    void CreateServers(); // 创建服务
    void DestroyServers(); // 销毁服务
    
private:
    // ...
    std::string middlewareType_;
    ServiceMetaInfo serviceInfo_; // 这里一个service由：service instance构成 
    std::shared_ptr<Discovery> discovery_;

    std::vector<std::shared_ptr<ReqResMiddlewareWrapper>> clientWrapper_;
    std::vector<std::shared_ptr<ReqResMiddlewareWrapper>> serverWrapper_;

    std::set<std::string> dummyevent_;
    std::unordered_map<std::string, std::shared_ptr<ReqResMiddlewareWrapper>> serverMap_;

    // 存放创建的线程
    std::vector<std::thread> threads_;

};

///
// int client() {
//     ServiceMetaInfo info = {"Service", "Instance", "Event"};
//     ClientServerRegistry registry("ICE", info);
//     registry.CreateDummyServer();
//     auto client = registry.CreateClient(ServiceMetaInfo{"Service", "Instance", "Event"});
//     OpenOpReqRes reqres("test", 1, 1);
//     client->OnRequest(reqres);
//     // 全局使用这一个client去操作请求
    
//     registry.DestroyDummyServer();
//     return 0;
// }

}
}




