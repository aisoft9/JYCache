#ifndef MADFS_GLOBAL_CACHE_SERVER_H
#define MADFS_GLOBAL_CACHE_SERVER_H

#include <gflags/gflags.h>
#include <butil/logging.h>
#include <brpc/server.h>
#include <folly/futures/Future.h>
#include <folly/futures/Promise.h>
#include <folly/executors/CPUThreadPoolExecutor.h>

#include "butil/time.h"
#include "bvar/bvar.h"

#include "gcache.pb.h"
#include "ReadCache.h"
#include "WriteCache.h"
#include "data_adaptor.h"

namespace gcache {
    class GlobalCacheServiceImpl : public GlobalCacheService {
    public:
        GlobalCacheServiceImpl(std::shared_ptr<folly::CPUThreadPoolExecutor> executor,
                               std::shared_ptr<DataAdaptor> base_adaptor);

        virtual ~GlobalCacheServiceImpl() {}

        virtual void GetEntryFromReadCache(google::protobuf::RpcController *cntl_base,
                                           const GetEntryRequest *request,
                                           GetEntryResponse *response,
                                           google::protobuf::Closure *done);

        virtual void PutEntryFromReadCache(google::protobuf::RpcController *cntl_base,
                                           const PutEntryRequest *request,
                                           PutEntryResponse *response,
                                           google::protobuf::Closure *done);

        virtual void DeleteEntryFromReadCache(google::protobuf::RpcController *cntl_base,
                                              const DeleteEntryRequest *request,
                                              DeleteEntryResponse *response,
                                              google::protobuf::Closure *done);

        virtual void GetEntryFromWriteCache(google::protobuf::RpcController *cntl_base,
                                            const GetEntryRequest *request,
                                            GetEntryResponse *response,
                                            google::protobuf::Closure *done);

        virtual void PutEntryFromWriteCache(google::protobuf::RpcController *cntl_base,
                                            const PutEntryRequest *request,
                                            PutEntryResponse *response,
                                            google::protobuf::Closure *done);

        virtual void DeleteEntryFromWriteCache(google::protobuf::RpcController *cntl_base,
                                               const DeleteEntryRequestForWriteCache *request,
                                               DeleteEntryResponse *response,
                                               google::protobuf::Closure *done);

        virtual void QueryTsFromWriteCache(google::protobuf::RpcController *cntl_base,
                                           const QueryTsRequest *request,
                                           QueryTsResponse *response,
                                           google::protobuf::Closure *done);

        virtual void Register(google::protobuf::RpcController *cntl_base,
                              const RegisterRequest *request,
                              RegisterResponse *response,
                              google::protobuf::Closure *done);

    private:
        std::shared_ptr<folly::CPUThreadPoolExecutor> executor_;
        std::shared_ptr<ReadCache> read_cache_;
        std::shared_ptr<WriteCache> write_cache_;
    };
}

#endif // MADFS_GLOBAL_CACHE_SERVER_H