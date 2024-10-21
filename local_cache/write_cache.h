/*
 * Project: HybridCache
 * Created Date: 24-3-18
 * Author: lshb
 */
#ifndef HYBRIDCACHE_WRITE_CACHE_H_
#define HYBRIDCACHE_WRITE_CACHE_H_

#include "folly/concurrency/ConcurrentHashMap.h"

#include "page_cache.h"
//-----------tqy--------------
#include "throttle.h"

namespace HybridCache {

class WriteCache {
 public:
    // WriteCache(const WriteCacheConfig& cfg) : cfg_(cfg) { Init(); }
    //added by tqy
    WriteCache(const WriteCacheConfig& cfg, PoolId curr_id_ = NULL, std::shared_ptr<Cache> curr_cache_ = nullptr) : cfg_(cfg) 
    { 
        if(curr_cache_ == nullptr)
        {
            Init(); 
        }
        else
        {
            CombinedInit(curr_id_, curr_cache_);
        }
        //Throttle
        if(cfg_.EnableThrottle)
        {
            throttling_thread_ = std::thread(&WriteCache::Dealing_throttling, this);
            LOG(WARNING) <<"[WriteCache] USE_THROTTLING";
        }
        else
        {
            LOG(WARNING) <<"[WriteCache] NO USE_THROTTLING";
        }
    }
    //---------------
    WriteCache() = default;
    ~WriteCache() { Close(); }
    
    enum class LockType {
        NONE = 0,
        ALREADY_LOCKED = -1,
    };

    int Put(const std::string &key,
            size_t start,
            size_t len,
            const ByteBuffer &buffer
           );

    int Get(const std::string &key,
            size_t start,
            size_t len,
            ByteBuffer &buffer, 
            std::vector<std::pair<size_t, size_t>>& dataBoundary  // valid data segment boundar
           );
 
    // lock to ensure the availability of the returned buf
    // After being locked, it can be read and written, but cannot be deleted
    int GetAllCacheWithLock(const std::string &key,
            std::vector<std::pair<ByteBuffer, size_t>>& dataSegments  // ByteBuffer + off of key value(file)
                           );

    int Delete(const std::string &key, LockType type = LockType::NONE);

    int Truncate(const std::string &key, size_t len);
    
    void UnLock(const std::string &key);

    int GetAllKeys(std::map<std::string, time_t>& keys);

    void Close();

    size_t GetCacheSize();
    size_t GetCacheMaxSize();

 private:
    int Init();
    
    //added by tqy
    int CombinedInit(PoolId curr_id_, std::shared_ptr<Cache> curr_cache_);
    void Dealing_throttling();
    //added end
    
    void Lock(const std::string &key);

    std::string GetPageKey(const std::string &key, size_t pageIndex);

 private:
    WriteCacheConfig cfg_;
    std::shared_ptr<PageCache> pageCache_;
    folly::ConcurrentHashMap<std::string, time_t> keys_;  // <key, create_time>
    StringSkipList::Accessor keyLocks_ = StringSkipList::create(SKIP_LIST_HEIGHT);  // presence key indicates lock
//------------------tqy---------------
    HybridCache::Throttle throttling;
    std::thread throttling_thread_;//改成一个单独的线程
    std::atomic<bool> throttling_thread_running_{false};//调度线程是否启用
};

}  // namespace HybridCache

#endif // HYBRIDCACHE_WRITE_CACHE_H_
