#include "GlobalCacheServer.h"

namespace gcache {
    GlobalCacheServiceImpl::GlobalCacheServiceImpl(std::shared_ptr<folly::CPUThreadPoolExecutor> executor,
                                                   std::shared_ptr<DataAdaptor> base_adaptor) 
            : executor_(executor) {
        read_cache_ = std::make_shared<ReadCache>(executor_, base_adaptor);
        write_cache_ = std::make_shared<WriteCache>(executor_);
    }

    void GlobalCacheServiceImpl::GetEntryFromReadCache(google::protobuf::RpcController *cntl_base,
                                                       const GetEntryRequest *request,
                                                       GetEntryResponse *response,
                                                       google::protobuf::Closure *done) {
        brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);
        read_cache_->Get(request->key(), request->start(), request->length())
                .thenValue([this, cntl, request, done, response](GetOutput output) {
            response->set_status_code(output.status);
            butil::Timer t;
            t.start();
            cntl->response_attachment().append(output.buf);
            t.stop();
            // LOG_EVERY_N(INFO, 1000) << t.u_elapsed();
            done->Run();
        });
    }

    void GlobalCacheServiceImpl:: PutEntryFromReadCache(google::protobuf::RpcController *cntl_base,
                                                        const PutEntryRequest *request,
                                                        PutEntryResponse *response,
                                                        google::protobuf::Closure *done) {
        brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);
        auto output = read_cache_->Put(request->key(), request->length(), cntl->request_attachment());
        response->set_status_code(output);
        done->Run();
    }

    void GlobalCacheServiceImpl::DeleteEntryFromReadCache(google::protobuf::RpcController *cntl_base,
                                                          const DeleteEntryRequest *request,
                                                          DeleteEntryResponse *response,
                                                          google::protobuf::Closure *done) {
        brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);
        if (request->has_chunk_size() && request->has_max_chunk_id()) {
            response->set_status_code(read_cache_->Delete(request->key(), 
                                                            request->chunk_size(),
                                                            request->max_chunk_id()));
        } else {
            response->set_status_code(read_cache_->Delete(request->key()));
        }
        done->Run();
    }

    void GlobalCacheServiceImpl::GetEntryFromWriteCache(google::protobuf::RpcController *cntl_base,
                                                        const GetEntryRequest *request,
                                                        GetEntryResponse *response,
                                                        google::protobuf::Closure *done) {
        brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);
        auto output = write_cache_->Get(request->key(), request->start(), request->length());
        response->set_status_code(output.status);
        cntl->response_attachment().append(output.buf);
        done->Run();
    }

    void GlobalCacheServiceImpl::PutEntryFromWriteCache(google::protobuf::RpcController *cntl_base,
                                                        const PutEntryRequest *request,
                                                        PutEntryResponse *response,
                                                        google::protobuf::Closure *done) {
        brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);
        auto output = write_cache_->Put(request->key(), request->length(), cntl->request_attachment());
        response->set_status_code(output.status);
        response->set_internal_key(output.internal_key);
        done->Run();
    }

    void GlobalCacheServiceImpl::DeleteEntryFromWriteCache(google::protobuf::RpcController *cntl_base,
                                                           const DeleteEntryRequestForWriteCache *request,
                                                           DeleteEntryResponse *response,
                                                           google::protobuf::Closure *done) {
        brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);
        std::unordered_set<std::string> except_keys;
        for (auto &entry : request->except_keys()) {
            except_keys.insert(entry);
        }
        auto output = write_cache_->Delete(request->key_prefix(), request->max_ts(), except_keys);
        response->set_status_code(output);
        done->Run();
    }

    void GlobalCacheServiceImpl::QueryTsFromWriteCache(google::protobuf::RpcController *cntl_base,
                                                       const QueryTsRequest *request,
                                                       QueryTsResponse *response,
                                                       google::protobuf::Closure *done) {
        brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);
        response->set_timestamp(write_cache_->QueryTS());
        response->set_status_code(OK);
        done->Run();
    }

    void GlobalCacheServiceImpl::Register(google::protobuf::RpcController *cntl_base,
                                          const RegisterRequest *request,
                                          RegisterResponse *response,
                                          google::protobuf::Closure *done) {
        brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);
        response->set_status_code(OK);
        done->Run();
    }
}