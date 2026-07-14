#pragma once

#include "capture/ISourceProvider.h"
#include "capture/AudioDeviceSource.h"
#include "capture/BrowserSource.h"
#include "capture/ColorSource.h"
#include "capture/DesktopAudioSource.h"
#include "capture/DisplayCaptureSource.h"
#include "capture/ImageSource.h"
#include "capture/MediaSource.h"
#include "capture/NdiSource.h"
#include "capture/ScoreboardSource.h"
#include "capture/TextSource.h"
#include "capture/VideoDeviceSource.h"
#include "capture/WasapiAudioCapture.h"
#include "capture/WindowCaptureSource.h"
#include "core/ThreadSafeQueue.h"
#include "core/models/SceneManager.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace railshot {

class SourceRegistry {
public:
    SourceRegistry();
    ~SourceRegistry();

    void syncScene(const Scene& scene);
    void syncCombinedScenes(const Scene& sceneA, const Scene& sceneB);
    void stopAll();

    [[nodiscard]] ISourceProvider* providerForSource(const std::string& sourceId);
    [[nodiscard]] std::vector<std::string> activeSourceIds() const;

    bool startMicCapture();
    void stopMicCapture();
    bool restartMicCapture();
    [[nodiscard]] std::optional<AudioFrame> latestMicFrame();

private:
    std::unique_ptr<ISourceProvider> createProvider(const Source& source);
    [[nodiscard]] static bool canReuseProvider(const ISourceProvider& provider, const Source& source);

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<ISourceProvider>> providers_;
    std::unique_ptr<WasapiAudioCapture> micCapture_;
    ThreadSafeQueue<AudioFrame> micQueue_;
    bool micRunning_ = false;
};

} // namespace railshot
