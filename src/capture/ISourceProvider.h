#pragma once

#include "core/FrameData.h"
#include "core/models/SourceTypes.h"

#include <memory>
#include <optional>
#include <string>

namespace railshot {

class ISourceProvider {
public:
    virtual ~ISourceProvider() = default;

    virtual bool start() = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual bool isRunning() const = 0;

    [[nodiscard]] virtual std::optional<VideoFrame> latestVideoFrame() = 0;
    [[nodiscard]] virtual std::optional<AudioFrame> latestAudioFrame() = 0;

    [[nodiscard]] virtual bool hasVideo() const = 0;
    [[nodiscard]] virtual bool hasAudio() const = 0;

    [[nodiscard]] virtual const Source& config() const = 0;

    // Keep long-lived providers (camera, WebView2) alive across scene syncs.
    virtual void updateConfig(const Source& source) { (void)source; }
};

} // namespace railshot
