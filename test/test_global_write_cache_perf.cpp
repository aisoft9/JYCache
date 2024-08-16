#include <vector>
#include <thread>
#include <atomic>
#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include <assert.h>

#include "S3DataAdaptor.h"
#include "FileSystemDataAdaptor.h"
#include "GlobalDataAdaptor.h"
#include "ReadCacheClient.h"

DEFINE_string(server, "0.0.0.0:8000", "IP Address of server");
DEFINE_string(local_dir, "", "Local S3 dir");
DEFINE_int32(threads, 1, "Thread count in perf test");
DEFINE_int32(duration, 5, "Test duration in seconds");
DEFINE_int64(size, 16, "File size in MB");
DEFINE_int32(depth, 1, "IO depth");
DEFINE_bool(use_s3, false, "Use S3 storage");

std::vector<std::string> SplitString(const std::string &input) {
    std::vector<std::string> result;
    std::stringstream ss(input);
    std::string item;
    while (std::getline(ss, item, ',')) {
        result.push_back(item);
    }
    return result;
}

TEST(global_cache_client, perf)
{
    auto etcd_client = std::make_shared<EtcdClient>("http://192.168.3.87:2379");

    std::shared_ptr<DataAdaptor> base_adaptor = std::make_shared<FileSystemDataAdaptor>();
    if (FLAGS_use_s3) {
        base_adaptor = std::make_shared<S3DataAdaptor>();
    } else {
        base_adaptor = std::make_shared<FileSystemDataAdaptor>(FLAGS_local_dir);
    }
    auto global_adaptor = std::make_shared<GlobalDataAdaptor>(base_adaptor, SplitString(FLAGS_server), etcd_client);
    const size_t chunk_size = FLAGS_size * 1024 * 1024;
    std::vector<std::thread> workers;
    std::atomic<bool> running(true);
    std::atomic<uint64_t> operations_total(0);
    butil::Timer t;
    t.start();
    for (int i = 0; i < FLAGS_threads; ++i) {
        workers.emplace_back([&] {
            ByteBuffer buffer[FLAGS_depth];
            for (int j = 0; j < FLAGS_depth; ++j) {
                int ret = posix_memalign((void **) &buffer[j].data, 4096, chunk_size);
                // memset(buffer[j].data, 'x', chunk_size);
                assert(!ret);
                buffer[j].len = chunk_size;
            }
            uint64_t operations = 0;
            std::vector <folly::Future<int>> future_list;
            std::map<std::string, std::string> header;
            while(running) {
                future_list.clear();
                for (int j = 0; j < FLAGS_depth; ++j) {
                    future_list.emplace_back(global_adaptor->UpLoad("foo/write-dummy-" + std::to_string(j), chunk_size, buffer[j], header));
                }
                folly::collectAll(future_list).wait();
                operations += FLAGS_depth;
            }
            operations_total.fetch_add(operations);
        });
    }
    sleep(FLAGS_duration);
    running = false;
    for (int i = 0; i < FLAGS_threads; ++i) {
        workers[i].join();
    }
    t.stop();

    LOG(INFO) << "operation per second: " << operations_total.load() / double(t.s_elapsed())
              << "data transfered (MB/s): " << chunk_size * operations_total.load() / double(t.s_elapsed()) / 1024.0 / 1024.0;
}

int main(int argc, char **argv)
{
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
