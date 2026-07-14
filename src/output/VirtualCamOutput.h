#pragma once

#include "core/FrameData.h"

#include <atomic>
#include <cstdint>
#include <mutex>

struct video_queue;

namespace railshot {

class VirtualCamOutput {
public:
    VirtualCamOutput();
    ~VirtualCamOutput();

    VirtualCamOutput(const VirtualCamOutput&) = delete;
    VirtualCamOutput& operator=(const VirtualCamOutput&) = delete;

    bool start(int width, int height, int fps);
    void stop();
    void pushFrame(const VideoFrame& frame);

    [[nodiscard]] bool isActive() const { return active_.load(); }

private:
    void writeResFile(int width, int height, uint64_t interval) const;

    mutable std::mutex mutex_;
    video_queue* queue_ = nullptr;
    std::atomic<bool> active_{false};
    int width_ = 0;
    int height_ = 0;
};

} // namespace railshot
