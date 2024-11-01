#include <iostream>

#include "gtest/gtest.h"

#include "errorcode.h"
#include "page_cache.h"

using namespace folly;

using namespace std;
using namespace HybridCache;

CacheConfig cfg;
std::shared_ptr<PageCacheImpl> page;

const std::string key1 = "007";
const std::string key2 = "009";

const size_t TEST_LEN = 64 * 1024;
std::unique_ptr<char[]> bufIn(new char[TEST_LEN]);
std::unique_ptr<char[]> bufOut(new char[TEST_LEN]);

TEST(PageCache, Init) {
    srand((unsigned)time(NULL));
    for (int i=0; i<TEST_LEN; ++i) {
        bufIn[i] = rand() % 36 + '0';
    }

    cfg.CacheName = "Read";
    cfg.MaxCacheSize = 100 * 1024 * 1024;
    cfg.PageBodySize = 64 * 1024;
    cfg.PageMetaSize = 1024;
    cfg.EnableCAS = true;
    cfg.SafeMode = true;
    cfg.CacheLibCfg.EnableNvmCache = false;
    page = std::make_shared<PageCacheImpl>(cfg);
    EXPECT_EQ(0, page->Init());
}

TEST(PageCache, Write) {
    EXPECT_EQ(0, page->Write(key1, 0, 4, bufIn.get()));
    EXPECT_EQ(0, page->Write(key1, 5, 4, bufIn.get()));
    EXPECT_EQ(0, page->Write(key2, 0, TEST_LEN, bufIn.get()));
}

TEST(PageCache, Read) {
    std::vector<std::pair<size_t, size_t>> dataBoundary;
    EXPECT_EQ(0, page->Read(key1, 0, 10, bufOut.get(), dataBoundary));

    for (int i=0; i<4; ++i) {
        EXPECT_EQ(bufIn[i], bufOut[i]);
    }
    for (int i=0; i<4; ++i) {
        EXPECT_EQ(bufIn[i], bufOut[i+5]);
    }

    EXPECT_EQ(2, dataBoundary.size());
    auto it = dataBoundary.begin();
    for (int i=0; i<2; ++i) {
        if (i==0) {
            EXPECT_EQ(0, it->first);
            EXPECT_EQ(4, it->second);
        } else {
            EXPECT_EQ(5, it->first);
            EXPECT_EQ(4, it->second);
        }
        ++it;
    }

    dataBoundary.clear();
    EXPECT_EQ(0, page->Read(key1, 5, 4, bufOut.get(), dataBoundary));
    for (int i=0; i<4; ++i) {
        EXPECT_EQ(bufIn[i], bufOut[i]);
    }
    EXPECT_EQ(1, dataBoundary.size());
    EXPECT_EQ(0, dataBoundary.begin()->first);
    EXPECT_EQ(4, dataBoundary.begin()->second);

    dataBoundary.clear();
    EXPECT_EQ(0, page->Read(key2, 0, TEST_LEN, bufOut.get(), dataBoundary));
    for (int i=0; i<TEST_LEN; ++i) {
        EXPECT_EQ(bufIn[i], bufOut[i]);
    }
    EXPECT_EQ(1, dataBoundary.size());
    EXPECT_EQ(0, dataBoundary.begin()->first);
    EXPECT_EQ(TEST_LEN, dataBoundary.begin()->second);

    dataBoundary.clear();
    EXPECT_EQ(0, page->Read(key2, 1, TEST_LEN-1, bufOut.get(), dataBoundary));
    for (int i=0; i<TEST_LEN-1; ++i) {
        EXPECT_EQ(bufIn[i+1], bufOut[i]);
    }
    EXPECT_EQ(1, dataBoundary.size());
    EXPECT_EQ(0, dataBoundary.begin()->first);
    EXPECT_EQ(TEST_LEN-1, dataBoundary.begin()->second);
}

TEST(PageCache, GetAllCache) {
    std::vector<std::pair<ByteBuffer, size_t>> dataSegments;
    page->GetAllCache(key1, dataSegments);
    EXPECT_EQ(2, dataSegments.size());
    EXPECT_EQ(0, dataSegments[0].second);
    EXPECT_EQ(5, dataSegments[1].second);

    for (auto& it : dataSegments) {
        EXPECT_EQ(4, it.first.len);
        for (int i=0; i<it.first.len; ++i) {
            EXPECT_EQ(bufIn[i], *(it.first.data+i));
        }
    }

    dataSegments.clear();
    page->GetAllCache(key2, dataSegments);
    EXPECT_EQ(1, dataSegments.size());
    EXPECT_EQ(0, dataSegments[0].second);
    EXPECT_EQ(TEST_LEN, dataSegments[0].first.len);
    for (int i=0; i<dataSegments[0].first.len; ++i) {
        EXPECT_EQ(bufIn[i], *(dataSegments[0].first.data+i));
    }
}

TEST(PageCache, GetCacheSize) {
    uint32_t bitmapSize = cfg.PageBodySize / BYTE_LEN;
    uint32_t realSize = cfg.PageMetaSize + bitmapSize + cfg.PageBodySize;
    EXPECT_EQ(realSize*2, page->GetCacheSize());
}

TEST(PageCache, GetCacheMaxSize) {
    EXPECT_EQ(100 * 1024 * 1024, page->GetCacheMaxSize());
}

TEST(PageCache, DeletePart) {
    EXPECT_EQ(0, page->DeletePart(key1, 0, 4));
    std::vector<std::pair<size_t, size_t>> dataBoundary;
    EXPECT_EQ(0, page->Read(key1, 0, 10, bufOut.get(), dataBoundary));
    for (int i=0; i<4; ++i) {
        EXPECT_EQ(bufIn[i], bufOut[i+5]);
    }
    EXPECT_EQ(1, dataBoundary.size());
    auto it = dataBoundary.begin();
    EXPECT_EQ(5, it->first);
    EXPECT_EQ(4, it->second);

    EXPECT_EQ(0, page->DeletePart(key2, 0, 1));
    dataBoundary.clear();
    EXPECT_EQ(0, page->Read(key2, 0, TEST_LEN, bufOut.get(), dataBoundary));
    for (int i=0; i<TEST_LEN-1; ++i) {
        EXPECT_EQ(bufIn[i+1], bufOut[i+1]);
    }
    EXPECT_EQ(1, dataBoundary.size());
    it = dataBoundary.begin();
    EXPECT_EQ(1, it->first);
    EXPECT_EQ(TEST_LEN-1, it->second);
}

TEST(PageCache, Delete) {
    EXPECT_EQ(0, page->Delete(key1));
    std::vector<std::pair<size_t, size_t>> dataBoundary;
    EXPECT_EQ(ErrCode::PAGE_NOT_FOUND,
              page->Read(key1, 0, 10, bufOut.get(), dataBoundary));

    EXPECT_EQ(0, page->Delete(key2));
    dataBoundary.clear();
    EXPECT_EQ(ErrCode::PAGE_NOT_FOUND,
              page->Read(key2, 0, TEST_LEN, bufOut.get(), dataBoundary));
}

int main(int argc, char **argv) {
    printf("Running PageCache test from %s\n", __FILE__);
    testing::InitGoogleTest(&argc, argv);
    int res = RUN_ALL_TESTS();
    page->Close();
    page.reset();
    return res;
}
