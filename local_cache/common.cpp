#include "common.h"

namespace HybridCache {

bool EnableLogging = true;

void split(const std::string& str, const char delim,
           std::vector<std::string>& items) {
    std::istringstream iss(str);
    std::string tmp;
    while (std::getline(iss, tmp, delim)) {
        if (!tmp.empty()) {
            items.emplace_back(std::move(tmp));
        }
    }
}

}  // namespace HybridCache
