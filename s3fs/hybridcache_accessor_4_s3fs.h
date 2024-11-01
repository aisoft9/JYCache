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

// added by tqy referring to xyq
using Cache = facebook::cachelib::LruAllocator;
using facebook::cachelib::PoolId;
#define MAXSIZE 1024
#define IP_ADDR "127.0.0.1"
#define IP_PORT 2333  // 服务器端口
const int64_t FIX_SIZE = 1024 * 1024 * 256;  // 256M为分配单位
const int64_t RESERVE_SIZE = 1024 * 1024 * 512;  // 512M保留空间

class HybridCacheAccessor4S3fs : public HybridCache::HybridCacheAccessor {
 public:
    HybridCacheAccessor4S3fs(const HybridCache::HybridCacheConfig& cfg);
    ~HybridCacheAccessor4S3fs();

    void Init();

    // added by tqy referring to xyq
    int InitCache();
    void LinUCBClient();

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
    bool IsWritePoolFull(size_t len);
    uint32_t WritePoolRatio();
    void BackGroundFlush();

 private:
    folly::ConcurrentHashMap<std::string, atomic_ptr_t> fileLock_;  // rwlock. write and flush are exclusive
    std::shared_ptr<HybridCache::ThreadPool> executor_;
    std::shared_ptr<folly::TokenBucket> tokenBucket_;  // upload flow limit
    std::atomic<bool> toStop_{false};
    std::atomic<bool> backFlushRunning_{false};
    std::thread bgFlushThread_;

    // added by tqy referring to xyq for Resizing
    std::shared_ptr<Cache> ResizeWriteCache_;
    std::shared_ptr<Cache> ResizeReadCache_;
    PoolId writePoolId_;
    PoolId readPoolId_;
    uint64_t writeCount_ = 0;
    uint64_t readCount_ = 0;
    uint64_t writeCacheSize_;
    uint64_t readCacheSize_;
    
    // added by tqy referring to xyq for LinUCB
    std::thread LinUCBThread_;
    std::atomic<bool> stopLinUCBThread_{false};
    uint64_t writeByteAcc_ = 0;
    uint64_t readByteAcc_ = 0;
    uint64_t writeTimeAcc_ = 0;
    uint64_t readTimeAcc_ = 0;
    uint32_t resizeInterval_;
};

#endif // HYBRIDCACHE_ACCESSOR_4_S3FS_H_
