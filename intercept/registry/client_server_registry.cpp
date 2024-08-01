#include <string>
#include <ctime>
#include <cstdlib>

#include "middleware/iceoryx_wrapper.h"
#include "client_server_registry.h"

namespace intercept {
namespace registry {
    
using intercept::discovery::IceoryxDiscovery;
using intercept::middleware::IceoryxWrapper;
std::string generateRandomString(int length) {
    std::string result;
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    srand(time(0)); // 初始化随机数生成器
    for (int i = 0; i < length; i++) {
        int randomIndex = rand() % strlen(charset);
        result += charset[randomIndex];
    }
    return result;
}

ClientServerRegistry::ClientServerRegistry(const std::string& middlewareType, const ServiceMetaInfo& info) {
    // 根据middlewareType创建对应的ServiceDiscovery
    discovery_ = std::make_shared<IceoryxDiscovery>();
    serviceInfo_ = info;
    middlewareType_ = middlewareType;
    spdlog::info("ClientServerRegistry init");
}

ClientServerRegistry::~ClientServerRegistry() {
    spdlog::info("ClientServerRegistry destory");
}

// 在用户侧，创建dummpserver

std::shared_ptr<ReqResMiddlewareWrapper> ClientServerRegistry::CreateDummyServer() {
    std::string dummpyserver = "dummy_server";
    ServiceMetaInfo info;
    info.service = SERVICE_FLAG;
    info.instance = DUMMY_INSTANCE_FLAG;
    pid_t pid = getpid();
    auto myid = std::this_thread::get_id();
    std::stringstream ss;
    ss << myid;
    std::string threadid = ss.str();
    info.event = generateRandomString(10) + std::to_string((long)pid) + threadid;
    info.serverType = "dummy";

    spdlog::info("ClientServerRegistry try to create dummy server, the service: {}, instance: {}, event: {}",
                info.service, info.instance, info.event);   

    std::shared_ptr<ReqResMiddlewareWrapper> wrapper;
    if (middlewareType_ == ICEORYX) {
        wrapper = std::make_shared<IceoryxWrapper>(info);
        wrapper->SetServiceType(intercept::middleware::ServiceType::DUMMYSERVER);   
    }
    wrapper->InitDummyServer();
    spdlog::info("ClientServerRegistry finish creating dummy server, server: {}, instance: {}, event: {}",
                info.service, info.instance, info.event);  
    return wrapper;
}

void ClientServerRegistry::DestroyDummyServer() {
    std::string dummpyserver = "dummy_server";
}

std::shared_ptr<ReqResMiddlewareWrapper> 
    ClientServerRegistry::CreateClient(const ServiceMetaInfo& info) {
        // 1. 获取客户端创建client的请求
        // 2. 创建对应的client
        // 3. 返回对应的client
        if (middlewareType_ == ICEORYX) {
            spdlog::info("ClientServerRegistry begin creating client, service: {}, instance: {}, event: {}",
                        info.service, info.instance, info.event);
            std::shared_ptr<IceoryxWrapper> wrapper = std::make_shared<IceoryxWrapper>(info);
            wrapper->SetServiceType(intercept::middleware::ServiceType::CLIENT);
            wrapper->InitClient();
            return wrapper;
        }
        return nullptr;
}

std::shared_ptr<ReqResMiddlewareWrapper> 
    ClientServerRegistry::CreateServer(const ServiceMetaInfo& info) {
        // 1. 获取客户端创建server的请求
        // 2. 创建对应的server
        // 3. 返回对应的server
        if (middlewareType_ == ICEORYX) {
            std::shared_ptr<IceoryxWrapper> wrapper = std::make_shared<IceoryxWrapper>(info);
            wrapper->SetServiceType(intercept::middleware::ServiceType::SERVER);
            // wrapper->InitServer();
            return wrapper;
        }
        return nullptr;
}


// 作用于服务端
void ClientServerRegistry::CreateServers() {
    // 1. 获取客户端创建server的请求
    std::vector<ServiceMetaInfo> results = discovery_->FindServices(serviceInfo_);
    std::vector<ServiceMetaInfo> neededServers;
    
    // 通过dummy请求获取创建server的需求
    for (auto& result : results) {
        if (result.instance == DUMMY_INSTANCE_FLAG && 
            serverMap_.find(result.event) == serverMap_.end()){
            // 根据dummy 创建一个serveiceinfo
            ServiceMetaInfo info;
            info.service = result.service;
            info.instance = INTERCEPT_INSTANCE_FLAG;
            info.event = result.event;
            neededServers.push_back(info);

            spdlog::info("ClientServerRegistry create server, service: {}, instance: {}, event: {}",
                        info.service, info.instance, info.event);
        }
    }

    // 2. 创建对应的server
    for (const auto& result : neededServers) {
        // 启动一个线程，创建ReqResMiddlewareWrapper 并调用它的StartServer函数
        // 2.1 是否已经创建对应server
        // 2.2 如果没有创建， 创建server，并添加到serverMap_中
        // 2.3 如果已经创建，跳过
        if (middlewareType_ == ICEORYX) {
            std::thread t([this, result]() {
                // 创建server
                auto wrapper = std::make_shared<IceoryxWrapper>(result);
                wrapper->SetServiceType(intercept::middleware::ServiceType::SERVER);
                this->serverMap_[result.event] = wrapper;
                // 启动server
                wrapper->InitServer();
                wrapper->StartServer();
                // 添加到serverMap_中
            });
            threads_.push_back(std::move(t));
        }
        sleep(0.1);
    }

}

void ClientServerRegistry::DestroyServers() {
    // 1. 获取客户端销毁server的请求
    // 2. 销毁对应的server
}

void ClientServerRegistry::MonitorServers() {
    spdlog::info("ClientServerRegistry monitor servers");
    while (1) {
        // create:
        CreateServers();
        // destroy:
        DestroyServers();
        // TODO: 这个等待很重要
        sleep(1);
    }
    for (auto& t : threads_) {
        t.join();
    }
}

} // namespace internal
} // namespace intercecpt


