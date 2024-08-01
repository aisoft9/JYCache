/*
 * Project: HybridCache
 * Created Date: 24-3-25
 * Author: lshb
 */
#ifndef HYBRIDCACHE_ACCESSOR_H_
#define HYBRIDCACHE_ACCESSOR_H_

#include "read_cache.h"
#include "write_cache.h"

namespace HybridCache {

class HybridCacheAccessor {
 public:
    HybridCacheAccessor(const HybridCacheConfig& cfg) : cfg_(cfg) {}
    ~HybridCacheAccessor() {}

    // Put in write cache.
    // If the write cache is full, block waiting for asynchronous flush to release the write cache space
    virtual int Put(const std::string &key, size_t start, size_t len, const char* buf) = 0;
    
    // 1.Read from write cache. 2.Read from read cache.
    virtual int Get(const std::string &key, size_t start, size_t len, char* buf) = 0;

    // Get4ReadHandle();
    
    // File flush. Need to handle flush/write concurrency.
    virtual int Flush(const std::string &key) = 0;
    
    // Flush to the final data source, such as global cache to s3.
    virtual int DeepFlush(const std::string &key) = 0;
    
    virtual int Delete(const std::string &key) = 0;

    // Invalidated the local read cache.
    // Delete read cache when open the file. That is a configuration item.
    virtual int Invalidate(const std::string &key) = 0;
    
    // Background asynchronous flush all files and releases write cache space.
    virtual int FsSync() = 0;

 protected:
    HybridCacheConfig cfg_;
    std::shared_ptr<WriteCache> writeCache_;
    std::shared_ptr<ReadCache> readCache_;
    std::shared_ptr<DataAdaptor> dataAdaptor_;
};

}  // namespace HybridCache

#endif // HYBRIDCACHE_ACCESSOR_H_
