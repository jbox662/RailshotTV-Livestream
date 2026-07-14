#pragma once

#include "core/FrameData.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

struct IAudioClient;
struct IAudioRenderClient;

namespace railshot {

// Plays program-mix PCM (S16 stereo @ 48 kHz) to a WASAPI render device.
class WasapiAudioMonitor {
public:
    WasapiAudioMonitor();
    ~WasapiAudioMonitor();

    WasapiAudioMonitor(const WasapiAudioMonitor&) = delete;
    WasapiAudioMonitor& operator=(const WasapiAudioMonitor&) = delete;

    // Empty deviceId → default render endpoint.
    bool open(const std::string& deviceId = {});
    void close();

    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const { return enabled_.load(); }
    [[nodiscard]] bool isOpen() const { return audioClient_ != nullptr; }

    void write(const AudioFrame& frame);

private:
    void releaseResources();

    IAudioClient* audioClient_ = nullptr;
    IAudioRenderClient* renderClient_ = nullptr;
    uint32_t bufferFrames_ = 0;
    int deviceRate_ = 48000;
    int deviceChannels_ = 2;
    bool deviceFloat_ = false;

    std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::vector<int16_t> pending_; // S16 stereo @ 48k awaiting device write
};

} // namespace railshot
