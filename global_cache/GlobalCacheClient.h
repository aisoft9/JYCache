#ifndef MADFS_GLOBAL_CACHE_CLIENT_H
#define MADFS_GLOBAL_CACHE_CLIENT_H

#include <brpc/channel.h>
#include <atomic>
#include <string>
#include <mutex>
#include <map>

#include "Common.h"
#include "common.h"

using HybridCache::ByteBuffer;

class GlobalCacheClient {
public:
    GlobalCacheClient(const std::string &group = "");

    ~GlobalCacheClient();

    int RegisterServer(int server_id, const char *hostname);

    Future<GetOutput> GetEntryFromReadCache(int server_id, const std::string &key, uint64_t start, uint64_t length) {
        return GetEntry(server_id, key, start, length, true);
    }

    Future<PutOutput> PutEntryFromReadCache(int server_id, const std::string &key, const ByteBuffer &buf, uint64_t length) {
        return PutEntry(server_id, key, buf, length, true);
    }

    Future<int> DeleteEntryFromReadCache(int server_id, const std::string &key, uint64_t chunk_size, uint64_t max_chunk_id);

    Future<GetOutput> GetEntryFromWriteCache(int server_id, const std::string &key, uint64_t start, uint64_t length){
        return GetEntry(server_id, key, start, length, false);
    }

    Future<PutOutput> PutEntryFromWriteCache(int server_id, const std::string &key, const ByteBuffer &buf, uint64_t length){
        return PutEntry(server_id, key, buf, length, false);
    }

    Future<QueryTsOutput> QueryTsFromWriteCache(int server_id);

    Future<int> DeleteEntryFromWriteCache(int server_id, 
                                          const std::string &key_prefix, 
                                          uint64_t max_ts, 
                                          std::vector<std::string> &except_keys);

private:
    brpc::Channel *GetChannelByServerId(int server_id);

    Future<GetOutput> GetEntry(int server_id, const std::string &key, uint64_t start, uint64_t length, bool is_read_cache);
    
    Future<PutOutput> PutEntry(int server_id, const std::string &key, const ByteBuffer &buf, uint64_t length, bool is_read_cache);

private:
    std::mutex mutex_;
    const std::string group_;
    std::map<int, brpc::Channel *> server_map_;
    std::atomic<uint64_t> inflight_payload_size_;
};

#endif // MADFS_GLOBAL_CACHE_CLIENT_H