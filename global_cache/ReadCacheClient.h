#ifndef MADFS_READ_CACHE_CLIENT_H
#define MADFS_READ_CACHE_CLIENT_H

#include <folly/futures/Future.h>
#include <folly/futures/Promise.h>
#include <folly/executors/CPUThreadPoolExecutor.h>

#include "Common.h"
#include "Placement.h"
#include "data_adaptor.h"

using HybridCache::ByteBuffer;

class GlobalDataAdaptor;

class ReadCacheClient {
    friend class GetChunkContext;

public:
    ReadCacheClient(GlobalDataAdaptor *parent);

    ~ReadCacheClient();

    virtual folly::Future<int> Get(const std::string &key,
                                   size_t start,
                                   size_t size,
                                   ByteBuffer &buffer);

    virtual folly::Future<int> Invalidate(const std::string &key, size_t size);

    // for testing only
public:
    struct GetChunkRequestV2 {
        std::string user_key;
        std::string internal_key;
        size_t chunk_id;
        size_t chunk_start;
        size_t chunk_len;
        size_t chunk_granularity;
        ByteBuffer buffer;
    };

    static void GenerateGetChunkRequestsV2(const std::string &key, 
                                           size_t start, 
                                           size_t size, 
                                           ByteBuffer &buffer, 
                                           std::vector<GetChunkRequestV2> &requests,
                                           size_t chunk_size);

    folly::Future<int> GetChunkAsync(int server_id, GetChunkRequestV2 context);

    folly::Future<int> GetChunkFromGlobalCache(int server_id, GetChunkRequestV2 context);

    std::vector<int> GetReplica(const std::string &key, int num_choose);

private:
    GlobalDataAdaptor *parent_;
};

#endif // MADFS_READ_CACHE_CLIENT_H