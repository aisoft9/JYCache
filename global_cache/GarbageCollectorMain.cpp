#include <vector>
#include <thread>
#include <atomic>
#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "S3DataAdaptor.h"
#include "FileSystemDataAdaptor.h"
#include "GlobalDataAdaptor.h"
#include "ReadCacheClient.h"

#include "GlobalCacheServer.h"
#include "S3DataAdaptor.h"

DEFINE_string(data_server, "0.0.0.0:8000", "IP address of global data servers");
DEFINE_string(etcd_server, "http://127.0.0.1:2379", "Location of etcd server");
DEFINE_string(prefix, "", "Key prefix for garbage collection");
DEFINE_bool(use_s3, false, "Use S3 storage");

std::vector<std::string> SplitString(const std::string &input) {
    std::vector<std::string> result;
    std::stringstream ss(input);
    std::string item;
    while (std::getline(ss, item, ',')) {
        result.push_back(item);
    }
    return result;
}

int main(int argc, char *argv[]) {
    std::cerr << YELLOW << "MADFS GC TOOL" << WHITE << std::endl;

    gflags::ParseCommandLineFlags(&argc, &argv, true);
    auto etcd_client = std::make_shared<EtcdClient>(FLAGS_etcd_server);
    std::shared_ptr<DataAdaptor> base_adaptor;
    if (FLAGS_use_s3) {
        base_adaptor = std::make_shared<S3DataAdaptor>();
    } else {
        base_adaptor = std::make_shared<FileSystemDataAdaptor>();
    }

    auto global_adaptor = std::make_shared<GlobalDataAdaptor>(base_adaptor, SplitString(FLAGS_data_server), etcd_client);
    if (global_adaptor->PerformGarbageCollection(FLAGS_prefix)) {
        std::cerr << RED << "Garbage collection failed!" << WHITE << std::endl;
        exit(EXIT_FAILURE);
    } else {
        std::cerr << GREEN << "Garbage collection successfully" << WHITE << std::endl;
        exit(EXIT_SUCCESS);
    }
}
