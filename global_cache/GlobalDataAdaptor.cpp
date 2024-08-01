#include "GlobalDataAdaptor.h"
#include "ReadCacheClient.h"
#include "ReplicationWriteCacheClient.h"
#include "ErasureCodingWriteCacheClient.h"

using HybridCache::ByteBuffer;

#define CONFIG_GC_ON_EXCEEDING_DISKSPACE

DEFINE_uint32(bg_execution_period, 10, "Background execution period in seconds");

GlobalDataAdaptor::GlobalDataAdaptor(std::shared_ptr<DataAdaptor> base_adaptor,
                                     const std::vector<std::string> &server_list,
                                     std::shared_ptr<EtcdClient> etcd_client,
                                     std::shared_ptr<folly::CPUThreadPoolExecutor> executor)
        : base_adaptor_(base_adaptor),
          executor_(executor),
          server_list_(server_list),
          etcd_client_(etcd_client),
          meta_cache_(GetGlobalConfig().meta_cache_max_size, GetGlobalConfig().meta_cache_clear_size) {
    if (!executor_) {
        executor_ = std::make_shared<folly::CPUThreadPoolExecutor>(GetGlobalConfig().folly_threads);
    } 
    
    read_cache_ = std::make_shared<ReadCacheClient>(this);
    write_caches_[WC_TYPE_REPLICATION] = std::make_shared<ReplicationWriteCacheClient>(this);
    write_caches_[WC_TYPE_REEDSOLOMON] = std::make_shared<ErasureCodingWriteCacheClient>(this);

    for (int conn_id = 0; conn_id < GetGlobalConfig().rpc_connections; conn_id++) {
        auto client = std::make_shared<GlobalCacheClient>(std::to_string(conn_id));
        int server_id = 0;
        for (auto &entry: server_list_) {
            if (client->RegisterServer(server_id, entry.c_str())) {
                // TODO 周期性尝试重连
                LOG(WARNING) << "Failed to connect with server id: " << server_id
                             << ", address: " << entry;
                bg_mutex_.lock();
                bg_tasks_.push_back([client,server_id, entry]() -> int { 
                    return client->RegisterServer(server_id, entry.c_str()); 
                });
                bg_mutex_.unlock();
            }
            server_id++;
        }
        rpc_client_.push_back(client);
    }
    srand48(time(nullptr));
    bg_running_ = true;
    bg_thread_ = std::thread(std::bind(&GlobalDataAdaptor::BackgroundWorker, this));
}

GlobalDataAdaptor::~GlobalDataAdaptor() {
    bg_running_ = false;
    bg_cv_.notify_all();
    bg_thread_.join();
}

void GlobalDataAdaptor::BackgroundWorker() {
    while (bg_running_) {
        std::unique_lock<std::mutex> lock(bg_mutex_);
        std::vector<std::function<int()>> bg_tasks_next;
        for (auto &entry : bg_tasks_) {
            if (entry()) {
                bg_tasks_next.push_back(entry);
            }
        }
        bg_tasks_ = bg_tasks_next;
        bg_cv_.wait_for(lock, std::chrono::seconds(FLAGS_bg_execution_period));
    }
}

struct DownloadArgs {
    DownloadArgs(const std::string &key, size_t start, size_t size, ByteBuffer &buffer)
            : key(key), start(start), size(size), buffer(buffer) {}

    std::string key;
    size_t start;
    size_t size;
    ByteBuffer &buffer;
};

folly::Future<int> GlobalDataAdaptor::DownLoad(const std::string &key,
                                               size_t start,
                                               size_t size,
                                               ByteBuffer &buffer) {
    return DownLoadFromGlobalCache(key, start, size, buffer).then(
        [this, key, start, size, &buffer](folly::Try<int> &&output) -> folly::Future<int> {
        if (output.value_or(FOLLY_ERROR) == RPC_FAILED) {
            return base_adaptor_->DownLoad(key, start, size, buffer);
        }
        return output.value_or(FOLLY_ERROR);
    });
}

folly::Future<int> GlobalDataAdaptor::DownLoadFromGlobalCache(const std::string &key,
                                                              size_t start,
                                                              size_t size,
                                                              ByteBuffer &buffer) {
    auto &policy = GetCachePolicy(key);
    auto meta_cache_entry = GetMetaCacheEntry(key);
    if (meta_cache_entry->present) {
        if (!meta_cache_entry->existed) {
            LOG(ERROR) << "Request for potential deleted file: " << key;
            return folly::makeFuture(NOT_FOUND);
        }
        if (start + size > meta_cache_entry->size) {
            LOG(ERROR) << "Request out of file range, key: " << key
                       << ", start: " << start
                       << ", size: " << size
                       << ", file length: " << meta_cache_entry->size;
            return folly::makeFuture(END_OF_FILE);
        }
    }

    if (policy.write_cache_type != NOCACHE) {
        auto args = std::make_shared<DownloadArgs>(key, start, size, buffer);
        if (meta_cache_entry->present) {
            if (meta_cache_entry->write_cached) {
                auto &root = meta_cache_entry->root;
                if (root["type"] == "replication") {
                    return write_caches_[WC_TYPE_REPLICATION]->Get(args->key, args->start, args->size, args->buffer, root);
                } else if (root["type"] == "reed-solomon") {
                    return write_caches_[WC_TYPE_REEDSOLOMON]->Get(args->key, args->start, args->size, args->buffer, root);
                }
                LOG(ERROR) << "Failed to download data, reason: unsuppported type, key: " << args->key
                           << ", start: " << args->start
                           << ", size: " << args->size
                           << ", type: " << root["type"];
                return folly::makeFuture(UNSUPPORTED_TYPE);
            } else {
                return read_cache_->Get(key, start, size, buffer);
            }
        } else {
            return etcd_client_->GetJson(key).then(
                    [this, args, meta_cache_entry](folly::Try<EtcdClient::GetResult> &&output) -> folly::Future<int> {
                if (!output.hasValue()) {                   // 当 GetJson 函数抛出异常时执行这部分代码
                    LOG(ERROR) << "Failed to download data, reason: internal error, key: " << args->key
                               << ", start: " << args->start
                               << ", size: " << args->size;
                    return folly::makeFuture(FOLLY_ERROR);
                }

                auto &status = output.value().status;
                if (status == NOT_FOUND) {
                    if (GetGlobalConfig().use_meta_cache) {
                        return base_adaptor_->Head(args->key, meta_cache_entry->size, meta_cache_entry->headers).then(
                                [this, meta_cache_entry, args](folly::Try<int> &&output) -> folly::Future<int> {
                            int res = output.value_or(FOLLY_ERROR);
                            if (res == OK || res == NOT_FOUND) {
                                meta_cache_entry->present = true;
                                meta_cache_entry->existed = (res == OK);
                                meta_cache_entry->write_cached = false;
                            }
                            if (res == OK) {
                                return read_cache_->Get(args->key, args->start, args->size, args->buffer);
                            }
                            return res;
                        });
                    } else {
                        return read_cache_->Get(args->key, args->start, args->size, args->buffer);
                    }
                } else if (status != OK) {
                    return folly::makeFuture(status);
                }

                auto &root = output.value().root;
                if (GetGlobalConfig().use_meta_cache) {
                    meta_cache_entry->present = true;
                    meta_cache_entry->existed = true;
                    meta_cache_entry->write_cached = true;
                    meta_cache_entry->size = root["size"].asInt64();
                    for (auto iter = root["headers"].begin(); iter != root["headers"].end(); iter++) {
                        meta_cache_entry->headers[iter.key().asString()] = (*iter).asString();
                    }
                    meta_cache_entry->root = root;
                }

                if (root["type"] == "replication") {
                    return write_caches_[WC_TYPE_REPLICATION]->Get(args->key, args->start, args->size, args->buffer, root);
                } else if (root["type"] == "reed-solomon") {
                    return write_caches_[WC_TYPE_REEDSOLOMON]->Get(args->key, args->start, args->size, args->buffer, root);
                }

                LOG(ERROR) << "Failed to download data, reason: unsuppported type, key: " << args->key
                           << ", start: " << args->start
                           << ", size: " << args->size
                           << ", type: " << root["type"];

                return folly::makeFuture(UNSUPPORTED_TYPE);
            });
        }
    } else {
        return read_cache_->Get(key, start, size, buffer);
    }
}

folly::Future<int> GlobalDataAdaptor::UpLoad(const std::string &key,
                                             size_t size,
                                             const ByteBuffer &buffer,
                                             const std::map <std::string, std::string> &headers) {
#ifdef CONFIG_GC_ON_EXCEEDING_DISKSPACE
    return DoUpLoad(key, size, buffer, headers).thenValue([this, key, size, &buffer, &headers](int &&res) -> int {
        if (res != NO_ENOUGH_DISKSPACE) {
            return res;
        }
        LOG(INFO) << "Disk limit exceeded - perform GC immediately";
        res = PerformGarbageCollection();
        if (res) {
            LOG(WARNING) << "GC failed";
            return res;
        }
        LOG(INFO) << "Disk limit exceeded - GC completed, now retry";
        return DoUpLoad(key, size, buffer, headers).get();
    });
#else
    return DoUpLoad(key, size, buffer, headers);
#endif
}

folly::Future<int> GlobalDataAdaptor::DoUpLoad(const std::string &key,
                                               size_t size,
                                               const ByteBuffer &buffer,
                                               const std::map <std::string, std::string> &headers) {
    butil::Timer *t = new butil::Timer();
    t->start();
    auto &policy = GetCachePolicy(key);
    auto meta_cache_entry = GetMetaCacheEntry(key);
    meta_cache_entry->present = false;
    meta_cache_entry->existed = true;
    meta_cache_entry->size = size;
    meta_cache_entry->headers = headers;
    auto pre_op = read_cache_->Invalidate(key, size);
    if (policy.write_cache_type == REPLICATION || policy.write_cache_type == REED_SOLOMON) {
        auto write_cache = policy.write_cache_type == REPLICATION
                           ? write_caches_[WC_TYPE_REPLICATION]
                           : write_caches_[WC_TYPE_REEDSOLOMON];
        return std::move(pre_op)
            .then(std::bind(&WriteCacheClient::Put, write_cache.get(), key, size, buffer, headers, 0))
            .then([this, key, meta_cache_entry, t] (folly::Try<WriteCacheClient::PutResult> output) -> folly::Future<int> {
                int status = output.hasValue() ? output.value().status : FOLLY_ERROR;
                if (status == OK) {
                    status = etcd_client_->PutJson(key, output.value().root).get();
                    if (status == OK && GetGlobalConfig().use_meta_cache) {
                        meta_cache_entry->root = output.value().root;
                        meta_cache_entry->write_cached = true;
                        meta_cache_entry->present = true;
                    }
                    t->stop();
                    LOG(INFO) << "JSON: " << t->u_elapsed();
                    delete t;
                }
                return folly::makeFuture(status);
            });
    } else if (policy.write_cache_type == NOCACHE) {
        return std::move(pre_op)
            .then(std::bind(&DataAdaptor::UpLoad, base_adaptor_.get(), key, size, buffer, headers))
            .thenValue([meta_cache_entry](int &&res) -> int {
                if (res == OK && GetGlobalConfig().use_meta_cache) {
                    meta_cache_entry->write_cached = false;
                    meta_cache_entry->present = true;
                }
                return res;
            });
    } else {
        LOG(ERROR) << "Failed to upload data, reason: unsuppported type, key: " << key
                   << ", size: " << size
                   << ", type: " << policy.write_cache_type;
        return folly::makeFuture(UNSUPPORTED_TYPE);
    }
}

folly::Future<int> GlobalDataAdaptor::Delete(const std::string &key) {
    auto &policy = GetCachePolicy(key);
    if (policy.write_cache_type == NOCACHE) {
        InvalidateMetaCacheEntry(key);
        return base_adaptor_->Delete(key);
    } else {
        auto meta_cache_entry = GetMetaCacheEntry(key);
        auto size = meta_cache_entry->size;
        bool present = meta_cache_entry->present;
        bool has_write_cache = false;

        if (!present) {
            auto result = etcd_client_->GetJson(key).get();
            if (result.status == OK) {
                size = result.root["size"].asInt64();
                has_write_cache = true;
            } else if (result.status == NOT_FOUND) { // 只在 S3 里存储
                std::map<std::string, std::string> headers;
                int ret = base_adaptor_->Head(key, size, headers).get();
                if (ret) return ret;
            } else {
                return folly::makeFuture(result.status);
            }
        } 

        InvalidateMetaCacheEntry(key);
        
        if (has_write_cache) {
            return base_adaptor_->Delete(key)
                    .then(std::bind(&ReadCacheClient::Invalidate, read_cache_.get(), key, size))
                    .then(std::bind(&EtcdClient::DeleteJson, etcd_client_.get(), key));
        } else {
            return base_adaptor_->Delete(key)
                    .then(std::bind(&ReadCacheClient::Invalidate, read_cache_.get(), key, size));
        }
    }
}

struct DeepFlushArgs {
    DeepFlushArgs(const std::string &key) : key(key) {}
    ~DeepFlushArgs() { if (buffer.data) delete []buffer.data; }

    std::string key;
    std::map <std::string, std::string> headers;
    ByteBuffer buffer;
};

folly::Future<int> GlobalDataAdaptor::DeepFlush(const std::string &key) {
    butil::Timer *t = new butil::Timer();
    t->start();
    auto &policy = GetCachePolicy(key);
    if (policy.write_cache_type == REPLICATION || policy.write_cache_type == REED_SOLOMON) {
        auto args = std::make_shared<DeepFlushArgs>(key);
        return etcd_client_->GetJson(key).then([this, t, args](folly::Try<EtcdClient::GetResult> &&output) -> folly::Future<int> {
            if (!output.hasValue()) {
                return folly::makeFuture(FOLLY_ERROR);
            }
            if (output.value().status != OK) {
                return folly::makeFuture(output.value().status);
            }
            auto &root = output.value().root;
            args->buffer.len = root["size"].asInt64();
            args->buffer.data = new char[args->buffer.len];
            for (auto iter = root["headers"].begin(); iter != root["headers"].end(); iter++) {
                args->headers[iter.key().asString()] = (*iter).asString();
            }
            t->stop();
            LOG(INFO) << "DeepFlush phase 1: " << t->u_elapsed();

            return DownLoad(args->key, 0, args->buffer.len, args->buffer);
        }).then([this, t, args](folly::Try<int> &&output) -> folly::Future<int> {
            int res = output.value_or(FOLLY_ERROR);
            t->stop();
            LOG(INFO) << "DeepFlush phase 2: " << t->u_elapsed();
            if (res != OK) {
                return folly::makeFuture(res);
            } else {
                return base_adaptor_->UpLoad(args->key, args->buffer.len, args->buffer, args->headers);
            }
        }).then([this, t, key, args](folly::Try<int> &&output) -> folly::Future<int> {
            t->stop();
            LOG(INFO) << "DeepFlush phase 3: " << t->u_elapsed();
            int res = output.value_or(FOLLY_ERROR);
            if (res != OK) {
                return folly::makeFuture(res);
            } else {
                InvalidateMetaCacheEntry(key);
                return etcd_client_->DeleteJson(key);
            }
        });
    } else {
        t->stop();
        LOG(INFO) << "DeepFlush phase 4: " << t->u_elapsed();
        return folly::makeFuture(OK);
    }
}

struct HeadArgs {
    HeadArgs(const std::string &key, size_t &size, std::map <std::string, std::string> &headers)
            : key(key), size(size), headers(headers) {}

    std::string key;
    size_t &size;
    std::map <std::string, std::string> &headers;
};

folly::Future<int> GlobalDataAdaptor::Head(const std::string &key,
                                           size_t &size,
                                           std::map <std::string, std::string> &headers) {
    auto &policy = GetCachePolicy(key);
    auto meta_cache_entry = GetMetaCacheEntry(key);
    if (meta_cache_entry->present) {
        if (!meta_cache_entry->existed) {
            LOG(ERROR) << "Request for potential deleted file: " << key;
            return folly::makeFuture(NOT_FOUND);
        }
        size = meta_cache_entry->size;
        headers = meta_cache_entry->headers;
        return folly::makeFuture(OK);
    }

    if (policy.write_cache_type == REPLICATION || policy.write_cache_type == REED_SOLOMON) {
        auto args = std::make_shared<HeadArgs>(key, size, headers);
        return etcd_client_->GetJson(key).then([this, args, meta_cache_entry](folly::Try<EtcdClient::GetResult> &&output) -> folly::Future<int> {
            if (!output.hasValue()) {
                return folly::makeFuture(FOLLY_ERROR);
            }
            if (output.value().status != OK) {
                return folly::makeFuture(output.value().status);
            }
            auto &root = output.value().root;
            args->size = root["size"].asInt64();
            for (auto iter = root["headers"].begin(); iter != root["headers"].end(); iter++) {
                args->headers[iter.key().asString()] = (*iter).asString();
            }

            if (GetGlobalConfig().use_meta_cache) {
                meta_cache_entry->present = true;
                meta_cache_entry->existed = true;
                meta_cache_entry->write_cached = true;
                meta_cache_entry->size = args->size;
                meta_cache_entry->headers = args->headers;
                meta_cache_entry->root = output.value().root;
            }

            return folly::makeFuture(OK);
        }).then([this, args, meta_cache_entry](folly::Try<int> &&output) -> folly::Future<int> {
            int res = output.value_or(FOLLY_ERROR);
            if (res != NOT_FOUND) {
                return folly::makeFuture(res);
            } else {
                return base_adaptor_->Head(args->key, args->size, args->headers).thenValue([args, meta_cache_entry](int &&res) -> int {
                    if (GetGlobalConfig().use_meta_cache && (res == OK || res == NOT_FOUND)) {
                        meta_cache_entry->present = true;
                        meta_cache_entry->existed = (res == OK);
                        meta_cache_entry->write_cached = false;
                        meta_cache_entry->size = args->size;
                        meta_cache_entry->headers = args->headers;
                    }
                    return res;
                });
            }
        });
    } else {
        return base_adaptor_->Head(key, size, headers).thenValue([meta_cache_entry, &size, &headers](int &&res) -> int {
            if (GetGlobalConfig().use_meta_cache && (res == OK || res == NOT_FOUND)) {
                meta_cache_entry->present = true;
                meta_cache_entry->existed = (res == OK);
                meta_cache_entry->write_cached = false;
                meta_cache_entry->size = size;
                meta_cache_entry->headers = headers;
            }
            return res;
        });
    }
}

void GlobalDataAdaptor::InvalidateMetaCache() {
    std::lock_guard<std::mutex> lock(meta_cache_mutex_);
    meta_cache_.clear();
}

void GlobalDataAdaptor::InvalidateMetaCacheEntry(const std::string &key) {
    std::lock_guard<std::mutex> lock(meta_cache_mutex_);
    meta_cache_.erase(key);
}

std::shared_ptr<GlobalDataAdaptor::MetaCacheEntry> GlobalDataAdaptor::GetMetaCacheEntry(const std::string &key) {
    std::lock_guard<std::mutex> lock(meta_cache_mutex_);
    auto iter = meta_cache_.find(key);
    if (iter == meta_cache_.end()) {
        auto entry = std::make_shared<MetaCacheEntry>(key);
        meta_cache_.insert(key, entry);
        return entry;
    } else {
        return iter->second;
    }
}


void GlobalDataAdaptor::SetCachePolicy(const std::string &key, CachePolicy &policy) {
    // ...
}

const CachePolicy &GlobalDataAdaptor::GetCachePolicy(const std::string &key) const {
    return GetGlobalConfig().default_policy;
}

std::shared_ptr<GlobalCacheClient> GlobalDataAdaptor::GetRpcClient() const {
    return rpc_client_[lrand48() % rpc_client_.size()];
}

int GlobalDataAdaptor::PerformGarbageCollection(const std::string &prefix) {
    LOG(INFO) << "==================GC START===================";
    butil::Timer t;
    t.start();

    std::vector<uint64_t> write_cache_ts;
    std::set<int> skipped_server_id_list;
    for (int server_id = 0; server_id < server_list_.size(); ++server_id) {
        auto res = GetRpcClient()->QueryTsFromWriteCache(server_id).get();
        if (res.status != OK) {
            std::cerr << RED << "Skip recycling write cache data in server " << server_id << WHITE << std::endl;
            skipped_server_id_list.insert(server_id);
        }
        write_cache_ts.push_back(res.timestamp);
        LOG(INFO) << "TS for server " << server_id << ": " << res.timestamp;
    }

    t.stop();
    LOG(INFO) << "Flush stage 1: " << t.u_elapsed();

    if (server_list_.size() == skipped_server_id_list.size()) {
        std::cerr << RED << "All servers are not available." << WHITE << std::endl;
        return RPC_FAILED;
    }

    std::vector<std::string> key_list;
    int rc = etcd_client_->ListJson(prefix, key_list).get();
    if (rc) {
        std::cerr << RED << "Failed to list metadata in write cache. "
                  << "Check the availability of etcd server." << WHITE << std::endl;
        return rc;
    }

    for (auto &key : key_list) {
        LOG(INFO) << "Found entry: " << key;
    }

    t.stop();
    LOG(INFO) << "Flush stage 2: " << t.u_elapsed();
    
    std::vector<folly::Future<int>> future_list;
    for (auto &key : key_list) {
        future_list.emplace_back(DeepFlush(key));
    }

    auto output = folly::collectAll(future_list).get();
    for (auto &entry: output)
        if (entry.value_or(FOLLY_ERROR) != OK) {
            LOG(ERROR) << "Cannot flush data to S3 storage";
        }

    t.stop();
    LOG(INFO) << "Flush stage 3: " << t.u_elapsed();

    // Recheck the JSON metadata from etcd server
    rc = etcd_client_->ListJson(prefix, key_list).get();
    if (rc != 0 && rc != NOT_FOUND) {
        return rc;
    }

    t.stop();
    LOG(INFO) << "Flush stage 4: " << t.u_elapsed();

    std::unordered_map<int, std::vector<std::string>> preserve_chunk_keys_map;
    for (auto &key : key_list) {
        auto resp = etcd_client_->GetJson(key).get();
        if (resp.status) {
            continue;
        }

        std::vector<int> replicas;
        for (auto &entry : resp.root["replica"]) {
            replicas.push_back(entry.asInt());
        }

        std::vector<std::string> internal_keys;
        for (auto &entry : resp.root["path"]) {
            internal_keys.push_back(entry.asString());
        }

        assert(!replicas.empty() && !internal_keys.empty());
        for (int i = 0; i < internal_keys.size(); ++i) {
            preserve_chunk_keys_map[replicas[i % replicas.size()]].push_back(internal_keys[i]);
        }
    }

    for (int server_id = 0; server_id < server_list_.size(); ++server_id) {
        if (skipped_server_id_list.count(server_id)) {
            continue;
        }

        std::vector<std::string> except_keys;
        if (preserve_chunk_keys_map.count(server_id)) {
            except_keys = preserve_chunk_keys_map[server_id];
        }

        int rc = GetRpcClient()->DeleteEntryFromWriteCache(server_id, 
                                                           prefix, 
                                                           write_cache_ts[server_id],
                                                           except_keys).get();
        if (rc) {
            LOG(WARNING) << "Cannot delete unused entries from write cache. Server id: " << server_id;
        }
    }

    t.stop();
    LOG(INFO) << "Flush stage 5: " << t.u_elapsed();

    LOG(INFO) << "==================GC END===================";
    return 0;
}

folly::Future<int> GlobalDataAdaptor::UpLoadPart(const std::string &key,
                                                 size_t off,
                                                 size_t size,
                                                 const ByteBuffer &buffer,
                                                 const std::map<std::string, std::string> &headers,
                                                 Json::Value& root) {
#ifdef CONFIG_GC_ON_EXCEEDING_DISKSPACE
    return DoUpLoadPart(key, off, size, buffer, headers, root)
            .thenValue([this, key, off, size, &buffer, &headers, &root](int &&res) -> int {
        if (res != NO_ENOUGH_DISKSPACE) {
            return res;
        }
        LOG(INFO) << "Disk limit exceeded - perform GC immediately";
        res = PerformGarbageCollection();
        if (res) {
            LOG(WARNING) << "GC failed";
            return res;
        }
        LOG(INFO) << "Disk limit exceeded - GC completed, now retry";
        return DoUpLoadPart(key, off, size, buffer, headers, root).get();
    });
#else
    return DoUpLoadPart(key, off, size, buffer, headers, root);
#endif
}

folly::Future<int> GlobalDataAdaptor::DoUpLoadPart(const std::string &key,
                                                   size_t off,
                                                   size_t size,
                                                   const ByteBuffer &buffer,
                                                   const std::map<std::string, std::string> &headers,
                                                   Json::Value& root) {
    butil::Timer *t = new butil::Timer();
    t->start();
    auto &policy = GetCachePolicy(key);
    auto pre_op = read_cache_->Invalidate(key, off + size);
    if (policy.write_cache_type == REPLICATION || policy.write_cache_type == REED_SOLOMON) {
        auto write_cache = policy.write_cache_type == REPLICATION
                           ? write_caches_[WC_TYPE_REPLICATION]
                           : write_caches_[WC_TYPE_REEDSOLOMON];
        return std::move(pre_op)
            .then(std::bind(&WriteCacheClient::Put, write_cache.get(), key, size, buffer, headers, off))
            .then([this, t, &root] (folly::Try<WriteCacheClient::PutResult> output) -> folly::Future<int> {
                int status = output.hasValue() ? output.value().status : FOLLY_ERROR;
                if (status == OK) {
                    root = std::move(output.value().root);
                    t->stop();
                    delete t;
                }
                return folly::makeFuture(status);
            });
    } else {
        LOG(ERROR) << "Failed to upload data, reason: unsuppported type, key: " << key
                   << ", size: " << size
                   << ", type: " << policy.write_cache_type;
        return folly::makeFuture(UNSUPPORTED_TYPE);
    }
}

folly::Future<int> GlobalDataAdaptor::Completed(const std::string &key,
                                                const std::vector<Json::Value> &roots,
                                                size_t size) {
    if (!roots.empty()) {
        auto meta_cache_entry = GetMetaCacheEntry(key);
        meta_cache_entry->present = false;

        Json::Value json_path(Json::arrayValue);
        for (int i=0; i<roots.size(); ++i) {
            for (auto& partial_key : roots[i]["path"])
                json_path.append(partial_key.asString());
        }
        Json::Value new_root = roots[0];
        new_root["path"] = json_path;
        new_root["size"] = size;

        return etcd_client_->PutJson(key, new_root);
    }
    return folly::makeFuture(OK);
}
