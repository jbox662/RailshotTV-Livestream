#pragma once

#include "core/FrameData.h"
#include "core/ThreadSafeQueue.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>

struct IAudioClient;
struct IAudioCaptureClient;

namespace railshot {

class WasapiAudioCapture {
public:
    WasapiAudioCapture();
    ~WasapiAudioCapture();

    WasapiAudioCapture(const WasapiAudioCapture&) = delete;
    WasapiAudioCapture& operator=(const WasapiAudioCapture&) = delete;

    bool openDefaultMicrophone();
    bool openDefaultDesktopLoopback();
    void close();
    bool start(ThreadSafeQueue<AudioFrame>* outputQueue);
    void stop();

    [[nodiscard]] bool isRunning() const { return running_.load(); }
    [[nodiscard]] int sampleRate() const { return sampleRate_; }
    [[nodiscard]] int channels() const { return channels_; }

private:
    void captureThreadFunc();
    void releaseResources();

    int sampleRate_ = 48000;
    int channels_ = 2;
    int bytesPerSample_ = 2;

    ThreadSafeQueue<AudioFrame>* outputQueue_ = nullptr;

    IAudioClient* audioClient_ = nullptr;
    IAudioCaptureClient* captureClient_ = nullptr;

    std::unique_ptr<std::thread> captureThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
};

} // namespace railshot
