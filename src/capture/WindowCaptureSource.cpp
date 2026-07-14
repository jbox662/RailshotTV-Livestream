#include "capture/WindowCaptureSource.h"

#include "core/Logger.h"

#include <cstdlib>

namespace railshot {

WindowCaptureSource::WindowCaptureSource(Source config)
    : config_(std::move(config))
    , capture_(std::make_unique<WindowBitbltCapture>()) {}

WindowCaptureSource::~WindowCaptureSource() {
    stop();
}

bool WindowCaptureSource::start() {
    if (running_.load()) {
        return true;
    }
    if (config_.pathOrDeviceId.empty()) {
        Logger::error("WindowCaptureSource: no HWND for " + config_.name);
        return false;
    }
    const auto hwnd = static_cast<uintptr_t>(std::strtoull(config_.pathOrDeviceId.c_str(), nullptr, 10));
    if (!capture_->open(hwnd)) {
        return false;
    }
    queue_.reset();
    if (!capture_->start(&queue_)) {
        return false;
    }
    running_ = true;
    return true;
}

void WindowCaptureSource::stop() {
    if (!running_.load()) {
        return;
    }
    capture_->stop();
    queue_.shutdown();
    capture_->close();
    running_ = false;
}

bool WindowCaptureSource::isRunning() const {
    return running_.load();
}

void WindowCaptureSource::updateConfig(const Source& source) {
    const bool pathChanged = config_.pathOrDeviceId != source.pathOrDeviceId;
    config_ = source;
    if (running_.load() && pathChanged) {
        stop();
        start();
    }
}

std::optional<VideoFrame> WindowCaptureSource::latestVideoFrame() {
    std::optional<VideoFrame> latest;
    while (auto frame = queue_.pop(0)) {
        latest = std::move(*frame);
    }
    return latest;
}

std::optional<AudioFrame> WindowCaptureSource::latestAudioFrame() {
    return std::nullopt;
}

} // namespace railshot
