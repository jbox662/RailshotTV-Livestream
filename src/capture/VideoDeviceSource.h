#pragma once

#include "capture/ISourceProvider.h"
#include "capture/DirectShowCapture.h"
#include "core/ThreadSafeQueue.h"

#include <atomic>
#include <memory>

namespace railshot {

class VideoDeviceSource : public ISourceProvider {
public:
    explicit VideoDeviceSource(Source config);
    ~VideoDeviceSource() override;

    bool start() override;
    void stop() override;
    [[nodiscard]] bool isRunning() const override;

    [[nodiscard]] std::optional<VideoFrame> latestVideoFrame() override;
    [[nodiscard]] std::optional<AudioFrame> latestAudioFrame() override;

    [[nodiscard]] bool hasVideo() const override { return true; }
    [[nodiscard]] bool hasAudio() const override { return false; }
    [[nodiscard]] const Source& config() const override { return config_; }

private:
    Source config_;
    std::unique_ptr<DirectShowCapture> capture_;
    ThreadSafeQueue<VideoFrame> videoQueue_;
    std::atomic<bool> running_{false};
};

} // namespace railshot
