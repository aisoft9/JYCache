#ifndef MADFS_S3_DATA_ADAPTOR_H
#define MADFS_S3_DATA_ADAPTOR_H

#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/auth/AWSCredentialsProvider.h>

#include "data_adaptor.h"

#include "Common.h"

using HybridCache::ByteBuffer;
using HybridCache::DataAdaptor;

class S3DataAdaptor : public DataAdaptor {
public:
    S3DataAdaptor();

    ~S3DataAdaptor();

    // 从数据服务器加载数据
    virtual folly::Future<int> DownLoad(const std::string &key,
                                        size_t start,
                                        size_t size,
                                        ByteBuffer &buffer);

    // 上传数据到数据服务器
    virtual folly::Future<int> UpLoad(const std::string &key,
                                      size_t size,
                                      const ByteBuffer &buffer,
                                      const std::map <std::string, std::string> &headers);

    // 删除数据服务器的数据
    virtual folly::Future<int> Delete(const std::string &key);

    // 获取数据的元数据
    virtual folly::Future<int> Head(const std::string &key,
                                    size_t &size,
                                    std::map <std::string, std::string> &headers);

private:
    Aws::Client::ClientConfiguration *clientCfg_;
    Aws::S3::S3Client *s3Client_;
};

#endif // MADFS_S3_DATA_ADAPTOR_H