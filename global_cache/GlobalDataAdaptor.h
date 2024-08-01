#ifndef MADFS_GLOBAL_DATA_ADAPTOR_H
#define MADFS_GLOBAL_DATA_ADAPTOR_H

#include <string>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/container/EvictingCacheMap.h>

#include "data_adaptor.h"
#include "EtcdClient.h"
#include "ReadCacheClient.h"
#include "WriteCacheClient.h"
#include "GlobalCacheClient.h"

#define NUM_WC_TYPES            2
#define WC_TYPE_REPLICATION     0
#define WC_TYPE_REEDSOLOMON     1

using HybridCache::ByteBuffer;
using HybridCache::DataAdaptor;

class GlobalDataAdaptor : public DataAdaptor {
    friend class ReadCacheClient;

    friend class ReplicationWriteCacheClient;
    friend class ErasureCodingWriteCacheClient;

public:
    GlobalDataAdaptor(std::shared_ptr<DataAdaptor> base_adaptor,
                      const std::vector<std::string> &server_list,
                      std::shared_ptr<EtcdClient> etcd_client = nullptr,
                      std::shared_ptr<folly::CPUThreadPoolExecutor> executor = nullptr);

    ~GlobalDataAdaptor();

    // 从数据服务器加载数据
    virtual folly::Future<int> DownLoad(const std::string &key,
                                        size_t start,
                                        size_t size,
                                        ByteBuffer &buffer);

    folly::Future<int> DownLoadFromGlobalCache(const std::string &key,
                                               size_t start,
                                               size_t size,
                                               ByteBuffer &buffer);

    // 上传数据到数据服务器
    virtual folly::Future<int> UpLoad(const std::string &key,
                                      size_t size,
                                      const ByteBuffer &buffer,
                                      const std::map <std::string, std::string> &headers);

    virtual folly::Future<int> DoUpLoad(const std::string &key,
                                        size_t size,
                                        const ByteBuffer &buffer,
                                        const std::map <std::string, std::string> &headers);

    virtual folly::Future<int> UpLoadPart(const std::string &key,
                                          size_t off,
                                          size_t size,
                                          const ByteBuffer &buffer,
                                          const std::map<std::string, std::string> &headers,
                                          Json::Value& root);

    virtual folly::Future<int> DoUpLoadPart(const std::string &key,
                                            size_t off,
                                            size_t size,
                                            const ByteBuffer &buffer,
                                            const std::map<std::string, std::string> &headers,
                                            Json::Value& root);

    virtual folly::Future<int> Completed(const std::string &key,
                                         const std::vector<Json::Value> &roots,
                                         size_t size);

    // 删除数据服务器的数据
    virtual folly::Future<int> Delete(const std::string &key);

    // 数据源flush到S3(全局缓存用)
    virtual folly::Future<int> DeepFlush(const std::string &key);

    // 获取数据的元数据
    virtual folly::Future<int> Head(const std::string &key,
                                    size_t &size,
                                    std::map <std::string, std::string> &headers);
    
    int PerformGarbageCollection(const std::string &prefix = "");
    
    void SetCachePolicy(const std::string &key, CachePolicy &policy);

public:
    struct MetaCacheEntry {
        MetaCacheEntry(const std::string &key) : key(key), present(false) {}

        const std::string key;
        bool present;           // 只有设为 true，这个缓存才有效
        bool existed;           // key 目前是存在的
        bool write_cached;      // key 的数据位于全局写缓存
        size_t size;
        std::map<std::string, std::string> headers;
        Json::Value root; 
    };

    void InvalidateMetaCache();

    void InvalidateMetaCacheEntry(const std::string &key);

    std::shared_ptr<MetaCacheEntry> GetMetaCacheEntry(const std::string &key);

    const CachePolicy &GetCachePolicy(const std::string &key) const;

    std::shared_ptr<GlobalCacheClient> GetRpcClient() const;

    const std::string GetServerHostname(int server_id) const {
        if (server_id >= 0 && server_id < server_list_.size())
            return server_list_[server_id];
        return "<invalid>";
    };

    void BackgroundWorker();

private:
    std::shared_ptr<folly::CPUThreadPoolExecutor> executor_;

    std::shared_ptr<ReadCacheClient> read_cache_;
    std::shared_ptr<WriteCacheClient> write_caches_[NUM_WC_TYPES];

    std::shared_ptr<DataAdaptor> base_adaptor_;

    std::vector<std::shared_ptr<GlobalCacheClient>> rpc_client_;
    std::shared_ptr<EtcdClient> etcd_client_;
    std::vector<std::string> server_list_;

    std::mutex meta_cache_mutex_;
    folly::EvictingCacheMap<std::string, std::shared_ptr<MetaCacheEntry>> meta_cache_;

    std::atomic<bool> bg_running_;
    std::thread bg_thread_;
    std::mutex bg_mutex_;
    std::condition_variable bg_cv_;
    std::vector<std::function<int()>> bg_tasks_;
};

#endif // MADFS_GLOBAL_DATA_ADAPTOR_H