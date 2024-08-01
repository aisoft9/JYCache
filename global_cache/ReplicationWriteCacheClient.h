#ifndef MADFS_REPLICATION_WRITE_CACHE_CLIENT_H
#define MADFS_REPLICATION_WRITE_CACHE_CLIENT_H

#include "WriteCacheClient.h"

using HybridCache::ByteBuffer;

class GlobalDataAdaptor;

using PutResult = WriteCacheClient::PutResult;

class ReplicationWriteCacheClient : public WriteCacheClient {
    friend class GetChunkContext;

public:
    ReplicationWriteCacheClient(GlobalDataAdaptor *parent) : parent_(parent) {}

    ~ReplicationWriteCacheClient() {}

    virtual folly::Future<PutResult> Put(const std::string &key,
                                           size_t size,
                                           const ByteBuffer &buffer,
                                           const std::map <std::string, std::string> &headers,
                                           size_t off = 0);

    virtual folly::Future<int> Get(const std::string &key,
                                   size_t start,
                                   size_t size,
                                   ByteBuffer &buffer, 
                                   Json::Value &root);

public:
    std::vector<int> GetReplica(const std::string &key);

    struct GetChunkRequestV2 {
        std::string user_key;
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

    folly::Future<int> GetChunkAsync(int server_id, GetChunkRequestV2 context, std::string &internal_key);

private:
    GlobalDataAdaptor *parent_;
};

#endif // MADFS_REPLICATION_WRITE_CACHE_CLIENT_H