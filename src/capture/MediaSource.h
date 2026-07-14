#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include "capture/ISourceProvider.h"
#include "capture/MediaSourceSettings.h"
#include "core/FrameData.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace railshot {

class MediaSource : public ISourceProvider {
public:
    explicit MediaSource(Source config);
    ~MediaSource() override;

    static void applyLiveConfig(const Source& source);
    static void restartPlayback(const std::string& sourceId);
    static void setPaused(const std::string& sourceId, bool paused);

    bool start() override;
    void stop() override;
    [[nodiscard]] bool isRunning() const override;
    void updateConfig(const Source& source) override;

    [[nodiscard]] std::optional<VideoFrame> latestVideoFrame() override;
    [[nodiscard]] std::optional<AudioFrame> latestAudioFrame() override;

    [[nodiscard]] bool hasVideo() const override;
    [[nodiscard]] bool hasAudio() const override;
    [[nodiscard]] const Source& config() const override { return config_; }

private:
    void decodeThreadFunc();
    bool openMedia();
    void closeMedia();
    bool decodeNext();
    void requestSeekToStart();
    void flushCodecs();
    [[nodiscard]] MediaSourceSettings settings() const;
    void registerInstance();
    void unregisterInstance();
    [[nodiscard]] int frameDelayMs() const;

    Source config_;
    std::unique_ptr<std::thread> decodeThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> paused_{false};
    std::atomic<bool> seekToStart_{false};
    std::atomic<double> playbackRate_{1.0};

    AVFormatContext* formatCtx_ = nullptr;
    AVCodecContext* videoCtx_ = nullptr;
    AVCodecContext* audioCtx_ = nullptr;
    AVBufferRef* hwDeviceCtx_ = nullptr;
    SwsContext* swsCtx_ = nullptr;
    SwrContext* swrCtx_ = nullptr;
    int videoStreamIndex_ = -1;
    int audioStreamIndex_ = -1;
    AVPixelFormat swsSrcFmt_ = AV_PIX_FMT_NONE;

    bool hasVideoStream_ = false;
    bool hasAudioStream_ = false;
    bool usingHwDecode_ = false;

    std::mutex videoMutex_;
    VideoFrame latestVideo_;
    std::mutex audioMutex_;
    AudioFrame latestAudio_;
};

} // namespace railshot
