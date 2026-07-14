#pragma once

#include "core/FrameData.h"
#include "core/ThreadSafeQueue.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

namespace railshot {

// Windows.Graphics.Capture based window capturer (GPU/flip-model aware).
class WgcWindowCapture {
public:
    WgcWindowCapture();
    ~WgcWindowCapture();

    WgcWindowCapture(const WgcWindowCapture&) = delete;
    WgcWindowCapture& operator=(const WgcWindowCapture&) = delete;

    [[nodiscard]] static bool isSupported();

    bool open(uintptr_t hwnd);
    void close();
    bool start(ThreadSafeQueue<VideoFrame>* outputQueue);
    void stop();

    [[nodiscard]] bool isRunning() const { return running_.load(); }

private:
    void captureThreadFunc();

    uintptr_t hwnd_ = 0;
    ThreadSafeQueue<VideoFrame>* outputQueue_ = nullptr;
    std::unique_ptr<std::thread> captureThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
};

} // namespace railshot
