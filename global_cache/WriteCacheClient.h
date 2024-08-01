#ifndef MADFS_WRITE_CACHE_CLIENT_H
#define MADFS_WRITE_CACHE_CLIENT_H

#include <folly/futures/Future.h>
#include <folly/futures/Promise.h>
#include <folly/executors/CPUThreadPoolExecutor.h>

#include "Common.h"
#include "Placement.h"
#include "data_adaptor.h"
#include "EtcdClient.h"

using HybridCache::ByteBuffer;

class GlobalDataAdaptor;

class WriteCacheClient {
public:
    struct PutResult {
        int status;
        Json::Value root;
    };

public:
    WriteCacheClient() {}

    ~WriteCacheClient() {}

    virtual folly::Future<PutResult> Put(const std::string &key,
                                   size_t size,
                                   const ByteBuffer &buffer,
                                   const std::map <std::string, std::string> &headers,
                                   size_t off = 0) = 0;

    virtual folly::Future<int> Get(const std::string &key,
                                   size_t start,
                                   size_t size,
                                   ByteBuffer &buffer, 
                                   Json::Value &root) = 0;
};

#endif // MADFS_WRITE_CACHE_CLIENT_H