#include <iostream>
#include <thread>

#include "folly/futures/Future.h"
#include "gtest/gtest.h"

#include "common.h"

using namespace folly;
using namespace std;

std::shared_ptr<HybridCache::ThreadPool> executor;

folly::Future<int> test(int i) {
    std::cout << i << " start" << endl;
    return folly::via(executor.get(), [i]() -> int {
        std::cout << i << " download ..." << endl;
        std::this_thread::sleep_for(2000ms);
        std::cout << i << " end" << endl;
        return 0;
    });
}

folly::Future<int> testCombine() {
    std::cout << "testCombine start" << endl;

    std::vector<Future<int>> fs;
    for (int i = 0; i < 3; i++) {
        fs.push_back(test(i));
    }

    std::cout << "testCombine mid" << endl;

    auto f = collectAll(fs).via(executor.get())
      .thenValue([](std::vector<folly::Try<int>, std::allocator<folly::Try<int>>>&& tups) {
          int res = 0;
          for (const auto& t : tups) {
              if (t.value() == 0) ++res;
          }
         std::cout << "testCombine end" << endl;
         return res;
      });

    return f;
}

TEST(FollyFuture, combine) {
    auto f = testCombine();
    std::cout << "testCombine running..." << endl;
    f.wait();
    std::cout << "testCombine res:" << f.value() << endl;
    EXPECT_EQ(3, f.value());
}

TEST(FollyFuture, chaining) {
    std::cout << "test chaining..." << endl;
    auto f = test(1);
    auto f2 = move(f).thenValue([](int i){ return i + 100; });
    f2.wait();
    std::cout << "chaining res:" << f2.value() << endl;
}

int main(int argc, char* argv[]) {
    executor = std::make_shared<HybridCache::ThreadPool>(16);
    
    printf("Running folly::future test from %s\n", __FILE__);
    testing::InitGoogleTest(&argc, argv);
    int res = RUN_ALL_TESTS();

    executor->stop();
    return res;
}
