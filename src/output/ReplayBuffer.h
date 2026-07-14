#pragma once

#include "encoder/FFmpegEncoder.h"

#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace railshot {

class ReplayBuffer {
public:
    void setSeconds(int seconds);
    void clear();
    void push(const EncodedPacket& packet);
    [[nodiscard]] bool empty() const;
    [[nodiscard]] int seconds() const;

    // Writes buffered packets to path using encoder codec contexts. Starts at first keyframe.
    bool save(const std::string& filePath,
              AVCodecContext* videoCtx,
              AVCodecContext* audioCtx);

private:
    void trimLocked(int fps);

    mutable std::mutex mutex_;
    std::deque<EncodedPacket> packets_;
    int seconds_ = 30;
};

} // namespace railshot
