#ifndef MADFS_FILE_SYSTEM_DATA_ADAPTOR_H
#define MADFS_FILE_SYSTEM_DATA_ADAPTOR_H

#include <sys/types.h>
#include <sys/stat.h>
#include <butil/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string>

#include "Common.h"
#include "data_adaptor.h"

#include <folly/File.h>
#include <folly/futures/Future.h>
#include <folly/futures/Promise.h>
#include <folly/experimental/io/SimpleAsyncIO.h>

#include <sys/statvfs.h>

using HybridCache::ByteBuffer;
using HybridCache::DataAdaptor;

static inline ssize_t fully_pread(int fd, void* buf, size_t n, size_t offset) {
    ssize_t total_read = 0;
    ssize_t bytes_read;
    while (total_read < n) {
        bytes_read = pread(fd, buf + total_read, n - total_read, offset);
        if (bytes_read < 0) {
            if (errno == EAGAIN) continue;
            return -1;
        } else if (bytes_read == 0) {
            break;
        }
        total_read += bytes_read;
        offset += bytes_read;
    }
    return total_read;
}

static inline ssize_t fully_pwrite(int fd, void* buf, size_t n, size_t offset) {
    ssize_t total_written = 0;
    ssize_t bytes_written;
    while (total_written < n) {
        bytes_written = pwrite(fd, buf + total_written, n - total_written, offset);
        if (bytes_written < 0) {
            if (errno == EAGAIN) continue;
            return -1;
        } else if (bytes_written == 0) {
            break;
        }
        total_written += bytes_written;
        offset += bytes_written;
    }
    return total_written;
}

class FileSystemDataAdaptor : public DataAdaptor {
    const std::string prefix_;
    std::shared_ptr<DataAdaptor> base_adaptor_;
    bool use_optimized_path_;
    std::shared_ptr<folly::CPUThreadPoolExecutor> executor_;
    bool fsync_required_;

public:
    FileSystemDataAdaptor(const std::string &prefix = "", 
                          std::shared_ptr<DataAdaptor> base_adaptor = nullptr, 
                          bool use_optimized_path = false,
                          std::shared_ptr<folly::CPUThreadPoolExecutor> executor = nullptr,
                          bool fsync_required = true)
        : prefix_(prefix), 
          base_adaptor_(base_adaptor), 
          use_optimized_path_(use_optimized_path), 
          executor_(executor), 
          fsync_required_(fsync_required) {}

    ~FileSystemDataAdaptor() {}

    virtual folly::Future<int> DownLoad(const std::string &key,
                                        size_t start,
                                        size_t size,
                                        ByteBuffer &buffer) {
        LOG_IF(INFO, FLAGS_verbose) << "Download key: " << key << ", start: " << start << ", size: " << size;

        if (!buffer.data || buffer.len < size) {
            LOG(ERROR) << "Buffer capacity is not enough, expected " << size
                       << ", actual " << buffer.len;
            return folly::makeFuture(INVALID_ARGUMENT);
        }

        auto path = BuildPath(prefix_, key);
        if (access(path.c_str(), F_OK)) {
            if (base_adaptor_) {
            #if 1
                size_t full_size;
                std::map<std::string, std::string> headers;
                if (base_adaptor_->Head(key, full_size, headers).get()) {
                    LOG(ERROR) << "Fail to retrive metadata of key: " << key;
                    return folly::makeFuture(IO_ERROR);
                }
                ByteBuffer tmp_buffer(new char[full_size], full_size);
                return base_adaptor_->DownLoad(key, 0, full_size, tmp_buffer).thenValue([buffer, tmp_buffer, start, size, key](int rc) -> int {
                    if (rc) {
                        LOG(ERROR) << "Fail to retrive data of key: " << key;
                        return IO_ERROR;
                    }
                    memcpy(buffer.data, tmp_buffer.data + start, size);
                    delete []tmp_buffer.data;
                    return OK;
                });
            #else 
                return base_adaptor_->DownLoad(key, start, size, buffer);
            #endif
            } else if (errno == ENOENT) {
                LOG_IF(ERROR, FLAGS_verbose) << "File not found: " << path;
                return folly::makeFuture(NOT_FOUND);
            } else {
                PLOG(ERROR) << "Fail inaccessible: " << path;
                return folly::makeFuture(IO_ERROR);
            }
        }

        butil::Timer t;
        t.start();

        const bool kUseDirectIO = false; // ((uint64_t) buffer.data & 4095) == 0 && (size & 4095) == 0;
        int flags = O_RDONLY;
        flags |= kUseDirectIO ? O_DIRECT : 0;
        int fd = open(path.c_str(), flags);
        if (fd < 0) {
            PLOG(ERROR) << "Fail to open file: " << path;
            return folly::makeFuture(IO_ERROR);
        }

#ifdef ASYNC_IO
        if (kUseDirectIO) {
            thread_local folly::SimpleAsyncIO aio(folly::SimpleAsyncIO::Config().setCompletionExecutor(executor_.get()));
            auto promise = std::make_shared<folly::Promise<int>>();
            aio.pread(fd, buffer.data, size, start, [key, size, promise, fd](int rc) {
                if (rc != size) {
                    PLOG(ERROR) << "Fail to read file: " << key 
                                << ", expected read " << size 
                                << ", actual read " << rc;
                    close(fd);
                    promise->setValue(IO_ERROR);
                } else {
                    close(fd);
                    promise->setValue(OK);
                }
            });
            return promise->getFuture();
        }
#endif 

        ssize_t nbytes = fully_pread(fd, buffer.data, size, start);
        if (nbytes != size) {
            PLOG(ERROR) << "Fail to read file: " << key 
                        << ", expected read " << size 
                        << ", actual read " << nbytes;
            close(fd);
            return folly::makeFuture(IO_ERROR);
        }

        t.stop();
        // LOG_EVERY_N(INFO, 1) << t.u_elapsed() << " " << size;

        close(fd);
        return folly::makeFuture(OK);
    }

    virtual folly::Future<int> UpLoad(const std::string &key,
                                      size_t size,
                                      const ByteBuffer &buffer,
                                      const std::map <std::string, std::string> &headers) {
        butil::Timer t;
        t.start();
        LOG_IF(INFO, FLAGS_verbose) << "Upload key: " << key << ", size: " << size;
        if (!buffer.data || buffer.len < size) {
            LOG(ERROR) << "Buffer capacity is not enough, expected " << size
                       << ", actual " << buffer.len;
            return folly::makeFuture(INVALID_ARGUMENT);
        }

        auto path = BuildPath(prefix_, key);
        if (CreateParentDirectories(path)) {
            return folly::makeFuture(IO_ERROR);
        }

        t.stop();
        //LOG(INFO) << "Upload P0: " << key << " " << t.u_elapsed() << " " << size;
        const bool kUseDirectIO = false; // ((uint64_t) buffer.data & 4095) == 0 && (size & 4095) == 0;
        int flags = O_WRONLY | O_CREAT;
        flags |= kUseDirectIO ? O_DIRECT : 0;
        int fd = open(path.c_str(), flags, 0644);
        if (fd < 0) {
            PLOG(ERROR) << "Fail to open file: " << path;
            return folly::makeFuture(IO_ERROR);
        }

#ifdef ASYNC_IO
        if (kUseDirectIO) {
            thread_local folly::SimpleAsyncIO aio(folly::SimpleAsyncIO::Config().setCompletionExecutor(executor_.get()));
            auto promise = std::make_shared<folly::Promise<int>>();
            aio.pwrite(fd, buffer.data, size, 0, [key, size, promise, fd](int rc) {
                if (rc != size) {
                    PLOG(ERROR) << "Fail to write file: " << key 
                                << ", expected " << size 
                                << ", actual " << rc;
                    close(fd);
                    promise->setValue(IO_ERROR);
                }
                
                if (ftruncate64(fd, size) < 0) {
                    PLOG(ERROR) << "Fail to truncate file: " << key;
                    close(fd);
                    return folly::makeFuture(IO_ERROR);
                }

                if (fsync_required_ && fsync(fd) < 0) {
                    PLOG(ERROR) << "Fail to sync file: " << key;
                    close(fd);
                    return folly::makeFuture(IO_ERROR);
                }

                close(fd);
                promise->setValue(OK);
                
            });
            return promise->getFuture();
        }
#endif 

        ssize_t nbytes = fully_pwrite(fd, buffer.data, size, 0);
        if (nbytes != size) {
            PLOG(ERROR) << "Fail to write file: " << key 
                        << ", expected read " << size 
                        << ", actual read " << nbytes;
            close(fd);
            return folly::makeFuture(IO_ERROR);
        }

        t.stop();
        //LOG(INFO) << "Upload P2: " << key << " " << t.u_elapsed() << " " << size;
        if (ftruncate64(fd, size) < 0) {
            PLOG(ERROR) << "Fail to truncate file: " << key;
            close(fd);
            return folly::makeFuture(IO_ERROR);
        }

        t.stop();
        //LOG(INFO) << "Upload P3: " << key << " " << t.u_elapsed() << " " << size;
        if (fsync_required_ && fsync(fd) < 0) {
            PLOG(ERROR) << "Fail to sync file: " << key;
            close(fd);
            return folly::makeFuture(IO_ERROR);
        }

        close(fd);

        if (base_adaptor_) {
            return base_adaptor_->UpLoad(key, size, buffer, headers);
        } 
        t.stop();
        // LOG(INFO) << "Upload P4: " << key << " " << t.u_elapsed() << " " << size;
        return folly::makeFuture(OK);
    }

    virtual folly::Future<int> Delete(const std::string &key) {
        LOG_IF(INFO, FLAGS_verbose) << "Delete key: " << key;
        auto path = BuildPath(prefix_, key);
        if (remove(path.c_str())) {
            if (errno == ENOENT) {
                LOG_IF(ERROR, FLAGS_verbose) << "File not found: " << path;
                return folly::makeFuture(NOT_FOUND);
            } else {
                PLOG(ERROR) << "Failed to remove file: " << path;
                return folly::makeFuture(IO_ERROR);
            }
        }
        if (base_adaptor_) {
            return base_adaptor_->Delete(key);
        } 
        return folly::makeFuture(OK);
    }

    virtual folly::Future<int> Head(const std::string &key,
                                    size_t &size,
                                    std::map <std::string, std::string> &headers) {
        LOG_IF(INFO, FLAGS_verbose) << "Head key: " << key;
        if (base_adaptor_) {
            return base_adaptor_->Head(key, size, headers);
        }
        auto path = BuildPath(prefix_, key);
        struct stat st;
        if (access(path.c_str(), F_OK)) {
            if (errno == ENOENT) {
                LOG_IF(ERROR, FLAGS_verbose) << "File not found: " << path;
                return folly::makeFuture(NOT_FOUND);
            } else {
                PLOG(ERROR) << "Failed to access file: " << path;
                return folly::makeFuture(IO_ERROR);
            }
        }
        if (stat(path.c_str(), &st)) {
            PLOG(ERROR) << "Fail to state file: " << path;
            return folly::makeFuture(IO_ERROR);
        }
        size = st.st_size;
        return folly::makeFuture(OK);
    }

    std::string BuildPath(const std::string &prefix, const std::string &key) {
        if (use_optimized_path_) {
            std::size_t h1 = std::hash<std::string>{}(key);
            std::string suffix = std::to_string(h1 % 256) + '/' + std::to_string(h1 % 65536) + '/' + key;
            return PathJoin(prefix, suffix);
        } else {
            return PathJoin(prefix, key);
        }
    }
};

#endif // MADFS_FILE_SYSTEM_DATA_ADAPTOR_H