#include "output/VirtualCamOutput.h"

#include "core/Logger.h"
#include "output/virtualcam/shared-memory-queue.h"

#include <cstdlib>
#include <fstream>
#include <string>

namespace railshot {

VirtualCamOutput::VirtualCamOutput() = default;

VirtualCamOutput::~VirtualCamOutput() {
    stop();
}

void VirtualCamOutput::writeResFile(int width, int height, uint64_t interval) const {
    const char* appData = std::getenv("APPDATA");
    if (!appData) {
        return;
    }
    const std::string path = std::string(appData) + "\\railshot-virtualcam.txt";
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        return;
    }
    out << width << "x" << height << "x" << interval;
}

bool VirtualCamOutput::start(int width, int height, int fps) {
    std::lock_guard lock(mutex_);
    if (active_.load()) {
        return true;
    }
    if (width < 16 || height < 16 || fps < 1) {
        return false;
    }

    const uint64_t interval = 10000000ULL / static_cast<uint64_t>(fps);
    writeResFile(width, height, interval);

    queue_ = video_queue_create(static_cast<uint32_t>(width), static_cast<uint32_t>(height), interval);
    if (!queue_) {
        Logger::error("VirtualCamOutput: failed to create shared queue (already running?)");
        return false;
    }

    width_ = width;
    height_ = height;
    active_ = true;
    Logger::info("VirtualCamOutput: started " + std::to_string(width) + "x" + std::to_string(height)
                  + "@" + std::to_string(fps));
    return true;
}

void VirtualCamOutput::stop() {
    std::lock_guard lock(mutex_);
    if (!active_.load() && !queue_) {
        return;
    }
    active_ = false;
    video_queue_close(queue_);
    queue_ = nullptr;
    width_ = 0;
    height_ = 0;
    Logger::info("VirtualCamOutput: stopped");
}

void VirtualCamOutput::pushFrame(const VideoFrame& frame) {
    if (!active_.load()) {
        return;
    }
    std::lock_guard lock(mutex_);
    if (!queue_ || !frame.isValid() || frame.format != PixelFormat::NV12) {
        return;
    }
    if (frame.width != width_ || frame.height != height_) {
        return;
    }

    uint8_t* planes[2];
    uint32_t linesize[2];
    planes[0] = const_cast<uint8_t*>(frame.data.data());
    planes[1] = planes[0] + static_cast<size_t>(frame.width) * static_cast<size_t>(frame.height);
    linesize[0] = static_cast<uint32_t>(frame.width);
    linesize[1] = static_cast<uint32_t>(frame.width);

    const uint64_t ts = static_cast<uint64_t>(frame.timestampUs) * 10ULL; // us -> 100ns
    video_queue_write(queue_, planes, linesize, ts);
}

} // namespace railshot
