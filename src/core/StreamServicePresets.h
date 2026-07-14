#pragma once

#include <string>
#include <vector>

namespace railshot {

struct StreamServicePreset {
    std::string id;
    std::string displayName;
    std::string serverUrl; // empty = Custom (full URL in stream field)
};

[[nodiscard]] std::vector<StreamServicePreset> streamServicePresets();
[[nodiscard]] const StreamServicePreset* findStreamService(const std::string& id);
[[nodiscard]] std::string combineRtmpUrl(const std::string& server, const std::string& key);

} // namespace railshot
