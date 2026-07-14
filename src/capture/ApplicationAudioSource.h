#pragma once

#include "capture/ISourceProvider.h"
#include "capture/WasapiAudioCapture.h"
#include "core/ThreadSafeQueue.h"

#include <atomic>
#include <memory>

namespace railshot {

class ApplicationAudioSource : public ISourceProvider {
public:
    explicit ApplicationAudioSource(Source config);
    ~ApplicationAudioSource() override;

    bool start() override;
    void stop() override;
    [[nodiscard]] bool isRunning() const override;
    void updateConfig(const Source& source) override;

    [[nodiscard]] std::optional<VideoFrame> latestVideoFrame() override;
    [[nodiscard]] std::optional<AudioFrame> latestAudioFrame() override;
    [[nodiscard]] bool hasVideo() const override { return false; }
    [[nodiscard]] bool hasAudio() const override { return true; }
    [[nodiscard]] const Source& config() const override { return config_; }

private:
    Source config_;
    std::unique_ptr<WasapiAudioCapture> capture_;
    ThreadSafeQueue<AudioFrame> queue_;
    std::atomic<bool> running_{false};
};

} // namespace railshot
