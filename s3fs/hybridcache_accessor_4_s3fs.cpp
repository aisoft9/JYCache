#include "fdcache_entity.h"
#include "fdcache.h"
#include "hybridcache_accessor_4_s3fs.h"
#include "hybridcache_disk_data_adaptor.h"
#include "hybridcache_s3_data_adaptor.h"
#include "s3fs_logger.h"
#include "time.h"

#include "Common.h"
#include "FileSystemDataAdaptor.h"
#include "GlobalDataAdaptor.h"

using HybridCache::ByteBuffer;
using HybridCache::WriteCache;
using HybridCache::ReadCache;
using HybridCache::ErrCode::SUCCESS;
using HybridCache::EnableLogging;

HybridCacheAccessor4S3fs::HybridCacheAccessor4S3fs(
        const HybridCache::HybridCacheConfig& cfg) : HybridCacheAccessor(cfg) {
    Init();
}

HybridCacheAccessor4S3fs::~HybridCacheAccessor4S3fs() {
    Stop();
}

void HybridCacheAccessor4S3fs::Init() {
    InitLog();

    if (cfg_.UseGlobalCache) {
        std::shared_ptr<EtcdClient> etcd_client = nullptr;
        if (cfg_.GlobalCacheCfg.EnableWriteCache) {
            GetGlobalConfig().default_policy.write_cache_type = REPLICATION;
            GetGlobalConfig().default_policy.write_replication_factor = 1;
            etcd_client = std::make_shared<EtcdClient>(cfg_.GlobalCacheCfg.EtcdAddress);
        }
        if (!cfg_.GlobalCacheCfg.GflagFile.empty()) {
            HybridCache::ParseFlagFromFile(cfg_.GlobalCacheCfg.GflagFile);
        }
        dataAdaptor_ = std::make_shared<GlobalDataAdaptor>(
                std::make_shared<DiskDataAdaptor>(std::make_shared<S3DataAdaptor>()),
                cfg_.GlobalCacheCfg.GlobalServers, etcd_client);
    } else {
        dataAdaptor_ = std::make_shared<DiskDataAdaptor>(
                std::make_shared<S3DataAdaptor>());
    }

    executor_ = std::make_shared<HybridCache::ThreadPool>(cfg_.ThreadNum);
    dataAdaptor_->SetExecutor(executor_);
    writeCache_ = std::make_shared<WriteCache>(cfg_.WriteCacheCfg);
    readCache_ = std::make_shared<ReadCache>(cfg_.ReadCacheCfg, dataAdaptor_,
                                             executor_);
    tokenBucket_ = std::make_shared<folly::TokenBucket>(
            cfg_.UploadNormalFlowLimit, cfg_.UploadBurstFlowLimit);
    toStop_.store(false, std::memory_order_release);
    bgFlushThread_ = std::thread(&HybridCacheAccessor4S3fs::BackGroundFlush, this);
    LOG(WARNING) << "[Accessor]Init, useGlobalCache:" << cfg_.UseGlobalCache;
}

void HybridCacheAccessor4S3fs::Stop() {
    toStop_.store(true, std::memory_order_release);
    if (bgFlushThread_.joinable()) {
        bgFlushThread_.join();
    }
    executor_->stop();
    writeCache_.reset();
    readCache_.reset();
    LOG(WARNING) << "[Accessor]Stop";
}

int HybridCacheAccessor4S3fs::Put(const std::string &key, size_t start,
                                  size_t len, const char* buf) {
    std::chrono::steady_clock::time_point startTime;
    if (EnableLogging) startTime = std::chrono::steady_clock::now();

    // When the write cache is full, 
    // block waiting for asynchronous flush to release the write cache space.
    while(IsWriteCacheFull(len)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // shared lock
    auto fileLock = fileLock_.find(key);
    while(true) {
        if (fileLock_.end() != fileLock) break;
        auto res = fileLock_.insert(key, std::make_shared<std::atomic<int>>(0));
        if (res.second) {
            fileLock = std::move(res.first);
            break;
        }
        fileLock = fileLock_.find(key);
    }
    while(true) {
        int lock = fileLock->second->load();
        if (lock >= 0 && fileLock->second->compare_exchange_weak(lock, lock + 1))
            break;
    }

    int res = writeCache_->Put(key, start, len, ByteBuffer(const_cast<char *>(buf), len));

    int fd = -1;
    FdEntity* ent = nullptr;
    if (SUCCESS == res && nullptr == (ent = FdManager::get()->GetFdEntity(
                key.c_str(), fd, false, AutoLock::ALREADY_LOCKED))) {
        res = -EIO;
        LOG(ERROR) << "[Accessor]Put, can't find opened path, file:" << key;
    }
    if (SUCCESS == res) {
        ent->UpdateRealsize(start + len);  // TODO: size如何获取?并发情况下的一致性?
    }

    fileLock->second->fetch_sub(1);  // release shared lock

    if (EnableLogging) {
        double totalTime = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - startTime).count();
        LOG(INFO) << "[Accessor]Put, key:" << key << ", start:" << start
                  << ", len:" << len << ", res:" << res
                  << ", time:" << totalTime << "ms";
    }
    return res;
}

int HybridCacheAccessor4S3fs::Get(const std::string &key, size_t start,
                                  size_t len, char* buf) {
    std::chrono::steady_clock::time_point startTime;
    if (EnableLogging) startTime = std::chrono::steady_clock::now();

    int res = SUCCESS;
    ByteBuffer buffer(buf, len);
    std::vector<std::pair<size_t, size_t>> dataBoundary;
    res = writeCache_->Get(key, start, len, buffer, dataBoundary);

    size_t remainLen = len;
    for (auto it : dataBoundary) {
        remainLen -= it.second;
    }

    // handle cache misses
    size_t readLen = 0;
    size_t stepStart = 0;
    size_t fileStartOff = 0;
    std::vector<folly::Future<int>> fs;
    auto it = dataBoundary.begin();
    while (remainLen > 0 && SUCCESS == res) {
        ByteBuffer buffer(buf + stepStart);
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
        buffer.len = readLen;
        remainLen -= readLen;
        fs.emplace_back(std::move(readCache_->Get(key, fileStartOff, readLen, buffer)));
    }

    if (!fs.empty()) {
        auto collectRes = folly::collectAll(fs).get();
        for (auto& entry: collectRes) {
            int tmpRes = entry.value();
            if (SUCCESS != tmpRes && -ENOENT != tmpRes)
                res = tmpRes;
        }
    }

    if (EnableLogging) {
        double totalTime = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - startTime).count();
        LOG(INFO) << "[Accessor]Get, key:" << key << ", start:" << start
                  << ", len:" << len << ", res:" << res
                  << ", time:" << totalTime << "ms";
    }
    return res;
}

int HybridCacheAccessor4S3fs::Flush(const std::string &key) {
    std::chrono::steady_clock::time_point startTime;
    if (EnableLogging) {
        startTime = std::chrono::steady_clock::now();
        LOG(INFO) << "[Accessor]Flush start, key:" << key;
    }

    // exclusive lock
    auto fileLock = fileLock_.find(key);
    while(true) {
        if (fileLock_.end() != fileLock) break;
        auto res = fileLock_.insert(key, std::make_shared<std::atomic<int>>(0));
        if (res.second) {
            fileLock = std::move(res.first);
            break;
        }
        fileLock = fileLock_.find(key);
    }
    while(true) {
        int expected = 0;
        if (fileLock->second->compare_exchange_weak(expected, -1))
            break;
    }

    int res = SUCCESS;
    int fd = -1;
    FdEntity* ent = nullptr;
    if (nullptr == (ent = FdManager::get()->GetFdEntity(
                key.c_str(), fd, false, AutoLock::ALREADY_LOCKED))) {
        res = -EIO;
        LOG(ERROR) << "[Accessor]Flush, can't find opened path, file:" << key;
    }
    size_t realSize = 0;
    std::map<std::string, std::string> realHeaders;
    if (SUCCESS == res) {
        realSize = ent->GetRealsize();
        for (auto &it : ent->GetOriginalHeaders()) {
            realHeaders[it.first] = it.second;
        }
    }

    if (SUCCESS == res && cfg_.UseGlobalCache) {
        // first head S3，upload a empty file when the file does not exist
        size_t size;
        std::map<std::string, std::string> headers;
        S3DataAdaptor s3Adaptor;
        res = s3Adaptor.Head(key, size, headers).get();
        if (-ENOENT == res) {
            res = s3Adaptor.UpLoad(key, 0, ByteBuffer(nullptr, 0), realHeaders).get();
            if (SUCCESS != res) {
                LOG(ERROR) << "[Accessor]Flush, upload empty file error, file:" << key
                           << ", res:" << res;
            }
        } else if (SUCCESS != res) {
            LOG(ERROR) << "[Accessor]Flush, head error, file:" << key
                       << ", res:" << res;
        }
    }

    char *buf = nullptr;
    while(0 != posix_memalign((void **) &buf, 4096, realSize));
    ByteBuffer buffer(buf, realSize);
    if (SUCCESS == res) {
        const size_t chunkSize = GetGlobalConfig().write_chunk_size * 2;
        const uint64_t chunkNum = realSize / chunkSize + (realSize % chunkSize == 0 ? 0 : 1);
        std::vector<Json::Value> jsonRoots(chunkNum);
        std::vector<folly::Future<int>> fs;
        uint64_t cur = 0;
        for (size_t offset = 0; offset < realSize; offset += chunkSize) {
            size_t len = std::min(chunkSize, realSize - offset);
            fs.emplace_back(folly::via(executor_.get(), [this, key, offset, len, buf, &realHeaders, &jsonRoots, cur]() {
                int getRes = Get(key, offset, len, buf + offset);
                if (!cfg_.UseGlobalCache || SUCCESS != getRes) return getRes;
                while(!tokenBucket_->consume(len));  // upload flow control
                ByteBuffer buffer(buf + offset, len);
                GlobalDataAdaptor* adaptor = dynamic_cast<GlobalDataAdaptor*>(dataAdaptor_.get());
                return adaptor->UpLoadPart(key, offset, len, buffer, realHeaders, jsonRoots[cur]).get();
            }));
            ++cur;
        }
        auto collectRes = folly::collectAll(fs).get();
        for (auto& entry: collectRes) {
            int tmpRes = entry.value();
            if (SUCCESS != tmpRes) res = tmpRes;
        }
        if (cfg_.UseGlobalCache && SUCCESS == res) {
            GlobalDataAdaptor* adaptor = dynamic_cast<GlobalDataAdaptor*>(dataAdaptor_.get());
            res = adaptor->Completed(key, jsonRoots, realSize).get();
        }
    }

    if (SUCCESS == res && !cfg_.UseGlobalCache) {  // Get success
        while(!tokenBucket_->consume(realSize));  // upload flow control
        res = dataAdaptor_->UpLoad(key, realSize, buffer, realHeaders).get();
        if (SUCCESS != res){
            LOG(ERROR) << "[Accessor]Flush, upload error, file:" << key
                       << ", res:" << res;
        }
    }

    // folly via is not executed immediately, so use separate thread
    std::thread t([this, key, res]() {
        if (SUCCESS == res)  // upload success
            writeCache_->Delete(key);
        auto fileLock = fileLock_.find(key);
        if (fileLock_.end() != fileLock) {
            fileLock->second->store(0);
            fileLock_.erase(fileLock);  // release exclusive lock
        }
    });
    t.detach();

    if (SUCCESS == res && cfg_.FlushToRead) {  // upload success
        // TODO: 为提升性能，解锁可能会先于put readCache，可能导致并发flush时写脏数据
        std::vector<folly::Future<int>> fs;
        const size_t chunkSize = 32 * 1024 * 1024;
        for (size_t offset = 0; offset < realSize; offset += chunkSize) {
            size_t len = std::min(chunkSize, realSize - offset);
            fs.emplace_back(folly::via(executor_.get(), [this, key, offset, len, buf]() {
                return readCache_->Put(key, offset, len, ByteBuffer(buf + offset, len));
            }));
        }
        folly::collectAll(fs).via(executor_.get()).thenValue([this, buf](
                std::vector<folly::Try<int>, std::allocator<folly::Try<int>>>&& tups) {
            if (buf) free(buf);
            return 0;
        });
    } else {
        folly::via(executor_.get(), [this, buf]() {
            if (buf) free(buf);
        });
    }

    if (EnableLogging) {
        double totalTime = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - startTime).count();
        LOG(INFO) << "[Accessor]Flush end, key:" << key << ", size:" << realSize
                  << ", res:" << res << ", time:" << totalTime << "ms";
    }
    return res;
}

int HybridCacheAccessor4S3fs::DeepFlush(const std::string &key) {
    std::chrono::steady_clock::time_point startTime;
    if (EnableLogging) startTime = std::chrono::steady_clock::now();

    int res = SUCCESS;
    if (cfg_.UseGlobalCache) {
        res = dataAdaptor_->DeepFlush(key).get();
    }
    if (EnableLogging) {
        double totalTime = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - startTime).count();
        LOG(INFO) << "[Accessor]DeepFlush, key:" << key << ", res:" << res
                  << ", time:" << totalTime << "ms";
    }
    return res;
}

int HybridCacheAccessor4S3fs::Delete(const std::string &key) {
    std::chrono::steady_clock::time_point startTime;
    if (EnableLogging) startTime = std::chrono::steady_clock::now();

    // exclusive lock
    auto fileLock = fileLock_.find(key);
    while(true) {
        if (fileLock_.end() != fileLock) break;
        auto res = fileLock_.insert(key, std::make_shared<std::atomic<int>>(0));
        if (res.second) {
            fileLock = std::move(res.first);
            break;
        }
        fileLock = fileLock_.find(key);
    }
    while(true) {
        int expected = 0;
        if (fileLock->second->compare_exchange_weak(expected, -1))
            break;
    }

    int res = writeCache_->Delete(key);
    if (SUCCESS == res) {
        res = readCache_->Delete(key);
    }
    if (SUCCESS == res) {
        res = dataAdaptor_->Delete(key).get();
    }

    fileLock->second->store(0);
    fileLock_.erase(fileLock);  // release exclusive lock
    if (EnableLogging) {
        double totalTime = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - startTime).count();
        LOG(INFO) << "[Accessor]Delete, key:" << key << ", res:" << res
                  << ", time:" << totalTime << "ms";
    }
    return res;
}

int HybridCacheAccessor4S3fs::Truncate(const std::string &key, size_t size) {
    std::chrono::steady_clock::time_point startTime;
    if (EnableLogging) startTime = std::chrono::steady_clock::now();

    // exclusive lock
    auto fileLock = fileLock_.find(key);
    while(true) {
        if (fileLock_.end() != fileLock) break;
        auto res = fileLock_.insert(key, std::make_shared<std::atomic<int>>(0));
        if (res.second) {
            fileLock = std::move(res.first);
            break;
        }
        fileLock = fileLock_.find(key);
    }
    while(true) {
        int expected = 0;
        if (fileLock->second->compare_exchange_weak(expected, -1))
            break;
    }

    int res = SUCCESS;
    int fd = -1;
    FdEntity* ent = nullptr;
    if (nullptr == (ent = FdManager::get()->GetFdEntity(key.c_str(), fd,
                false, AutoLock::ALREADY_LOCKED))) {
        res = -EIO;                    
        LOG(ERROR) << "[Accessor]Flush, can't find opened path, file:" << key;
    }
    size_t realSize = 0;
    if (SUCCESS == res) {
        realSize = ent->GetRealsize();
        if (size < realSize) {
            res = writeCache_->Truncate(key, size);
        } else if (size > realSize) {
            // fill write cache 
            size_t fillSize = size - realSize;
            std::unique_ptr<char[]> buf = std::make_unique<char[]>(fillSize);
            res = writeCache_->Put(key, realSize, fillSize,
                                   ByteBuffer(buf.get(), fillSize));
        }
    }

    if (SUCCESS == res && size != realSize) {
        ent->TruncateRealsize(size);
    }

    // release exclusive lock
    fileLock->second->store(0);

    if (EnableLogging) {
        double totalTime = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - startTime).count();
        LOG(INFO) << "[Accessor]Truncate, key:" << key << ", size:" << size
                  << ", res:" << res << ", time:" << totalTime << "ms";
    }
    return res;
}

int HybridCacheAccessor4S3fs::Invalidate(const std::string &key) {
    std::chrono::steady_clock::time_point startTime;
    if (EnableLogging) startTime = std::chrono::steady_clock::now();
    int res = SUCCESS;
    if (cfg_.CleanCacheByOpen) {
        res = readCache_->Delete(key);
        if (EnableLogging) {
            double totalTime = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - startTime).count();
            LOG(INFO) << "[Accessor]Invalidate, key:" << key
                      << ", res:" << res << ", time:" << totalTime << "ms";
        }
    }
    return res;
}

int HybridCacheAccessor4S3fs::Head(const std::string &key, size_t& size,
             std::map<std::string, std::string>& headers) {
    std::chrono::steady_clock::time_point startTime;
    if (EnableLogging) startTime = std::chrono::steady_clock::now();
    int res = dataAdaptor_->Head(key, size, headers).get();
    if (EnableLogging) {
        double totalTime = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - startTime).count();
        LOG(INFO) << "[Accessor]Head, key:" << key << ", res:" << res
                  << ", size:" << size << ", headerCnt:" << headers.size()
                  << ", time:" << totalTime << "ms";
    }
    return res;
}

int HybridCacheAccessor4S3fs::FsSync() {
    std::chrono::steady_clock::time_point startTime;
    if (EnableLogging) startTime = std::chrono::steady_clock::now();
    if (EnableLogging) {
        LOG(WARNING) << "[Accessor]FsSync start";
    }
    while(true) {
        bool expected = false;
        if (backFlushRunning_.compare_exchange_weak(expected, true))
            break;
    }

    std::map<std::string, time_t> files;
    writeCache_->GetAllKeys(files);
    std::vector<std::pair<std::string, time_t>> filesVec(files.begin(), files.end());
    std::sort(filesVec.begin(), filesVec.end(),
            [](std::pair<std::string, time_t> lhs, std::pair<std::string, time_t> rhs) {
        return lhs.second < rhs.second;
    });

    std::vector<folly::Future<int>> fs;
    for (auto& file : filesVec) {
        std::string key = file.first;
        fs.emplace_back(folly::via(executor_.get(), [this, key]() {
            int res = this->Flush(key);
            if (res) {
                LOG(ERROR) << "[Accessor]FsSync, flush error in FsSync, file:" << key
                           << ", res:" << res;
            }
            return res;
        }));
    }
    if (fs.size()) {
        collectAll(fs).get();
    }
    backFlushRunning_.store(false);
    if (EnableLogging) {
        double totalTime = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - startTime).count();
        LOG(WARNING) << "[Accessor]FsSync end, fileCnt:" << filesVec.size()
                  << ", time:" << totalTime << "ms";
    }
    return SUCCESS;
}

bool HybridCacheAccessor4S3fs::UseGlobalCache() {
    return cfg_.UseGlobalCache;
}

void HybridCacheAccessor4S3fs::BackGroundFlush() {
    LOG(WARNING) << "[Accessor]BackGroundFlush start";
    while (!toStop_.load(std::memory_order_acquire)) {
        if (WriteCacheRatio() < cfg_.BackFlushCacheRatio) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        LOG(WARNING) << "[Accessor]BackGroundFlush radically, write cache ratio:"
                     << WriteCacheRatio();
        FsSync();
    }
    if (0 < writeCache_->GetCacheSize()) {
        FsSync();
    }
    LOG(WARNING) << "[Accessor]BackGroundFlush end";
}

void HybridCacheAccessor4S3fs::InitLog() {
    FLAGS_log_dir = cfg_.LogPath;
    FLAGS_minloglevel = cfg_.LogLevel;
    EnableLogging = cfg_.EnableLog;
    google::InitGoogleLogging("hybridcache");
}

uint32_t HybridCacheAccessor4S3fs::WriteCacheRatio() {
    return writeCache_->GetCacheSize() * 100 / writeCache_->GetCacheMaxSize();
}

bool HybridCacheAccessor4S3fs::IsWriteCacheFull(size_t len) {
    return writeCache_->GetCacheSize() + len >=
            (writeCache_->GetCacheMaxSize() * cfg_.WriteCacheCfg.CacheSafeRatio / 100);
}
