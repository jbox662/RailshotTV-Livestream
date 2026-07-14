#include "capture/AudioDeviceSource.h"

#include "core/Logger.h"

namespace railshot {

AudioDeviceSource::AudioDeviceSource(Source config)
    : config_(std::move(config))
    , capture_(std::make_unique<WasapiAudioCapture>()) {}

AudioDeviceSource::~AudioDeviceSource() {
    stop();
}

bool AudioDeviceSource::start() {
    if (running_.load()) {
        return true;
    }
    if (!capture_->openMicrophone(config_.pathOrDeviceId)) {
        Logger::error("AudioDeviceSource: failed to open device for " + config_.name);
        return false;
    }
    queue_.reset();
    if (!capture_->start(&queue_)) {
        return false;
    }
    running_ = true;
    return true;
}

void AudioDeviceSource::stop() {
    if (!running_.load()) {
        return;
    }
    capture_->stop();
    queue_.shutdown();
    capture_->close();
    running_ = false;
}

bool AudioDeviceSource::isRunning() const {
    return running_.load();
}

void AudioDeviceSource::updateConfig(const Source& source) {
    const std::string oldId = config_.pathOrDeviceId;
    config_ = source;
    if (!running_.load()) {
        return;
    }
    if (oldId == config_.pathOrDeviceId) {
        return;
    }
    capture_->stop();
    queue_.shutdown();
    queue_.reset();
    if (!capture_->openMicrophone(config_.pathOrDeviceId) || !capture_->start(&queue_)) {
        Logger::error("AudioDeviceSource: failed to reopen " + config_.name);
        running_ = false;
    }
}

std::optional<VideoFrame> AudioDeviceSource::latestVideoFrame() {
    return std::nullopt;
}

std::optional<AudioFrame> AudioDeviceSource::latestAudioFrame() {
    std::optional<AudioFrame> latest;
    while (auto frame = queue_.pop(0)) {
        latest = std::move(*frame);
    }
    return latest;
}

} // namespace railshot
