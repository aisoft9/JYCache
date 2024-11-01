#include "glog/logging.h"

#include "errorcode.h"
#include "write_cache.h"

namespace HybridCache {

WriteCache::WriteCache(const WriteCacheConfig& cfg, PoolId curr_id,
                       std::shared_ptr<Cache> curr_cache) : cfg_(cfg) {
    if (nullptr == curr_cache)
        Init();
    else
        CombinedInit(curr_id, curr_cache);

    // Throttle
    if (cfg_.EnableThrottle) {
        throttling_thread_ = std::thread(&WriteCache::Dealing_throttling, this);
        LOG(WARNING) << "[WriteCache] USE_THROTTLING";
    }
}

int WriteCache::Put(const std::string &key, size_t start, size_t len,
                    const ByteBuffer &buffer) {
    std::chrono::steady_clock::time_point startTime;
    if (EnableLogging) startTime = std::chrono::steady_clock::now();

    int res = SUCCESS;
    uint32_t pageSize = cfg_.CacheCfg.PageBodySize;
    uint64_t index = start / pageSize;
    uint64_t pagePos = start % pageSize;
    uint64_t writeLen = 0;
    uint64_t writeOffset = 0;
    uint64_t writePageCnt = 0;
    size_t remainLen = len;

    // added by tqy
    if (cfg_.EnableThrottle)
        this->throttling_.Put_Consume(key, len);

    while (remainLen > 0) {
        writeLen = pagePos + remainLen > pageSize ? pageSize - pagePos : remainLen;
        std::string pageKey = std::move(GetPageKey(key, index));
        res = pageCache_->Write(pageKey, pagePos, writeLen,
                                (buffer.data + writeOffset));
        if (SUCCESS != res) break;
        ++writePageCnt;
        remainLen -= writeLen;
        ++index;
        writeOffset += writeLen;
        pagePos = (pagePos + writeLen) % pageSize;
    }
    if (0 < writePageCnt)
        keys_.insert(key, time(nullptr));

    if (EnableLogging) {
        double totalTime = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - startTime).count();
        LOG(INFO) << "[WriteCache]Put, key:" << key << ", start:" << start
                  << ", len:" << len << ", res:" << res
                  << ", writePageCnt:" << writePageCnt
                  << ", time:" << totalTime << "ms";
    }
    return res;
}

int WriteCache::Get(const std::string &key, size_t start, size_t len,
                    ByteBuffer &buffer,
                    std::vector<std::pair<size_t, size_t>>& dataBoundary) {
    std::chrono::steady_clock::time_point startTime;
    if (EnableLogging) startTime = std::chrono::steady_clock::now();

    int res = SUCCESS;
    uint32_t pageSize = cfg_.CacheCfg.PageBodySize;
    size_t index = start / pageSize;
    uint32_t pagePos = start % pageSize;
    size_t readLen = 0;
    size_t bufOffset = 0;
    size_t remainLen = len;
    uint64_t readPageCnt = 0;

    while (remainLen > 0) {
        readLen = pagePos + remainLen > pageSize ? pageSize - pagePos : remainLen;
        std::string pageKey = std::move(GetPageKey(key, index));
        std::vector<std::pair<size_t, size_t>> stepDataBoundary;
        int tmpRes = pageCache_->Read(pageKey, pagePos, readLen,
                (buffer.data + bufOffset), stepDataBoundary);
        if (SUCCESS == tmpRes) {
            ++readPageCnt;
        } else if (PAGE_NOT_FOUND != tmpRes) {
            res = tmpRes;
            break;
        }

        for (auto& it : stepDataBoundary) {
            size_t realStart = it.first + bufOffset;
            auto last = dataBoundary.rbegin();
            if (last != dataBoundary.rend() && (last->first + last->second) == realStart) {
                last->second += it.second;
            } else {
                dataBoundary.push_back(std::make_pair(realStart, it.second));
            }
        }
        remainLen -= readLen;
        ++index;
        bufOffset += readLen;
        pagePos = (pagePos + readLen) % pageSize;
    }

    if (EnableLogging) {
        double totalTime = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - startTime).count();
        LOG(INFO) << "[WriteCache]Get, key:" << key << ", start:" << start
                  << ", len:" << len << ", res:" << res
                  << ", boundaryVecSize:" << dataBoundary.size()
                  << ", readPageCnt:" << readPageCnt
                  << ", time:" << totalTime << "ms";
    }
    return res;
}

int WriteCache::GetAllCacheWithLock(const std::string &key,
                    std::vector<std::pair<ByteBuffer, size_t>>& dataSegments) {
    std::chrono::steady_clock::time_point startTime;
    if (EnableLogging) startTime = std::chrono::steady_clock::now();

    int res = SUCCESS;
    Lock(key);

    std::string firstPage = std::move(GetPageKey(key, 0));
    auto pageKey = pageCache_->GetPageList().lower_bound(firstPage);
    while (pageKey != pageCache_->GetPageList().end()) {
        std::vector<std::string> tokens;
        split(*pageKey, PAGE_SEPARATOR, tokens);
        if (key != tokens[0]) break;

        size_t pageIdx = 0;
        std::stringstream sstream(tokens[1]);
        sstream >> pageIdx;
        size_t wholeValueOff = pageIdx * cfg_.CacheCfg.PageBodySize;

        std::vector<std::pair<ByteBuffer, size_t>> stepDataSegments;
        res = pageCache_->GetAllCache(*pageKey, stepDataSegments);
        if (SUCCESS != res) break;
        for (auto& it : stepDataSegments) {
            dataSegments.push_back(std::make_pair(it.first,
                                   it.second + wholeValueOff));
        }
        ++pageKey;
    }

    if (EnableLogging) {
        double totalTime = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - startTime).count();
        LOG(INFO) << "[WriteCache]Get all cache with lock, key:" << key
                  << ", res:" << res << ", dataVecSize:" << dataSegments.size()
                  << ", time:" << totalTime << "ms";
    }
    return res;
}

int WriteCache::Delete(const std::string &key, LockType type) {
    std::chrono::steady_clock::time_point startTime;
    if (EnableLogging) startTime = std::chrono::steady_clock::now();

    int res = SUCCESS;
    if (LockType::ALREADY_LOCKED != type) {
        Lock(key);
    }

    if (cfg_.EnableThrottle)  // added by tqy
        throttling_.Del_File(key);
    keys_.erase(key);
    size_t delPageNum = 0;
    std::string firstPage = std::move(GetPageKey(key, 0));
    auto pageKey = pageCache_->GetPageList().lower_bound(firstPage);
    while (pageKey != pageCache_->GetPageList().end()) {
        std::vector<std::string> tokens;
        split(*pageKey, PAGE_SEPARATOR, tokens);
        if (key != tokens[0]) break;
        int tmpRes = pageCache_->Delete(*pageKey);
        if (SUCCESS == tmpRes) {
            ++delPageNum;
        } else if (PAGE_NOT_FOUND != tmpRes) {
            res = tmpRes;
            break;
        }
        ++pageKey;
    }

    UnLock(key);

    if (EnableLogging) {
        double totalTime = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - startTime).count();
        LOG(INFO) << "[WriteCache]Delete, key:" << key << ", res:" << res
                  << ", delPageCnt:" << delPageNum
                  << ", time:" << totalTime << "ms";
    }
    return res;
}

int WriteCache::Truncate(const std::string &key, size_t len) {
    std::chrono::steady_clock::time_point startTime;
    if (EnableLogging) startTime = std::chrono::steady_clock::now();

    int res = SUCCESS;
    uint32_t pageSize = cfg_.CacheCfg.PageBodySize;
    uint64_t index = len / pageSize;
    uint64_t pagePos = len % pageSize;

    if (0 != pagePos) {
        uint32_t TruncateLen = pageSize - pagePos;
        std::string TruncatePage = std::move(GetPageKey(key, index));
        int tmpRes = pageCache_->DeletePart(TruncatePage, pagePos, TruncateLen);
        if (SUCCESS != tmpRes && PAGE_NOT_FOUND != tmpRes) {
            res = tmpRes;
        }
        ++index;
    }

    size_t delPageNum = 0;
    if (SUCCESS == res) {
        Lock(key);
        std::string firstPage = std::move(GetPageKey(key, index));
        auto pageKey = pageCache_->GetPageList().lower_bound(firstPage);
        while (pageKey != pageCache_->GetPageList().end()) {
            std::vector<std::string> tokens;
            split(*pageKey, PAGE_SEPARATOR, tokens);
            if (key != tokens[0]) break;
            int tmpRes = pageCache_->Delete(*pageKey);
            if (SUCCESS == tmpRes) {
                ++delPageNum;
            } else if (PAGE_NOT_FOUND != tmpRes) {
                res = tmpRes;
                break;
            }
            ++pageKey;
        }
        UnLock(key);
    }

    if (EnableLogging) {
        double totalTime = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - startTime).count();
        LOG(INFO) << "[WriteCache]Truncate, key:" << key << ", len:" << len
                  << ", res:" << res << ", delPageCnt:" << delPageNum
                  << ", time:" << totalTime << "ms";
    }
    return res;
}

void WriteCache::UnLock(const std::string &key) {
    keyLocks_.erase(key);
    if (EnableLogging) {
        LOG(INFO) << "[WriteCache]UnLock, key:" << key;
    }
}

int WriteCache::GetAllKeys(std::map<std::string, time_t>& keys) {
    std::chrono::steady_clock::time_point startTime;
    if (EnableLogging) startTime = std::chrono::steady_clock::now();

    for (auto& it : keys_) {
        keys[it.first] = it.second;
    }
    if (EnableLogging) {
        double totalTime = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - startTime).count();
        LOG(INFO) << "[WriteCache]Get all keys, keyCnt:" << keys.size()
                  << ", time:" << totalTime << "ms";
    }
    return SUCCESS;
}

void WriteCache::Close() {
    pageCache_->Close();
    keys_.clear();
    // added by tqy
    if (cfg_.EnableThrottle) {
        throttling_.Close();
        throttling_thread_running_.store(false, std::memory_order_release);  // 线程终止
        if (throttling_thread_.joinable()) {
            throttling_thread_.join();
        }
    }
    LOG(WARNING) << "[WriteCache]Close";
}

size_t WriteCache::GetCacheSize() {
    return pageCache_->GetCacheSize();
}

size_t WriteCache::GetCacheMaxSize() {
    return pageCache_->GetCacheMaxSize();
}

int WriteCache::Init() {
    pageCache_ = std::make_shared<PageCacheImpl>(cfg_.CacheCfg);
    int res = pageCache_->Init();
    LOG(WARNING) << "[WriteCache]Init, res:" << res;
    return res;
}

void WriteCache::Lock(const std::string &key) {
    while(!keyLocks_.add(key));
}

std::string WriteCache::GetPageKey(const std::string &key, size_t pageIndex) {
    std::string pageKey;
    if (key.length() <= 200) {
        pageKey.append(key);
    } else {
        pageKey.append(key.substr(0, 200)).append(md5(key));
    }
    pageKey.append("_W").append(std::string(1, PAGE_SEPARATOR))
           .append(std::to_string(pageIndex));
    return pageKey;
}

// added by tqy
int WriteCache::CombinedInit(PoolId curr_id, std::shared_ptr<Cache> curr_cache) {
    this->pageCache_ = std::make_shared<PageCacheImpl>(cfg_.CacheCfg, curr_id, curr_cache);
    LOG(WARNING) << "[WriteCache]CombinedInit, curr_id:"<< static_cast<int>(curr_id);
    return SUCCESS;
}

// 开一个线程负责记录文件的流量，并且后续与调度器进行交互
void WriteCache::Dealing_throttling() {
    if (!cfg_.EnableThrottle)
        return;

    LOG(WARNING) << "[WriteCache] throttling_ Thread start";
    // memory_order_release确保在此操作之前的所有写操作在其他线程中可见。
    throttling_thread_running_.store(true, std::memory_order_release);
    while (throttling_thread_running_.load(std::memory_order_acquire)) {
        // 接收到调度器传来的新的带宽
        // memory_order_acquire确保此操作之后的所有读写操作在其他线程中可见。
        std::string new_limit = throttling_.Cal_New4Test();
        // new_bw = 0;
        throttling_.SetNewLimits(new_limit);
        throttling_.CleanBlockTime();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));  // 每0.1s Resize一次
        // for(auto temp : throttling_.job_bandwidth_)
        // {
        //     new_bw +=   temp.second;
        // }
        // curr_bw = std::max(new_bw, 649651540.0);
    }
    LOG(WARNING) << "[WriteCache] throttling_ Thread end";
}

}  // namespace HybridCache
