#pragma once

#include "capture/ISourceProvider.h"
#include "core/models/SourceTypes.h"

#include <atomic>
#include <cstdint>
#include <mutex>

namespace railshot {

class ScoreboardSource : public ISourceProvider {
public:
    explicit ScoreboardSource(Source config);
    ~ScoreboardSource() override;

    static void refreshAll();
    static void applyLiveConfig(const Source& source);

    bool start() override;
    void stop() override;
    bool isRunning() const override;

    std::optional<VideoFrame> latestVideoFrame() override;
    std::optional<AudioFrame> latestAudioFrame() override;
    bool hasVideo() const override;
    bool hasAudio() const override;
    const Source& config() const override { return config_; }
    void updateConfig(const Source& source) override;

private:
    void renderIfNeeded();
    void invalidateFrame();
    void registerInstance();
    void unregisterInstance();

    Source config_;
    std::atomic<bool> running_{false};
    std::mutex mutex_;
    VideoFrame frame_;
    uint64_t lastVersion_ = 0;
    std::string lastStyleKey_;
};

} // namespace railshot
