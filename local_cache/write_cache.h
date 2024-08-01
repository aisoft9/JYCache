/*
 * Project: HybridCache
 * Created Date: 24-3-18
 * Author: lshb
 */
#ifndef HYBRIDCACHE_WRITE_CACHE_H_
#define HYBRIDCACHE_WRITE_CACHE_H_

#include "folly/concurrency/ConcurrentHashMap.h"

#include "page_cache.h"

namespace HybridCache {

class WriteCache {
 public:
    WriteCache(const WriteCacheConfig& cfg) : cfg_(cfg) { Init(); }
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
    
    void Lock(const std::string &key);

    std::string GetPageKey(const std::string &key, size_t pageIndex);

 private:
    WriteCacheConfig cfg_;
    std::shared_ptr<PageCache> pageCache_;
    folly::ConcurrentHashMap<std::string, time_t> keys_;  // <key, create_time>
    StringSkipList::Accessor keyLocks_ = StringSkipList::create(SKIP_LIST_HEIGHT);  // presence key indicates lock
};

}  // namespace HybridCache

#endif // HYBRIDCACHE_WRITE_CACHE_H_
