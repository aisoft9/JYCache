#include "errorcode.h"
#include "read_cache.h"

namespace HybridCache {

ReadCache::ReadCache(const ReadCacheConfig& cfg,
        std::shared_ptr<DataAdaptor> dataAdaptor,
        std::shared_ptr<ThreadPool> executor,
        PoolId curr_id, std::shared_ptr<Cache> curr_cache) :
            cfg_(cfg), dataAdaptor_(dataAdaptor), executor_(executor) {
    if (nullptr == curr_cache)
        Init();
    else
        CombinedInit(curr_id, curr_cache);
}

folly::Future<int> ReadCache::Get(const std::string &key, size_t start,
                                  size_t len, ByteBuffer &buffer) {
    std::chrono::steady_clock::time_point startTime;
    if (EnableLogging) startTime = std::chrono::steady_clock::now();

    int res = SUCCESS;
    uint32_t pageSize = cfg_.CacheCfg.PageBodySize;
    size_t index = start / pageSize;
    uint32_t pagePos = start % pageSize;
    size_t readLen = 0;
    size_t realReadLen = 0;
    size_t bufOffset = 0;
    size_t remainLen = len;
    uint64_t readPageCnt = 0;
    std::vector<std::pair<size_t, size_t>> dataBoundary;

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
            dataBoundary.push_back(std::make_pair(it.first + bufOffset, it.second));
            realReadLen += it.second;
        }
        remainLen -= readLen;
        ++index;
        bufOffset += readLen;
        pagePos = (pagePos + readLen) % pageSize;
    }

    remainLen = len - realReadLen;
    if (remainLen > 0 && !dataAdaptor_) {
        res = ADAPTOR_NOT_FOUND;
    }

    // handle cache misses
    readLen = 0;
    size_t stepStart = 0;
    size_t fileStartOff = 0;
    std::vector<folly::Future<int>> fs;
    auto it = dataBoundary.begin();
    while (remainLen > 0 && SUCCESS == res) {
        ByteBuffer stepBuffer(buffer.data + stepStart);
        fileStartOff = start + stepStart;
        if (it != dataBoundary.end()) {
            readLen = it->first - stepStart;
            if (!readLen) {
                stepStart = it->first + it->second;
                ++it;
                continue;
            }
            stepStart = it->first + it->second;
            ++it;
        } else {
            readLen = remainLen;
        }
        stepBuffer.len = readLen;
        remainLen -= readLen;
        
        auto download = folly::via(executor_.get(), [this, readLen]() {
            // download flow control
            while(!this->tokenBucket_->consume(readLen));
            return SUCCESS;
        }).thenValue([this, key, fileStartOff, readLen, stepBuffer](int i) {
            ByteBuffer tmpBuffer(stepBuffer.data, readLen);
            return this->dataAdaptor_->DownLoad(key, fileStartOff, readLen, tmpBuffer).get();
        }).thenValue([this, key, fileStartOff, readLen, stepBuffer](int downRes) {
            if (EnableLogging && SUCCESS != downRes) {
                LOG(ERROR) << "[ReadCache]DownLoad failed, file:" << key
                           << ", start:" << fileStartOff << ", len:" << readLen
                           << ", res:" << downRes;
                return downRes;
            }
            return this->Put(key, fileStartOff, readLen, stepBuffer);
        });

        fs.emplace_back(std::move(download));
    }

    if (!fs.empty()) {
        return collectAll(fs).via(executor_.get())
                .thenValue([key, start, len, readPageCnt, startTime](
                std::vector<folly::Try<int>, std::allocator<folly::Try<int>>>&& tups) {
            int finalRes = SUCCESS;
            for (const auto& t : tups) {
                if (SUCCESS != t.value()) finalRes = t.value();
            }
            if (EnableLogging) {
                double totalTime = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - startTime).count();
                LOG(INFO) << "[ReadCache]Get, key:" << key << ", start:" << start
                            << ", len:" << len << ", res:" << finalRes
                            << ", readPageCnt:" << readPageCnt
                            << ", time:" << totalTime << "ms";
            }
            return finalRes;
        });
        // auto tups = collectAll(fs).get();
        // int finalRes = SUCCESS;
        // for (const auto& t : tups) {
        //     if (SUCCESS != t.value()) finalRes = t.value();
        // }
        // if (EnableLogging) {
        //     double totalTime = std::chrono::duration<double, std::milli>(
        //             std::chrono::steady_clock::now() - startTime).count();
        //     LOG(INFO) << "[ReadCache]Get, key:" << key << ", start:" << start
        //                 << ", len:" << len << ", res:" << finalRes
        //                 << ", readPageCnt:" << readPageCnt
        //                 << ", time:" << totalTime << "ms";
        // }
        // return finalRes;
    }

    if (EnableLogging) {
        double totalTime = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - startTime).count();
        LOG(INFO) << "[ReadCache]Get, key:" << key << ", start:" << start
                  << ", len:" << len << ", res:" << res
                  << ", readPageCnt:" << readPageCnt
                  << ", time:" << totalTime << "ms";
    }
    return folly::makeFuture(res);
}

int ReadCache::Put(const std::string &key, size_t start, size_t len,
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

    if (EnableLogging) {
        double totalTime = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - startTime).count();
        LOG(INFO) << "[ReadCache]Put, key:" << key << ", start:" << start
                  << ", len:" << len << ", res:" << res
                  << ", writePageCnt:" << writePageCnt
                  << ", time:" << totalTime << "ms";
    }
    return res;
}

int ReadCache::Delete(const std::string &key) {
    std::chrono::steady_clock::time_point startTime;
    if (EnableLogging) startTime = std::chrono::steady_clock::now();

    int res = SUCCESS;
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

    if (EnableLogging) {
        double totalTime = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - startTime).count();
        LOG(INFO) << "[ReadCache]Delete, key:" << key << ", res:" << res
                  << ", delPageCnt:" << delPageNum
                  << ", time:" << totalTime << "ms";
    }
    return res;
}

int ReadCache::GetAllKeys(std::set<std::string>& keys) {
    std::chrono::steady_clock::time_point startTime;
    if (EnableLogging) startTime = std::chrono::steady_clock::now();

    auto pageKey = pageCache_->GetPageList().begin();
    while (pageKey != pageCache_->GetPageList().end()) {
        std::vector<std::string> tokens;
        split(*pageKey, PAGE_SEPARATOR, tokens);
        keys.insert(tokens[0]);
        ++pageKey;
    }
    if (EnableLogging) {
        double totalTime = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - startTime).count();
        LOG(INFO) << "[ReadCache]Get all keys, keyCnt:" << keys.size()
                  << ", time:" << totalTime << "ms";
    }
    return SUCCESS;
}

void ReadCache::Close() {
    pageCache_->Close();
    LOG(WARNING) << "[ReadCache]Close";
}

int ReadCache::Init() {
    pageCache_ = std::make_shared<PageCacheImpl>(cfg_.CacheCfg);
    tokenBucket_ = std::make_shared<folly::TokenBucket>(
            cfg_.DownloadNormalFlowLimit, cfg_.DownloadBurstFlowLimit);
    int res = pageCache_->Init();
    LOG(WARNING) << "[ReadCache]Init, res:" << res;
    return res;
}

// added by tqy
int ReadCache::CombinedInit(PoolId curr_id, std::shared_ptr<Cache> curr_cache) {
    pageCache_ = std::make_shared<PageCacheImpl>(cfg_.CacheCfg, curr_id, curr_cache);
    tokenBucket_ = std::make_shared<folly::TokenBucket>(
            cfg_.DownloadNormalFlowLimit, cfg_.DownloadBurstFlowLimit);
    LOG(WARNING) << "[ReadCache]CombinedInit, curr_id:" << static_cast<int>(curr_id);
    return SUCCESS;
}

std::string ReadCache::GetPageKey(const std::string &key, size_t pageIndex) {
    std::string pageKey;
    if (key.length() <= 200) {
        pageKey.append(key);
    } else {
        pageKey.append(key.substr(0, 200)).append(md5(key));
    }
    pageKey.append("_R").append(std::string(1, PAGE_SEPARATOR))
           .append(std::to_string(pageIndex));
    return pageKey;
}

}  // namespace HybridCache
