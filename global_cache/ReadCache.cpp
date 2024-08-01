#include <unistd.h>
#include <butil/iobuf.h>
#include <bvar/bvar.h>
#include <butil/time.h>

#define BRPC_WITH_RDMA 1
#include <brpc/rdma/block_pool.h>

#include "ReadCache.h"
#include "FileSystemDataAdaptor.h"

bvar::LatencyRecorder g_latency_readcache4cachelib_get("readcache4cachelib_get");

class ReadCache4Cachelib : public ReadCacheImpl {
public:
    explicit ReadCache4Cachelib(std::shared_ptr<folly::CPUThreadPoolExecutor> executor, 
                                std::shared_ptr<DataAdaptor> base_adaptor = nullptr);

    ~ReadCache4Cachelib() {}

    virtual Future<GetOutput> Get(const std::string &key, uint64_t start, uint64_t length);

    virtual int Put(const std::string &key, uint64_t length, const butil::IOBuf &buf);

    virtual int Delete(const std::string &key);

    virtual int Delete(const std::string &key, uint64_t chunk_size, uint64_t max_chunk_id);

private:
    std::shared_ptr<folly::CPUThreadPoolExecutor> executor_;
    std::shared_ptr<DataAdaptor> base_adaptor_;
    std::shared_ptr<HybridCache::ReadCache> impl_;
};

ReadCache4Cachelib::ReadCache4Cachelib(std::shared_ptr<folly::CPUThreadPoolExecutor> executor,
                                       std::shared_ptr<DataAdaptor> base_adaptor) 
                                       : executor_(executor), base_adaptor_(base_adaptor)  {
    HybridCache::EnableLogging = false;
    impl_ = std::make_shared<HybridCache::ReadCache>(GetGlobalConfig().read_cache, 
                                                     base_adaptor_, 
                                                     executor);
}

Future<GetOutput> ReadCache4Cachelib::Get(const std::string &key, uint64_t start, uint64_t length) {
    butil::Timer *t = new butil::Timer();
    t->start();
#ifndef BRPC_WITH_RDMA
    auto wrap = HybridCache::ByteBuffer(new char[length], length); 
#else
    auto wrap = HybridCache::ByteBuffer((char *) brpc::rdma::AllocBlock(length), length); 
#endif
    return impl_->Get(key, start, length, wrap).thenValue([wrap, key, start, length, t](int res) -> GetOutput {
        t->stop();
        g_latency_readcache4cachelib_get << t->u_elapsed();
        delete t;
        GetOutput output;
        output.status = res;
#ifndef BRPC_WITH_RDMA
        if (res == OK) {
            output.buf.append(wrap.data, wrap.len);
        }
        delete []wrap.data;
#else
        if (res == OK) {
            output.buf.append_user_data(wrap.data, wrap.len, brpc::rdma::DeallocBlock);
        } else {
            brpc::rdma::DeallocBlock(wrap.data);
        }
#endif
        LOG_IF(INFO, FLAGS_verbose) << "Get key: " << key 
                                    << ", start: " << start
                                    << ", length: " << length 
                                    << ", status: " << res;
        return output;
    });
}

int ReadCache4Cachelib::Put(const std::string &key, uint64_t length, const butil::IOBuf &buf) {
    auto data_len = buf.length();
    auto aux_buffer = malloc(data_len);
    auto data = buf.fetch(aux_buffer, data_len);
    auto wrap = HybridCache::ByteBuffer((char *) data, data_len);
    int res = impl_->Put(key, 0, length, wrap);
    free(aux_buffer);
    LOG_IF(INFO, FLAGS_verbose) << "Put key: " << key 
                                << ", length: " << length 
                                << ", status: " << res;
    return res;
}

int ReadCache4Cachelib::Delete(const std::string &key) {
    LOG_IF(INFO, FLAGS_verbose) << "Delete key: " << key;
    return impl_->Delete(key);
}

int ReadCache4Cachelib::Delete(const std::string &key, uint64_t chunk_size, uint64_t max_chunk_id) {
    LOG_IF(INFO, FLAGS_verbose) << "Delete key: " << key;
    for (uint64_t chunk_id = 0; chunk_id < max_chunk_id; chunk_id++) {
        auto internal_key = key + "-" + std::to_string(chunk_id) + "-" + std::to_string(chunk_size);
        int ret = impl_->Delete(internal_key);
        if (ret) {
            return ret;
        }
    }
    return OK;
}

bvar::LatencyRecorder g_latency_readcache4disk_get("readcache4disk_get");

// ----------------------------------------------------------------------------
class ReadCache4Disk : public ReadCacheImpl {
public:
    explicit ReadCache4Disk(std::shared_ptr<folly::CPUThreadPoolExecutor> executor, 
                            std::shared_ptr<DataAdaptor> base_adaptor = nullptr);

    ~ReadCache4Disk() {}

    virtual Future<GetOutput> Get(const std::string &key, uint64_t start, uint64_t length);

    virtual int Put(const std::string &key, uint64_t length, const butil::IOBuf &buf);

    virtual int Delete(const std::string &key);

    virtual int Delete(const std::string &key, uint64_t chunk_size, uint64_t max_chunk_id);

private:
    std::shared_ptr<folly::CPUThreadPoolExecutor> executor_;
    std::shared_ptr<DataAdaptor> base_adaptor_;
    std::shared_ptr<DataAdaptor> cache_fs_adaptor_;
};

ReadCache4Disk::ReadCache4Disk(std::shared_ptr<folly::CPUThreadPoolExecutor> executor,
                               std::shared_ptr<DataAdaptor> base_adaptor) 
                               : executor_(executor), base_adaptor_(base_adaptor)  {
    cache_fs_adaptor_ = std::make_shared<FileSystemDataAdaptor>(GetGlobalConfig().read_cache_dir, base_adaptor_, true, executor_);
}

Future<GetOutput> ReadCache4Disk::Get(const std::string &key, uint64_t start, uint64_t length) {
    butil::Timer *t = new butil::Timer();
    t->start();
#ifndef BRPC_WITH_RDMA
    auto wrap = HybridCache::ByteBuffer(new char[length], length); 
#else
    auto wrap = HybridCache::ByteBuffer((char *) brpc::rdma::AllocBlock(length), length); 
#endif
    return cache_fs_adaptor_->DownLoad(key, start, length, wrap).thenValue([wrap, key, start, length, t](int res) -> GetOutput {
        GetOutput output;
        output.status = res;
#ifndef BRPC_WITH_RDMA
        if (res == OK) {
            output.buf.append(wrap.data, wrap.len);
        }
        delete []wrap.data;
#else
        if (res == OK) {
            output.buf.append_user_data(wrap.data, wrap.len, brpc::rdma::DeallocBlock);
        } else {
            brpc::rdma::DeallocBlock(wrap.data);
        }
#endif
        t->stop();
        g_latency_readcache4disk_get << t->u_elapsed();
        delete t;
        LOG_IF(INFO, FLAGS_verbose) << "Get key: " << key 
                                    << ", start: " << start
                                    << ", length: " << length 
                                    << ", status: " << res;
        return output;
    });
}

int ReadCache4Disk::Put(const std::string &key, uint64_t length, const butil::IOBuf &buf) {
    auto data_len = buf.length();
    auto aux_buffer = malloc(data_len);
    auto data = buf.fetch(aux_buffer, data_len);
    auto wrap = HybridCache::ByteBuffer((char *) data, data_len);
    std::map<std::string, std::string> headers;
    int res = cache_fs_adaptor_->UpLoad(key, length, wrap, headers).get();
    free(aux_buffer);
    LOG_IF(INFO, FLAGS_verbose) << "Put key: " << key 
                                << ", length: " << length 
                                << ", status: " << res;
    return res;
}

int ReadCache4Disk::Delete(const std::string &key) {
    LOG_IF(INFO, FLAGS_verbose) << "Delete key: " << key;
    return cache_fs_adaptor_->Delete(key).get();
}

int ReadCache4Disk::Delete(const std::string &key, uint64_t chunk_size, uint64_t max_chunk_id) {
    LOG_IF(INFO, FLAGS_verbose) << "Delete key: " << key;
    for (uint64_t chunk_id = 0; chunk_id < max_chunk_id; chunk_id++) {
        auto internal_key = key + "-" + std::to_string(chunk_id) + "-" + std::to_string(chunk_size);
        int ret = cache_fs_adaptor_->Delete(internal_key).get();
        if (ret) {
            return ret;
        }
    }
    return OK;
}

DEFINE_string(read_cache_engine, "cachelib", "Read cache engine: cachelib | disk");

ReadCache::ReadCache(std::shared_ptr<folly::CPUThreadPoolExecutor> executor, 
                     std::shared_ptr<DataAdaptor> base_adaptor) {
    if (FLAGS_read_cache_engine == "cachelib")
        impl_ = new ReadCache4Cachelib(executor, base_adaptor);
    else if (FLAGS_read_cache_engine == "disk")
        impl_ = new ReadCache4Disk(executor, base_adaptor);
    else {
        LOG(FATAL) << "unsupported read cache engine";
        exit(EXIT_FAILURE);
    }
}