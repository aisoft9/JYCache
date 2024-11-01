#include <iostream>

#include "gtest/gtest.h"

#include "read_cache.h"

using namespace folly;

using namespace std;
using namespace HybridCache;

ReadCacheConfig cfg;
std::shared_ptr<ThreadPool> executor;
std::shared_ptr<ReadCache> readCache;

const std::string file1 = "testfile1";
const std::string file2 = "testfile2";
const std::string file3 = "testfile3";

const size_t TEST_LEN = 100 * 1024;
std::unique_ptr<char[]> bufIn(new char[TEST_LEN]);
std::unique_ptr<char[]> bufOut(new char[TEST_LEN]);

TEST(ReadCache, Init) {
    srand((unsigned)time(NULL));
    for (int i=0; i<TEST_LEN; ++i) {
        bufIn[i] = rand() % 36 + '0';
    }

    cfg.DownloadNormalFlowLimit = 1024;
    cfg.DownloadBurstFlowLimit = 10 * 1024;

    cfg.CacheCfg.CacheName = "Read";
    cfg.CacheCfg.MaxCacheSize = 100 * 1024 * 1024;
    cfg.CacheCfg.PageBodySize = 64 * 1024;
    cfg.CacheCfg.PageMetaSize = 1024;
    cfg.CacheCfg.EnableCAS = true;
    cfg.CacheCfg.SafeMode = true;
    cfg.CacheCfg.CacheLibCfg.EnableNvmCache = false;

    executor = std::make_shared<ThreadPool>(16);
    auto dataAdaptor = std::make_shared<DataAdaptor4Test>();
    dataAdaptor->SetExecutor(executor);
    readCache = std::make_shared<ReadCache>(cfg, dataAdaptor, executor);
}

TEST(ReadCache, Put) {
    ByteBuffer stepBuffer(bufIn.get(), TEST_LEN);
    EXPECT_EQ(0, readCache->Put(file1, 0, 4, stepBuffer));
    EXPECT_EQ(0, readCache->Put(file2, 5, 4, stepBuffer));
    EXPECT_EQ(0, readCache->Put(file3, 0, TEST_LEN, stepBuffer));
}

TEST(ReadCache, Get_From_Local) {
    ByteBuffer stepBuffer(bufOut.get(), TEST_LEN);

    auto f = readCache->Get(file1, 0, 4, stepBuffer);
    f.wait();
    EXPECT_EQ(0, f.value());
    for (int i=0; i<4; ++i) {
        EXPECT_EQ(bufIn[i], bufOut[i]);
    }

    stepBuffer.data = (bufOut.get() + 5);
    f = readCache->Get(file2, 5, 4, stepBuffer);
    f.wait();
    EXPECT_EQ(0, f.value());
    for (int i=0; i<4; ++i) {
        EXPECT_EQ(bufIn[i], bufOut[i+5]);
    }

    stepBuffer.data = (bufOut.get() + 3);
    f = readCache->Get(file2, 6, 2, stepBuffer);
    f.wait();
    EXPECT_EQ(0, f.value());
    for (int i=0; i<2; ++i) {
        EXPECT_EQ(bufIn[i+1], bufOut[i+3]);
    }

    stepBuffer.data = bufOut.get();
    f = readCache->Get(file3, 0, TEST_LEN, stepBuffer);
    f.wait();
    EXPECT_EQ(0, f.value());
    for (int i=0; i<TEST_LEN; ++i) {
        EXPECT_EQ(bufIn[i], bufOut[i]);
    }
}

TEST(ReadCache, Get_From_Fake_S3) {
    ByteBuffer stepBuffer(bufOut.get(), TEST_LEN);

    auto f = readCache->Get(file2, 0, 10, stepBuffer);
    cout << "wait download from s3 ..." << endl;
    f.wait();
    EXPECT_EQ(REMOTE_FILE_NOT_FOUND, f.value());
    for (int i=0; i<4; ++i) {
        EXPECT_EQ(bufIn[i], bufOut[i+5]);
    }

    f = readCache->Get(file3, 1, TEST_LEN+1, stepBuffer);
    cout << "wait download from s3 ..." << endl;
    f.wait();
    EXPECT_EQ(REMOTE_FILE_NOT_FOUND, f.value());
    for (int i=0; i<TEST_LEN-1; ++i) {
        EXPECT_EQ(bufIn[i+1], bufOut[i]);
    }
}

TEST(ReadCache, GetAllKeys) {
    std::set<std::string> keys;
    readCache->GetAllKeys(keys);
    EXPECT_EQ(3, keys.size());
    EXPECT_EQ(1, keys.count(file1));
    EXPECT_EQ(1, keys.count(file2));
    EXPECT_EQ(1, keys.count(file3));
}

TEST(ReadCache, Delete) {
    std::set<std::string> keys;
    readCache->Delete(file1);
    readCache->GetAllKeys(keys);
    EXPECT_EQ(2, keys.size());
    EXPECT_EQ(0, keys.count(file1));
    EXPECT_EQ(1, keys.count(file2));
    EXPECT_EQ(1, keys.count(file3));
}

int main(int argc, char **argv) {
    printf("Running ReadCache test from %s\n", __FILE__);
    testing::InitGoogleTest(&argc, argv);
    int res = RUN_ALL_TESTS();
    executor->stop();
    readCache.reset();
    return res;
}
