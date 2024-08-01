#include <vector>
#include <thread>
#include <atomic>
#include <stdio.h>
#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "FileSystemDataAdaptor.h"
#include "GlobalDataAdaptor.h"
#include "ReadCacheClient.h"

DEFINE_string(server, "0.0.0.0:8000", "IP Address of server");
DEFINE_int32(bench_repeat, 1000, "Repeat count");
DEFINE_int32(bench_size, 1024 * 16, "Request size in bytes");
DEFINE_string(filename, "sample.dat", "Test file name");

std::string ReadDirectly(const std::string &path, size_t start, size_t length) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        PLOG(ERROR) << "Fail to open file: " << path;
        return "<error>";
    }

    if (lseek(fd, start, SEEK_SET) < 0) {
        PLOG(ERROR) << "Fail to seek file: " << path << " at pos " << start;
        close(fd);
        return "<error>";
    }

    std::string output;
    output.resize(length);
    ssize_t nbytes = read(fd, &output[0], length);
    if (nbytes != length) {
        PLOG(ERROR) << "Fail to read file: " << path 
                    << ", expected read " << length 
                    << ", actual read " << nbytes;
        close(fd);
        return "<error>";
    }
    close(fd);
    return output;
}

ssize_t GetSize(const std::string &path) {
    struct stat st;
    if (stat(path.c_str(), &st)) {
        PLOG(ERROR) << "Fail to state file: " << path;
        return -1;
    }
    return st.st_size;
}

std::vector<std::string> SplitString(const std::string &input) {
    std::vector<std::string> result;
    std::stringstream ss(input);
    std::string item;
    while (std::getline(ss, item, ',')) {
        result.push_back(item);
    }
    return result;
}

TEST(read_cache, generate_get_chunk_request)
{
    const size_t chunk_size = GetGlobalConfig().default_policy.read_chunk_size;
    ByteBuffer mock_buffer((char *) 0, 10 * chunk_size);
    auto get_chunk_request = ReadCacheClient::GenerateGetChunkRequestsV2;

    // 0 ... CS+16========2CS
    {
        std::vector<ReadCacheClient::GetChunkRequestV2> requests;
        get_chunk_request("foo", chunk_size + 16, chunk_size - 16, mock_buffer, requests, chunk_size);
        ASSERT_EQ(requests.size(), 1);
        ASSERT_EQ(requests[0].chunk_id, 1);
        ASSERT_EQ(requests[0].chunk_start, 16);
        ASSERT_EQ(requests[0].chunk_len, chunk_size - 16);
        ASSERT_EQ(requests[0].buffer.data, (char *) 0);
        ASSERT_EQ(requests[0].buffer.len, chunk_size - 16);
        ASSERT_EQ(requests[0].user_key, "foo");
        ASSERT_EQ(requests[0].internal_key, "foo-1-" + std::to_string(chunk_size));
    }

    // 0 ... CS+16========2CS===2CS+16
    {
        std::vector<ReadCacheClient::GetChunkRequestV2> requests;
        get_chunk_request("foo", chunk_size + 16, chunk_size, mock_buffer, requests, chunk_size);
        ASSERT_EQ(requests.size(), 2);
        ASSERT_EQ(requests[0].chunk_id, 1);
        ASSERT_EQ(requests[0].chunk_start, 16);
        ASSERT_EQ(requests[0].chunk_len, chunk_size - 16);
        ASSERT_EQ(requests[0].buffer.data, (char *) 0);
        ASSERT_EQ(requests[0].buffer.len, chunk_size - 16);
        ASSERT_EQ(requests[0].user_key, "foo");
        ASSERT_EQ(requests[0].internal_key, "foo-1-" + std::to_string(chunk_size));
        ASSERT_EQ(requests[1].chunk_id, 2);
        ASSERT_EQ(requests[1].chunk_start, 0);
        ASSERT_EQ(requests[1].chunk_len, 16);
        ASSERT_EQ(requests[1].buffer.data, (char *) chunk_size - 16);
        ASSERT_EQ(requests[1].buffer.len, 16);
        ASSERT_EQ(requests[1].user_key, "foo");
        ASSERT_EQ(requests[1].internal_key, "foo-2-" + std::to_string(chunk_size));
    }

    // empty request
    {
        std::vector<ReadCacheClient::GetChunkRequestV2> requests;
        get_chunk_request("foo", chunk_size + 16, 0, mock_buffer, requests, chunk_size);
        ASSERT_EQ(requests.size(), 0);
    }
}

TEST(read_cache, get_chunk)
{
    auto etcd_client = std::make_shared<EtcdClient>("http://127.0.0.1:2379");
    auto base = std::make_shared<FileSystemDataAdaptor>();
    auto global = std::make_shared<GlobalDataAdaptor>(base, SplitString(FLAGS_server), etcd_client);

    const size_t chunk_size = GetGlobalConfig().default_policy.read_chunk_size;
    ByteBuffer buffer(new char[size_t(FLAGS_bench_size)], size_t(FLAGS_bench_size));
    size_t file_size = GetSize("sample.dat");

    for (int i = 0; i < FLAGS_bench_repeat; ++i) {
        size_t start_pos = lrand48() % file_size;
        size_t length = std::min(size_t(FLAGS_bench_size), file_size - start_pos);
        if (length) length = lrand48() % length;
        ASSERT_EQ(0, global->DownLoad("sample.dat", start_pos, length, buffer).get());
        buffer.data[length] = '\0';
        std::string buffer_cpp(buffer.data, length);
        ASSERT_EQ(buffer_cpp, ReadDirectly("sample.dat", start_pos, length));
    }

    ASSERT_EQ(OK, global->DownLoad("sample.dat", file_size - 2, 0, buffer).get());
    ASSERT_EQ(OK, global->DownLoad("sample.dat", file_size - 2, 2, buffer).get());
    ASSERT_EQ(END_OF_FILE, global->DownLoad("sample.dat", file_size - 2, 5, buffer).get());
}

TEST(read_cache, mix_read_write)
{
    auto etcd_client = std::make_shared<EtcdClient>("http://127.0.0.1:2379");
    auto base = std::make_shared<FileSystemDataAdaptor>();
    auto global = std::make_shared<GlobalDataAdaptor>(base, SplitString(FLAGS_server), etcd_client);

    const size_t chunk_size = GetGlobalConfig().default_policy.read_chunk_size;
    ByteBuffer buffer(new char[10 * chunk_size], 10 * chunk_size);
    std::map <std::string, std::string> headers;
    for (size_t i = 0; i < buffer.len; ++i) {
        buffer.data[i] = lrand48() % 26 + 'a';
    }
    std::string buffer_backup(buffer.data, buffer.len);
    ASSERT_EQ(0, global->UpLoad("hello", buffer.len, buffer, headers).get());
    memset(buffer.data, 0, buffer.len);
    ASSERT_EQ(0, global->DownLoad("hello", 0, buffer.len, buffer).get());
    ASSERT_EQ(std::string(buffer.data, buffer.len).substr(32), buffer_backup.substr(32));

    strcpy(buffer.data, "Hello Madfs-----");
    ASSERT_EQ(0, global->UpLoad("hello", 17, buffer, headers).get());
    memset(buffer.data, 0, buffer.len);
    ASSERT_EQ(0, global->DownLoad("hello", 6, 5, buffer).get());
    ASSERT_EQ(buffer.data, std::string("Madfs"));

    size_t fsize;
    ASSERT_EQ(0, global->Head("hello", fsize, headers).get());
    ASSERT_EQ(fsize, 17);

    ASSERT_EQ(0, global->DeepFlush("hello").get());

    ASSERT_EQ(0, global->Delete("hello").get());
    ASSERT_EQ(NOT_FOUND, global->Head("hello", fsize, headers).get());
}

int main(int argc, char **argv)
{
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
