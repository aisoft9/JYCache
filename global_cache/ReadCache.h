#ifndef MADFS_READ_CACHE_H
#define MADFS_READ_CACHE_H

#include <map>
#include <string>
#include <folly/executors/CPUThreadPoolExecutor.h>

#include <butil/iobuf.h>

#include "Common.h"
#include "data_adaptor.h"
#include "read_cache.h"

using HybridCache::DataAdaptor;

class ReadCacheImpl {
public:
    virtual Future<GetOutput> Get(const std::string &key, uint64_t start, uint64_t length) = 0;
    virtual int Put(const std::string &key, uint64_t length, const butil::IOBuf &buf) = 0;
    virtual int Delete(const std::string &key) = 0;
    virtual int Delete(const std::string &key, uint64_t chunk_size, uint64_t max_chunk_id) = 0;
};

class ReadCache {
public:
    explicit ReadCache(std::shared_ptr<folly::CPUThreadPoolExecutor> executor, 
                       std::shared_ptr<DataAdaptor> base_adaptor = nullptr);

    ~ReadCache() {
        delete impl_;
    }

    Future<GetOutput> Get(const std::string &key, uint64_t start, uint64_t length) {
        return impl_->Get(key, start, length);
    }

    int Put(const std::string &key, uint64_t length, const butil::IOBuf &buf) {
        return impl_->Put(key, length, buf);
    }

    int Delete(const std::string &key) {
        return impl_->Delete(key);
    }

    int Delete(const std::string &key, uint64_t chunk_size, uint64_t max_chunk_id) {
        return impl_->Delete(key, chunk_size, max_chunk_id);
    }

private:
    ReadCacheImpl *impl_;
};

#endif // MADFS_READ_CACHE_H