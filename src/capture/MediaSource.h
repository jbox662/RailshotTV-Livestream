#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include "capture/ISourceProvider.h"
#include "core/FrameData.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

namespace railshot {

class MediaSource : public ISourceProvider {
public:
    explicit MediaSource(Source config);
    ~MediaSource() override;

    bool start() override;
    void stop() override;
    [[nodiscard]] bool isRunning() const override;

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

    Source config_;
    std::unique_ptr<std::thread> decodeThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};

    AVFormatContext* formatCtx_ = nullptr;
    AVCodecContext* videoCtx_ = nullptr;
    AVCodecContext* audioCtx_ = nullptr;
    SwsContext* swsCtx_ = nullptr;
    SwrContext* swrCtx_ = nullptr;
    int videoStreamIndex_ = -1;
    int audioStreamIndex_ = -1;

    bool hasVideoStream_ = false;
    bool hasAudioStream_ = false;

    std::mutex videoMutex_;
    VideoFrame latestVideo_;
    std::mutex audioMutex_;
    AudioFrame latestAudio_;
};

} // namespace railshot
