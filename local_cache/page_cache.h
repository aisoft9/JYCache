/*
 * Project: HybridCache
 * Created Date: 24-2-21
 * Author: lshb
 */
#ifndef HYBRIDCACHE_PAGE_CACHE_H_
#define HYBRIDCACHE_PAGE_CACHE_H_

#include <string>
#include <set>

#include "folly/ConcurrentSkipList.h"
#include "cachelib/allocator/CacheAllocator.h"

#include "common.h"
#include "config.h"

namespace HybridCache {

typedef folly::ConcurrentSkipList<std::string> StringSkipList;
using facebook::cachelib::PoolId;
using Cache = facebook::cachelib::LruAllocator;

enum class MetaPos {
    LOCK = 0,
    LASTVER,
    NEWVER,
    FAST_BITMAP
};

class PageCache {
 public:
    PageCache(const CacheConfig& cfg): cfg_(cfg) {}
    virtual ~PageCache() {}

    virtual int Init() = 0;
    virtual int Close() = 0;

    virtual int Write(const std::string &key,  // page key
                      uint32_t pagePos,
                      uint32_t length,
                      const char *buf  // user buf
                     ) = 0;
    
    virtual int Read(const std::string &key,
                     uint32_t pagePos,
                     uint32_t length,
                     char *buf,  // user buf
                     std::vector<std::pair<size_t, size_t>>& dataBoundary  // valid data segment boundar
                    ) = 0;

    // upper layer need to guarantee that the page will not be delete
    virtual int GetAllCache(const std::string &key,
                    std::vector<std::pair<ByteBuffer, size_t>>& dataSegments  // <ByteBuffer(buf+len), pageOff>
                           ) = 0;

    // delete part data from page
    // if the whole page is empty then delete that page
    virtual int DeletePart(const std::string &key,
                           uint32_t pagePos,
                           uint32_t length
                          ) = 0;

    virtual int Delete(const std::string &key) = 0;
    
    virtual size_t GetCacheSize() = 0;
    virtual size_t GetCacheMaxSize() = 0;

    const folly::ConcurrentSkipList<std::string>::Accessor& GetPageList() {
        return this->pagesList_;
    }

 protected:
    // CAS operate
    bool Lock(char* pageMemory);
    void UnLock(char* pageMemory);
    uint8_t AddNewVer(char* pageMemory);
    void SetLastVer(char* pageMemory, uint8_t newVer);
    uint8_t GetLastVer(const char* pageMemory);
    uint8_t GetNewVer(const char* pageMemory);

    // bitmap operate
    void SetFastBitmap(char* pageMemory, bool valid);
    bool GetFastBitmap(const char* pageMemory);
    void SetBitMap(char* pageMemory, int pos, int len, bool valid);
    void SetBit(char *x, int n) { *x |= (1 << n); }
    void ClearBit(char *x, int n) { *x &= ~ (1 << n); }
    bool GetBit(const char *x, int n) { return *x & (1 << n); }

 protected:
    StringSkipList::Accessor pagesList_ = StringSkipList::create(SKIP_LIST_HEIGHT);
    CacheConfig cfg_;
};

class PageCacheImpl : public PageCache {
 public:
    PageCacheImpl(const CacheConfig& cfg): PageCache(cfg) {
        bitmapSize_ = cfg_.PageBodySize / BYTE_LEN;
        LOG(WARNING) << "[TestOutPut] PageCache Init with size : "<<GetCacheSize();
    }
    //added by tqy
    PageCacheImpl(const CacheConfig& cfg, PoolId curr_pool_id_, std::shared_ptr<Cache> curr_cache_):PageCache(cfg)
    {
        bitmapSize_ = cfg_.PageBodySize / BYTE_LEN;
        cache_ = curr_cache_;
        pool_ = curr_pool_id_;
    }
    

    //added end
    ~PageCacheImpl() {}

    int Init();

    int Close();

    int Write(const std::string &key,
              uint32_t pagePos,
              uint32_t length,
              const char *buf
             );
    
    int Read(const std::string &key,
             uint32_t pagePos,
             uint32_t length,
             char *buf,
             std::vector<std::pair<size_t, size_t>>& dataBoundary
            );

    int GetAllCache(const std::string &key,
                    std::vector<std::pair<ByteBuffer, size_t>>& dataSegments
                   );

    int DeletePart(const std::string &key,
                   uint32_t pagePos,
                   uint32_t length
                  );

    int Delete(const std::string &key);

    size_t GetCacheSize() {
        return GetPageNum() * GetRealPageSize();
    }
    size_t GetCacheMaxSize() {
        if (!cfg_.CacheLibCfg.EnableNvmCache)
            return cfg_.MaxCacheSize;
        size_t nvmMaxSize = cfg_.CacheLibCfg.RaidFileNum *
                            cfg_.CacheLibCfg.RaidFileSize;
        return cfg_.MaxCacheSize + nvmMaxSize;
    }

 private:
    uint64_t GetPageNum() {
        return pageNum_.load();
    }

    uint32_t GetRealPageSize() {
        return cfg_.PageMetaSize + bitmapSize_ + cfg_.PageBodySize;
    }

    Cache::WriteHandle FindOrCreateWriteHandle(const std::string &key);
    

 private:
    std::shared_ptr<Cache> cache_;
    PoolId pool_;
    std::atomic<uint64_t> pageNum_{0};
    uint32_t bitmapSize_;
};

}  // namespace HybridCache

#endif // HYBRIDCACHE_PAGE_CACHE_H_
