#include "glog/logging.h"

#include "common.h"
#include "errorcode.h"
#include "page_cache.h"

namespace HybridCache {

bool PageCache::Lock(char* pageMemory) {
   if (!cfg_.EnableCAS) return true;
   uint8_t* lock = reinterpret_cast<uint8_t*>(pageMemory + int(MetaPos::LOCK));
   uint8_t lockExpected = 0;
   return __atomic_compare_exchange_n(lock, &lockExpected, 1, true,
          __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

void PageCache::UnLock(char* pageMemory) {
   if (!cfg_.EnableCAS) return;
   uint8_t* lock = reinterpret_cast<uint8_t*>(pageMemory + int(MetaPos::LOCK));
   __atomic_store_n(lock, 0, __ATOMIC_SEQ_CST);
}

uint8_t PageCache::AddNewVer(char* pageMemory) {
    if (!cfg_.EnableCAS) return 0;
    uint8_t* newVer = reinterpret_cast<uint8_t*>(pageMemory + int(MetaPos::NEWVER));
    return __atomic_add_fetch(newVer, 1, __ATOMIC_SEQ_CST);
}

void PageCache::SetLastVer(char* pageMemory, uint8_t newVer) {
   if (!cfg_.EnableCAS) return;
   uint8_t* lastVer = reinterpret_cast<uint8_t*>(pageMemory + int(MetaPos::LASTVER));
   __atomic_store_n(lastVer, newVer, __ATOMIC_SEQ_CST);
}

uint8_t PageCache::GetLastVer(const char* pageMemory) {
    if (!cfg_.EnableCAS) return 0;
    const uint8_t* lastVer = reinterpret_cast<const uint8_t*>(pageMemory + int(MetaPos::LASTVER));
    return __atomic_load_n(lastVer, __ATOMIC_SEQ_CST);
}

uint8_t PageCache::GetNewVer(const char* pageMemory) {
    if (!cfg_.EnableCAS) return 0;
    const uint8_t* newVer = reinterpret_cast<const uint8_t*>(pageMemory + int(MetaPos::NEWVER));
    return __atomic_load_n(newVer, __ATOMIC_SEQ_CST);
}

void PageCache::SetFastBitmap(char* pageMemory, bool valid) {
    uint8_t* fastBitmap = reinterpret_cast<uint8_t*>(pageMemory + int(MetaPos::FAST_BITMAP));
    if (valid) *fastBitmap = 1;
    else *fastBitmap = 0;
}

bool PageCache::GetFastBitmap(const char* pageMemory) {
    const uint8_t* fastBitmap = reinterpret_cast<const uint8_t*>(pageMemory + int(MetaPos::FAST_BITMAP));
    return *fastBitmap == 1;
}

void PageCache::SetBitMap(char* pageMemory, int pos, int len, bool valid) {
    if (len == cfg_.PageBodySize && valid)
        SetFastBitmap(pageMemory, valid);
    if (!valid)
        SetFastBitmap(pageMemory, valid);

    char* x = pageMemory + cfg_.PageMetaSize;
    uint32_t startByte = pos / BYTE_LEN;
    // head byte
    if (pos % BYTE_LEN > 0) {
        int headByteSetLen = BYTE_LEN - pos % BYTE_LEN;
        headByteSetLen = headByteSetLen > len ? len : headByteSetLen;
        len -= headByteSetLen;
        while (headByteSetLen) {
            if (valid)
                SetBit(x+startByte, pos%BYTE_LEN+(--headByteSetLen));
            else
                ClearBit(x+startByte, pos%BYTE_LEN+(--headByteSetLen));
        }
        ++startByte;
    }
    // mid bytes
    int midLen = len / BYTE_LEN;
    if (midLen > 0) {
        if (valid)
            memset(x+startByte, UINT8_MAX, midLen);
        else
            memset(x+startByte, 0, midLen);
        len -= BYTE_LEN * midLen;
        startByte += midLen;
    }
    // tail byte
    while (len > 0) {
        if (valid)
            SetBit(x+startByte, --len);
        else
            ClearBit(x+startByte, --len);
    }
}

int PageCacheImpl::Init() {
    const unsigned bucketsPower = 25;
    const unsigned locksPower = 15;

    Cache::Config config;
    config
        .setCacheSize(cfg_.MaxCacheSize)
        .setCacheName(cfg_.CacheName)
        .setAccessConfig({bucketsPower, locksPower})
        .enableItemReaperInBackground(std::chrono::milliseconds{0})
        .validate();
    if (cfg_.CacheLibCfg.EnableNvmCache) {
        Cache::NvmCacheConfig nvmConfig;
        std::vector<std::string> raidPaths;
        for (int i=0; i<cfg_.CacheLibCfg.RaidFileNum; ++i) {
            raidPaths.push_back(cfg_.CacheLibCfg.RaidPath + std::to_string(i));
        }
        nvmConfig.navyConfig.setRaidFiles(raidPaths,
                cfg_.CacheLibCfg.RaidFileSize, false);

        nvmConfig.navyConfig.blockCache()
            .setDataChecksum(cfg_.CacheLibCfg.DataChecksum);

        config.enableNvmCache(nvmConfig).validate();
    }
    cache_ = std::make_shared<Cache>(config);
    pool_ = cache_->addPool(cfg_.CacheName + "_pool",
                            cache_->getCacheMemoryStats().ramCacheSize);
    LOG(WARNING) << "[PageCache]Init, name:" << config.getCacheName()
                 << ", size:" << config.getCacheSize()
                 << ", dir:" << config.getCacheDir();
    return SUCCESS;
}

int PageCacheImpl::Close() {
    if (cache_)
        cache_.reset();
    LOG(WARNING) << "[PageCache]Close, name:" << cfg_.CacheName;
    return SUCCESS;
}

int PageCacheImpl::Write(const std::string &key,
                         uint32_t pagePos,
                         uint32_t length,
                         const char *buf) {
    assert(cfg_.PageBodySize >= pagePos + length);
    assert(cache_);

    Cache::WriteHandle writeHandle = nullptr;
    char* pageValue = nullptr;
    while (true) {
        writeHandle = std::move(FindOrCreateWriteHandle(key));
        pageValue = reinterpret_cast<char*>(writeHandle->getMemory());
        if (Lock(pageValue)) break;
    }

    uint64_t realOffset = cfg_.PageMetaSize + bitmapSize_ + pagePos;
    uint8_t newVer = AddNewVer(pageValue);
    std::memcpy(pageValue + realOffset, buf, length);
    SetBitMap(pageValue, pagePos, length, true);
    SetLastVer(pageValue, newVer);
    UnLock(pageValue);
    return SUCCESS;
}

int PageCacheImpl::Read(const std::string &key,
                        uint32_t pagePos,
                        uint32_t length,
                        char *buf,
                    std::vector<std::pair<size_t, size_t>>& dataBoundary) {
    assert(cfg_.PageBodySize >= pagePos + length);
    assert(cache_);

    int res = SUCCESS;
    while (true) {
        auto readHandle = cache_->find(key);
        if (!readHandle) {
            res = PAGE_NOT_FOUND;
            break;
        }
        while (!readHandle.isReady());

        const char* pageValue = reinterpret_cast<const char*>(
                readHandle->getMemory());
        uint8_t lastVer = GetLastVer(pageValue);
        uint8_t newVer = GetNewVer(pageValue);
        if (lastVer != newVer) continue;

        dataBoundary.clear();
        uint32_t cur = pagePos;
        if (GetFastBitmap(pageValue)) {
            uint32_t pageOff = cfg_.PageMetaSize + bitmapSize_ + pagePos;
            std::memcpy(buf, pageValue + pageOff, length);
            dataBoundary.push_back(std::make_pair(0, length));
            cur += length;
        }

        bool continuousDataValid = false;  // continuous Data valid or invalid
        uint32_t continuousLen = 0;
        while (cur < pagePos+length) {
            const char *byte = pageValue + cfg_.PageMetaSize + cur / BYTE_LEN;

            // fast to judge full byte of bitmap
            uint16_t batLen = 0;
            bool batByteValid = false, isBatFuncValid = false;

            batLen = 64;
            if (cur % batLen == 0 && (pagePos+length-cur) >= batLen) {
                uint64_t byteValue = *reinterpret_cast<const uint64_t*>(byte);
                if (byteValue == UINT64_MAX) {
                    batByteValid = true;
                    isBatFuncValid = true;
                } else if (byteValue == 0)  {
                    isBatFuncValid = true;
                }
            }

            if (isBatFuncValid && (continuousLen == 0 ||
                                continuousDataValid == batByteValid)) {
                continuousDataValid = batByteValid;
                continuousLen += batLen;
                cur += batLen;
                continue;
            }

            bool curByteValid = GetBit(byte, cur % BYTE_LEN);
            if (continuousLen == 0 || continuousDataValid == curByteValid) {
                continuousDataValid = curByteValid;
                ++continuousLen;
                ++cur;
                continue;
            }

            if (continuousDataValid) {
                uint32_t bufOff = cur - continuousLen - pagePos;
                uint32_t pageOff = cfg_.PageMetaSize + bitmapSize_ +
                                   cur - continuousLen;
                std::memcpy(buf + bufOff, pageValue + pageOff, continuousLen);
                dataBoundary.push_back(std::make_pair(bufOff, continuousLen));
            }

            continuousDataValid = curByteValid;
            continuousLen = 1;
            ++cur;
        }
        if (continuousDataValid) {
            uint32_t bufOff = cur - continuousLen - pagePos;
            uint32_t pageOff = cfg_.PageMetaSize + bitmapSize_ +
                               cur - continuousLen;
            std::memcpy(buf + bufOff, pageValue + pageOff, continuousLen);
            dataBoundary.push_back(std::make_pair(bufOff, continuousLen));
        }

        newVer = GetNewVer(pageValue);
        if (lastVer == newVer) break;
    }
    return res;
}

int PageCacheImpl::GetAllCache(const std::string &key,
                    std::vector<std::pair<ByteBuffer, size_t>>& dataSegments) {
    assert(cache_);
    uint32_t pageSize = cfg_.PageBodySize;

    int res = SUCCESS;
    while (true) {
        auto readHandle = cache_->find(key);
        if (!readHandle) {
            res = PAGE_NOT_FOUND;
            break;
        }
        while (!readHandle.isReady());

        const char* pageValue = reinterpret_cast<const char*>(
                readHandle->getMemory());
        uint8_t lastVer = GetLastVer(pageValue);
        uint8_t newVer = GetNewVer(pageValue);
        if (lastVer != newVer) continue;

        dataSegments.clear();
        uint32_t cur = 0;
        if (GetFastBitmap(pageValue)) {
            uint32_t pageOff = cfg_.PageMetaSize + bitmapSize_;
            dataSegments.push_back(std::make_pair(
                ByteBuffer(const_cast<char*>(pageValue + pageOff), pageSize), 0));
            cur += pageSize;
        }

        bool continuousDataValid = false;  // continuous Data valid or invalid
        uint32_t continuousLen = 0;
        while (cur < pageSize) {
            const char *byte = pageValue + cfg_.PageMetaSize + cur / BYTE_LEN;

            // fast to judge full byte of bitmap
            uint16_t batLen = 0;
            bool batByteValid = false, isBatFuncValid = false;

            batLen = 64;
            if (cur % batLen == 0 && (pageSize-cur) >= batLen) {
                uint64_t byteValue = *reinterpret_cast<const uint64_t*>(byte);
                if (byteValue == UINT64_MAX) {
                    batByteValid = true;
                    isBatFuncValid = true;
                } else if (byteValue == 0)  {
                    isBatFuncValid = true;
                }
            }

            if (isBatFuncValid && (continuousLen == 0 ||
                                continuousDataValid == batByteValid)) {
                continuousDataValid = batByteValid;
                continuousLen += batLen;
                cur += batLen;
                continue;
            }

            bool curByteValid = GetBit(byte, cur % BYTE_LEN);
            if (continuousLen == 0 || continuousDataValid == curByteValid) {
                continuousDataValid = curByteValid;
                ++continuousLen;
                ++cur;
                continue;
            }

            if (continuousDataValid) {
                uint32_t pageOff = cfg_.PageMetaSize + bitmapSize_ +
                                   cur - continuousLen;
                dataSegments.push_back(std::make_pair(
                    ByteBuffer(const_cast<char*>(pageValue + pageOff), continuousLen),
                    cur - continuousLen));
            }

            continuousDataValid = curByteValid;
            continuousLen = 1;
            ++cur;
        }
        if (continuousDataValid) {
            uint32_t pageOff = cfg_.PageMetaSize + bitmapSize_ +
                               cur - continuousLen;
            dataSegments.push_back(std::make_pair(
                ByteBuffer(const_cast<char*>(pageValue + pageOff), continuousLen),
                cur - continuousLen));
        }

        newVer = GetNewVer(pageValue);
        if (lastVer == newVer) break;
    }
    return res;
}

int PageCacheImpl::DeletePart(const std::string &key,
                              uint32_t pagePos,
                              uint32_t length) {
    assert(cfg_.PageBodySize >= pagePos + length);
    assert(cache_);

    int res = SUCCESS;
    Cache::WriteHandle writeHandle = nullptr;
    char* pageValue = nullptr;
    while (true) {
        writeHandle = cache_->findToWrite(key);
        if (!writeHandle) {
            res = PAGE_NOT_FOUND;
            break;
        }
        pageValue = reinterpret_cast<char*>(writeHandle->getMemory());
        if (Lock(pageValue)) break;
    }

    if (SUCCESS == res) {
        uint8_t newVer = AddNewVer(pageValue);
        SetBitMap(pageValue, pagePos, length, false);

        bool isEmpty = true;
        uint32_t pos = 0;
        while (pos < bitmapSize_) {
            if (*(pageValue + cfg_.PageMetaSize + pos) != 0) {
                isEmpty = false;
                break;
            }
            ++pos;
        }

        bool isDel = false;
        if (isEmpty) {
            if (cfg_.SafeMode) {  // exclusive lock
                while(true) {
                    int64_t expected = 0;
                    if (lock_.compare_exchange_weak(expected, -1))
                        break;
                }
            }
            auto rr = cache_->remove(writeHandle);
            if (cfg_.SafeMode) lock_.store(0);  // release exclusive lock
            if (rr == Cache::RemoveRes::kSuccess) {
                pageNum_.fetch_sub(1);
                pagesList_.erase(key);
                isDel = true;
            } else {
                res = PAGE_DEL_FAIL;
            }
        }

        if (!isDel) {
            SetLastVer(pageValue, newVer);
            UnLock(pageValue);
        }
    }
    return res;
}

int PageCacheImpl::Delete(const std::string &key) {
    assert(cache_);
    if (cfg_.SafeMode) {  // exclusive lock
        while(true) {
            int64_t expected = 0;
            if (lock_.compare_exchange_weak(expected, -1))
                break;
        }
    }
    int res = cache_->remove(key) == Cache::RemoveRes::kSuccess ? SUCCESS : PAGE_NOT_FOUND;
    if (cfg_.SafeMode) lock_.store(0);  // release exclusive lock
    if (SUCCESS == res) {
        pageNum_.fetch_sub(1);
        pagesList_.erase(key);
    }
    return res;
}

Cache::WriteHandle PageCacheImpl::FindOrCreateWriteHandle(const std::string &key) {
    auto writeHandle = cache_->findToWrite(key);
    if (!writeHandle) {
        if (cfg_.SafeMode) {  // shared lock
            while(true) {
                int64_t lock = lock_.load();
                if (lock >= 0 && lock_.compare_exchange_weak(lock, lock + 1))
                    break;
            }
        }
        writeHandle = cache_->allocate(pool_, key, GetRealPageSize());
        if (cfg_.SafeMode) lock_.fetch_sub(1);  // release shared lock

        assert(writeHandle);
        assert(writeHandle->getMemory());
        // need init
        memset(writeHandle->getMemory(), 0, cfg_.PageMetaSize + bitmapSize_);

        if (cfg_.CacheLibCfg.EnableNvmCache) {
            // insertOrReplace will insert or replace existing item for the key,
            // and return the handle of the replaced old item
            // Note: write cache nonsupport NVM, because it will be replaced
            if (!cache_->insertOrReplace(writeHandle)) {
                pageNum_.fetch_add(1);
                pagesList_.insert(key);
            }
        } else {
            if (cache_->insert(writeHandle)) {
                pageNum_.fetch_add(1);
                pagesList_.insert(key);
            } else {
                writeHandle = cache_->findToWrite(key);
            }
        }
    }
    return writeHandle;
}

}  // namespace HybridCache
