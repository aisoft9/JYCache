#include <iostream>

#include "gtest/gtest.h"

#include "write_cache.h"

using namespace folly;

using namespace std;
using namespace HybridCache;

WriteCacheConfig cfg;
std::shared_ptr<WriteCache> writeCache;

const std::string file1 = "testfile1";
const std::string file2 = "testfile2";
const std::string file3 = "testfile3";

const size_t TEST_LEN = 100 * 1024;
std::unique_ptr<char[]> bufIn(new char[TEST_LEN]);
std::unique_ptr<char[]> bufOut(new char[TEST_LEN]);

TEST(WriteCache, Init) {
    srand((unsigned)time(NULL));
    for (int i=0; i<TEST_LEN; ++i) {
        bufIn[i] = rand() % 36 + '0';
    }

    cfg.CacheCfg.CacheName = "Write";
    cfg.CacheCfg.MaxCacheSize = 100 * 1024 * 1024;
    cfg.CacheCfg.PageBodySize = 64 * 1024;
    cfg.CacheCfg.PageMetaSize = 1024;
    cfg.CacheCfg.EnableCAS = true;
    cfg.CacheCfg.CacheLibCfg.EnableNvmCache = false;

    writeCache = std::make_shared<WriteCache>(cfg);
}

TEST(WriteCache, Put) {
    ByteBuffer stepBuffer(bufIn.get(), TEST_LEN);
    EXPECT_EQ(0, writeCache->Put(file1, 0, 4, stepBuffer));
    EXPECT_EQ(0, writeCache->Put(file2, 5, 4, stepBuffer));
    EXPECT_EQ(0, writeCache->Put(file3, 0, TEST_LEN, stepBuffer));
}

TEST(WriteCache, Get) {
    ByteBuffer stepBuffer(bufOut.get(), TEST_LEN);
    std::vector<std::pair<size_t, size_t>> dataBoundary;
    EXPECT_EQ(0, writeCache->Get(file1, 0, 10, stepBuffer, dataBoundary));
    for (int i=0; i<4; ++i) {
        EXPECT_EQ(bufIn[i], bufOut[i]);
    }
    EXPECT_EQ(1, dataBoundary.size());
    EXPECT_EQ(0, dataBoundary.begin()->first);
    EXPECT_EQ(4, dataBoundary.begin()->second);

    dataBoundary.clear();
    EXPECT_EQ(0, writeCache->Get(file2, 0, 10, stepBuffer, dataBoundary));
    for (int i=0; i<4; ++i) {
        EXPECT_EQ(bufIn[i], bufOut[i+5]);
    }
    EXPECT_EQ(1, dataBoundary.size());
    EXPECT_EQ(5, dataBoundary.begin()->first);
    EXPECT_EQ(4, dataBoundary.begin()->second);

    dataBoundary.clear();
    EXPECT_EQ(0, writeCache->Get(file3, 0, TEST_LEN, stepBuffer, dataBoundary));
    for (int i=0; i<TEST_LEN; ++i) {
        EXPECT_EQ(bufIn[i], bufOut[i]);
    }
    EXPECT_EQ(1, dataBoundary.size());
    auto it = dataBoundary.begin();
    EXPECT_EQ(0, it->first);
    EXPECT_EQ(TEST_LEN, it->second);
}

TEST(WriteCache, GetAllCacheWithLock) {
    std::vector<std::pair<ByteBuffer, size_t>> dataSegments;
    EXPECT_EQ(0, writeCache->GetAllCacheWithLock(file1, dataSegments));
    EXPECT_EQ(1, dataSegments.size());
    EXPECT_EQ(0, dataSegments.begin()->second);
    EXPECT_EQ(4, dataSegments.begin()->first.len);
    for (int i=0; i<dataSegments.begin()->first.len; ++i) {
        EXPECT_EQ(bufIn[i], *(dataSegments.begin()->first.data+i));
    }

    dataSegments.clear();
    EXPECT_EQ(0, writeCache->GetAllCacheWithLock(file2, dataSegments));
    EXPECT_EQ(1, dataSegments.size());
    EXPECT_EQ(5, dataSegments.begin()->second);
    EXPECT_EQ(4, dataSegments.begin()->first.len);
    for (int i=0; i<dataSegments.begin()->first.len; ++i) {
        EXPECT_EQ(bufIn[i], *(dataSegments.begin()->first.data+i));
    }

    dataSegments.clear();
    EXPECT_EQ(0, writeCache->GetAllCacheWithLock(file3, dataSegments));
    EXPECT_EQ(2, dataSegments.size());
    auto it = dataSegments.begin();
    EXPECT_EQ(0, it->second);
    EXPECT_EQ(64*1024, it->first.len);
    for (int i=0; i<it->first.len; ++i) {
        EXPECT_EQ(bufIn[i], *(it->first.data+i));
    }
    ++it;
    EXPECT_EQ(64*1024, it->second);
    EXPECT_EQ(TEST_LEN-64*1024, it->first.len);
    for (int i=0; i<it->first.len; ++i) {
        EXPECT_EQ(bufIn[i+64*1024], *(it->first.data+i));
    }
}

TEST(WriteCache, GetAllKeys) {
    std::map<std::string, time_t> keys;
    EXPECT_EQ(0, writeCache->GetAllKeys(keys));
    EXPECT_EQ(3, keys.size());
    EXPECT_EQ(1, keys.count(file1));
    EXPECT_EQ(1, keys.count(file2));
    EXPECT_EQ(1, keys.count(file3));
    for (auto it : keys) {
        cout << "key:" << it.first << " create_time:" << it.second << endl;
    }
}

TEST(WriteCache, GetSize) {
    uint32_t realPageSize = cfg.CacheCfg.PageMetaSize +
            cfg.CacheCfg.PageBodySize/8 + cfg.CacheCfg.PageBodySize;
    cout << "CacheSize:" << writeCache->GetCacheSize() << endl;
    cout << "CacheMaxSize:" << writeCache->GetCacheMaxSize() << endl;
    EXPECT_EQ(realPageSize*4, writeCache->GetCacheSize());
    EXPECT_EQ(cfg.CacheCfg.MaxCacheSize, writeCache->GetCacheMaxSize());
}

TEST(WriteCache, UnLock) {
    writeCache->UnLock(file1);
    writeCache->UnLock(file2);
    writeCache->UnLock(file3);
}

TEST(WriteCache, Truncate) {
    uint32_t pageSize = cfg.CacheCfg.PageBodySize;
    EXPECT_EQ(0, writeCache->Truncate(file3, pageSize+1));

    ByteBuffer stepBuffer(bufOut.get(), TEST_LEN);
    std::vector<std::pair<size_t, size_t>> dataBoundary;
    EXPECT_EQ(0, writeCache->Get(file3, 0, TEST_LEN, stepBuffer, dataBoundary));
    for (int i=0; i<pageSize+1; ++i) {
        EXPECT_EQ(bufIn[i], bufOut[i]);
    }
    EXPECT_EQ(1, dataBoundary.size());
    auto it = dataBoundary.begin();
    EXPECT_EQ(0, it->first);
    EXPECT_EQ(pageSize+1, it->second);

    EXPECT_EQ(0, writeCache->Truncate(file3, pageSize));
    dataBoundary.clear();
    EXPECT_EQ(0, writeCache->Get(file3, 0, TEST_LEN, stepBuffer, dataBoundary));
    for (int i=0; i<pageSize; ++i) {
        EXPECT_EQ(bufIn[i], bufOut[i]);
    }
    EXPECT_EQ(1, dataBoundary.size());
    it = dataBoundary.begin();
    EXPECT_EQ(0, it->first);
    EXPECT_EQ(pageSize, it->second);

    EXPECT_EQ(0, writeCache->Truncate(file3, pageSize-1));
    dataBoundary.clear();
    EXPECT_EQ(0, writeCache->Get(file3, 0, TEST_LEN, stepBuffer, dataBoundary));
    for (int i=0; i<pageSize-1; ++i) {
        EXPECT_EQ(bufIn[i], bufOut[i]);
    }
    EXPECT_EQ(1, dataBoundary.size());
    it = dataBoundary.begin();
    EXPECT_EQ(0, it->first);
    EXPECT_EQ(pageSize-1, it->second);
}

TEST(WriteCache, Delete) {
    EXPECT_EQ(0, writeCache->Delete(file1));
    std::map<std::string, time_t> keys;
    EXPECT_EQ(0, writeCache->GetAllKeys(keys));
    EXPECT_EQ(2, keys.size());
    EXPECT_EQ(0, keys.count(file1));
    EXPECT_EQ(1, keys.count(file2));
    EXPECT_EQ(1, keys.count(file3));

    ByteBuffer stepBuffer(bufOut.get(), TEST_LEN);
    std::vector<std::pair<size_t, size_t>> dataBoundary;
    EXPECT_EQ(0, writeCache->Get(file1, 0, 10, stepBuffer, dataBoundary));
    EXPECT_EQ(0, dataBoundary.size());
}

int main(int argc, char **argv) {
    printf("Running WriteCache test from %s\n", __FILE__);
    testing::InitGoogleTest(&argc, argv);
    int res = RUN_ALL_TESTS();
    writeCache.reset();
    return res;
}
