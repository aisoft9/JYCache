/*
 * Project: HybridCache
 * Created Date: 24-2-26
 * Author: lshb
 */
#ifndef HYBRIDCACHE_DATA_ADAPTOR_H_
#define HYBRIDCACHE_DATA_ADAPTOR_H_

#include <thread>

#include "folly/futures/Future.h"
#include "glog/logging.h"

#include "common.h"
#include "errorcode.h"

namespace HybridCache {

class DataAdaptor {
 public:
    virtual folly::Future<int> DownLoad(const std::string &key,
                                        size_t start,
                                        size_t size,
                                        ByteBuffer &buffer) = 0;

    virtual folly::Future<int> UpLoad(const std::string &key,
                                      size_t size,
                                      const ByteBuffer &buffer,
                        const std::map<std::string, std::string>& headers) = 0;

    virtual folly::Future<int> Delete(const std::string &key) = 0;
    
    // for global cache
    virtual folly::Future<int> DeepFlush(const std::string &key) {
        return folly::makeFuture<int>(0);
    }

    virtual folly::Future<int> Head(const std::string &key,
                                    size_t& size,
                                    std::map<std::string,
                                    std::string>& headers) = 0;

    void SetExecutor(std::shared_ptr<ThreadPool> executor) {
        executor_ = executor;
    }

 protected:
    std::shared_ptr<ThreadPool> executor_;
};

class DataAdaptor4Test : public DataAdaptor {
 public:
    folly::Future<int> DownLoad(const std::string &key,
                                size_t start,
                                size_t size,
                                ByteBuffer &buffer) {
        assert(executor_);
        return folly::via(executor_.get(), [key, start, size, buffer]() -> int {
            LOG(INFO) << "[DataAdaptor]DownLoad start, key:" << key
                       << ", start:" << start << ", size:" << size;
            std::this_thread::sleep_for(std::chrono::seconds(3));
            LOG(INFO) << "[DataAdaptor]DownLoad error, key:" << key
                       << ", start:" << start << ", size:" << size;
            return REMOTE_FILE_NOT_FOUND;
        });
    }

    folly::Future<int> UpLoad(const std::string &key,
                              size_t size,
                              const ByteBuffer &buffer,
                    const std::map<std::string, std::string>& headers) {
        return folly::makeFuture<int>(REMOTE_FILE_NOT_FOUND);
    }

    folly::Future<int> Delete(const std::string &key) {
        return folly::makeFuture<int>(REMOTE_FILE_NOT_FOUND);
    }

    folly::Future<int> Head(const std::string &key,
                            size_t& size,
                            std::map<std::string,
                            std::string>& headers) {
        return folly::makeFuture<int>(REMOTE_FILE_NOT_FOUND);
    }
};

}  // namespace HybridCache

#endif // HYBRIDCACHE_DATA_ADAPTOR_H_
