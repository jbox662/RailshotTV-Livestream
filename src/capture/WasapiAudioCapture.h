#pragma once

#include "core/FrameData.h"
#include "core/ThreadSafeQueue.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

struct IAudioClient;
struct IAudioCaptureClient;

namespace railshot {

struct AudioDeviceInfo {
    std::string id;
    std::string name;
    bool isDefault = false;
};

class WasapiAudioCapture {
public:
    WasapiAudioCapture();
    ~WasapiAudioCapture();

    WasapiAudioCapture(const WasapiAudioCapture&) = delete;
    WasapiAudioCapture& operator=(const WasapiAudioCapture&) = delete;

    [[nodiscard]] static std::vector<AudioDeviceInfo> enumerateInputDevices();
    [[nodiscard]] static std::vector<AudioDeviceInfo> enumerateOutputDevices();

    // Empty deviceId → default endpoint.
    bool openMicrophone(const std::string& deviceId = {});
    bool openDesktopLoopback(const std::string& deviceId = {});
    bool openDefaultMicrophone() { return openMicrophone({}); }
    bool openDefaultDesktopLoopback() { return openDesktopLoopback({}); }

    void close();
    bool start(ThreadSafeQueue<AudioFrame>* outputQueue);
    void stop();

    [[nodiscard]] bool isRunning() const { return running_.load(); }
    // Output is always interleaved S16 stereo @ 48 kHz after conversion.
    [[nodiscard]] int sampleRate() const { return 48000; }
    [[nodiscard]] int channels() const { return 2; }

private:
    void captureThreadFunc();
    void releaseResources();
    bool openEndpoint(bool capture, bool loopback, const std::string& deviceId, const char* label);
    AudioFrame convertPacket(const uint8_t* data, uint32_t numFrames) const;

    int srcSampleRate_ = 48000;
    int srcChannels_ = 2;
    int srcBits_ = 16;
    bool srcFloat_ = false;

    ThreadSafeQueue<AudioFrame>* outputQueue_ = nullptr;

    IAudioClient* audioClient_ = nullptr;
    IAudioCaptureClient* captureClient_ = nullptr;

    std::unique_ptr<std::thread> captureThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
};

} // namespace railshot
