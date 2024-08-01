#pragma once
#include "discovery.h"
#include "iceoryx_posh/runtime/service_discovery.hpp"

namespace intercept {
namespace discovery {

class IceoryxDiscovery : public Discovery
{
public:
    IceoryxDiscovery();

    virtual ~IceoryxDiscovery();

    virtual void Init();

    virtual void Start();

    virtual void Stop();

    virtual  std::vector<ServiceMetaInfo> GetServers() const;

    virtual std::vector<ServiceMetaInfo> GetNewServers(const std::vector<ServiceMetaInfo>& oldservers, 
                                                const std::vector<ServiceMetaInfo>& newservers);

    virtual std::set<ServiceMetaInfo> GetRemovedServers(
        const std::vector<ServiceMetaInfo>& oldservers, const std::vector<ServiceMetaInfo>& newservers);

    virtual std::vector<ServiceMetaInfo>  FindServices(const ServiceMetaInfo& info);

    virtual void CreateServer(const ServiceMetaInfo& serverInfo);

    virtual void DeleteServer(const ServiceMetaInfo& serverInfo);

private:
    iox::runtime::ServiceDiscovery serviceDiscovery_;
};

} // namespace discovery
} // namespace intercept

