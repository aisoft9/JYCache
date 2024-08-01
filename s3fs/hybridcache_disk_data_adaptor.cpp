#include "fdcache_entity.h"
#include "fdcache.h"
#include "hybridcache_disk_data_adaptor.h"

using HybridCache::ErrCode::SUCCESS;
using HybridCache::EnableLogging;

const size_t SINGLE_WRITE_SIZE = 1024 * 1024 * 1024;

folly::Future<int> DiskDataAdaptor::DownLoad(const std::string &key,
                                           size_t start,
                                           size_t size,
                                           ByteBuffer &buffer) {
    assert(executor_);
    return folly::via(executor_.get(), [this, key, start, size, buffer]() -> int {
        std::chrono::steady_clock::time_point startTime;
        if (EnableLogging) startTime = std::chrono::steady_clock::now();

        int res = SUCCESS;
        int fd = -1;
        FdEntity* ent = FdManager::get()->GetFdEntity(
                    key.c_str(), fd, false, AutoLock::ALREADY_LOCKED);
        if (nullptr == ent) {
            LOG(ERROR) << "[DataAdaptor]DownLoad, can't find opened path, file:" << key;
            res = -EIO;
        }
        if (SUCCESS == res) {
            res = ent->ReadByAdaptor(fd, buffer.data, start, size, false, dataAdaptor_);
        }
        if (EnableLogging) {
            double totalTime = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - startTime).count();
            LOG(INFO) << "[DataAdaptor]DownLoad, file:" << key
                      << ", start:" << start << ", size:" << size
                      << ", res:" << res << ", time:" << totalTime << "ms";
        }
        return 0 < res ? SUCCESS : res;
    });
}

folly::Future<int> DiskDataAdaptor::UpLoad(const std::string &key,
                              size_t size,
                              const ByteBuffer &buffer,
                              const std::map<std::string, std::string>& headers) {
    return dataAdaptor_->UpLoad(key, size, buffer, headers).thenValue(
                [this, key, buffer, size](int upRes) {
            if (SUCCESS != upRes)
                return upRes;
            int fd = -1;
            FdEntity* ent = FdManager::get()->GetFdEntity(
                        key.c_str(), fd, false, AutoLock::ALREADY_LOCKED);
            if (nullptr == ent) {
                LOG(ERROR) << "[DataAdaptor]UpLoad, can't find opened path, file:" << key;
                return upRes;
            }
            size_t remainLen = size;
            size_t totalWriteLen = 0;
            while (0 < remainLen) {
                size_t stepLen = SINGLE_WRITE_SIZE < remainLen ? SINGLE_WRITE_SIZE : remainLen;
                totalWriteLen += ent->WriteCache(buffer.data + size - remainLen,
                                                 size - remainLen, stepLen);
                remainLen -= stepLen;
            }
            if (EnableLogging) {
                LOG(INFO) << "[DataAdaptor]UpLoad, write disk cache, file:" << key
                            << ", size:" << size << ", wsize:" << totalWriteLen;
            }
            return upRes;
        });
}

folly::Future<int> DiskDataAdaptor::Delete(const std::string &key) {
    return dataAdaptor_->Delete(key).thenValue([this, key](int delRes) {
            if (SUCCESS == delRes) {
                int tmpRes = FdManager::DeleteCacheFile(key.c_str());
                if (EnableLogging) {
                    LOG(INFO) << "[DataAdaptor]Delete, delete disk cache, file:" << key
                              << ", res:" << tmpRes;
                }
            }
            return delRes;
        });
}

folly::Future<int> DiskDataAdaptor::Head(const std::string &key,
                            size_t& size,
                            std::map<std::string, std::string>& headers) {
    return dataAdaptor_->Head(key, size, headers);
}
