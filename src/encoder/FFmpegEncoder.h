#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include "core/FrameData.h"
#include "core/ThreadSafeQueue.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace railshot {

struct EncodedPacket {
    std::vector<uint8_t> data;
    int64_t pts = 0;
    int64_t dts = 0;
    bool isKeyFrame = false;
    bool isVideo = true;
};

class FFmpegEncoder {
public:
    static constexpr int kWidth = 1920;
    static constexpr int kHeight = 1080;
    static constexpr int kFps = 30;
    static constexpr int kAudioSampleRate = 48000;
    static constexpr int kAudioChannels = 2;

    FFmpegEncoder();
    ~FFmpegEncoder();

    FFmpegEncoder(const FFmpegEncoder&) = delete;
    FFmpegEncoder& operator=(const FFmpegEncoder&) = delete;

    bool initialize();
    void shutdown();

    bool start(ThreadSafeQueue<VideoFrame>* videoInput,
               ThreadSafeQueue<AudioFrame>* audioInput,
               ThreadSafeQueue<EncodedPacket>* output);
    void stop();

    [[nodiscard]] bool isRunning() const { return running_.load(); }
    [[nodiscard]] std::string videoCodecName() const { return videoCodecName_; }
    [[nodiscard]] uint64_t encodedFrames() const { return encodedFrames_.load(); }
    [[nodiscard]] uint64_t droppedFrames() const { return droppedFrames_.load(); }
    [[nodiscard]] AVCodecContext* videoCodecContext() const { return videoCtx_; }
    [[nodiscard]] AVCodecContext* audioCodecContext() const { return audioCtx_; }

private:
    bool initVideoEncoder();
    bool initAudioEncoder();
    bool tryOpenVideoEncoder(const char* codecName, bool hardware);
    void encoderThreadFunc();
    void encodeVideoFrame(const VideoFrame& frame);
    void encodeAudioFrame(const AudioFrame& frame);
    void flushEncoders();
    void cleanup();

    std::string videoCodecName_;
    AVCodecContext* videoCtx_ = nullptr;
    AVCodecContext* audioCtx_ = nullptr;
    SwrContext* swrCtx_ = nullptr;
    AVFrame* videoFrame_ = nullptr;
    AVFrame* audioFrame_ = nullptr;
    AVPacket* packet_ = nullptr;

    ThreadSafeQueue<VideoFrame>* videoInput_ = nullptr;
    ThreadSafeQueue<AudioFrame>* audioInput_ = nullptr;
    ThreadSafeQueue<EncodedPacket>* output_ = nullptr;

    std::unique_ptr<std::thread> encoderThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
    std::atomic<uint64_t> encodedFrames_{0};
    std::atomic<uint64_t> droppedFrames_{0};

    int64_t videoPts_ = 0;
    int64_t audioPts_ = 0;
    int64_t lastVideoTimestampUs_ = 0;
};

} // namespace railshot
