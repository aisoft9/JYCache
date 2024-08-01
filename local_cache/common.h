/*
 * Project: HybridCache
 * Created Date: 24-2-21
 * Author: lshb
 */
#ifndef HYBRIDCACHE_COMMON_H_
#define HYBRIDCACHE_COMMON_H_

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "folly/executors/CPUThreadPoolExecutor.h"

namespace HybridCache {

typedef folly::CPUThreadPoolExecutor ThreadPool;

static const char PAGE_SEPARATOR = 26;

static const uint32_t BYTE_LEN = 8;

// ConcurrentSkipList height
static const int SKIP_LIST_HEIGHT = 2;

extern bool EnableLogging;

struct ByteBuffer {
    char* data;
    size_t len;
    ByteBuffer(char* buf = nullptr, size_t bufLen = 0) : data(buf), len(bufLen) {}
};

void split(const std::string& str, const char delim,
           std::vector<std::string>& items);

}  // namespace HybridCache

#endif // HYBRIDCACHE_COMMON_H_
