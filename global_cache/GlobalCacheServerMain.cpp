#include "GlobalCacheServer.h"
#include "S3DataAdaptor.h"

#include <folly/Singleton.h>

DEFINE_int32(port, 8000, "TCP Port of global cache server");
DEFINE_bool(fetch_s3_if_miss, false, "Allow fetch data from S3 if cache miss");

int main(int argc, char *argv[]) {
    LOG(INFO) << "MADFS Global Cache Server";
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    brpc::Server server;

    folly::SingletonVault::singleton()->registrationComplete();

    brpc::ServerOptions options;
    options.num_threads = GetGlobalConfig().rpc_threads;
    options.use_rdma = GetGlobalConfig().use_rdma;

    std::shared_ptr<S3DataAdaptor> base_adaptor = nullptr;
    if (FLAGS_fetch_s3_if_miss) {
        base_adaptor = std::make_shared<S3DataAdaptor>();
    }

    auto executor = std::make_shared<folly::CPUThreadPoolExecutor>(GetGlobalConfig().folly_threads);
    auto gcache_service = std::make_shared<gcache::GlobalCacheServiceImpl>(executor, base_adaptor);

    if (server.AddService(gcache_service.get(), brpc::SERVER_DOESNT_OWN_SERVICE)) {
        PLOG(ERROR) << "Failed to register global cache service";
        return -1;
    }

    butil::EndPoint point = butil::EndPoint(butil::IP_ANY, FLAGS_port);
    if (server.Start(point, &options) != 0) {
        PLOG(ERROR) << "Failed to start global cache server";
        return -1;
    }

    server.RunUntilAskedToQuit();
    return 0;
}
