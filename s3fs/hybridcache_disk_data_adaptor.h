/*
 * Project: HybridCache
 * Created Date: 24-6-7
 * Author: lshb
 */

#ifndef DISK_DATA_ADAPTOR_H_
#define DISK_DATA_ADAPTOR_H_

#include "data_adaptor.h"

using HybridCache::ByteBuffer;

class DiskDataAdaptor : public HybridCache::DataAdaptor {
 public:
    DiskDataAdaptor(std::shared_ptr<DataAdaptor> dataAdaptor) : dataAdaptor_(dataAdaptor) {}
    DiskDataAdaptor() = default;
    ~DiskDataAdaptor() {}

    folly::Future<int> DownLoad(const std::string &key,
                                size_t start,
                                size_t size,
                                ByteBuffer &buffer);
    
    folly::Future<int> UpLoad(const std::string &key,
                              size_t size,
                              const ByteBuffer &buffer,
                              const std::map<std::string, std::string>& headers);

    folly::Future<int> Delete(const std::string &key);

    folly::Future<int> Head(const std::string &key,
                            size_t& size,
                            std::map<std::string, std::string>& headers);

 private:
    std::shared_ptr<DataAdaptor> dataAdaptor_;
};

#endif // DISK_DATA_ADAPTOR_H_
