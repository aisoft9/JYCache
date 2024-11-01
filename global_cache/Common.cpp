#include "Common.h"

DEFINE_int32(rpc_timeout, 30000, "RPC timeout in milliseconds");
DEFINE_int32(rpc_threads, 16, "Maximum number of threads in brpc");
DEFINE_int32(folly_threads, 48, "Maximum number of threads in folly's executor");
DEFINE_int32(rpc_connections, 10, "RPC connections");
DEFINE_bool(use_rdma, true, "Use remote memory direct access");

DEFINE_int64(read_chunk_size, 256 * 1024, "Granularity of global read cache");
DEFINE_int32(read_replication_factor, 1, "Replication factor of global read cache");

DEFINE_string(read_cache_dir, "/mnt/nvme0/renfeng/readcache", "Read cache directory");
DEFINE_string(write_cache_dir, "/mnt/nvme0/renfeng/writecache", "Write cache directory");

DEFINE_string(write_cache_type, "nocache", "Policy of global write cache: nocache, replication, reed-solomon");
DEFINE_int32(write_replication_factor, 1, "Replication factor of global write cache, available if --write_cache_type=replication");
DEFINE_int32(write_data_blocks, 3, "Data blocks of global write cache, available if --write_cache_type=reed-solomon");
DEFINE_int32(write_parity_blocks, 2, "Parity blocks of global write cache, available if --write_cache_type=reed-solomon");

DEFINE_string(s3_address, "<undefined>", "S3 - server address (URL)");
DEFINE_string(s3_access_key, "<undefined>", "S3 - AccessKey");
DEFINE_string(s3_secret_access_key, "<undefined>", "S3 - SecretAccessKey");
DEFINE_string(s3_bucket, "madfs", "S3 - bucket name");
DEFINE_int32(s3_bg_threads, 4, "S3 - number of background threads");

DEFINE_uint64(read_normal_flow_limit, 1024, "Read cache normal flow limit");
DEFINE_uint64(read_burst_flow_limit, 10 * 1024, "Read cache burst flow limit");
DEFINE_uint64(read_capacity_mb, 4096, "Read cache capacity in MB");
DEFINE_uint64(read_page_body_size, 64 * 1024, "Read cache page body size");
DEFINE_uint64(read_page_meta_size, 1024, "Read cache page meta size");
DEFINE_bool(read_cas, true, "Read cache enable CAS");
DEFINE_bool(read_nvm_cache, false, "Read cache enable NVM cache");

DEFINE_bool(use_meta_cache, true, "Enable meta cache");
DEFINE_uint64(meta_cache_max_size, 1024 * 1024, "Max size of meta cache");
DEFINE_uint64(meta_cache_clear_size, 512 * 1024, "Read cache burst flow limit");

DEFINE_uint64(write_chunk_size, 16 * 1024 * 1024, "Granularity of global write cache");
DEFINE_uint64(max_inflight_payload_size, 256 * 1024 * 1024, "Max inflight payload size in bytes");

DEFINE_string(etcd_prefix, "/madfs/", "Etcd directory prefix");

DEFINE_bool(verbose, false, "Print debug logging");

namespace brpc {
    DECLARE_int64(socket_max_unwritten_bytes);
};

static GlobalConfig g_cfg;
std::once_flag g_cfg_once;

#define SAFE_ASSIGN(conf, flag, min_val, max_val) {                     \
    const static auto flag##_min = (min_val);                           \
    const static auto flag##_max = (max_val);                           \
    if (flag < (min_val) || flag > (max_val)) {                         \
        LOG(WARNING) << "Invalid " #flag ", reset to " << (max_val);    \
        flag = (max_val);                                               \
    }                                                                   \
    conf = flag;                                                        \
}

void InitGlobalConfig() {
    SAFE_ASSIGN(g_cfg.rpc_timeout, FLAGS_rpc_timeout, 0, 60000);
    SAFE_ASSIGN(g_cfg.rpc_threads, FLAGS_rpc_threads, 0, 256);
    SAFE_ASSIGN(g_cfg.rpc_connections, FLAGS_rpc_connections, 0, 64);
    SAFE_ASSIGN(g_cfg.folly_threads, FLAGS_folly_threads, 0, 256);
    g_cfg.use_rdma = FLAGS_use_rdma;
    g_cfg.write_chunk_size = FLAGS_write_chunk_size;

    g_cfg.default_policy.read_chunk_size = FLAGS_read_chunk_size;
    g_cfg.default_policy.read_replication_factor = FLAGS_read_replication_factor;

    g_cfg.default_policy.read_chunk_size = FLAGS_read_chunk_size;
    g_cfg.default_policy.read_replication_factor = FLAGS_read_replication_factor;

    g_cfg.use_meta_cache = FLAGS_use_meta_cache;
    g_cfg.meta_cache_max_size = size_t(FLAGS_meta_cache_max_size);
    g_cfg.meta_cache_clear_size = size_t(FLAGS_meta_cache_clear_size);

    g_cfg.read_cache_dir = FLAGS_read_cache_dir;
    g_cfg.write_cache_dir = FLAGS_write_cache_dir;

    g_cfg.etcd_prefix = FLAGS_etcd_prefix;
    g_cfg.max_inflight_payload_size = FLAGS_max_inflight_payload_size;

    if (FLAGS_write_cache_type == "nocache") {
        g_cfg.default_policy.write_cache_type = NOCACHE;
    } else if (FLAGS_write_cache_type == "replication") {
        g_cfg.default_policy.write_cache_type = REPLICATION;
        g_cfg.default_policy.write_replication_factor = FLAGS_write_replication_factor;
    } else if (FLAGS_write_cache_type == "reed-solomon") {
        g_cfg.default_policy.write_cache_type = REED_SOLOMON;
        g_cfg.default_policy.write_data_blocks = FLAGS_write_data_blocks;
        g_cfg.default_policy.write_parity_blocks = FLAGS_write_parity_blocks;
    } else {
        LOG(ERROR) << "The program will be terminated because of unsupported write cache type: " << FLAGS_write_cache_type;
        exit(EXIT_FAILURE);
    }

    g_cfg.s3_config.address = FLAGS_s3_address;
    g_cfg.s3_config.access_key = FLAGS_s3_access_key;
    g_cfg.s3_config.secret_access_key = FLAGS_s3_secret_access_key;
    g_cfg.s3_config.bucket = FLAGS_s3_bucket;
    g_cfg.s3_config.bg_threads = FLAGS_s3_bg_threads;

    HybridCache::ReadCacheConfig &read_cache = g_cfg.read_cache;
    read_cache.DownloadNormalFlowLimit = FLAGS_read_normal_flow_limit;
    read_cache.DownloadBurstFlowLimit = FLAGS_read_burst_flow_limit;
    read_cache.CacheCfg.CacheName = "Read";
    read_cache.CacheCfg.MaxCacheSize = FLAGS_read_capacity_mb * 1024 * 1024;;
    read_cache.CacheCfg.PageBodySize = FLAGS_read_page_body_size;
    read_cache.CacheCfg.PageMetaSize = FLAGS_read_page_meta_size;
    read_cache.CacheCfg.EnableCAS = FLAGS_read_cas;
    read_cache.CacheCfg.SafeMode = true;
    read_cache.CacheCfg.CacheLibCfg.EnableNvmCache = FLAGS_read_nvm_cache;

    brpc::FLAGS_socket_max_unwritten_bytes = FLAGS_max_inflight_payload_size * 2;
}

GlobalConfig &GetGlobalConfig() {
    std::call_once(g_cfg_once, InitGlobalConfig);
    return g_cfg;
}
