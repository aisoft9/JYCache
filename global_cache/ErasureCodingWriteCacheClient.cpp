#include "ErasureCodingWriteCacheClient.h"
#include "GlobalDataAdaptor.h"

// #define CONFIG_JERASURE

#ifdef CONFIG_JERASURE
#include <jerasure.h>
#include <jerasure/reed_sol.h>

static int _roundup(int a, int b) {
    if (a % b == 0) return a;
    return a + b - (a % b);
}

folly::Future<PutResult> ErasureCodingWriteCacheClient::Put(const std::string &key,
                                                            size_t size,
                                                            const ByteBuffer &buffer,
                                                            const std::map <std::string, std::string> &headers,
                                                            size_t off) {
    std::vector <folly::Future<PutOutput>> future_list;
    Json::Value root;
    Json::Value json_replica(Json::arrayValue), json_headers;

    const std::vector<int> replicas = GetReplica(key);
    for (auto server_id: replicas) {
        json_replica.append(server_id);
    }

    auto &policy = parent_->GetCachePolicy(key);
    const int k = policy.write_data_blocks;
    const int m = policy.write_parity_blocks;
    const int w = 32;
    auto matrix = reed_sol_vandermonde_coding_matrix(k, m, w);
    std::vector<char *> data_buf_list;
    auto rpc_client = parent_->GetRpcClient();
    auto write_chunk_size = GetGlobalConfig().write_chunk_size;
    for (uint64_t offset = 0; offset < size; offset += write_chunk_size) {
        const auto unit_size = _roundup((write_chunk_size + k - 1) / k, w);
        const auto region_size = std::min(write_chunk_size, size - offset);
        char *data_buf = new char[(k + m) * unit_size];
        data_buf_list.push_back(data_buf);
        memcpy(data_buf, &buffer.data[offset], region_size);
        memset(data_buf + region_size, 0, k * unit_size - region_size);
        char *data_ptrs[k] = { nullptr }, *coding_ptrs[m] = { nullptr };
        for (int i = 0; i < k + m; ++i) {
            if (i < k) {
                data_ptrs[i] = &data_buf[i * unit_size];
            } else {
                coding_ptrs[i - k] = &data_buf[i * unit_size];
            }
        }
        jerasure_matrix_encode(k, m, w, matrix, data_ptrs, coding_ptrs, unit_size);
        auto cur_data_buf = data_buf;
        for (auto server_id: replicas) {
            ByteBuffer region_buffer(cur_data_buf, unit_size);
            cur_data_buf += unit_size;
            std::string partial_key = key
                                      + "-" + std::to_string(offset / write_chunk_size)
                                      + "-" + std::to_string(write_chunk_size);
            future_list.emplace_back(rpc_client->PutEntryFromWriteCache(server_id, partial_key, region_buffer, unit_size));
        }
    }
    for (auto iter = headers.begin(); iter != headers.end(); ++iter) {
        json_headers[iter->first] = iter->second;
    }

    root["type"] = "reed-solomon";
    root["size"] = size;
    root["replica"] = json_replica;
    root["headers"] = json_headers;

    return folly::collectAll(future_list).via(parent_->executor_.get()).thenValue(
        [this, root, data_buf_list, matrix](std::vector <folly::Try<PutOutput>> output) -> PutResult {
            free(matrix);
            for (auto &entry : data_buf_list) {
                delete []entry;
            }
            Json::Value res_root;
            Json::Value json_path(Json::arrayValue);
            for (auto &entry: output) {
                if (!entry.hasValue())
                    return PutResult { FOLLY_ERROR, res_root };
                if (entry.value().status != OK)
                    return PutResult { entry.value().status, res_root };
                json_path.append(entry.value().internal_key);
            }
            res_root = root;
            res_root["path"] = json_path;
            return PutResult { OK, res_root };
        });
}

folly::Future<int> ErasureCodingWriteCacheClient::Get(const std::string &key,
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

    for (auto &entry: requests) {
        auto &policy = parent_->GetCachePolicy(key);
        const int k = policy.write_data_blocks;
        const int m = policy.write_parity_blocks;
        const int w = 32;
        const auto unit_size = _roundup((write_chunk_size + k - 1) / k, w);
        const auto start_replica_id = entry.chunk_start / unit_size;
        const auto end_replica_id = (entry.chunk_start + entry.chunk_len + unit_size - 1) / unit_size;
        size_t dest_buf_pos = 0;
        for (auto replica_id = start_replica_id; replica_id < end_replica_id; ++replica_id) {
            auto start_off = (replica_id == start_replica_id) ? entry.chunk_start % unit_size : 0;
            auto end_off = (replica_id + 1 == end_replica_id) ? (entry.chunk_start + entry.chunk_len) - replica_id * unit_size : unit_size;
            int server_id = replicas[replica_id];
	        std::string internal_key = internal_keys[entry.chunk_id * replicas.size() + replica_id];
            auto cur_dest_buf_pos = dest_buf_pos;
            dest_buf_pos += (end_off - start_off);
            future_list.emplace_back(parent_->GetRpcClient()->GetEntryFromWriteCache(server_id, internal_key, start_off, end_off - start_off)
                .then([this, server_id, entry, start_off, end_off, cur_dest_buf_pos](folly::Try<GetOutput> &&output) -> folly::Future<int> {
                    if (!output.hasValue()) {
                        return folly::makeFuture(FOLLY_ERROR);
                    }
                    auto &value = output.value();
                    if (value.status == OK) {
                        value.buf.copy_to(entry.buffer.data + cur_dest_buf_pos, end_off - start_off);
                        return folly::makeFuture(OK);
                    } else {
                        return folly::makeFuture(value.status);
                    }
                }));
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
}


folly::Future<int> ErasureCodingWriteCacheClient::GetDecode(const std::string &key,
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

    std::vector<GetChunkRequestV2> requests;
    auto write_chunk_size = GetGlobalConfig().write_chunk_size;
    GenerateGetChunkRequestsV2(key, start, size, buffer, requests, write_chunk_size);
    if (requests.empty())
        return folly::makeFuture(OK);

    std::vector <folly::Future<GetOutput>> future_list;

    for (auto &entry: requests) {
        auto &policy = parent_->GetCachePolicy(key);
        const int k = policy.write_data_blocks;
        const int m = policy.write_parity_blocks;
        const int w = 32;
        auto matrix = reed_sol_vandermonde_coding_matrix(k, m, w);
        const auto unit_size = _roundup((write_chunk_size + k - 1) / k, w);
        const auto start_replica_id = entry.chunk_start / unit_size;
        const auto end_replica_id = (entry.chunk_start + entry.chunk_len + unit_size - 1) / unit_size;
        int erasures[k + m + 1] = { 0 };
        int erasures_idx = 0;

        char *data_buf = new char[(k + m) * unit_size];
        char *data_ptrs[k] = { nullptr }, *coding_ptrs[m] = { nullptr };
        for (int i = 0; i < k + m; ++i) {
            if (i < k) {
                data_ptrs[i] = &data_buf[i * unit_size];
            } else {
                coding_ptrs[i - k] = &data_buf[i * unit_size];
            }
        }

        // rarely occurred, can be synchronized
        for (auto replica_id = 0; replica_id < k + m; ++replica_id) {
            int server_id = replicas[replica_id];
	        std::string internal_key = internal_keys[entry.chunk_id * replicas.size() + replica_id];
            auto output = parent_->GetRpcClient()->GetEntryFromWriteCache(server_id, internal_key, 0, unit_size).get();
            if (output.status == OK) {
                if (replica_id < k) {
                    output.buf.copy_to(data_ptrs[replica_id], unit_size);
                } else {
                    output.buf.copy_to(coding_ptrs[replica_id - k], unit_size);
                }
            } else {
                erasures[erasures_idx++] = replica_id;
            }
        }

        erasures[erasures_idx] = -1;

        int rc = jerasure_matrix_decode(k, m, w, matrix, 1, erasures, data_ptrs, coding_ptrs, unit_size);
        if (rc == -1) {
            LOG(FATAL) << "Unable to decode RS matrix";
            return IO_ERROR;
        }

        auto cur_pos = 0;
        for (auto replica_id = start_replica_id; replica_id < end_replica_id; ++replica_id) {
            auto start_pos = (replica_id == start_replica_id) ? entry.chunk_start % unit_size : 0;
            auto end_pos = (replica_id + 1 == end_replica_id) ? (entry.chunk_start + entry.chunk_len) - replica_id * unit_size : unit_size;
            memcpy(entry.buffer.data + cur_pos, data_ptrs[replica_id] + start_pos, end_pos - start_pos);
            cur_pos += end_pos - start_pos;
        }

        delete []data_buf;
        free(matrix);
    }

    return OK;
}

std::vector<int> ErasureCodingWriteCacheClient::GetReplica(const std::string &key) {
    const int num_available = parent_->server_list_.size();
    auto &policy = parent_->GetCachePolicy(key);
    const int num_choose = policy.write_data_blocks + policy.write_parity_blocks;
    uint64_t seed = std::hash < std::string > {}(key);
    std::vector<int> output;
    // for (int i = 0; i < std::min(num_available, num_choose); ++i)
    for (int i = 0; i < num_choose; ++i)
        output.push_back((seed + i) % num_available);
    return output;
}

void ErasureCodingWriteCacheClient::GenerateGetChunkRequestsV2(const std::string &key, 
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
#else
folly::Future<PutResult> ErasureCodingWriteCacheClient::Put(const std::string &key,
                                                            size_t size,
                                                            const ByteBuffer &buffer,
                                                            const std::map <std::string, std::string> &headers,
                                                            size_t off) {
    PutResult res;
    res.status = UNSUPPORTED_OPERATION;
    return res;
}

folly::Future<int> ErasureCodingWriteCacheClient::Get(const std::string &key,
                                                      size_t start,
                                                      size_t size,
                                                      ByteBuffer &buffer, 
                                                      Json::Value &root) {
    return UNSUPPORTED_OPERATION;
}


folly::Future<int> ErasureCodingWriteCacheClient::GetDecode(const std::string &key,
                                                            size_t start,
                                                            size_t size,
                                                            ByteBuffer &buffer, 
                                                            Json::Value &root) {
    return UNSUPPORTED_OPERATION;
}

std::vector<int> ErasureCodingWriteCacheClient::GetReplica(const std::string &key) {
    return std::vector<int>{};
}

void ErasureCodingWriteCacheClient::GenerateGetChunkRequestsV2(const std::string &key, 
                                                               size_t start, 
                                                               size_t size, 
                                                               ByteBuffer &buffer, 
                                                               std::vector<GetChunkRequestV2> &requests,
                                                               size_t chunk_size) {
    
}
#endif