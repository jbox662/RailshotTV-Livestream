#pragma once

#include "capture/ISourceProvider.h"
#include "core/models/SourceTypes.h"

#include <atomic>
#include <memory>

namespace railshot {

class NdiSource : public ISourceProvider {
public:
    explicit NdiSource(Source config);
    ~NdiSource() override;

    bool start() override;
    void stop() override;
    bool isRunning() const override;

    std::optional<VideoFrame> latestVideoFrame() override;
    std::optional<AudioFrame> latestAudioFrame() override;
    bool hasVideo() const override;
    bool hasAudio() const override;
    const Source& config() const override { return config_; }

private:
    Source config_;
    std::atomic<bool> running_{false};
};

} // namespace railshot
