#include "ReplicationWriteCacheClient.h"
#include "GlobalDataAdaptor.h"

folly::Future<PutResult> ReplicationWriteCacheClient::Put(const std::string &key,
                                                          size_t size,
                                                          const ByteBuffer &buffer,
                                                          const std::map <std::string, std::string> &headers,
                                                          size_t off) {
    std::vector <folly::Future<PutOutput>> future_list;
    Json::Value root, dummy_root;
    Json::Value json_replica(Json::arrayValue), json_path(Json::arrayValue), json_headers;

    butil::Timer *t = new butil::Timer();
    t->start();

    const std::vector<int> replicas = GetReplica(key);
    for (auto server_id: replicas) {
        json_replica.append(server_id);
    }

    auto rpc_client = parent_->GetRpcClient();
    auto write_chunk_size = GetGlobalConfig().write_chunk_size;

    for (auto iter = headers.begin(); iter != headers.end(); ++iter) {
        json_headers[iter->first] = iter->second;
    }

    size_t aggregated_size = 0;
    for (uint64_t offset = 0; offset < size; offset += write_chunk_size) {
        for (auto server_id: replicas) {
            auto region_size = std::min(size - offset, write_chunk_size);
            ByteBuffer region_buffer(buffer.data + offset, region_size);
            std::string partial_key = key
                                      + "-" + std::to_string((off + offset) / write_chunk_size)
                                      + "-" + std::to_string(write_chunk_size);
            auto PutRPC = folly::via(parent_->executor_.get(), [this, server_id, partial_key, region_buffer, region_size]() -> PutOutput {
                return parent_->GetRpcClient()->PutEntryFromWriteCache(server_id, partial_key, region_buffer, region_size).get();
            });
            future_list.emplace_back(std::move(PutRPC));
        }
    }

    t->stop();
    LOG(INFO) << "Phase 1: " << t->u_elapsed();

    root["type"] = "replication";
    root["size"] = size;
    root["replica"] = json_replica;
    root["headers"] = json_headers;

    return folly::collectAll(future_list).via(parent_->executor_.get()).thenValue([root, t](std::vector<folly::Try<PutOutput>> &&output) -> folly::Future<PutResult> {
        Json::Value dummy_root;
        Json::Value json_path(Json::arrayValue);
        for (auto &entry: output) {
            if (!entry.hasValue())
                return PutResult { FOLLY_ERROR, dummy_root };
            if (entry.value().status != OK) {
                LOG(INFO) << "Found error";
                return PutResult { entry.value().status, dummy_root };
            }
            json_path.append(entry.value().internal_key);
        }
        Json::Value new_root = root;
        new_root["path"] = json_path;
        t->stop();
        LOG(INFO) << "Duration: " << t->u_elapsed();
        delete t;
        return PutResult { OK, new_root };
    });
}

folly::Future<int> ReplicationWriteCacheClient::Get(const std::string &key,
                                                    size_t start,
                                                    size_t size,
                                                    ByteBuffer &buffer, 
                                                    Json::Value &root) {
    std::vector<int> replicas;
    for (auto &entry : root["replica"]) {
        replicas.push_back(entry.asInt());
    }

    std::vector<std::string> internal_keys;
    for (auto &entry : root["path"]) {
        internal_keys.push_back(entry.asString());
    }

    std::vector <folly::Future<int>> future_list;
    std::vector<GetChunkRequestV2> requests;
    auto write_chunk_size = GetGlobalConfig().write_chunk_size;
    GenerateGetChunkRequestsV2(key, start, size, buffer, requests, write_chunk_size);
    if (requests.empty())
        return folly::makeFuture(OK);

    size_t aggregated_size = 0;
    for (auto &entry: requests) {
        int primary_replica_id  = lrand48() % replicas.size();
        int primary_server_id = replicas[primary_replica_id];
        std::string internal_key = internal_keys[entry.chunk_id * replicas.size() + primary_replica_id];
        future_list.emplace_back(GetChunkAsync(primary_server_id, entry, internal_key)
            .thenValue([this, replicas, entry, primary_server_id, internal_keys](int res) -> int {
            if (res != RPC_FAILED) {
                return res;
            }
            LOG_EVERY_SECOND(WARNING) << "Unable to connect primary replicas. server_id " << primary_server_id
                                        << ", hostname: " << parent_->GetServerHostname(primary_server_id);
            for (auto &server_id : replicas) {
                if (server_id == primary_server_id) {
                    continue;
                }
                auto internal_key = internal_keys[entry.chunk_id * replicas.size() + server_id];
                res = GetChunkAsync(server_id, entry, internal_key).get();
                if (res != RPC_FAILED) {
                    return res;
                }
                LOG_EVERY_SECOND(WARNING) << "Unable to connect secondary replicas. server_id " << server_id
                                            << ", hostname: " << parent_->GetServerHostname(server_id);
            }
            LOG_EVERY_SECOND(ERROR) << "Unable to connect all target replicas";
            return RPC_FAILED;
        }));

        aggregated_size += entry.chunk_len;
        if (aggregated_size >= GetGlobalConfig().max_inflight_payload_size) {
            auto output = folly::collectAll(future_list).get();
            for (auto &entry: output)
                if (entry.value_or(FOLLY_ERROR) != OK) {
                    LOG(ERROR) << "Failed to get data from write cache, key: " << key
                                << ", start: " << start
                                << ", size: " << size
                                << ", buf: " << (void *) buffer.data << " " << buffer.len
                                << ", error code: " << entry.hasValue() << " "  << entry.value_or(FOLLY_ERROR);
                    return entry.value_or(FOLLY_ERROR);
                }
            aggregated_size = 0;
            future_list.clear();
        }
    }

    return folly::collectAll(future_list).via(parent_->executor_.get()).thenValue(
            [=](std::vector <folly::Try<int>> output) -> int {
                for (auto &entry: output)
                    if (entry.value_or(FOLLY_ERROR) != OK) {
                        LOG(ERROR) << "Failed to get data from write cache, key: " << key
                                   << ", start: " << start
                                   << ", size: " << size
                                   << ", buf: " << (void *) buffer.data << " " << buffer.len
                                   << ", error code: " << entry.hasValue() << " "  << entry.value_or(FOLLY_ERROR);
                        return entry.value_or(FOLLY_ERROR);
                    }
                return OK;
            });

    // return parent_->GetRpcClient()->GetEntryFromWriteCache(replica[primary_index], internal_keys[primary_index], start, size).thenValue(
    //     [this, &buffer, start, size, replica, internal_keys, primary_index](GetOutput &&output) -> int {
    //         if (output.status == OK) {
    //             output.buf.copy_to(buffer.data, size);
    //         }
    //         if (output.status == RPC_FAILED) {
    //             for (int index = 0; index < replica.size(); ++index) {
    //                 if (index == primary_index) {
    //                     continue;
    //                 }
    //                 auto res = parent_->GetRpcClient()->GetEntryFromWriteCache(replica[index], internal_keys[index], start, size).get();
    //                 if (res.status == OK) {
    //                     output.buf.copy_to(buffer.data, size);
    //                 }
    //                 if (res.status != RPC_FAILED) {
    //                     return res.status;
    //                 }
    //             }
    //             LOG(ERROR) << "All target replicas are crashed";
    //             return RPC_FAILED;
    //         }
    //         return output.status;
    //     }
    // );
}

folly::Future<int> ReplicationWriteCacheClient::GetChunkAsync(int server_id, GetChunkRequestV2 request, std::string &internal_key) {
    LOG_IF(INFO, FLAGS_verbose) << "GetChunkAsync server_id=" << server_id
                                << ", internal_key=" << internal_key
                                << ", chunk_id=" << request.chunk_id
                                << ", chunk_start=" << request.chunk_start
                                << ", chunk_len=" << request.chunk_len
                                << ", buffer=" << (void *) request.buffer.data;
    auto f = parent_->GetRpcClient()->GetEntryFromWriteCache(server_id, internal_key, request.chunk_start, request.chunk_len)
        .then([this, server_id, request](folly::Try<GetOutput> &&output) -> folly::Future<int> {
            if (!output.hasValue()) {
                return folly::makeFuture(FOLLY_ERROR);
            }
            auto &value = output.value();
            if (value.status == OK) {
                value.buf.copy_to(request.buffer.data, request.buffer.len);
                return folly::makeFuture(OK);
            } else {
                return folly::makeFuture(value.status);
            }
        }).via(parent_->executor_.get());
    return f;
    // memset(request.buffer.data, 'x', request.buffer.len);
    // return folly::makeFuture(OK);
}

std::vector<int> ReplicationWriteCacheClient::GetReplica(const std::string &key) {
    const int num_available = parent_->server_list_.size();
    auto &policy = parent_->GetCachePolicy(key);
    const int num_choose = policy.write_replication_factor;
    uint64_t seed = std::hash < std::string > {}(key);
    std::vector<int> output;
    for (int i = 0; i < num_choose; ++i)
        output.push_back((seed + i) % num_available);
    return output;
}

void ReplicationWriteCacheClient::GenerateGetChunkRequestsV2(const std::string &key, 
                                                             size_t start, 
                                                             size_t size, 
                                                             ByteBuffer &buffer, 
                                                             std::vector<GetChunkRequestV2> &requests,
                                                             size_t chunk_size) {
    const size_t end = start + size;

    const size_t begin_chunk_id = start / chunk_size;
    const size_t end_chunk_id = (end + chunk_size - 1) / chunk_size;

    if (buffer.len < size) {
        LOG(WARNING) << "Buffer capacity may be not enough, expect " << size << ", actual " << buffer.len;
    }

    size_t buffer_offset = 0;
    for (size_t chunk_id = begin_chunk_id; chunk_id < end_chunk_id; ++chunk_id) {
        size_t chunk_start = std::max(chunk_id * chunk_size, start);
        size_t chunk_stop = std::min((chunk_id + 1) * chunk_size, end);
        if (chunk_stop <= chunk_start)
            return;
        GetChunkRequestV2 item;
        item.user_key = key;
        item.chunk_id = chunk_id;
        item.chunk_start = chunk_start % chunk_size;
        item.chunk_len = chunk_stop - chunk_start;
        item.chunk_granularity = chunk_size;
        item.buffer.data = buffer.data + buffer_offset;
        item.buffer.len = item.chunk_len;
        buffer_offset += item.chunk_len;
        requests.emplace_back(item);
    }
    LOG_ASSERT(buffer_offset == size);
}
