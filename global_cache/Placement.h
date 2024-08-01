#ifndef MADFS_PLACEMENT_H
#define MADFS_PLACEMENT_H

#include <vector>
#include "Common.h"

inline static std::vector<int> Placement(const std::string &key, int num_available, int num_choose) {
    uint64_t seed = std::hash < std::string > {}(key);
    std::vector<int> output;
    for (int i = 0; i < std::min(num_available, num_choose); ++i)
        output.push_back((seed + i) % num_available);
    return output;
}

#endif // MADFS_PLACEMENT_H