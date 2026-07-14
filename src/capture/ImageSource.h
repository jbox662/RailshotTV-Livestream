#pragma once

#include "capture/ISourceProvider.h"
#include "core/FrameData.h"

#include <atomic>
#include <mutex>
#include <string>

namespace railshot {

class ImageSource : public ISourceProvider {
public:
    explicit ImageSource(Source config);
    ~ImageSource() override;

    bool start() override;
    void stop() override;
    [[nodiscard]] bool isRunning() const override;

    [[nodiscard]] std::optional<VideoFrame> latestVideoFrame() override;
    [[nodiscard]] std::optional<AudioFrame> latestAudioFrame() override;

    [[nodiscard]] bool hasVideo() const override { return true; }
    [[nodiscard]] bool hasAudio() const override { return false; }
    [[nodiscard]] const Source& config() const override { return config_; }

private:
    bool loadImage();

    Source config_;
    VideoFrame frame_;
    std::mutex mutex_;
    std::atomic<bool> running_{false};
};

} // namespace railshot
