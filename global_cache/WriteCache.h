#ifndef MADFS_WRITE_CACHE_H
#define MADFS_WRITE_CACHE_H

#include <map>
#include <string>
#include <atomic>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/synchronization/RWSpinLock.h>

#include <butil/iobuf.h>
#include <rocksdb/db.h>

#include "Common.h"

class WriteCacheImpl {
public:
    WriteCacheImpl(std::shared_ptr<folly::CPUThreadPoolExecutor> executor) : executor_(executor), next_object_id_(0) {}
    virtual GetOutput Get(const std::string &internal_key, uint64_t start, uint64_t length) = 0;
    virtual PutOutput Put(const std::string &key, uint64_t length, const butil::IOBuf &buf) = 0;
    virtual uint64_t QueryTS() { return next_object_id_.load(); }
    virtual int Delete(const std::string &key_prefix, uint64_t ts, const std::unordered_set<std::string> &except_keys) = 0;
    
    std::shared_ptr<folly::CPUThreadPoolExecutor> executor_;
    std::atomic<uint64_t> next_object_id_;
};

class WriteCache {
public:
    explicit WriteCache(std::shared_ptr<folly::CPUThreadPoolExecutor> executor);

    ~WriteCache() {
        delete impl_;
    }

    GetOutput Get(const std::string &internal_key, uint64_t start, uint64_t length) {
        return impl_->Get(internal_key, start, length);
    }

    PutOutput Put(const std::string &key, uint64_t length, const butil::IOBuf &buf) {
        return impl_->Put(key, length, buf);
    }

    uint64_t QueryTS() { return impl_->QueryTS(); }

    int Delete(const std::string &key_prefix, uint64_t ts, const std::unordered_set<std::string> &except_keys) {
        return impl_->Delete(key_prefix, ts, except_keys);
    }

private:
    WriteCacheImpl *impl_;
};

#endif // MADFS_WRITE_CACHE_H