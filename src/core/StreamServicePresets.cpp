#include "core/StreamServicePresets.h"

namespace railshot {

std::vector<StreamServicePreset> streamServicePresets() {
    return {
        {"Custom", "Custom", ""},
        {"Twitch", "Twitch", "rtmp://live.twitch.tv/app"},
        {"YouTube", "YouTube", "rtmp://a.rtmp.youtube.com/live2"},
        {"Kick", "Kick", "rtmp://fa723fc1b171.global-contribute.live-video.net/app"},
        {"Facebook", "Facebook Live", "rtmps://live-api-s.facebook.com:443/rtmp"},
    };
}

const StreamServicePreset* findStreamService(const std::string& id) {
    static const auto presets = streamServicePresets();
    for (const auto& p : presets) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

std::string combineRtmpUrl(const std::string& server, const std::string& key) {
    if (server.empty()) {
        return key;
    }
    if (key.empty()) {
        return server;
    }
    if (server.back() == '/') {
        return server + key;
    }
    return server + "/" + key;
}

} // namespace railshot
