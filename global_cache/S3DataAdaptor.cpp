#include "S3DataAdaptor.h"

#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/HeadObjectRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/DeleteObjectRequest.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/core/utils/stream/PreallocatedStreamBuf.h>

#define STRINGIFY_HELPER(val) #val
#define STRINGIFY(val) STRINGIFY_HELPER(val)
#define AWS_ALLOCATE_TAG __FILE__ ":" STRINGIFY(__LINE__)

std::once_flag S3INIT_FLAG;
std::once_flag S3SHUTDOWN_FLAG;
Aws::SDKOptions AWS_SDK_OPTIONS;

// https://github.com/aws/aws-sdk-cpp/issues/1430
class PreallocatedIOStream : public Aws::IOStream {
public:
    PreallocatedIOStream(char *buf, size_t size)
            : Aws::IOStream(new Aws::Utils::Stream::PreallocatedStreamBuf(
            reinterpret_cast<unsigned char *>(buf), size)) {}

    PreallocatedIOStream(const char *buf, size_t size)
            : PreallocatedIOStream(const_cast<char *>(buf), size) {}

    ~PreallocatedIOStream() {
        // corresponding new in constructor
        delete rdbuf();
    }
};

Aws::String GetObjectRequestRange(uint64_t offset, uint64_t len) {
    auto range =
            "bytes=" + std::to_string(offset) + "-" + std::to_string(offset + len);
    return {range.data(), range.size()};
}

S3DataAdaptor::S3DataAdaptor() {
    auto initSDK = [&]() {
        Aws::InitAPI(AWS_SDK_OPTIONS);
    };
    std::call_once(S3INIT_FLAG, initSDK);
    auto &s3_config = GetGlobalConfig().s3_config;
    setenv("AWS_EC2_METADATA_DISABLED", "true", 1);
    clientCfg_ = Aws::New<Aws::Client::ClientConfiguration>(AWS_ALLOCATE_TAG, true);
    clientCfg_->scheme = Aws::Http::Scheme::HTTP;
    clientCfg_->verifySSL = false;
    clientCfg_->maxConnections = 10;
    clientCfg_->endpointOverride = s3_config.address;
    clientCfg_->executor = Aws::MakeShared<Aws::Utils::Threading::PooledThreadExecutor>("S3Adapter.S3Client", s3_config.bg_threads);

    s3Client_ = Aws::New<Aws::S3::S3Client>(AWS_ALLOCATE_TAG,
                                            Aws::Auth::AWSCredentials(s3_config.access_key, s3_config.secret_access_key),
                                            *clientCfg_,
                                            Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never,
                                            false);
}

S3DataAdaptor::~S3DataAdaptor() {
    if (clientCfg_ != nullptr) {
        Aws::Delete<Aws::Client::ClientConfiguration>(clientCfg_);
        clientCfg_ = nullptr;
    }
    if (s3Client_ != nullptr) {
        Aws::Delete<Aws::S3::S3Client>(s3Client_);
        s3Client_ = nullptr;
    }
    auto shutdownSDK = [&]() {
        Aws::ShutdownAPI(AWS_SDK_OPTIONS);
    };
    std::call_once(S3SHUTDOWN_FLAG, shutdownSDK);
}

folly::Future<int> S3DataAdaptor::DownLoad(const std::string &key,
                                           size_t start,
                                           size_t size,
                                           ByteBuffer &buffer) {
    Aws::S3::Model::GetObjectRequest request;
    request.SetBucket(GetGlobalConfig().s3_config.bucket);
    request.SetKey(Aws::String{key.c_str(), key.size()});
    request.SetRange(GetObjectRequestRange(start, size));
    request.SetResponseStreamFactory(
            [&buffer]() { return Aws::New<PreallocatedIOStream>(AWS_ALLOCATE_TAG, buffer.data, buffer.len); });
    auto promise = std::make_shared < folly::Promise < int >> ();
    Aws::S3::GetObjectResponseReceivedHandler handler =
            [&buffer, size, promise](
                    const Aws::S3::S3Client */*client*/,
                    const Aws::S3::Model::GetObjectRequest &/*request*/,
                    const Aws::S3::Model::GetObjectOutcome &response,
                    const std::shared_ptr<const Aws::Client::AsyncCallerContext> &awsCtx) {
                if (response.IsSuccess()) {
                    promise->setValue(OK);
                } else if (response.GetError().GetErrorType() == Aws::S3::S3Errors::NO_SUCH_KEY) {
                    promise->setValue(NOT_FOUND);
                } else {
                    LOG(ERROR) << "GetObjectAsync error: "
                               << response.GetError().GetExceptionName()
                               << "message: " << response.GetError().GetMessage();
                    promise->setValue(S3_INTERNAL_ERROR);
                }
            };
    s3Client_->GetObjectAsync(request, handler, nullptr);
    return promise->getFuture();
}

folly::Future<int> S3DataAdaptor::UpLoad(const std::string &key,
                                         size_t size,
                                         const ByteBuffer &buffer,
                                         const std::map <std::string, std::string> &headers) {
    Aws::S3::Model::PutObjectRequest request;
    request.SetBucket(GetGlobalConfig().s3_config.bucket);
    request.SetKey(key);
    request.SetMetadata(headers);
    request.SetBody(Aws::MakeShared<PreallocatedIOStream>(AWS_ALLOCATE_TAG, buffer.data, buffer.len));
    auto promise = std::make_shared < folly::Promise < int >> ();
    Aws::S3::PutObjectResponseReceivedHandler handler =
            [promise](
                    const Aws::S3::S3Client */*client*/,
                    const Aws::S3::Model::PutObjectRequest &/*request*/,
                    const Aws::S3::Model::PutObjectOutcome &response,
                    const std::shared_ptr<const Aws::Client::AsyncCallerContext> &awsCtx) {
                LOG_IF(ERROR, !response.IsSuccess())
                        << "PutObjectAsync error: "
                        << response.GetError().GetExceptionName()
                        << "message: " << response.GetError().GetMessage();
                promise->setValue(response.IsSuccess() ? OK : S3_INTERNAL_ERROR);
            };
    s3Client_->PutObjectAsync(request, handler, nullptr);
    return promise->getFuture();
}

folly::Future<int> S3DataAdaptor::Delete(const std::string &key) {
    Aws::S3::Model::DeleteObjectRequest request;
    request.SetBucket(GetGlobalConfig().s3_config.bucket);
    request.SetKey(key);
    auto promise = std::make_shared < folly::Promise < int >> ();
    Aws::S3::DeleteObjectResponseReceivedHandler handler =
            [promise](
                    const Aws::S3::S3Client */*client*/,
                    const Aws::S3::Model::DeleteObjectRequest &/*request*/,
                    const Aws::S3::Model::DeleteObjectOutcome &response,
                    const std::shared_ptr<const Aws::Client::AsyncCallerContext> &awsCtx) {
                if (response.IsSuccess()) {
                    promise->setValue(OK);
                } else if (response.GetError().GetErrorType() == Aws::S3::S3Errors::NO_SUCH_KEY) {
                    promise->setValue(NOT_FOUND);
                } else {
                    LOG(ERROR) << "DeleteObjectAsync error: "
                               << response.GetError().GetExceptionName()
                               << "message: " << response.GetError().GetMessage();
                    promise->setValue(S3_INTERNAL_ERROR);
                }
            };
    s3Client_->DeleteObjectAsync(request, handler, nullptr);
    return promise->getFuture();
}

folly::Future<int> S3DataAdaptor::Head(const std::string &key,
                                       size_t &size,
                                       std::map <std::string, std::string> &headers) {
    Aws::S3::Model::HeadObjectRequest request;
    request.SetBucket(GetGlobalConfig().s3_config.bucket);
    request.SetKey(key);
    auto promise = std::make_shared < folly::Promise < int >> ();
    Aws::S3::HeadObjectResponseReceivedHandler handler =
            [promise, &size, &headers](
                    const Aws::S3::S3Client */*client*/,
                    const Aws::S3::Model::HeadObjectRequest &/*request*/,
                    const Aws::S3::Model::HeadObjectOutcome &response,
                    const std::shared_ptr<const Aws::Client::AsyncCallerContext> &awsCtx) {
                if (response.IsSuccess()) {
                    headers = response.GetResult().GetMetadata();
                    size = response.GetResult().GetContentLength();
                    promise->setValue(OK);
                } else if (response.GetError().GetErrorType() == Aws::S3::S3Errors::NO_SUCH_KEY) {
                    promise->setValue(NOT_FOUND);
                } else {
                    LOG(ERROR) << "HeadObjectAsync error: "
                               << response.GetError().GetExceptionName()
                               << "message: " << response.GetError().GetMessage();
                    promise->setValue(S3_INTERNAL_ERROR);
                }
            };
    s3Client_->HeadObjectAsync(request, handler, nullptr);
    return promise->getFuture();
}
