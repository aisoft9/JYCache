#include <gflags/gflags.h>
#include <butil/logging.h>
#include <butil/time.h>
#include <map>

#include "gcache.pb.h"
#include "GlobalCacheClient.h"

GlobalCacheClient::GlobalCacheClient(const std::string &group) : group_(group), inflight_payload_size_(0) {}

GlobalCacheClient::~GlobalCacheClient() {
    for (auto &entry: server_map_) {
        delete entry.second;
    }
    server_map_.clear();
}

int GlobalCacheClient::RegisterServer(int server_id, const char *hostname) {
    std::lock_guard <std::mutex> lock(mutex_);
    if (server_map_.count(server_id)) {
        LOG(WARNING) << "Server has been registered, previous regitration will be override"
                     << ", group: " << group_
                     << ", server_id: " << server_id
                     << ", hostname: " << hostname;
    }

    brpc::ChannelOptions options;
    options.use_rdma = GetGlobalConfig().use_rdma;
    options.timeout_ms = GetGlobalConfig().rpc_timeout;
    options.connection_group = group_;

    int32_t fixed_backoff_time_ms = 100;                // 固定时间间隔（毫秒）
    int32_t no_backoff_remaining_rpc_time_ms = 150;     // 无需重试退避的剩余rpc时间阈值（毫秒）
    bool retry_backoff_in_pthread = false;
    static brpc::RpcRetryPolicyWithFixedBackoff g_retry_policy_with_fixed_backoff(
            fixed_backoff_time_ms, no_backoff_remaining_rpc_time_ms, retry_backoff_in_pthread);
    options.retry_policy = &g_retry_policy_with_fixed_backoff;
    options.max_retry = 5;


    auto channel = new brpc::Channel();
    if (channel->Init(hostname, &options)) {
        PLOG(ERROR) << "Unable to initialize channel object"
                    << ", group: " << group_
                    << ", server_id: " << server_id
                    << ", hostname: " << hostname;
        delete channel;
        return RPC_FAILED;
    }

    // Sending sync register RPC
    gcache::GlobalCacheService_Stub stub(channel);
    brpc::Controller cntl;
    gcache::RegisterRequest request;
    gcache::RegisterResponse response;
    stub.Register(&cntl, &request, &response, nullptr);
    if (cntl.Failed() || response.status_code() != OK) {
        LOG(ERROR) << "Failed to register server, reason: " << cntl.ErrorText()
                   << ", group: " << group_
                   << ", server_id: " << server_id
                   << ", hostname: " << hostname;
        delete channel;
        return RPC_FAILED;
    }

    LOG_IF(INFO, FLAGS_verbose) << "Register server successfully"
                                << ", group: " << group_
                                << ", server_id: " << server_id
                                << ", hostname: " << hostname;

    server_map_[server_id] = channel;
    return OK;
}

brpc::Channel *GlobalCacheClient::GetChannelByServerId(int server_id) {
    std::lock_guard <std::mutex> lock(mutex_);
    if (!server_map_.count(server_id)) {
        LOG_EVERY_SECOND(ERROR) << "Server not registered. server_id: " << server_id;
        return nullptr;
    }
    return server_map_[server_id];
}

Future<GetOutput> GlobalCacheClient::GetEntry(int server_id, 
                                              const std::string &key, 
                                              uint64_t start, 
                                              uint64_t length, 
                                              bool is_read_cache) {
    // while (inflight_payload_size_.load() >= GetGlobalConfig().max_inflight_payload_size) {
    //     LOG_EVERY_SECOND(INFO) << "Overcroweded " << inflight_payload_size_.load();
    //     sched_yield();
    // }
    inflight_payload_size_.fetch_add(length);

    auto channel = GetChannelByServerId(server_id);
    if (!channel) {
        GetOutput output;
        output.status = RPC_FAILED;
        return folly::makeFuture(output);
    }

    gcache::GlobalCacheService_Stub stub(channel);
    gcache::GetEntryRequest request;
    request.set_key(key);
    request.set_start(start);
    request.set_length(length);

    struct OnRPCDone : public google::protobuf::Closure {
        virtual void Run() {
            GetOutput output;
            if (cntl.Failed()) {
                LOG(WARNING) << "RPC error: " << cntl.ErrorText()
                             << ", server id: " << server_id
                             << ", key: " << key
                             << ", start: " << start
                             << ", length: " << length;
                output.status = RPC_FAILED;
            } else {
                output.status = response.status_code();
                output.buf = cntl.response_attachment();
                if (output.status == OK && output.buf.length() != length) {
                    LOG(WARNING) << "Received truncated attachment, expected " << length
                                 << " bytes, actual " << output.buf.length() << " bytes"
                                 << ", server id: " << server_id
                                 << ", key: " << key
                                 << ", start: " << start
                                 << ", length: " << length;
                    output.status = RPC_FAILED;
                }
            }
            promise.setValue(output);
            parent->inflight_payload_size_.fetch_sub(length);
            t.stop();
            LOG_EVERY_N(INFO, 1000) << t.u_elapsed();
            delete this;
        }

        brpc::Controller cntl;
        gcache::GetEntryResponse response;
        Promise<GetOutput> promise;

        int server_id;
        std::string key;
        uint64_t start;
        uint64_t length;
        GlobalCacheClient *parent;
        butil::Timer t;
    };

    auto done = new OnRPCDone();
    done->t.start();
    done->parent = this;
    done->server_id = server_id;
    done->key = key;
    done->start = start;
    done->length = length;

    auto future = done->promise.getFuture();
    if (is_read_cache)
        stub.GetEntryFromReadCache(&done->cntl, &request, &done->response, done);
    else
        stub.GetEntryFromWriteCache(&done->cntl, &request, &done->response, done);
    return std::move(future);
}

Future<PutOutput> GlobalCacheClient::PutEntry(int server_id, 
                                              const std::string &key, 
                                              const ByteBuffer &buf, 
                                              uint64_t length,
                                              bool is_read_cache) {
    // while (inflight_payload_size_.load() >= GetGlobalConfig().max_inflight_payload_size) {
    //     LOG_EVERY_SECOND(INFO) << "Overcroweded " << inflight_payload_size_.load();
    //     sched_yield();
    // }
    inflight_payload_size_.fetch_add(length);

    auto channel = GetChannelByServerId(server_id);
    if (!channel) {
        PutOutput output;
        output.status = RPC_FAILED;
        return folly::makeFuture(output);
    }

    gcache::GlobalCacheService_Stub stub(channel);
    gcache::PutEntryRequest request;
    request.set_key(key);
    request.set_length(length);

    struct OnRPCDone : public google::protobuf::Closure {
        virtual void Run() {
            PutOutput output;
            if (cntl.Failed()) {
                LOG(WARNING) << "RPC error: " << cntl.ErrorText()
                             << ", server id: " << server_id
                             << ", key: " << key
                             << ", length: " << length;
                output.status = RPC_FAILED;
            } else {
                output.status = response.status_code();
                output.internal_key = response.internal_key();
            }
            promise.setValue(output);
            parent->inflight_payload_size_.fetch_sub(length);
            delete this;
        }

        brpc::Controller cntl;
        gcache::PutEntryResponse response;
        Promise<PutOutput> promise;

        int server_id;
        std::string key;
        uint64_t length;
        GlobalCacheClient *parent;
    };

    auto done = new OnRPCDone();
    done->parent = this;
    done->server_id = server_id;
    done->key = key;
    done->length = length;

    done->cntl.request_attachment().append(buf.data, length);
    auto future = done->promise.getFuture();
    if (is_read_cache)
        stub.PutEntryFromReadCache(&done->cntl, &request, &done->response, done);
    else
        stub.PutEntryFromWriteCache(&done->cntl, &request, &done->response, done);
    return std::move(future);
}

Future<int> GlobalCacheClient::DeleteEntryFromReadCache(int server_id, 
                                                        const std::string &key, 
                                                        uint64_t chunk_size, 
                                                        uint64_t max_chunk_id) {
    auto channel = GetChannelByServerId(server_id);
    if (!channel) {
        LOG(ERROR) << "Cannot find channel for server " << server_id;
        return folly::makeFuture(RPC_FAILED);
    }

    gcache::GlobalCacheService_Stub stub(channel);
    gcache::DeleteEntryRequest request;
    request.set_key(key);
    request.set_chunk_size(chunk_size);
    request.set_max_chunk_id(max_chunk_id);

    struct OnRPCDone : public google::protobuf::Closure {
        virtual void Run() {
            int status;
            if (cntl.Failed()) {
                LOG(WARNING) << "RPC error: " << cntl.ErrorText()
                             << ", server id: " << server_id
                             << ", key: " << key;
                status = RPC_FAILED;
            } else {
                status = response.status_code();
            }
            promise.setValue(status);
            delete this;
        }

        brpc::Controller cntl;
        gcache::DeleteEntryResponse response;
        Promise<int> promise;

        int server_id;
        std::string key;
    };

    auto done = new OnRPCDone();
    done->server_id = server_id;
    done->key = key;

    auto future = done->promise.getFuture();
    stub.DeleteEntryFromReadCache(&done->cntl, &request, &done->response, done);
    return std::move(future);
}

Future<QueryTsOutput> GlobalCacheClient::QueryTsFromWriteCache(int server_id) {
    auto channel = GetChannelByServerId(server_id);
    if (!channel) {
        QueryTsOutput output;
        output.status = RPC_FAILED;
        return folly::makeFuture(output);
    }

    gcache::GlobalCacheService_Stub stub(channel);
    gcache::QueryTsRequest request;

    struct OnRPCDone : public google::protobuf::Closure {
        virtual void Run() {
            QueryTsOutput output;
            if (cntl.Failed()) {
                LOG(WARNING) << "RPC error: " << cntl.ErrorText()
                             << ", server id: " << server_id;
                output.status = RPC_FAILED;
            } else {
                output.status = response.status_code();
                output.timestamp = response.timestamp();
            }
            promise.setValue(output);
            delete this;
        }

        brpc::Controller cntl;
        gcache::QueryTsResponse response;
        Promise<QueryTsOutput> promise;

        int server_id;
    };

    auto done = new OnRPCDone();
    done->server_id = server_id;

    auto future = done->promise.getFuture();
    stub.QueryTsFromWriteCache(&done->cntl, &request, &done->response, done);
    return std::move(future);
}

Future<int> GlobalCacheClient::DeleteEntryFromWriteCache(int server_id, 
                                                         const std::string &key_prefix, 
                                                         uint64_t max_ts, 
                                                         std::vector<std::string> &except_keys) {
    auto channel = GetChannelByServerId(server_id);
    if (!channel) {
        LOG(ERROR) << "Cannot find channel for server " << server_id;
        return folly::makeFuture(RPC_FAILED);
    }

    gcache::GlobalCacheService_Stub stub(channel);
    gcache::DeleteEntryRequestForWriteCache request;
    request.set_key_prefix(key_prefix);
    request.set_max_ts(max_ts);
    for (auto &entry : except_keys)
        request.add_except_keys(entry);

    struct OnRPCDone : public google::protobuf::Closure {
        virtual void Run() {
            int status;
            if (cntl.Failed()) {
                LOG(WARNING) << "RPC error: " << cntl.ErrorText()
                             << ", server id: " << server_id
                             << ", key: " << key;
                status = RPC_FAILED;
            } else {
                status = response.status_code();
            }
            promise.setValue(status);
            delete this;
        }

        brpc::Controller cntl;
        gcache::DeleteEntryResponse response;
        Promise<int> promise;

        int server_id;
        std::string key;
    };

    auto done = new OnRPCDone();
    done->server_id = server_id;
    done->key = key_prefix;

    auto future = done->promise.getFuture();
    stub.DeleteEntryFromWriteCache(&done->cntl, &request, &done->response, done);
    return std::move(future);
}