#ifndef HYBRIDCACHE_THROTTLE_H_
#define HYBRIDCACHE_THROTTLE_H_

#include "folly/TokenBucket.h"
#include "folly/concurrency/ConcurrentHashMap.h"

namespace HybridCache {

class Throttle {
 public:
    Throttle() {}

    // 带宽重分配
    int SetNewLimits(const std::string &input);
    int Put_Consume(const std::string&, size_t);
    void Del_File(const std::string&);
    void Close();
    std::string Cal_New4Test();
    void CleanBlockTime();

 public:
   folly::ConcurrentHashMap<std::string, std::shared_ptr<folly::TokenBucket>> job_tokenlimit_;  // 文件和每个任务的写上限
   folly::ConcurrentHashMap<std::string, double> job_waiting_;  // 文件和阻塞时间
   folly::ConcurrentHashMap<std::string, double> job_bandwidth_;
   //std::atomic<bool> is_resetting_{false};  // 正在重新调整各个文件的带宽
};

}

#endif // HYBRIDCACHE_THROTTLE_H_
