#include "capture/DesktopAudioSource.h"

#include "core/Logger.h"

namespace railshot {

DesktopAudioSource::DesktopAudioSource(Source config)
    : config_(std::move(config))
    , capture_(std::make_unique<WasapiAudioCapture>()) {}

DesktopAudioSource::~DesktopAudioSource() {
    stop();
}

bool DesktopAudioSource::start() {
    if (running_.load()) {
        return true;
    }
    if (!capture_->openDefaultDesktopLoopback()) {
        Logger::error("DesktopAudioSource: failed to open loopback for " + config_.name);
        return false;
    }
    queue_.reset();
    if (!capture_->start(&queue_)) {
        return false;
    }
    running_ = true;
    return true;
}

void DesktopAudioSource::stop() {
    if (!running_.load()) {
        return;
    }
    capture_->stop();
    queue_.shutdown();
    capture_->close();
    running_ = false;
}

bool DesktopAudioSource::isRunning() const {
    return running_.load();
}

void DesktopAudioSource::updateConfig(const Source& source) {
    config_ = source;
}

std::optional<VideoFrame> DesktopAudioSource::latestVideoFrame() {
    return std::nullopt;
}

std::optional<AudioFrame> DesktopAudioSource::latestAudioFrame() {
    std::optional<AudioFrame> latest;
    while (auto frame = queue_.pop(0)) {
        latest = std::move(*frame);
    }
    return latest;
}

} // namespace railshot
