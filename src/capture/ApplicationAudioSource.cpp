#include "capture/ApplicationAudioSource.h"

#include "core/Logger.h"

#include <cstdlib>

namespace railshot {

ApplicationAudioSource::ApplicationAudioSource(Source config)
    : config_(std::move(config))
    , capture_(std::make_unique<WasapiAudioCapture>()) {}

ApplicationAudioSource::~ApplicationAudioSource() {
    stop();
}

bool ApplicationAudioSource::start() {
    if (running_.load()) {
        return true;
    }
    const unsigned long pid =
        static_cast<unsigned long>(std::strtoul(config_.pathOrDeviceId.c_str(), nullptr, 10));
    if (!capture_->openProcessLoopback(pid)) {
        Logger::error("ApplicationAudioSource: failed to open pid for " + config_.name);
        return false;
    }
    queue_.reset();
    if (!capture_->start(&queue_)) {
        return false;
    }
    running_ = true;
    return true;
}

void ApplicationAudioSource::stop() {
    if (!running_.load()) {
        return;
    }
    capture_->stop();
    queue_.shutdown();
    capture_->close();
    running_ = false;
}

bool ApplicationAudioSource::isRunning() const {
    return running_.load();
}

void ApplicationAudioSource::updateConfig(const Source& source) {
    const std::string oldId = config_.pathOrDeviceId;
    config_ = source;
    if (!running_.load() || oldId == config_.pathOrDeviceId) {
        return;
    }
    stop();
    start();
}

std::optional<VideoFrame> ApplicationAudioSource::latestVideoFrame() {
    return std::nullopt;
}

std::optional<AudioFrame> ApplicationAudioSource::latestAudioFrame() {
    std::optional<AudioFrame> latest;
    while (auto frame = queue_.pop(0)) {
        latest = std::move(*frame);
    }
    return latest;
}

} // namespace railshot
