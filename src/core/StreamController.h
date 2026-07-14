#pragma once

#include "capture/DirectShowCapture.h"
#include "capture/WasapiAudioMonitor.h"
#include "core/AudioMixer.h"
#include "core/FrameData.h"
#include "core/GLCompositor.h"
#include "core/SourceRegistry.h"
#include "core/ThreadSafeQueue.h"
#include "core/models/SceneManager.h"
#include "encoder/FFmpegEncoder.h"
#include "output/FileRecorder.h"
#include "output/RtmpOutput.h"
#include "output/VirtualCamOutput.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

namespace railshot {

struct StreamStats {
    double captureFps = 0.0;
    double compositorFps = 0.0;
    double encodedFps = 0.0;
    uint64_t totalFrames = 0;
    uint64_t droppedFrames = 0;
    double dropRate = 0.0;
    bool isStreaming = false;
    bool isRecording = false;
    bool isConnected = false;
    std::string encoderName;
    std::string recordingPath;
    uint64_t bytesSent = 0;
    uint64_t bytesRecorded = 0;
    int reconnectCount = 0;
    std::chrono::steady_clock::time_point streamStartTime;
};

class StreamController {
public:
    StreamController();
    ~StreamController();

    StreamController(const StreamController&) = delete;
    StreamController& operator=(const StreamController&) = delete;

    static std::vector<CaptureDevice> enumerateVideoDevices();

    bool setRtmpUrl(const std::string& url);
    bool startStream();
    void stopStream();

    bool startRecording();
    void stopRecording();

    bool startVirtualCamera();
    void stopVirtualCamera();
    [[nodiscard]] bool isVirtualCameraActive() const;

    void setStudioModeEnabled(bool enabled);
    [[nodiscard]] bool isStudioModeEnabled() const { return studioModeEnabled_.load(); }

    void onSceneCollectionChanged();
    void setActiveScene(const std::string& sceneId);
    void setPreviewScene(const std::string& sceneId);
    void transitionPreviewToProgram();

    void ensurePreviewEngine();
    void stopPreviewEngine();
    void applyVideoSettings(); // restart preview compositor when not streaming
    void applyAudioSettings(); // re-open mic/monitor after Settings change

    [[nodiscard]] bool isStreaming() const { return streaming_.load(); }
    [[nodiscard]] bool isRecording() const { return recording_.load(); }
    [[nodiscard]] StreamStats stats() const;
    [[nodiscard]] SourceRegistry& sourceRegistry() { return sourceRegistry_; }

    ThreadSafeQueue<VideoFrame>& programPreviewQueue() { return programPreviewQueue_; }
    ThreadSafeQueue<VideoFrame>& studioPreviewQueue() { return studioPreviewQueue_; }

private:
    void restartPreviewEngine();
    void syncProvidersForCompositor();
    void ensureMeterThread();
    void stopMeterThread();
    void audioThreadFunc();
    void packetDispatchThreadFunc();
    void syncAudioMonitor();

    std::unique_ptr<GLCompositor> compositor_;
    std::unique_ptr<FFmpegEncoder> encoder_;
    std::unique_ptr<RtmpOutput> rtmpOutput_;
    std::unique_ptr<FileRecorder> fileRecorder_;
    std::unique_ptr<VirtualCamOutput> virtualCam_;
    std::unique_ptr<WasapiAudioMonitor> audioMonitor_;
    SourceRegistry sourceRegistry_;

    ThreadSafeQueue<VideoFrame> compositedVideoQueue_;
    ThreadSafeQueue<AudioFrame> rawAudioQueue_;
    ThreadSafeQueue<EncodedPacket> encoderOutputQueue_;
    ThreadSafeQueue<EncodedPacket> rtmpQueue_;
    ThreadSafeQueue<EncodedPacket> recordQueue_;
    ThreadSafeQueue<VideoFrame> programPreviewQueue_;
    ThreadSafeQueue<VideoFrame> studioPreviewQueue_;

    std::unique_ptr<std::thread> audioThread_;
    std::unique_ptr<std::thread> dispatchThread_;
    std::atomic<bool> audioStopRequested_{false};
    std::atomic<bool> dispatchStopRequested_{false};

    std::string rtmpUrl_;
    std::string recordingPath_;
    std::atomic<bool> streaming_{false};
    std::atomic<bool> recording_{false};
    std::atomic<bool> studioModeEnabled_{false};
    std::atomic<bool> previewEngineRunning_{false};
    std::chrono::steady_clock::time_point streamStartTime_;
};

} // namespace railshot
