#pragma once

#include "core/FrameData.h"
#include "core/ThreadSafeQueue.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGIOutputDuplication;
struct ID3D11Texture2D;

namespace railshot {

struct MonitorInfo {
    int index = 0;
    std::string name;
    int width = 0;
    int height = 0;
};

class DxgiMonitorCapture {
public:
    DxgiMonitorCapture();
    ~DxgiMonitorCapture();

    DxgiMonitorCapture(const DxgiMonitorCapture&) = delete;
    DxgiMonitorCapture& operator=(const DxgiMonitorCapture&) = delete;

    [[nodiscard]] static std::vector<MonitorInfo> enumerateMonitors();

    bool open(int monitorIndex);
    void close();
    bool start(ThreadSafeQueue<VideoFrame>* outputQueue);
    void stop();

    [[nodiscard]] bool isRunning() const { return running_.load(); }

private:
    void captureThreadFunc();
    void releaseResources();

    int monitorIndex_ = 0;
    int width_ = 0;
    int height_ = 0;
    ThreadSafeQueue<VideoFrame>* outputQueue_ = nullptr;

    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    IDXGIOutputDuplication* duplication_ = nullptr;
    ID3D11Texture2D* staging_ = nullptr;

    std::unique_ptr<std::thread> captureThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
};

} // namespace railshot
