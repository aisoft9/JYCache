#include <vector>
#include <string>

#include "iceoryx_discovery.h"


#include "iox/signal_watcher.hpp"
#include "iceoryx_posh/runtime/posh_runtime.hpp"
#include "iceoryx_posh/runtime/service_discovery.hpp"

namespace intercept {
namespace discovery {

// TODO: Add your own discovery service implementation here
#define DISCOVERY_SERVICE_NAME "IceoryxDiscoveryService"
#define DISCOVERY_SERVICE_VERSION "1.0.0"
#define DISCOVERY_SERVICE_DESCRIPTION "IceoryxDiscoveryServiceDescription"
#define DISCOVERY_SERVICE_PROVIDER "IceoryxDiscoveryServiceProvider"
// constexpr char APP_NAME[] = "iox-discovery-service";

IceoryxDiscovery::IceoryxDiscovery() {

}

IceoryxDiscovery::~IceoryxDiscovery() {
    // TODO: Clean up your discovery service implementation here
}

void IceoryxDiscovery::Init() {
    // TODO: Initialize your discovery service implementation here
}

void IceoryxDiscovery::Start() {
    // TODO: Start your discovery service implementation here
    while (!iox::hasTerminationRequested()) {
        // TODO: Implement discovery service logic here
        const auto& servers = GetServers();
        const auto& newservers = GetNewServers(existingServers, servers);
        for (auto& server : newservers) {
            // TODO: Implement logic to handle new servers here
            CreateServer(server);
        }

        const auto& removedServers = GetRemovedServers(existingServers, servers);
        for (auto& server : removedServers) {
            // TODO: Implement logic to handle deleted servers here
            DeleteServer(server);
        }
        existingServers = servers;

    }
}

void IceoryxDiscovery::Stop() {
    // TODO: Stop your discovery service implementation here
}

std::vector<ServiceMetaInfo> IceoryxDiscovery::GetServers() const {
    return {};
}

std::vector<ServiceMetaInfo> IceoryxDiscovery::GetNewServers(const std::vector<ServiceMetaInfo>& existingServers, const std::vector<ServiceMetaInfo>& newServers) {
    std::vector<ServiceMetaInfo> newServersList;
    return newServersList;
}

std::set<ServiceMetaInfo> IceoryxDiscovery::GetRemovedServers(const std::vector<ServiceMetaInfo>& existingServers, const std::vector<ServiceMetaInfo>& newServers) {
    std::set<ServiceMetaInfo> removedServersList;
    return removedServersList;
}

std::vector<ServiceMetaInfo> IceoryxDiscovery::FindServices(const ServiceMetaInfo& info) {
    iox::capro::IdString_t serviceStr(iox::TruncateToCapacity, info.service.c_str());
    iox::capro::IdString_t instanceStr(iox::TruncateToCapacity, info.instance.c_str());
    iox::capro::IdString_t eventStr(iox::TruncateToCapacity, info.instance.c_str());

    iox::optional<iox::capro::IdString_t> service = serviceStr;
    iox::optional<iox::capro::IdString_t> instance = instanceStr;
    iox::optional<iox::capro::IdString_t> event = eventStr;

    if (info.service == "") {
        //service = iox::capro::Wildcard;
        service = iox::optional<iox::capro::IdString_t>(iox::capro::Wildcard);

    }
    if (info.instance == "") {
        //instance = iox::capro::Wildcard;
        instance = iox::optional<iox::capro::IdString_t>(iox::capro::Wildcard);

    }
    if (info.event == "") {
        //event = iox::capro::Wildcard;
        event = iox::optional<iox::capro::IdString_t>(iox::capro::Wildcard);
    }
    
    std::vector<iox::capro::ServiceDescription> results;
    serviceDiscovery_.findService(service, instance, event,
        [&results](const iox::capro::ServiceDescription& serviceDescription) {
            results.push_back(serviceDescription);
        },
        iox::popo::MessagingPattern::REQ_RES
    );
    std::vector<ServiceMetaInfo> metainfos;
    for (const iox::capro::ServiceDescription& result : results) {
        ServiceMetaInfo metaInfo;
        metaInfo.service = result.getServiceIDString().c_str();
        metaInfo.instance = result.getInstanceIDString().c_str();
        metaInfo.event = result.getEventIDString().c_str();
        metainfos.push_back(metaInfo);
        // std::cout << "Found service: " << metaInfo.service 
        //           << " instance: " << metaInfo.instance << " event: " << metaInfo.event << std::endl;
    }
    return metainfos;
}

void IceoryxDiscovery::CreateServer(const ServiceMetaInfo& server) {
    // TODO: Implement logic to handle new servers here
}

void IceoryxDiscovery::DeleteServer(const ServiceMetaInfo& server) {

}

} // namespace discovery
} // namespace intercept