#include "capture/WindowBitbltCapture.h"

#include "capture/CaptureConvert.h"
#include "core/Logger.h"

#include <chrono>
#include <vector>

namespace railshot {

namespace {

struct EnumCtx {
    std::vector<WindowInfo>* out = nullptr;
};

BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam) {
    auto* ctx = reinterpret_cast<EnumCtx*>(lParam);
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }
    if (GetWindow(hwnd, GW_OWNER) != nullptr) {
        return TRUE;
    }
    wchar_t titleW[512]{};
    GetWindowTextW(hwnd, titleW, 512);
    if (titleW[0] == L'\0') {
        return TRUE;
    }
    wchar_t classW[256]{};
    GetClassNameW(hwnd, classW, 256);

    WindowInfo info;
    info.hwnd = reinterpret_cast<uintptr_t>(hwnd);
    info.title = std::string(titleW, titleW + wcslen(titleW));
    info.className = std::string(classW, classW + wcslen(classW));
    ctx->out->push_back(info);
    return TRUE;
}

} // namespace

WindowBitbltCapture::WindowBitbltCapture() = default;

WindowBitbltCapture::~WindowBitbltCapture() {
    stop();
    close();
}

std::vector<WindowInfo> WindowBitbltCapture::enumerateWindows() {
    std::vector<WindowInfo> windows;
    EnumCtx ctx{&windows};
    EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&ctx));
    return windows;
}

bool WindowBitbltCapture::open(uintptr_t hwnd) {
    close();
    hwnd_ = reinterpret_cast<HWND>(hwnd);
    if (!IsWindow(hwnd_)) {
        Logger::error("WindowBitblt: invalid HWND");
        hwnd_ = nullptr;
        return false;
    }
    return true;
}

void WindowBitbltCapture::close() {
    stop();
    hwnd_ = nullptr;
}

bool WindowBitbltCapture::start(ThreadSafeQueue<VideoFrame>* outputQueue) {
    if (running_.load() || !outputQueue || !hwnd_) {
        return false;
    }
    outputQueue_ = outputQueue;
    stopRequested_ = false;
    running_ = true;
    captureThread_ = std::make_unique<std::thread>(&WindowBitbltCapture::captureThreadFunc, this);
    return true;
}

void WindowBitbltCapture::stop() {
    stopRequested_ = true;
    if (captureThread_ && captureThread_->joinable()) {
        captureThread_->join();
    }
    captureThread_.reset();
    running_ = false;
    outputQueue_ = nullptr;
}

void WindowBitbltCapture::captureThreadFunc() {
    while (!stopRequested_.load()) {
        if (!IsWindow(hwnd_)) {
            break;
        }

        RECT rect{};
        if (!GetClientRect(hwnd_, &rect)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        const int width = rect.right - rect.left;
        const int height = rect.bottom - rect.top;
        if (width < 2 || height < 2) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        HDC windowDc = GetDC(hwnd_);
        if (!windowDc) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        HDC memDc = CreateCompatibleDC(windowDc);
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height; // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        HBITMAP dib = CreateDIBSection(memDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!dib || !bits) {
            if (dib) {
                DeleteObject(dib);
            }
            DeleteDC(memDc);
            ReleaseDC(hwnd_, windowDc);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        HGDIOBJ old = SelectObject(memDc, dib);
        BitBlt(memDc, 0, 0, width, height, windowDc, 0, 0, SRCCOPY);

        VideoFrame frame;
        bgraToNv12(static_cast<const uint8_t*>(bits), width, height, width * 4, frame);
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        frame.timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
        if (outputQueue_) {
            outputQueue_->push(std::move(frame));
        }

        SelectObject(memDc, old);
        DeleteObject(dib);
        DeleteDC(memDc);
        ReleaseDC(hwnd_, windowDc);

        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
    running_ = false;
}

} // namespace railshot
