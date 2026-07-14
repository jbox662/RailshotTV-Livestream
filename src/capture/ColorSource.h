#pragma once

#include "capture/ISourceProvider.h"
#include "core/FrameData.h"

#include <atomic>
#include <mutex>
#include <string>

namespace railshot {

struct ColorSourceSettings {
    int width = 1920;
    int height = 1080;
    unsigned int color = 0xFF1E4D34; // ARGB felt green

    [[nodiscard]] static ColorSourceSettings fromSource(const Source& source);
    [[nodiscard]] std::string toJson() const;
    void applyToSource(Source& source) const;
};

class ColorSource : public ISourceProvider {
public:
    explicit ColorSource(Source config);
    ~ColorSource() override;

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
    void regenerate();

    Source config_;
    VideoFrame frame_;
    std::mutex mutex_;
    std::atomic<bool> running_{false};
};

} // namespace railshot
