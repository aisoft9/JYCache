/*
 * Project: HybridCache
 * Created Date: 24-2-29
 * Author: lshb
 */
#ifndef HYBRIDCACHE_READ_CACHE_H_
#define HYBRIDCACHE_READ_CACHE_H_

#include "folly/TokenBucket.h"

#include "page_cache.h"
#include "data_adaptor.h"

namespace HybridCache {

class ReadCache {
 public:
    ReadCache(const ReadCacheConfig& cfg,
              std::shared_ptr<DataAdaptor> dataAdaptor,
              std::shared_ptr<ThreadPool> executor,   //below added by tqy
              PoolId curr_id_ = NULL, 
              std::shared_ptr<Cache> curr_cache_ = nullptr);
    ReadCache() = default;
    ~ReadCache() { Close(); }

    // Read the local page cache first, and get it from the DataAdaptor if it misses
    folly::Future<int> Get(const std::string &key,
                           size_t start,
                           size_t len,
                           ByteBuffer &buffer // user buf
                          );

    int Put(const std::string &key,
            size_t start,
            size_t len,
            const ByteBuffer &buffer);

    int Delete(const std::string &key);

    int GetAllKeys(std::set<std::string>& keys);

    void Close();

 private:
    int Init();
    //added by tqy
    int CombinedInit(PoolId curr_id_, std::shared_ptr<Cache> curr_cache_);
    //added end

    std::string GetPageKey(const std::string &key, size_t pageIndex);

 private:
    ReadCacheConfig cfg_;
    std::shared_ptr<PageCache> pageCache_;
    std::shared_ptr<DataAdaptor> dataAdaptor_;
    std::shared_ptr<ThreadPool> executor_;
    std::shared_ptr<folly::TokenBucket> tokenBucket_;  // download flow limit
};

}  // namespace HybridCache

#endif // HYBRIDCACHE_READ_CACHE_H_
