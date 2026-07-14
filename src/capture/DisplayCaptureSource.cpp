#include "capture/DisplayCaptureSource.h"

#include "core/Logger.h"

#include <cstdlib>

namespace railshot {

DisplayCaptureSource::DisplayCaptureSource(Source config)
    : config_(std::move(config))
    , capture_(std::make_unique<DxgiMonitorCapture>()) {}

DisplayCaptureSource::~DisplayCaptureSource() {
    stop();
}

bool DisplayCaptureSource::start() {
    if (running_.load()) {
        return true;
    }
    int index = 0;
    if (!config_.pathOrDeviceId.empty()) {
        index = std::atoi(config_.pathOrDeviceId.c_str());
    }
    if (!capture_->open(index)) {
        return false;
    }
    queue_.reset();
    if (!capture_->start(&queue_)) {
        return false;
    }
    running_ = true;
    return true;
}

void DisplayCaptureSource::stop() {
    if (!running_.load()) {
        return;
    }
    capture_->stop();
    queue_.shutdown();
    capture_->close();
    running_ = false;
}

bool DisplayCaptureSource::isRunning() const {
    return running_.load();
}

void DisplayCaptureSource::updateConfig(const Source& source) {
    const bool pathChanged = config_.pathOrDeviceId != source.pathOrDeviceId;
    config_ = source;
    if (running_.load() && pathChanged) {
        stop();
        start();
    }
}

std::optional<VideoFrame> DisplayCaptureSource::latestVideoFrame() {
    std::optional<VideoFrame> latest;
    while (auto frame = queue_.pop(0)) {
        latest = std::move(*frame);
    }
    return latest;
}

std::optional<AudioFrame> DisplayCaptureSource::latestAudioFrame() {
    return std::nullopt;
}

} // namespace railshot
