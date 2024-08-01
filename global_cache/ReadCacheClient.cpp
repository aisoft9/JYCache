#include "ReadCacheClient.h"
#include "GlobalDataAdaptor.h"

#define AWS_BUFFER_PADDING 64

ReadCacheClient::ReadCacheClient(GlobalDataAdaptor *parent)
        : parent_(parent) {}

ReadCacheClient::~ReadCacheClient() {}

folly::Future<int> ReadCacheClient::Get(const std::string &key, size_t start, size_t size, ByteBuffer &buffer) {
    butil::Timer t;
    t.start();
    LOG_IF(INFO, FLAGS_verbose) << "Get key=" << key << ", start=" << start << ", size=" << size;
    std::vector<folly::Future<int>> future_list;
    std::vector<GetChunkRequestV2> requests;
    auto &policy = parent_->GetCachePolicy(key);
    const int num_choose = policy.read_replication_factor;
    GenerateGetChunkRequestsV2(key, start, size, buffer, requests, policy.read_chunk_size);
    if (requests.empty())
        return folly::makeFuture(OK);

    auto DoGetChunkAsync = [this, num_choose](GetChunkRequestV2 &entry) -> folly::Future<int> {
        auto replicas = GetReplica(entry.internal_key, num_choose);
        int primary_server_id = replicas[lrand48() % replicas.size()];
        return GetChunkAsync(primary_server_id, entry).thenValue([this, replicas, entry, primary_server_id](int res) -> int {
            if (res != RPC_FAILED) {
                return res;
            }
            LOG_EVERY_SECOND(WARNING) << "Unable to connect primary replicas. server_id " << primary_server_id
                                      << ", hostname: " << parent_->GetServerHostname(primary_server_id);
            for (auto &server_id : replicas) {
                if (server_id == primary_server_id) {
                    continue;
                }
                res = GetChunkAsync(server_id, entry).get();
                if (res != RPC_FAILED) {
                    return res;
                }
                LOG_EVERY_SECOND(WARNING) << "Unable to connect secondary replicas. server_id " << server_id
                                            << ", hostname: " << parent_->GetServerHostname(server_id);
            }
            LOG_EVERY_SECOND(ERROR) << "Unable to connect all target replicas";
            return RPC_FAILED;
        });
    };

    if (requests.size() == 1) {
        return DoGetChunkAsync(requests[0]);
    }

    size_t aggregated_size = 0;
    for (auto &entry: requests) {
        aggregated_size += entry.chunk_len;
        future_list.emplace_back(DoGetChunkAsync(entry));
        if (aggregated_size >= GetGlobalConfig().max_inflight_payload_size) {
            auto output = folly::collectAll(future_list).get();
            for (auto &entry: output)
                if (entry.value_or(FOLLY_ERROR) != OK) {
                    LOG(ERROR) << "Failed to get data from read cache, key: " << key
                                << ", start: " << start
                                << ", size: " << size
                                << ", buf: " << (void *) buffer.data << " " << buffer.len
                                << ", error code: " << entry.value_or(FOLLY_ERROR);
                    return entry.value_or(FOLLY_ERROR);
                }
            future_list.clear();
        }
    }
    
    if (future_list.empty()) return OK;
    
    return folly::collectAll(future_list).via(parent_->executor_.get()).thenValue(
            [=](std::vector <folly::Try<int>> output) -> int {
                for (auto &entry: output)
                    if (entry.value_or(FOLLY_ERROR) != OK) {
                        LOG(ERROR) << "Failed to get data from read cache, key: " << key
                                   << ", start: " << start
                                   << ", size: " << size
                                   << ", buf: " << (void *) buffer.data << " " << buffer.len
                                   << ", error code: " << entry.value_or(FOLLY_ERROR);
                        return entry.value_or(FOLLY_ERROR);
                    }
                return OK;
            });
}

folly::Future<int> ReadCacheClient::GetChunkAsync(int server_id, GetChunkRequestV2 request) {
    LOG_IF(INFO, FLAGS_verbose) << "GetChunkAsync server_id=" << server_id
                                << ", internal_key=" << request.internal_key
                                << ", chunk_id=" << request.chunk_id
                                << ", chunk_start=" << request.chunk_start
                                << ", chunk_len=" << request.chunk_len
                                << ", buffer=" << (void *) request.buffer.data;
    return parent_->GetRpcClient()->GetEntryFromReadCache(server_id, request.internal_key, request.chunk_start, request.chunk_len)
        .then([this, server_id, request](folly::Try<GetOutput> &&output) -> folly::Future<int> {
            if (!output.hasValue()) {
                return folly::makeFuture(FOLLY_ERROR);
            }
            auto &value = output.value();
            if (value.status == OK) {
                value.buf.copy_to(request.buffer.data, request.buffer.len);
                return folly::makeFuture(OK);
            } else if (value.status == CACHE_ENTRY_NOT_FOUND) {
                return GetChunkFromGlobalCache(server_id, request);
            } else {
                return folly::makeFuture(value.status);
            }
        });
}

folly::Future<int> ReadCacheClient::GetChunkFromGlobalCache(int server_id, GetChunkRequestV2 request) {
    struct Args {
        size_t size;
        std::map<std::string, std::string> headers;
        ByteBuffer data_buf;

        ~Args() {
            if (data_buf.data) {
                delete []data_buf.data;
            }
        }
    };
    auto args = std::make_shared<Args>();
    // auto f = parent_->base_adaptor_->Head(request.user_key, args->size, args->headers)
    auto f = parent_->Head(request.user_key, args->size, args->headers)
        .then([this, args, request] (folly::Try<int> &&output) -> folly::Future<int> {
            if (output.value_or(FOLLY_ERROR) != OK) {
                return folly::makeFuture(output.value_or(FOLLY_ERROR));
            }

            const size_t align_chunk_start = request.chunk_id * request.chunk_granularity;
            const size_t align_chunk_stop = std::min(align_chunk_start + request.chunk_granularity, args->size);
            
            if (align_chunk_start + request.chunk_start + request.chunk_len > args->size) {
                LOG(WARNING) << "Requested data range exceeds object size, key: " << request.user_key
                             << " request offset: " << align_chunk_start + request.chunk_start + request.chunk_len
                             << ", size: " << args->size;
                return folly::makeFuture(END_OF_FILE);
            } else if (align_chunk_start == align_chunk_stop) {
                return folly::makeFuture(OK);
            } else if (align_chunk_start > align_chunk_stop) {
                LOG(WARNING) << "Unexpected request range, key: " << request.user_key
                             << " start offset: " << align_chunk_start
                             << ", end offset: " << align_chunk_stop;
                return folly::makeFuture(INVALID_ARGUMENT);
            }

            args->data_buf.len = align_chunk_stop - align_chunk_start + AWS_BUFFER_PADDING;
            args->data_buf.data = new char[args->data_buf.len];
            return parent_->base_adaptor_->DownLoad(request.user_key, 
                                                    align_chunk_start,
                                                    align_chunk_stop - align_chunk_start,
                                                    args->data_buf);
        }).then([this, args, request] (folly::Try<int> &&output) -> folly::Future<int> {
            if (output.value_or(FOLLY_ERROR) != OK) {
                return folly::makeFuture(output.value_or(FOLLY_ERROR));
            }
            
            memcpy(request.buffer.data, args->data_buf.data + request.chunk_start, request.chunk_len);
            args->data_buf.len -= AWS_BUFFER_PADDING;
            auto &policy = parent_->GetCachePolicy(request.user_key);
            auto replicas = GetReplica(request.internal_key, policy.read_replication_factor);
            std::vector <folly::Future<PutOutput>> future_list;
            for (auto server_id: replicas)
                future_list.emplace_back(parent_->GetRpcClient()->PutEntryFromReadCache(server_id, 
                                                                                        request.internal_key, 
                                                                                        args->data_buf,
                                                                                        args->data_buf.len));
            return folly::collectAll(std::move(future_list)).via(parent_->executor_.get()).thenValue(
                [](std::vector <folly::Try<PutOutput>> &&output) -> int {
                    for (auto &entry: output) {
                        if (!entry.hasValue())
                            return FOLLY_ERROR;
                        if (entry.value().status != OK)
                            return entry.value().status;
                    }
                    return OK;
                });
        });
    return f;
}

folly::Future<int> ReadCacheClient::Invalidate(const std::string &key, size_t size) {
    // LOG(INFO) << "Invalidate key=" << key;
    std::vector <folly::Future<int>> future_list;
    auto &policy = parent_->GetCachePolicy(key);
    const size_t chunk_size = policy.read_chunk_size;
    const size_t end_chunk_id = (size + chunk_size - 1) / chunk_size;
    for (int server_id = 0; server_id < parent_->server_list_.size(); server_id++) {
        future_list.emplace_back(parent_->GetRpcClient()->DeleteEntryFromReadCache(server_id, key, chunk_size, end_chunk_id));
    }
    return folly::collectAll(future_list).via(parent_->executor_.get()).thenValue(
            [](std::vector <folly::Try<int>> output) -> int {
                for (auto &entry: output)
                    if (entry.value_or(FOLLY_ERROR) != OK)
                        return entry.value_or(FOLLY_ERROR);
                return OK;
            });
}

void ReadCacheClient::GenerateGetChunkRequestsV2(const std::string &key, 
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
        item.internal_key = key + "-" + std::to_string(chunk_id) + "-" + std::to_string(chunk_size);
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

std::vector<int> ReadCacheClient::GetReplica(const std::string &key, int num_choose) {
    const int num_available = parent_->server_list_.size();
    uint64_t seed = std::hash < std::string > {}(key);
    std::vector<int> output;
    for (int i = 0; i < std::min(num_available, num_choose); ++i)
        output.push_back((seed + i) % num_available);
    return output;
}