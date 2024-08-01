
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "registry/client_server_registry.h"

using namespace intercept::internal;
using namespace intercept::registry;
std::mutex mtx;
std::condition_variable cv;
std::atomic<bool> discovery_thread_running{false};

int main() {
    constexpr char APP_NAME[] = "iox-intercept-server";
    if (intercept::common::Configure::getInstance().loadConfig(intercept::common::CONFIG_FILE)) {
        std::cout << "Config file loaded" << std::endl;
    } else {
        std::cout << "Config file not loaded: server.conf" << std::endl;
        return 0;
    }
    intercept::common::InitLog();
    iox::runtime::PoshRuntime::initRuntime(APP_NAME);
    ServiceMetaInfo info = {SERVICE_FLAG, "", ""};
    std::string type = ICEORYX;
    ClientServerRegistry registry(type, info);
    spdlog::info("begin to monitor servers");
    registry.MonitorServers();
    return 0;
}