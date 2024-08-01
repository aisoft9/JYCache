#include <iostream>

#include "gtest/gtest.h"

#include "config.h"

using namespace std;
using namespace HybridCache;

TEST(ConfigRead, Read) {
    HybridCacheConfig cfg;
    EXPECT_EQ(true, GetHybridCacheConfig("../../test/hybridcache.conf", cfg));
    EXPECT_EQ(1073741824, cfg.ReadCacheCfg.CacheCfg.MaxCacheSize);
    EXPECT_EQ(16, cfg.ThreadNum);

    EXPECT_EQ(true, cfg.UseGlobalCache);
    EXPECT_EQ("http://192.168.1.87:2379", cfg.GlobalCacheCfg.EtcdAddress);
    EXPECT_EQ(2, cfg.GlobalCacheCfg.GlobalServers.size());
    EXPECT_EQ("optane08:8000", cfg.GlobalCacheCfg.GlobalServers[1]);
}

int main(int argc, char **argv) {
    printf("Running ConfigRead test from %s\n", __FILE__);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
