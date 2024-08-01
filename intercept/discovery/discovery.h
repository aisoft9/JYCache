#pragma once
#include <iostream>
#include <vector>
#include <set>
#include <thread>

#include "internal/metainfo.h"

namespace intercept {
namespace discovery {

using intercept::internal::ServiceMetaInfo;



// Discovery : use to discover the existing servers
// and the servers to be deleted
class Discovery {
public:
    // Constructor
    Discovery() {
        // Initialization code
    }

    // Initialize the discovery
    virtual void Init() = 0;

    // Start the discovery loop
    virtual void Start() = 0;

    // Stop the discovery loop
    virtual void Stop() = 0;

    // Get the existing servers
    virtual std::vector<ServiceMetaInfo> GetServers() const {
        // Return the existing servers
        return std::vector<ServiceMetaInfo>();
    }

    // Get the servers to be deleted
    virtual std::set<ServiceMetaInfo> GetServersToDelete() const {
        // Return the servers to be deleted
        return std::set<ServiceMetaInfo>();
    }

    virtual std::vector<ServiceMetaInfo>  FindServices(const ServiceMetaInfo& info) = 0;

    // Create a new server
    virtual void CreateServer(const ServiceMetaInfo& serverInfo) {
        // Create a new server using the serverInfo
    }

    // Delete a server
    virtual void DeleteServer(const ServiceMetaInfo& serverInfo) {
        // Delete a server using the serverInfo
    }

protected:
    std::vector<ServiceMetaInfo> existingServers;
    std::set<ServiceMetaInfo> serversToDelete;
    bool DISCOVERY_RUNNING;
};

}
}

