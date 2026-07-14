#include "capture/GameCaptureSource.h"

#include "core/Logger.h"

#include <cstdlib>

namespace railshot {

GameCaptureSource::GameCaptureSource(Source config)
    : config_(std::move(config))
    , wgc_(std::make_unique<WgcWindowCapture>())
    , bitblt_(std::make_unique<WindowBitbltCapture>()) {}

GameCaptureSource::~GameCaptureSource() {
    stop();
}

bool GameCaptureSource::start() {
    if (running_.load()) {
        return true;
    }
    if (config_.pathOrDeviceId.empty()) {
        Logger::error("GameCaptureSource: no HWND for " + config_.name);
        return false;
    }
    const auto hwnd = static_cast<uintptr_t>(std::strtoull(config_.pathOrDeviceId.c_str(), nullptr, 10));
    queue_.reset();
    usingWgc_ = false;

    if (WgcWindowCapture::isSupported() && wgc_->open(hwnd) && wgc_->start(&queue_)) {
        usingWgc_ = true;
        running_ = true;
        Logger::info("GameCaptureSource: WGC capture for " + config_.name);
        return true;
    }
    wgc_->close();
    if (!bitblt_->open(hwnd) || !bitblt_->start(&queue_)) {
        return false;
    }
    running_ = true;
    Logger::warn("GameCaptureSource: BitBlt fallback (WGC unavailable) for " + config_.name);
    return true;
}

void GameCaptureSource::stop() {
    if (!running_.load()) {
        return;
    }
    if (usingWgc_) {
        wgc_->stop();
        wgc_->close();
    } else {
        bitblt_->stop();
        bitblt_->close();
    }
    queue_.shutdown();
    running_ = false;
    usingWgc_ = false;
}

bool GameCaptureSource::isRunning() const {
    return running_.load();
}

void GameCaptureSource::updateConfig(const Source& source) {
    const bool pathChanged = config_.pathOrDeviceId != source.pathOrDeviceId;
    config_ = source;
    if (running_.load() && pathChanged) {
        stop();
        start();
    }
}

std::optional<VideoFrame> GameCaptureSource::latestVideoFrame() {
    std::optional<VideoFrame> latest;
    while (auto frame = queue_.pop(0)) {
        latest = std::move(*frame);
    }
    return latest;
}

std::optional<AudioFrame> GameCaptureSource::latestAudioFrame() {
    return std::nullopt;
}

} // namespace railshot
