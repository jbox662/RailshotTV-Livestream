#pragma once

#include "capture/ISourceProvider.h"
#include "capture/WindowBitbltCapture.h"
#include "core/ThreadSafeQueue.h"

#include <atomic>
#include <memory>

namespace railshot {

class WindowCaptureSource : public ISourceProvider {
public:
    explicit WindowCaptureSource(Source config);
    ~WindowCaptureSource() override;

    bool start() override;
    void stop() override;
    [[nodiscard]] bool isRunning() const override;
    void updateConfig(const Source& source) override;

    [[nodiscard]] std::optional<VideoFrame> latestVideoFrame() override;
    [[nodiscard]] std::optional<AudioFrame> latestAudioFrame() override;

    [[nodiscard]] bool hasVideo() const override { return true; }
    [[nodiscard]] bool hasAudio() const override { return false; }
    [[nodiscard]] const Source& config() const override { return config_; }

private:
    Source config_;
    std::unique_ptr<WindowBitbltCapture> capture_;
    ThreadSafeQueue<VideoFrame> queue_;
    std::atomic<bool> running_{false};
};

} // namespace railshot
