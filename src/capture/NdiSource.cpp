#include "capture/NdiSource.h"

#include "core/Logger.h"
#include "output/IsoRecorder.h"

namespace railshot {

NdiSource::NdiSource(Source config)
    : config_(std::move(config)) {}

NdiSource::~NdiSource() {
    stop();
}

bool NdiSource::start() {
    if (running_.load()) {
        return true;
    }
    if (config_.pathOrDeviceId.empty()) {
        Logger::error("NdiSource: no NDI source URL for " + config_.name);
        return false;
    }

#ifdef RAILSHOT_HAS_NDI
    Logger::info("NdiSource: connecting to " + config_.pathOrDeviceId);
    running_ = true;
    return true;
#else
    Logger::warn("NdiSource: NDI SDK not installed — cannot start " + config_.name);
    return false;
#endif
}

void NdiSource::stop() {
    running_ = false;
}

bool NdiSource::isRunning() const {
    return running_.load();
}

std::optional<VideoFrame> NdiSource::latestVideoFrame() {
    return std::nullopt;
}

std::optional<AudioFrame> NdiSource::latestAudioFrame() {
    return std::nullopt;
}

bool NdiSource::hasVideo() const {
    return running_.load();
}

bool NdiSource::hasAudio() const {
    return running_.load();
}

} // namespace railshot
