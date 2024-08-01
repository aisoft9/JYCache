#include <unistd.h>

#include "WriteCache.h"
#include "FileSystemDataAdaptor.h"
#include <dirent.h>
#include "write_cache.h"


//#define BRPC_WITH_RDMA 1
//#include <brpc/rdma/block_pool.h>

class WriteCache4RocksDB : public WriteCacheImpl {
public:
    explicit WriteCache4RocksDB(std::shared_ptr<folly::CPUThreadPoolExecutor> executor);

    ~WriteCache4RocksDB();

    virtual GetOutput Get(const std::string &internal_key, uint64_t start, uint64_t length);

    virtual PutOutput Put(const std::string &key, uint64_t length, const butil::IOBuf &buf);

    virtual int Delete(const std::string &key_prefix, uint64_t ts, const std::unordered_set<std::string> &except_keys);

private:
    std::string rocksdb_path_;
    rocksdb::DB *db_;
};

WriteCache4RocksDB::WriteCache4RocksDB(std::shared_ptr<folly::CPUThreadPoolExecutor> executor) 
        : WriteCacheImpl(executor) {
    rocksdb::Options options;
    options.create_if_missing = true;
    rocksdb_path_ = PathJoin(GetGlobalConfig().write_cache_dir, ".write_cache.db");
    if (CreateParentDirectories(rocksdb_path_)) {
        LOG(WARNING) << "Failed to create directory: " << rocksdb_path_;
        abort();
    }
    auto status = rocksdb::DB::Open(options, rocksdb_path_, &db_);
    if (!status.ok()) {
        LOG(WARNING) << "Failed to open RocksDB: " << status.ToString();
        abort();
    }
}

WriteCache4RocksDB::~WriteCache4RocksDB() {
    if (db_) {
        db_->Close();
    }
}

GetOutput WriteCache4RocksDB::Get(const std::string &internal_key, uint64_t start, uint64_t length) {
    rocksdb::ReadOptions options;
    std::string value;
    auto status = db_->Get(options, internal_key, &value);
    GetOutput output;
    if (status.IsNotFound()) {
        output.status = CACHE_ENTRY_NOT_FOUND;
        return output;
    } else if (!status.ok()) {
        LOG(WARNING) << "Failed to get key " << internal_key << " from RocksDB: " << status.ToString();
        output.status = IO_ERROR;
        return output;
    }
    if (length == 0 || start + length > value.size()) {
        output.status = INVALID_ARGUMENT;
        return output;
    }
    output.status = OK;
    output.buf.append(&value[start], length);
    LOG_IF(INFO, FLAGS_verbose) << "GetWriteCache internal_key: " << internal_key << ", size: " << length;
    return output;
}

PutOutput WriteCache4RocksDB::Put(const std::string &key, uint64_t length, const butil::IOBuf &buf) {
    auto oid = next_object_id_.fetch_add(1);
    auto internal_key = key + "-" + std::to_string(oid);
    rocksdb::WriteOptions options;
    std::string value = buf.to_string();
    auto status = db_->Put(options, internal_key, value);
    if (!status.ok()) {
        LOG(WARNING) << "Failed to put key " << internal_key << " from RocksDB: " << status.ToString();
        return {IO_ERROR, "<undefined>"};
    }
    LOG_IF(INFO, FLAGS_verbose) << "PutWriteCache key: " << key << ", internal_key: " << internal_key << ", size: " << length;
    return {OK, internal_key};
}

static bool HasPrefix(const std::string &key, const std::string &key_prefix) {
    return key.substr(0, key_prefix.size()) == key_prefix;
}

static uint64_t ParseTS(const std::string &key) {
    size_t pos = key.rfind('-');
    if (pos != std::string::npos) {
        std::string lastSubStr = key.substr(pos + 1);
        uint64_t number;
        std::istringstream(lastSubStr) >> number;
        if (!std::cin.fail()) {
            return number;
        } else {
            return UINT64_MAX;
        }
    } else {
        return UINT64_MAX;
    }
}

// Delete all entries that: match the prefix, < ts, and not in except_keys
int WriteCache4RocksDB::Delete(const std::string &key_prefix, uint64_t ts, const std::unordered_set<std::string> &except_keys) {
    LOG(INFO) << "Request key_prefix = " << key_prefix << ", ts = " << ts;
    rocksdb::ReadOptions read_options;
    rocksdb::WriteOptions write_options;
    auto iter = db_->NewIterator(read_options);
    iter->Seek(key_prefix);
    for (; iter->Valid(); iter->Next()) {
        std::string key = iter->key().ToString();
        LOG(INFO) << "Processing key " << key;
        if (!HasPrefix(key, key_prefix)) {
            break;
        }
        if (ParseTS(key) >= ts || except_keys.count(key)) {
            continue;
        }
        auto status = db_->Delete(write_options, key);
        if (!status.ok() && !status.IsNotFound()) {
            LOG(WARNING) << "Failed to delete key " << key << " from RocksDB: " << status.ToString();
            iter->Reset();
            return IO_ERROR;
        }
        LOG(INFO) << "Deleted key " << key;
    }
    iter->Reset();
    return OK;
}

// ----------------------------------------------------------------------------

class WriteCache4Disk : public WriteCacheImpl {
public:
    explicit WriteCache4Disk(std::shared_ptr<folly::CPUThreadPoolExecutor> executor);

    ~WriteCache4Disk();

    virtual GetOutput Get(const std::string &internal_key, uint64_t start, uint64_t length);

    virtual PutOutput Put(const std::string &key, uint64_t length, const butil::IOBuf &buf);

    virtual int Delete(const std::string &key_prefix, uint64_t ts, const std::unordered_set<std::string> &except_keys);

private:
    std::shared_ptr<DataAdaptor> cache_fs_adaptor_;
};

WriteCache4Disk::WriteCache4Disk(std::shared_ptr<folly::CPUThreadPoolExecutor> executor) 
        : WriteCacheImpl(executor) {
    cache_fs_adaptor_ = std::make_shared<FileSystemDataAdaptor>(GetGlobalConfig().write_cache_dir, nullptr, false, nullptr, false);
}

WriteCache4Disk::~WriteCache4Disk() {}

GetOutput WriteCache4Disk::Get(const std::string &internal_key, uint64_t start, uint64_t length) {
    butil::Timer t;
    t.start();
#ifndef BRPC_WITH_RDMA
    auto wrap = HybridCache::ByteBuffer(new char[length], length); 
#else
    auto wrap = HybridCache::ByteBuffer((char *) brpc::rdma::AllocBlock(length), length); 
#endif
    int res = cache_fs_adaptor_->DownLoad(internal_key, start, length, wrap).get();
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
    t.stop();
    LOG_IF(INFO, FLAGS_verbose) << "Get key: " << internal_key 
                                << ", start: " << start
                                << ", length: " << length 
                                << ", status: " << res
                                << ", latency: " << t.u_elapsed();
    return output;
}

uint64_t ReportAvailableDiskSpace(std::string &path) {
    struct statvfs stat;
    if (statvfs(path.c_str(), &stat)) {
        PLOG(ERROR) << "Failed to statvfs";
        return 0;
    }
    return stat.f_bavail * stat.f_bsize;
}

const static size_t kMinDiskFreeSpace = 1024 * 1024 * 512;

PutOutput WriteCache4Disk::Put(const std::string &key, uint64_t length, const butil::IOBuf &buf) {
    butil::Timer t;
    t.start();
    auto oid = next_object_id_.fetch_add(1);
    auto internal_key = key + "-" + std::to_string(oid);

    if (ReportAvailableDiskSpace(GetGlobalConfig().write_cache_dir) < std::max(length, kMinDiskFreeSpace)) {
        // LOG(WARNING) << "No enough space to persist data, please perform one GC immediately";
        return {NO_ENOUGH_DISKSPACE, "<undefined>"};
    }

    t.stop();
    // LOG_IF(INFO, FLAGS_verbose) << "duration: " << t.u_elapsed();

    auto data_len = buf.length();

    thread_local void *aux_buffer = nullptr;
    if (!aux_buffer)
        posix_memalign(&aux_buffer, 4096, GetGlobalConfig().write_chunk_size);

    auto data = buf.fetch(aux_buffer, data_len);
    auto wrap = HybridCache::ByteBuffer((char *) data, data_len);
    std::map<std::string, std::string> headers;

    t.stop();
    // LOG_IF(INFO, FLAGS_verbose) << "duration: " << t.u_elapsed();

    int res = cache_fs_adaptor_->UpLoad(internal_key, length, wrap, headers).get();
    // free(aux_buffer);
    if (res) {
        LOG(WARNING) << "Failed to put key " << internal_key << " to disk";
        return {IO_ERROR, "<undefined>"};
    }
    t.stop();
    LOG_IF(INFO, FLAGS_verbose) << "PutWriteCache key: " << key << ", internal_key: " << internal_key << ", size: " << length << ", duration: " << t.u_elapsed();
    return {OK, internal_key};
}


void listFilesRecursively(const std::string &directoryPath,
                          std::vector<std::string> &to_remove, 
                          const std::string &key_prefix, 
                          uint64_t ts, 
                          const std::unordered_set<std::string> &except_keys) {
    DIR* dir = opendir(directoryPath.c_str());
    if (dir == nullptr) {
        std::cerr << "Error opening directory: " << directoryPath << std::endl;
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Skip "." and ".." entries
        if (std::string(entry->d_name) == "." || std::string(entry->d_name) == "..") {
            continue;
        }

        std::string fullPath = PathJoin(directoryPath, entry->d_name);
        std::string rootPath = GetGlobalConfig().write_cache_dir;
        struct stat statbuf;
        if (stat(fullPath.c_str(), &statbuf) == 0) {
            if (S_ISDIR(statbuf.st_mode)) {
                // It's a directory, recurse into it
                listFilesRecursively(fullPath, to_remove, key_prefix, ts, except_keys);
            } else if (S_ISREG(statbuf.st_mode)) {
                std::string key = fullPath.substr(rootPath.length());
                if (!key.empty() && key[0] == '/') {
                    key = key.substr(1);
                }
                if (!HasPrefix(key, key_prefix) || except_keys.count(key) || ParseTS(key) >= ts) {
                    continue;
                }
                to_remove.push_back(fullPath);
                // LOG(INFO) << "Deleted key " << key << ", location " << fullPath;
            }
        }
    }
    closedir(dir);
}


// Delete all entries that: match the prefix, < ts, and not in except_keys
int WriteCache4Disk::Delete(const std::string &key_prefix, uint64_t ts, const std::unordered_set<std::string> &except_keys) {
    LOG(INFO) << "Request key_prefix = " << key_prefix << ", ts = " << ts;
    std::vector<std::string> to_remove;
    listFilesRecursively(GetGlobalConfig().write_cache_dir, 
                         to_remove, 
                         key_prefix, 
                         ts, 
                         except_keys);
    for (auto &entry : to_remove) {
        if (remove(entry.c_str())) {
            LOG(WARNING) << "Failed to remove file: " << entry;
            return IO_ERROR;
        }
    }
    return OK;
}


class WriteCache4Fake : public WriteCacheImpl {
public:
    explicit WriteCache4Fake(std::shared_ptr<folly::CPUThreadPoolExecutor> executor) : WriteCacheImpl(executor) {}

    virtual  ~WriteCache4Fake() {}

    virtual GetOutput Get(const std::string &internal_key, uint64_t start, uint64_t length) {
        LOG_IF(INFO, FLAGS_verbose) << "Get internal_key " << internal_key << " start " << start << " length " << length;
        GetOutput ret;
        ret.status = OK;
        ret.buf.resize(length, 'x');
        return ret;
    }

    virtual PutOutput Put(const std::string &key, uint64_t length, const butil::IOBuf &buf) {
        LOG_IF(INFO, FLAGS_verbose) << "Put key " << key << " length " << length;
        PutOutput ret;
        ret.status = OK;
        ret.internal_key = key;
        return ret;
    }

    virtual int Delete(const std::string &key_prefix, uint64_t ts, const std::unordered_set<std::string> &except_keys) {
        return OK;
    }
};

class WriteCache4Cachelib : public WriteCacheImpl {
public:
    explicit WriteCache4Cachelib(std::shared_ptr<folly::CPUThreadPoolExecutor> executor) : WriteCacheImpl(executor) {
        HybridCache::EnableLogging = false;
        impl_ = std::make_shared<HybridCache::WriteCache>(GetGlobalConfig().write_cache);
    }

    virtual  ~WriteCache4Cachelib() {}

    virtual GetOutput Get(const std::string &internal_key, uint64_t start, uint64_t length) {
        butil::Timer t;
        t.start();
        std::vector<std::pair<size_t, size_t>> dataBoundary;
#ifndef BRPC_WITH_RDMA
        auto wrap = HybridCache::ByteBuffer(new char[length], length); 
#else
        auto wrap = HybridCache::ByteBuffer((char *) brpc::rdma::AllocBlock(length), length); 
#endif
        int res = impl_->Get(internal_key, start, length, wrap, dataBoundary);
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
        t.stop();
        LOG_IF(INFO, FLAGS_verbose) << "Get key: " << internal_key 
                                    << ", start: " << start
                                    << ", length: " << length 
                                    << ", status: " << res
                                    << ", latency: " << t.u_elapsed();
        return output;
    }

    virtual PutOutput Put(const std::string &key, uint64_t length, const butil::IOBuf &buf) {
        LOG_IF(INFO, FLAGS_verbose) << "Put key " << key << " length " << length;
        PutOutput ret;
        ret.status = OK;
        ret.internal_key = key;
        return ret;
    }

    virtual int Delete(const std::string &key_prefix, uint64_t ts, const std::unordered_set<std::string> &except_keys) {
        return OK;
    }

private:
    std::shared_ptr<HybridCache::WriteCache> impl_;

};


DEFINE_string(write_cache_engine, "disk", "Write cache engine: rocksdb | disk");

WriteCache::WriteCache(std::shared_ptr<folly::CPUThreadPoolExecutor> executor) {
    if (FLAGS_write_cache_engine == "rocksdb")
        impl_ = new WriteCache4RocksDB(executor);
    else if (FLAGS_write_cache_engine == "disk")
        impl_ = new WriteCache4Disk(executor);
    else if (FLAGS_write_cache_engine == "fake")
        impl_ = new WriteCache4Fake(executor);
    else {
        LOG(WARNING) << "unsupported write cache engine";
        exit(EXIT_FAILURE);
    }
}
