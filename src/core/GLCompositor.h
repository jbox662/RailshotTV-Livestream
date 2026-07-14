#pragma once

#include "core/FrameData.h"
#include "core/SourceRegistry.h"
#include "core/ThreadSafeQueue.h"
#include "core/models/SceneManager.h"

#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

class QOffscreenSurface;
class QOpenGLContext;
class QOpenGLFramebufferObject;
class QOpenGLShaderProgram;
class QImage;

namespace railshot {

class VirtualCamOutput;

class GLCompositor {
public:
    GLCompositor();
    ~GLCompositor();

    GLCompositor(const GLCompositor&) = delete;
    GLCompositor& operator=(const GLCompositor&) = delete;

    bool start(SourceRegistry* registry,
               ThreadSafeQueue<VideoFrame>* outputQueue,
               ThreadSafeQueue<VideoFrame>* programPreviewQueue = nullptr,
               ThreadSafeQueue<VideoFrame>* studioPreviewQueue = nullptr);
    void stop();
    void setStudioModeEnabled(bool enabled);
    void setPreviewQueues(ThreadSafeQueue<VideoFrame>* programPreviewQueue,
                          ThreadSafeQueue<VideoFrame>* studioPreviewQueue);
    void setOutputQueue(ThreadSafeQueue<VideoFrame>* outputQueue);
    void setVirtualCamOutput(VirtualCamOutput* output);

    [[nodiscard]] bool isRunning() const { return running_.load(); }
    [[nodiscard]] bool studioModeEnabled() const { return studioModeEnabled_.load(); }
    [[nodiscard]] int outputWidth() const { return outputWidth_.load(); }
    [[nodiscard]] int outputHeight() const { return outputHeight_.load(); }
    [[nodiscard]] int fps() const { return fps_.load(); }

private:
    void compositorThreadFunc();
    bool initGl();
    void cleanupGl();
    void renderScene(const Scene& scene, float alpha, float xOffset = 0.0f);
    void renderAndReadback(const Scene& scene, float alpha, VideoFrame& output);
    void drawLayer(const VideoFrame& frame, const SourceTransform& transform, float alpha,
                   float xOffset = 0.0f);
    void compositeRgbaOverlays(const Scene& scene, float alpha, QImage& canvas,
                               float xOffset = 0.0f);
    void readbackNv12(const Scene* sceneA, float alphaA, const Scene* sceneB, float alphaB,
                      VideoFrame& output, float offsetA = 0.0f, float offsetB = 0.0f);
    [[nodiscard]] std::optional<VideoFrame> delayedFrame(const std::string& sourceId,
                                                         VideoFrame frame, int delayMs);

    SourceRegistry* registry_ = nullptr;
    ThreadSafeQueue<VideoFrame>* outputQueue_ = nullptr;
    ThreadSafeQueue<VideoFrame>* programPreviewQueue_ = nullptr;
    ThreadSafeQueue<VideoFrame>* studioPreviewQueue_ = nullptr;
    VirtualCamOutput* virtualCamOutput_ = nullptr;

    std::atomic<bool> studioModeEnabled_{false};
    std::atomic<int> outputWidth_{1920};
    std::atomic<int> outputHeight_{1080};
    std::atomic<int> fps_{30};

    std::unique_ptr<QOffscreenSurface> surface_;
    std::unique_ptr<QOpenGLContext> context_;
    std::unique_ptr<QOpenGLFramebufferObject> fbo_;
    std::unique_ptr<QOpenGLShaderProgram> nv12Shader_;
    std::unique_ptr<QOpenGLShaderProgram> rgbaShader_;

    std::unique_ptr<std::thread> compositorThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};

    std::unordered_map<std::string, std::deque<std::pair<int64_t, VideoFrame>>> delayQueues_;
    double scrollClockSec_ = 0.0;
};

} // namespace railshot
