/*
 * Project: HybridCache
 * Created Date: 24-3-25
 * Author: lshb
 */

#ifndef HYBRIDCACHE_ACCESSOR_4_S3FS_H_
#define HYBRIDCACHE_ACCESSOR_4_S3FS_H_

#include <thread>

#include "accessor.h"

using atomic_ptr_t = std::shared_ptr<std::atomic<int>>;

class HybridCacheAccessor4S3fs : public HybridCache::HybridCacheAccessor {
 public:
    HybridCacheAccessor4S3fs(const HybridCache::HybridCacheConfig& cfg);
    ~HybridCacheAccessor4S3fs();

    void Init();
    void Stop();

    int Put(const std::string &key, size_t start, size_t len, const char* buf);

    int Get(const std::string &key, size_t start, size_t len, char* buf);

    int Flush(const std::string &key);

    int DeepFlush(const std::string &key);
    
    int Delete(const std::string &key);

    int Truncate(const std::string &key, size_t size);

    int Invalidate(const std::string &key);

    int Head(const std::string &key, size_t& size,
             std::map<std::string, std::string>& headers);
    
    // async full files flush in background
    int FsSync();

    bool UseGlobalCache();

    HybridCache::ThreadPool* GetExecutor() {
        return executor_.get();
    }

 private:
    void InitLog();
    bool IsWriteCacheFull(size_t len);
    uint32_t WriteCacheRatio();
    void BackGroundFlush();

 private:
    folly::ConcurrentHashMap<std::string, atomic_ptr_t> fileLock_;  // rwlock. write and flush are exclusive
    std::shared_ptr<HybridCache::ThreadPool> executor_;
    std::shared_ptr<folly::TokenBucket> tokenBucket_;  // upload flow limit
    std::atomic<bool> toStop_{false};
    std::atomic<bool> backFlushRunning_{false};
    std::thread bgFlushThread_;
};

#endif // HYBRIDCACHE_ACCESSOR_4_S3FS_H_
