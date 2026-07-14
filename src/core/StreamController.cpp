#include "core/StreamController.h"

#include "capture/DirectShowCapture.h"
#include "capture/ScoreboardSource.h"
#include "capture/BrowserSource.h"
#include "core/AppSettings.h"
#include "core/Logger.h"
#include "output/Remux.h"

#include <QStandardPaths>

#include <filesystem>

namespace railshot {

StreamController::StreamController()
    : compositor_(std::make_unique<GLCompositor>())
    , encoder_(std::make_unique<FFmpegEncoder>())
    , rtmpOutput_(std::make_unique<RtmpOutput>())
    , fileRecorder_(std::make_unique<FileRecorder>())
    , replayBuffer_(std::make_unique<ReplayBuffer>())
    , virtualCam_(std::make_unique<VirtualCamOutput>())
    , audioMonitor_(std::make_unique<WasapiAudioMonitor>()) {
    SceneManager::instance().setOnScenesChanged([this]() { onSceneCollectionChanged(); });
}

StreamController::~StreamController() {
    stopVirtualCamera();
    stopReplayBuffer();
    stopRecording();
    stopStream();
    stopEncodePipeline();
    stopPreviewEngine();
}

std::string StreamController::recordingsDirectory() const {
    const AppSettingsData settings = AppSettings::instance().data();
    std::string dir = settings.recordingDirectory;
    if (dir.empty()) {
        dir = (QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)
               + "/RailShot/Recordings")
                  .toStdString();
    }
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

std::string StreamController::makeRecordingPath(const std::string& prefix) const {
    const std::string ext = AppSettings::instance().data().recordingFormat;
    return recordingsDirectory() + "/" + FileRecorder::generateFilename(prefix, ext);
}

std::vector<CaptureDevice> StreamController::enumerateVideoDevices() {
    return DirectShowCapture::enumerateDevices();
}

bool StreamController::setRtmpUrl(const std::string& url) {
    if (streaming_.load()) {
        return false;
    }
    rtmpUrl_ = url;
    return rtmpOutput_->open(url);
}

void StreamController::syncProvidersForCompositor() {
    auto& sceneManager = SceneManager::instance();

    if (studioModeEnabled_.load()) {
        auto preview = sceneManager.previewSceneSnapshot();
        auto program = sceneManager.activeSceneSnapshot();
        if (preview.has_value() && program.has_value()) {
            if (sceneManager.isTransitioning()) {
                auto outgoing = sceneManager.sceneSnapshot(sceneManager.outgoingSceneId());
                if (outgoing.has_value()) {
                    Scene combined;
                    combined.id = "transition";
                    combined.sources = outgoing->sources;
                    for (const auto& src : program->sources) {
                        combined.sources.push_back(src);
                    }
                    for (const auto& src : preview->sources) {
                        combined.sources.push_back(src);
                    }
                    sourceRegistry_.syncScene(combined);
                    return;
                }
            }
            sourceRegistry_.syncCombinedScenes(*preview, *program);
            return;
        }
    }

    auto active = sceneManager.activeSceneSnapshot();
    if (!active.has_value()) {
        return;
    }

    if (sceneManager.isTransitioning()) {
        auto outgoing = sceneManager.sceneSnapshot(sceneManager.outgoingSceneId());
        if (outgoing.has_value()) {
            Scene combined;
            combined.id = "transition";
            combined.sources = outgoing->sources;
            for (const auto& src : active->sources) {
                combined.sources.push_back(src);
            }
            sourceRegistry_.syncScene(combined);
            return;
        }
    }

    sourceRegistry_.syncScene(*active);
}

void StreamController::setStudioModeEnabled(bool enabled) {
    studioModeEnabled_ = enabled;
    SceneManager::instance().setStudioModeEnabled(enabled);
    if (!streaming_.load()) {
        ensurePreviewEngine();
        return;
    }

    if (compositor_) {
        compositor_->setStudioModeEnabled(enabled);
        compositor_->setPreviewQueues(&programPreviewQueue_,
                                      enabled ? &studioPreviewQueue_ : nullptr);
    }
    syncProvidersForCompositor();
    ScoreboardSource::refreshAll();
    BrowserSource::refreshAll();
}

void StreamController::applyVideoSettings() {
    if (encoding_.load() || streaming_.load()) {
        Logger::warn("StreamController: cannot apply video settings while encoding");
        return;
    }
    if (isVirtualCameraActive()) {
        Logger::warn("StreamController: cannot apply video settings while virtual camera is active");
        return;
    }
    restartPreviewEngine();
}

void StreamController::applyAudioSettings() {
    sourceRegistry_.restartMicCapture();
    syncAudioMonitor();
}

void StreamController::syncAudioMonitor() {
    if (!audioMonitor_) {
        return;
    }
    const AppSettingsData settings = AppSettings::instance().data();
    audioMonitor_->close();
    audioMonitor_->setEnabled(settings.audioMonitoringEnabled);
    if (!settings.audioMonitoringEnabled) {
        return;
    }
    if (!audioMonitor_->open(settings.monitoringDeviceId)) {
        Logger::warn("StreamController: audio monitoring device unavailable");
        audioMonitor_->setEnabled(false);
    }
}

bool StreamController::startVirtualCamera() {
    if (isVirtualCameraActive()) {
        return true;
    }

    ensurePreviewEngine();
    if (!compositor_ || !compositor_->isRunning()) {
        Logger::error("StreamController: preview engine required for virtual camera");
        return false;
    }

    const int w = AppSettings::instance().canvasWidth();
    const int h = AppSettings::instance().canvasHeight();
    const int fps = AppSettings::instance().fps();
    if (!virtualCam_->start(w, h, fps)) {
        return false;
    }

    compositor_->setVirtualCamOutput(virtualCam_.get());
    Logger::info("StreamController: virtual camera started");
    return true;
}

void StreamController::stopVirtualCamera() {
    if (compositor_) {
        compositor_->setVirtualCamOutput(nullptr);
    }
    if (virtualCam_) {
        virtualCam_->stop();
    }
}

bool StreamController::isVirtualCameraActive() const {
    return virtualCam_ && virtualCam_->isActive();
}

void StreamController::restartPreviewEngine() {
    if (encoding_.load() || streaming_.load()) {
        syncProvidersForCompositor();
        return;
    }

    stopMeterThread();
    if (compositor_->isRunning()) {
        compositor_->stop();
        sourceRegistry_.stopAll();
        previewEngineRunning_ = false;
    }

    programPreviewQueue_.reset();
    studioPreviewQueue_.reset();
    ensurePreviewEngine();
}

void StreamController::ensurePreviewEngine() {
    if (previewEngineRunning_.load() || compositor_->isRunning()) {
        compositor_->setStudioModeEnabled(studioModeEnabled_.load());
        compositor_->setPreviewQueues(&programPreviewQueue_,
                                      studioModeEnabled_.load() ? &studioPreviewQueue_ : nullptr);
        syncProvidersForCompositor();
        ScoreboardSource::refreshAll();
        BrowserSource::refreshAll();
        ensureMeterThread();
        return;
    }

    programPreviewQueue_.reset();
    studioPreviewQueue_.reset();

    syncProvidersForCompositor();
    sourceRegistry_.startMicCapture();

    compositor_->setStudioModeEnabled(studioModeEnabled_.load());
    if (!compositor_->start(&sourceRegistry_,
                            streaming_.load() ? &compositedVideoQueue_ : nullptr,
                            &programPreviewQueue_,
                            studioModeEnabled_.load() ? &studioPreviewQueue_ : nullptr)) {
        Logger::error("StreamController: failed to start preview compositor");
        return;
    }

    previewEngineRunning_ = true;
    Logger::info("StreamController: preview engine started");
    ScoreboardSource::refreshAll();
    BrowserSource::refreshAll();
    ensureMeterThread();
}

void StreamController::stopPreviewEngine() {
    if (isVirtualCameraActive()) {
        stopVirtualCamera();
    }
    if (encoding_.load() || streaming_.load()) {
        return;
    }
    if (!previewEngineRunning_.load() && !compositor_->isRunning()) {
        return;
    }

    stopMeterThread();
    compositor_->stop();
    sourceRegistry_.stopAll();
    programPreviewQueue_.shutdown();
    studioPreviewQueue_.shutdown();
    previewEngineRunning_ = false;
    Logger::info("StreamController: preview engine stopped");
}

void StreamController::onSceneCollectionChanged() {
    if (streaming_.load() || previewEngineRunning_.load()) {
        syncProvidersForCompositor();
    }
}

void StreamController::setActiveScene(const std::string& sceneId) {
    SceneManager::instance().setActiveScene(sceneId);
    if (streaming_.load() || previewEngineRunning_.load()) {
        syncProvidersForCompositor();
    }
}

void StreamController::setPreviewScene(const std::string& sceneId) {
    SceneManager::instance().setPreviewScene(sceneId);
    if (streaming_.load() || previewEngineRunning_.load()) {
        syncProvidersForCompositor();
    }
}

void StreamController::transitionPreviewToProgram() {
    SceneManager::instance().swapPreviewAndProgram();
    if (streaming_.load() || previewEngineRunning_.load()) {
        syncProvidersForCompositor();
    }
}

bool StreamController::ensureEncodePipeline() {
    if (encoding_.load()) {
        return true;
    }

    auto active = SceneManager::instance().activeSceneSnapshot();
    if (!active.has_value() || active->sources.empty()) {
        Logger::error("StreamController: active scene has no sources");
        return false;
    }

    compositedVideoQueue_.reset();
    rawAudioQueue_.reset();
    encoderOutputQueue_.reset();
    rtmpQueue_.reset();
    recordQueue_.reset();

    if (!encoder_->initialize()) {
        Logger::error("StreamController: encoder init failed");
        encoder_->shutdown();
        return false;
    }

    syncProvidersForCompositor();
    sourceRegistry_.startMicCapture();

    ensurePreviewEngine();
    compositor_->setStudioModeEnabled(studioModeEnabled_.load());
    compositor_->setOutputQueue(&compositedVideoQueue_);
    compositor_->setPreviewQueues(&programPreviewQueue_,
                                  studioModeEnabled_.load() ? &studioPreviewQueue_ : nullptr);

    audioStopRequested_ = false;
    ensureMeterThread();

    if (!encoder_->start(&compositedVideoQueue_, &rawAudioQueue_, &encoderOutputQueue_)) {
        Logger::error("StreamController: failed to start encoder");
        compositor_->setOutputQueue(nullptr);
        encoder_->shutdown();
        return false;
    }

    dispatchStopRequested_ = false;
    dispatchThread_ = std::make_unique<std::thread>(&StreamController::packetDispatchThreadFunc, this);

    encoding_ = true;
    streamStartTime_ = std::chrono::steady_clock::now();
    Logger::info("StreamController: encode pipeline started (" + encoder_->videoCodecName() + ")");
    return true;
}

void StreamController::stopEncodePipeline() {
    if (!encoding_.load()) {
        return;
    }

    encoding_ = false;
    dispatchStopRequested_ = true;
    encoderOutputQueue_.shutdown();
    if (dispatchThread_ && dispatchThread_->joinable()) {
        dispatchThread_->join();
    }
    dispatchThread_.reset();

    encoder_->stop();
    encoder_->shutdown();
    compositor_->setOutputQueue(nullptr);

    compositedVideoQueue_.shutdown();
    rawAudioQueue_.shutdown();
    encoderOutputQueue_.shutdown();
    rtmpQueue_.shutdown();
    recordQueue_.shutdown();

    Logger::info("StreamController: encode pipeline stopped");
}

void StreamController::maybeStopEncodePipeline() {
    if (streaming_.load() || recording_.load() || replayActive_.load()) {
        return;
    }
    stopEncodePipeline();
}

bool StreamController::startRecording() {
    if (recording_.load()) {
        return false;
    }
    if (!ensureEncodePipeline()) {
        return false;
    }

    const AppSettingsData settings = AppSettings::instance().data();
    recordingPath_ = makeRecordingPath("RailShot");
    if (!fileRecorder_->open(recordingPath_,
                             encoder_->videoCodecContext(),
                             encoder_->audioCodecContext(),
                             settings.recordingFormat)) {
        recordingPath_.clear();
        maybeStopEncodePipeline();
        return false;
    }

    recordQueue_.reset();
    if (!fileRecorder_->start(&recordQueue_)) {
        fileRecorder_->close();
        recordingPath_.clear();
        maybeStopEncodePipeline();
        return false;
    }

    recording_ = true;
    Logger::info("StreamController: program recording started -> " + recordingPath_);
    return true;
}

void StreamController::stopRecording() {
    if (!recording_.load()) {
        return;
    }

    recording_ = false;
    recordQueue_.shutdown();
    fileRecorder_->stop();
    fileRecorder_->close();
    Logger::info("StreamController: program recording stopped");
    maybeStopEncodePipeline();
}

bool StreamController::startReplayBuffer() {
    if (replayActive_.load()) {
        return true;
    }
    if (!ensureEncodePipeline()) {
        return false;
    }

    const AppSettingsData settings = AppSettings::instance().data();
    replayBuffer_->setSeconds(settings.replayBufferSeconds);
    replayBuffer_->clear();
    replayActive_ = true;
    Logger::info("StreamController: replay buffer active ("
                 + std::to_string(settings.replayBufferSeconds) + "s)");
    return true;
}

void StreamController::stopReplayBuffer() {
    if (!replayActive_.load()) {
        return;
    }
    replayActive_ = false;
    replayBuffer_->clear();
    Logger::info("StreamController: replay buffer stopped");
    maybeStopEncodePipeline();
}

bool StreamController::saveReplayBuffer() {
    if (!replayActive_.load() || !encoding_.load()) {
        return false;
    }
    if (replayBuffer_->empty()) {
        Logger::warn("StreamController: replay buffer empty");
        return false;
    }

    lastReplayPath_ = makeRecordingPath("Replay");
    if (!replayBuffer_->save(lastReplayPath_,
                             encoder_->videoCodecContext(),
                             encoder_->audioCodecContext())) {
        lastReplayPath_.clear();
        return false;
    }
    return true;
}

bool StreamController::remuxRecording(const std::string& inputPath, const std::string& outputPath) {
    return remuxFile(inputPath, outputPath);
}

void StreamController::packetDispatchThreadFunc() {
    while (!dispatchStopRequested_.load()) {
        auto packet = encoderOutputQueue_.pop(100);
        if (!packet.has_value()) {
            continue;
        }

        if (streaming_.load()) {
            rtmpQueue_.push(*packet);
        }
        if (recording_.load()) {
            recordQueue_.push(*packet);
        }
        if (replayActive_.load() && replayBuffer_) {
            replayBuffer_->push(*packet);
        }
    }
}

bool StreamController::startStream() {
    if (streaming_.load()) {
        return false;
    }

    if (rtmpUrl_.empty()) {
        Logger::error("StreamController: no RTMP URL configured");
        return false;
    }

    if (!ensureEncodePipeline()) {
        return false;
    }

    if (!rtmpOutput_->open(rtmpUrl_)) {
        Logger::error("StreamController: RTMP output init failed");
        maybeStopEncodePipeline();
        return false;
    }

    rtmpQueue_.reset();
    if (!rtmpOutput_->start(&rtmpQueue_,
                            encoder_->videoCodecContext(),
                            encoder_->audioCodecContext())) {
        Logger::error("StreamController: failed to start RTMP output");
        maybeStopEncodePipeline();
        return false;
    }

    streaming_ = true;
    streamStartTime_ = std::chrono::steady_clock::now();
    Logger::info("StreamController: streaming started");

    if (AppSettings::instance().data().replayBufferEnabled && !replayActive_.load()) {
        startReplayBuffer();
    }
    return true;
}

void StreamController::ensureMeterThread() {
    syncAudioMonitor();
    if (audioThread_) {
        return;
    }
    audioStopRequested_ = false;
    audioThread_ = std::make_unique<std::thread>(&StreamController::audioThreadFunc, this);
}

void StreamController::stopMeterThread() {
    if (!audioThread_) {
        return;
    }
    audioStopRequested_ = true;
    if (audioThread_->joinable()) {
        audioThread_->join();
    }
    audioThread_.reset();
    audioStopRequested_ = false;
    if (audioMonitor_) {
        audioMonitor_->close();
    }
}

void StreamController::audioThreadFunc() {
    const auto interval = std::chrono::microseconds(1'000'000 / 50);
    auto next = std::chrono::steady_clock::now();

    while (!audioStopRequested_.load()) {
        if (auto mixed = AudioMixer::mixActiveScene(sourceRegistry_)) {
            if (audioMonitor_ && audioMonitor_->isEnabled()) {
                audioMonitor_->write(*mixed);
            }
            if (encoding_.load()) {
                rawAudioQueue_.push(std::move(*mixed));
            }
        }
        next += interval;
        std::this_thread::sleep_until(next);
    }
}

void StreamController::stopStream() {
    if (!streaming_.load()) {
        return;
    }

    streaming_ = false;
    rtmpOutput_->stop();
    rtmpQueue_.shutdown();
    Logger::info("StreamController: streaming stopped");
    maybeStopEncodePipeline();
}

StreamStats StreamController::stats() const {
    StreamStats s;
    s.isStreaming = streaming_.load();
    s.isRecording = recording_.load();
    s.isReplayBufferActive = replayActive_.load();
    s.isConnected = rtmpOutput_->isConnected();
    s.encoderName = encoder_->videoCodecName();
    s.droppedFrames = encoder_->droppedFrames();
    s.bytesSent = rtmpOutput_->bytesSent();
    s.bytesRecorded = fileRecorder_->bytesWritten();
    s.recordingPath = recordingPath_;
    s.lastReplayPath = lastReplayPath_;
    s.reconnectCount = rtmpOutput_->reconnectCount();
    s.streamStartTime = streamStartTime_;

    const uint64_t encoded = encoder_->encodedFrames();
    s.totalFrames = encoded;

    if (encoding_.load()) {
        const auto elapsed = std::chrono::steady_clock::now() - streamStartTime_;
        const double seconds = std::chrono::duration_cast<std::chrono::duration<double>>(elapsed).count();
        if (seconds > 0) {
            s.encodedFps = static_cast<double>(encoded) / seconds;
        }
        if (encoded + s.droppedFrames > 0) {
            s.dropRate = static_cast<double>(s.droppedFrames) /
                         static_cast<double>(encoded + s.droppedFrames) * 100.0;
        }
    }

    return s;
}

} // namespace railshot
