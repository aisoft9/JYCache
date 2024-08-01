#include <iostream>

#include "middleware/req_res_middleware_wrapper.h"
#ifndef CLIENT_BUILD
#include "filesystem/curve_filesystem.h"
#include "filesystem/s3fs_filesystem.h"
#include "filesystem/dummy_filesystem.h"
#endif
#include "filesystem/abstract_filesystem.h"


namespace intercept {
namespace middleware {
using intercept::common::Configure;
void ReqResMiddlewareWrapper::Init() {

}

void ReqResMiddlewareWrapper::InitServer() {
    if (info_.serverType == "dummy") {
        spdlog::info("dont create fileSystem in ReqResMiddlewareWrapper::InitServer");
        return;
    }
    if (!fileSystem_) {
        #ifndef CLIENT_BUILD
        if (Configure::getInstance().getConfig("backendFilesystem") == "s3fs") {
            fileSystem_.reset(new intercept::filesystem::S3fsFileSystem);
        } else if (Configure::getInstance().getConfig("backendFilesystem") == "curvefs") {
            fileSystem_.reset(new intercept::filesystem::CurveFileSystem);
        } else if (Configure::getInstance().getConfig("backendFilesystem") == "dummyfs") {
            fileSystem_.reset(new intercept::filesystem::DummyFileSystem);
        } else {
            spdlog::error("dont create fileSystem in ReqResMiddlewareWrapper::InitServer");
            return;
        }
        fileSystem_->Init();
        spdlog::info("Initserver, filesystem: {}", Configure::getInstance().getConfig("backendFilesystem"));
        #endif
    } else {
        spdlog::info("ReqResMiddlewareWrapper::InitServer, have inited, donot need to init again");
    }
}

void ReqResMiddlewareWrapper::InitClient() {

}

} // namespace middleware
} // namespace intercept