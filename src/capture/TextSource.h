#pragma once

#include "capture/ISourceProvider.h"
#include "core/FrameData.h"

#include <QString>

#include <atomic>
#include <mutex>
#include <string>

namespace railshot {

struct TextSourceSettings {
    QString text = QStringLiteral("RailShot TV");
    QString fontFamily = QStringLiteral("Bahnschrift");
    int fontSize = 72;
    unsigned int color = 0xFFFFFFFF; // ARGB
    int width = 800;
    int height = 200;

    [[nodiscard]] static TextSourceSettings fromSource(const Source& source);
    [[nodiscard]] std::string toJson() const;
    void applyToSource(Source& source) const;
};

class TextSource : public ISourceProvider {
public:
    explicit TextSource(Source config);
    ~TextSource() override;

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
