#pragma once

#include "core/FrameData.h"
#include "core/ThreadSafeQueue.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace railshot {

struct WindowInfo {
    uintptr_t hwnd = 0;
    std::string title;
    std::string className;
};

class WindowBitbltCapture {
public:
    WindowBitbltCapture();
    ~WindowBitbltCapture();

    WindowBitbltCapture(const WindowBitbltCapture&) = delete;
    WindowBitbltCapture& operator=(const WindowBitbltCapture&) = delete;

    [[nodiscard]] static std::vector<WindowInfo> enumerateWindows();

    bool open(uintptr_t hwnd);
    void close();
    bool start(ThreadSafeQueue<VideoFrame>* outputQueue);
    void stop();

    [[nodiscard]] bool isRunning() const { return running_.load(); }

private:
    void captureThreadFunc();

    HWND hwnd_ = nullptr;
    ThreadSafeQueue<VideoFrame>* outputQueue_ = nullptr;
    std::unique_ptr<std::thread> captureThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
};

} // namespace railshot
