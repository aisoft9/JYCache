#ifndef ETCD_CLIENT_H
#define ETCD_CLIENT_H

#include <etcd/SyncClient.hpp>
#include <json/json.h>
#include <mutex>

#include "WriteCacheClient.h"

class EtcdClient {
public:
    EtcdClient(const std::string &etcd_url) : client_(etcd_url) {};

    ~EtcdClient() {}

    struct GetResult {
        int status;
        Json::Value root;
    };

    folly::Future<GetResult> GetJson(const std::string &key) {
        std::lock_guard<std::mutex> lock(mutex_);
        Json::Reader reader;
        Json::Value root;
        auto resp = client_.get(PathJoin(GetGlobalConfig().etcd_prefix, key));
        if (!resp.is_ok()) {
            if (resp.error_code() != 100) {
                LOG(ERROR) << "Error from etcd client: " << resp.error_code()
                           << ", message: " << resp.error_message();
                return folly::makeFuture(GetResult{ METADATA_ERROR, root });
            } else {
                LOG(WARNING) << "Record not found in the etcd storage: key " << key;
                return folly::makeFuture(GetResult{ NOT_FOUND, root });
            }
        }
        if (!reader.parse(resp.value().as_string(), root)) {
            LOG(ERROR) << "Error from etcd client: failed to parse record: " << resp.value().as_string();
            return folly::makeFuture(GetResult{ METADATA_ERROR, root });
        }
        LOG(INFO) << "Record get: " << key;
        return folly::makeFuture(GetResult{ OK, root });
    }

    folly::Future<int> PutJson(const std::string &key, const Json::Value &root) {
        std::lock_guard<std::mutex> lock(mutex_);
        Json::FastWriter writer;
        const std::string json_file = writer.write(root);
        auto resp = client_.put(PathJoin(GetGlobalConfig().etcd_prefix, key), json_file);
        if (!resp.is_ok()) {
            LOG(ERROR) << "Error from etcd client: " << resp.error_code()
                       << ", message: " << resp.error_message(); 
            return folly::makeFuture(METADATA_ERROR);
        }
        LOG(INFO) << "Record put: " << key;
        return folly::makeFuture(OK);
    }

    folly::Future<int> DeleteJson(const std::string &key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto resp = client_.rm(PathJoin(GetGlobalConfig().etcd_prefix, key));
        if (!resp.is_ok()) {
            if (resp.error_code() != 100) {
                LOG(ERROR) << "Error from etcd client: " << resp.error_code()
                           << ", message: " << resp.error_message();       
                return folly::makeFuture(METADATA_ERROR);
            } else {
                LOG(WARNING) << "Record not found in the etcd storage: key " << key;
                return folly::makeFuture(NOT_FOUND);
            }
            return folly::makeFuture(METADATA_ERROR);
        }
        return folly::makeFuture(OK);
    }

    folly::Future<int> ListJson(const std::string &key_prefix, std::vector<std::string> &key_list) {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::string etcd_prefix = GetGlobalConfig().etcd_prefix;
        auto resp = client_.keys(PathJoin(etcd_prefix, key_prefix));
        if (!resp.is_ok()) {
            if (resp.error_code() != 100) {
                LOG(ERROR) << "Error from etcd client: " << resp.error_code()
                           << ", message: " << resp.error_message();       
                return folly::makeFuture(METADATA_ERROR);
            } else {
                LOG(WARNING) << "Record not found in the etcd storage: key " << key_prefix;
                return folly::makeFuture(NOT_FOUND);
            }
            return folly::makeFuture(METADATA_ERROR);
        }
        for (auto &entry : resp.keys()) {
            key_list.push_back(entry.substr(etcd_prefix.length()));
        }
        return folly::makeFuture(OK);
    }

private:
    std::mutex mutex_;
    etcd::SyncClient client_;
};

#endif // ETCD_CLIENT_H