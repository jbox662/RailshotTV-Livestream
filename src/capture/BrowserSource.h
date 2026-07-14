#pragma once

#include "capture/ISourceProvider.h"
#include "core/models/SourceTypes.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

class QImage;
class QTimer;

namespace railshot {

class WebView2BrowserHost;
struct BrowserSourceSettings;

class BrowserSource : public ISourceProvider {
public:
    explicit BrowserSource(Source config);
    ~BrowserSource() override;

    static void refreshAll();
    static void applyLiveConfig(const Source& source);
    static void reloadAll();

    bool start() override;
    void stop() override;
    bool isRunning() const override;
    void updateConfig(const Source& source) override;

    std::optional<VideoFrame> latestVideoFrame() override;
    std::optional<AudioFrame> latestAudioFrame() override;
    bool hasVideo() const override;
    bool hasAudio() const override;
    const Source& config() const override { return config_; }

private:
    [[nodiscard]] BrowserSourceSettings settings() const;
    void applySettingsToHost();
    void syncNavigation();
    void requestCapture();
    void refreshFallbackContent();
    void applyCapturedImage(const QImage& image);
    void invalidateFrame();
    void registerInstance();
    void unregisterInstance();
    void startLiveRefresh();
    void stopLiveRefresh();

    Source config_;
    std::atomic<bool> running_{false};
    std::mutex mutex_;
    VideoFrame frame_;
    std::unique_ptr<WebView2BrowserHost> webHost_;
    QTimer* liveTimer_ = nullptr;
    bool captureInFlight_ = false;
    std::string lastSettingsKey_;
};

} // namespace railshot
