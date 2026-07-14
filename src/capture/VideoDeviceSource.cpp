#include "capture/VideoDeviceSource.h"

#include "core/Logger.h"
#include "output/IsoRecorder.h"

namespace railshot {

VideoDeviceSource::VideoDeviceSource(Source config)
    : config_(std::move(config))
    , capture_(std::make_unique<DirectShowCapture>()) {}

VideoDeviceSource::~VideoDeviceSource() {
    stop();
}

bool VideoDeviceSource::start() {
    if (running_.load()) {
        return true;
    }
    if (config_.pathOrDeviceId.empty()) {
        Logger::error("VideoDeviceSource: no device id for " + config_.name);
        return false;
    }
    if (!capture_->open(config_.pathOrDeviceId)) {
        return false;
    }
    videoQueue_.reset();
    if (!capture_->start(&videoQueue_)) {
        return false;
    }
    running_ = true;
    return true;
}

void VideoDeviceSource::stop() {
    if (!running_.load()) {
        return;
    }
    capture_->stop();
    videoQueue_.shutdown();
    running_ = false;
}

bool VideoDeviceSource::isRunning() const {
    return running_.load();
}

std::optional<VideoFrame> VideoDeviceSource::latestVideoFrame() {
    std::optional<VideoFrame> latest;
    while (auto frame = videoQueue_.pop(0)) {
        if (config_.isoRecording) {
            IsoRecorderManager::instance().onRawFrame(config_.id, config_.name, *frame);
        }
        latest = std::move(*frame);
    }
    return latest;
}

std::optional<AudioFrame> VideoDeviceSource::latestAudioFrame() {
    return std::nullopt;
}

} // namespace railshot
