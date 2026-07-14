#pragma once

#include "core/FrameData.h"
#include "core/ThreadSafeQueue.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

struct IMediaControl;
struct IGraphBuilder;
struct ICaptureGraphBuilder2;
struct IBaseFilter;
struct ISampleGrabber;
struct ISampleGrabberCB;

namespace railshot {

struct CaptureDevice {
    std::string id;
    std::string name;
};

class DirectShowCapture {
public:
    DirectShowCapture();
    ~DirectShowCapture();

    DirectShowCapture(const DirectShowCapture&) = delete;
    DirectShowCapture& operator=(const DirectShowCapture&) = delete;

    static std::vector<CaptureDevice> enumerateDevices();

    bool open(const std::string& deviceId);
    void close();
    bool start(ThreadSafeQueue<VideoFrame>* outputQueue);
    void stop();

    [[nodiscard]] bool isRunning() const { return running_.load(); }
    [[nodiscard]] int captureWidth() const { return captureWidth_; }
    [[nodiscard]] int captureHeight() const { return captureHeight_; }

private:
    void captureThreadFunc();
    bool buildGraph();
    void releaseGraph();

    std::string deviceId_;
    int captureWidth_ = 0;
    int captureHeight_ = 0;
    PixelFormat captureFormat_ = PixelFormat::NV12;

    ThreadSafeQueue<VideoFrame>* outputQueue_ = nullptr;

    IGraphBuilder* graph_ = nullptr;
    ICaptureGraphBuilder2* captureBuilder_ = nullptr;
    IBaseFilter* captureFilter_ = nullptr;
    IBaseFilter* grabberFilter_ = nullptr;
    ISampleGrabber* sampleGrabber_ = nullptr;
    ISampleGrabberCB* grabberCallback_ = nullptr;
    IMediaControl* mediaControl_ = nullptr;

    std::unique_ptr<std::thread> captureThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
};

} // namespace railshot
