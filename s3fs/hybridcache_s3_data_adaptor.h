/*
 * Project: HybridCache
 * Created Date: 24-3-11
 * Author: lshb
 */

#ifndef S3_DATA_ADAPTOR_H_
#define S3_DATA_ADAPTOR_H_

#include "data_adaptor.h"

using HybridCache::ByteBuffer;

class S3DataAdaptor : public HybridCache::DataAdaptor {
 public:
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
};

#endif // S3_DATA_ADAPTOR_H_
