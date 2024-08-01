#ifndef MADFS_COMMON_H
#define MADFS_COMMON_H

#include <string>
#include <butil/iobuf.h>
#include <folly/futures/Future.h>
#include <folly/futures/Promise.h>
#include <folly/executors/ThreadedExecutor.h>
#include <boost/filesystem.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "config.h"

using folly::Future;
using folly::Promise;

#define RED "\033[1;31m"
#define GREEN "\033[1;32m"
#define YELLOW "\033[1;33m"
#define WHITE "\033[0m"

DECLARE_bool(verbose);

const static int OK = 0;
const static int RPC_FAILED = -2;
const static int NOT_FOUND = -3;
const static int CACHE_ENTRY_NOT_FOUND = -3; // deprecated
const static int INVALID_ARGUMENT = -4;
const static int S3_INTERNAL_ERROR = -5;
const static int FOLLY_ERROR = -6;
const static int NO_ENOUGH_REPLICAS = -7;
const static int METADATA_ERROR = -8;
const static int IO_ERROR = -9;
const static int END_OF_FILE = -10;
const static int NO_ENOUGH_DISKSPACE = -11;
const static int UNSUPPORTED_TYPE = -12;
const static int UNSUPPORTED_OPERATION = -13;
const static int UNIMPLEMENTED = -128;

struct GetOutput {
    int status;
    butil::IOBuf buf;
};

struct PutOutput {
    int status;
    std::string internal_key;
};

struct QueryTsOutput {
    int status;
    uint64_t timestamp;
};

enum WriteCacheType {
    NOCACHE, REPLICATION, REED_SOLOMON
};

struct S3Config {
    std::string address;
    std::string access_key;
    std::string secret_access_key;
    std::string bucket;
    int bg_threads;
};

struct CachePolicy {
    size_t read_chunk_size;
    size_t read_replication_factor;

    WriteCacheType write_cache_type;
    size_t write_replication_factor; // if write_cache_type == REPLICATION
    size_t write_data_blocks;
    size_t write_parity_blocks;      // if write_cache_type == REED_SOLOMON
};

struct GlobalConfig {
    int rpc_timeout;
    int rpc_threads;
    int rpc_connections;
    int folly_threads;
    bool use_rdma;

    bool use_meta_cache;
    size_t meta_cache_max_size;
    size_t meta_cache_clear_size;

    size_t write_chunk_size;

    size_t max_inflight_payload_size;

    CachePolicy default_policy;
    S3Config s3_config;
    
    HybridCache::ReadCacheConfig read_cache;
    HybridCache::WriteCacheConfig write_cache;

    std::string read_cache_dir;
    std::string write_cache_dir;

    std::string etcd_prefix;
};

GlobalConfig &GetGlobalConfig();

static inline std::string PathJoin(const std::string &left, const std::string &right) {
    if (left.empty()) {
        return right;
    } else if (left[left.length() - 1] == '/') {
        return left + right;
    } else {
        return left + "/" + right;
    }
}

static inline int CreateParentDirectories(const std::string &path) {
    auto pos = path.rfind('/');
    if (pos == path.npos) {
        return 0;
    }
    auto parent = path.substr(0, pos);
    boost::filesystem::create_directories(parent);
    return 0;
}

#endif // MADFS_COMMON_H