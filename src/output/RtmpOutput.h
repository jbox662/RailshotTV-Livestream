#pragma once

extern "C" {
#include <libavformat/avformat.h>
#include <rtmp.h>
}

#include "encoder/FFmpegEncoder.h"
#include "core/ThreadSafeQueue.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace railshot {

struct RtmpIoContext;

class RtmpOutput {
public:
    RtmpOutput();
    ~RtmpOutput();

    RtmpOutput(const RtmpOutput&) = delete;
    RtmpOutput& operator=(const RtmpOutput&) = delete;

    bool open(const std::string& rtmpUrl);
    void close();

    bool start(ThreadSafeQueue<EncodedPacket>* input,
               AVCodecContext* videoCtx,
               AVCodecContext* audioCtx);
    void stop();

    [[nodiscard]] bool isRunning() const { return running_.load(); }
    [[nodiscard]] bool isConnected() const { return connected_.load(); }
    [[nodiscard]] uint64_t bytesSent() const { return bytesSent_.load(); }
    [[nodiscard]] int reconnectCount() const { return reconnectCount_.load(); }

private:
    bool connect();
    void disconnect();
    bool writeHeader();
    void outputThreadFunc();
    bool writePacket(const EncodedPacket& packet);
    bool attemptReconnect();

    std::string rtmpUrl_;
    std::string rtmpHost_;
    std::string rtmpApp_;
    std::string rtmpStreamKey_;
    int rtmpPort_ = 1935;

    RTMP* rtmp_ = nullptr;
    AVFormatContext* formatCtx_ = nullptr;
    AVIOContext* avioCtx_ = nullptr;
    std::unique_ptr<RtmpIoContext> ioUserData_;
    int videoStreamIndex_ = -1;
    int audioStreamIndex_ = -1;

    AVCodecContext* videoCtx_ = nullptr;
    AVCodecContext* audioCtx_ = nullptr;
    ThreadSafeQueue<EncodedPacket>* input_ = nullptr;

    std::unique_ptr<std::thread> outputThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
    std::atomic<bool> stopRequested_{false};
    std::atomic<uint64_t> bytesSent_{0};
    std::atomic<int> reconnectCount_{0};

    int64_t videoFrameCount_ = 0;
    int64_t audioFrameCount_ = 0;
};

} // namespace railshot
