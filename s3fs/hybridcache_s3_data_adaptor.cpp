#include "common.h"
#include "curl.h"
#include "curl_multi.h"
#include "fdcache_entity.h"
#include "hybridcache_s3_data_adaptor.h"
#include "s3fs_logger.h"
#include "string_util.h"

using HybridCache::EnableLogging;

folly::Future<int> S3DataAdaptor::DownLoad(const std::string &key,
                                           size_t start,
                                           size_t size,
                                           ByteBuffer &buffer) {
    assert(executor_);
    return folly::via(executor_.get(), [key, start, size, buffer]() -> int {
        std::chrono::steady_clock::time_point startTime;
        if (EnableLogging) startTime = std::chrono::steady_clock::now();

        int res = 0;
        // parallel request
        if (S3fsCurl::GetMultipartSize() <= size && !nomultipart) {
            res = S3fsCurl::ParallelGetObjectRequest(key.c_str(),
                    NEW_CACHE_FAKE_FD, start, size, buffer.data);
        } else if (0 < size) {  // single request
            S3fsCurl s3fscurl;
            res = s3fscurl.GetObjectRequest(key.c_str(),
                    NEW_CACHE_FAKE_FD, start, size, buffer.data);
        }

        if (EnableLogging) {
            double totalTime = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - startTime).count();
            LOG(INFO) << "[DataAdaptor]DownLoad, file:" << key
                      << ", start:" << start << ", size:" << size
                      << ", res:" << res << ", time:" << totalTime << "ms";
        }
        return res;
    });
}

folly::Future<int> S3DataAdaptor::UpLoad(const std::string &key,
                            size_t size,
                            const ByteBuffer &buffer,
                            const std::map<std::string, std::string>& headers) {
    assert(executor_);

    // check size
    if (size > MAX_MULTIPART_CNT * S3fsCurl::GetMultipartSize()) {
        int res = -EFBIG;
        LOG(ERROR) << "[DataAdaptor]UpLoad, file size too large, "
                   << "increase multipart size and try again. Part count exceeds:" 
                   << MAX_MULTIPART_CNT << ", file:" << key << ", size:" << size;
        if (EnableLogging) {
            LOG(INFO) << "[DataAdaptor]UpLoad, file:" << key << ", size:" << size
                      << ", headerSize:" << headers.size() << ", res:" << res;
        }
        return res;
    }

    return folly::via(executor_.get(), [key, size, buffer, headers]() -> int {
        std::chrono::steady_clock::time_point startTime;
        if (EnableLogging) startTime = std::chrono::steady_clock::now();

        int res = 0;
        headers_t s3fsHeaders;
        for (auto it : headers) {
            s3fsHeaders[it.first] = it.second;
        }

        if (nomultipart || size < S3fsCurl::GetMultipartSize()) {  // normal uploading
            S3fsCurl s3fscurl(true);
            res = s3fscurl.PutRequest(key.c_str(), s3fsHeaders,
                    NEW_CACHE_FAKE_FD, size, buffer.data);
        } else {  // Multi part Upload
            res = S3fsCurl::ParallelMultipartUploadRequest(key.c_str(), s3fsHeaders,
                    NEW_CACHE_FAKE_FD, size, buffer.data);
        }

        if (EnableLogging) {
            double totalTime = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - startTime).count();
            LOG(INFO) << "[DataAdaptor]UpLoad, file:" << key << ", size:" << size
                      << ", headerSize:" << headers.size() << ", res:" << res
                      << ", time:" << totalTime << "ms";
        }
        return res;
    });
}

folly::Future<int> S3DataAdaptor::Delete(const std::string &key) {
    assert(executor_);
    return folly::via(executor_.get(), [key]() -> int {
        std::chrono::steady_clock::time_point startTime;
        if (EnableLogging) startTime = std::chrono::steady_clock::now();

        S3fsCurl s3fscurl;
        int res = s3fscurl.DeleteRequest(key.c_str());
        if (EnableLogging) {
            double totalTime = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - startTime).count();
            LOG(INFO) << "[DataAdaptor]Delete, file:" << key << ", res:" << res
                      << ", time:" << totalTime << "ms";
        }
        return res;
    });
}

folly::Future<int> S3DataAdaptor::Head(const std::string &key,
                                size_t& size,
                                std::map<std::string, std::string>& headers) {
    assert(executor_);
    return folly::via(executor_.get(), [key, &size, &headers]() -> int {
        std::chrono::steady_clock::time_point startTime;
        if (EnableLogging) startTime = std::chrono::steady_clock::now();

        headers_t s3fsHeaders;
        S3fsCurl s3fscurl;
        int res = s3fscurl.HeadRequest(key.c_str(), s3fsHeaders);
        for (auto it : s3fsHeaders) {
            headers[it.first] = it.second;
            if (lower(it.first) == "content-length") {
                std::stringstream sstream(it.second);
                sstream >> size;
            }
        }
        if (EnableLogging) {
            double totalTime = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - startTime).count();
            LOG(INFO) << "[DataAdaptor]Head, file:" << key << ", res:" << res
                      << ", size:" << size << ", headerSize:" << headers.size()
                      << ", time:" << totalTime << "ms";
        }
        return res;
    });
}
