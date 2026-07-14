#pragma once

#include <string>
#include <vector>

namespace railshot {

struct NdiSourceInfo {
    std::string name;
    std::string url;
};

class NdiFinder {
public:
    static std::vector<NdiSourceInfo> discoverSources(int timeoutMs = 2000);
    [[nodiscard]] static bool isAvailable();
};

} // namespace railshot
