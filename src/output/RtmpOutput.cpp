#include "output/RtmpOutput.h"

#include "core/Logger.h"

#include <cstring>

namespace railshot {

struct RtmpIoContext {
    RTMP* rtmp = nullptr;
    std::atomic<uint64_t>* bytesSent = nullptr;
};

namespace {

int rtmpWriteCallback(void* opaque, const uint8_t* buf, int bufSize) {
    auto* ctx = static_cast<RtmpIoContext*>(opaque);
    if (!ctx || !ctx->rtmp || bufSize <= 0) {
        return AVERROR(EIO);
    }

    const int written = RTMP_Write(ctx->rtmp, reinterpret_cast<const char*>(buf), bufSize);
    if (written <= 0) {
        return AVERROR(EIO);
    }

    if (ctx->bytesSent) {
        ctx->bytesSent->fetch_add(static_cast<uint64_t>(written));
    }
    return written;
}

bool parseRtmpUrl(const std::string& url, std::string& host, int& port,
                  std::string& app, std::string& streamKey) {
    AVal parsedHost{};
    AVal parsedApp{};
    AVal parsedPlaypath{};
    int protocol = 0;
    unsigned int parsedPort = 0;

    char urlBuf[1024];
    std::strncpy(urlBuf, url.c_str(), sizeof(urlBuf) - 1);
    urlBuf[sizeof(urlBuf) - 1] = '\0';

    if (!RTMP_ParseURL(urlBuf, &protocol, &parsedHost, &parsedPort,
                       &parsedPlaypath, &parsedApp)) {
        return false;
    }

    host.assign(parsedHost.av_val, parsedHost.av_len);
    port = static_cast<int>(parsedPort);
    app.assign(parsedApp.av_val, parsedApp.av_len);
    streamKey.assign(parsedPlaypath.av_val, parsedPlaypath.av_len);
    return true;
}

} // namespace

RtmpOutput::RtmpOutput() = default;

RtmpOutput::~RtmpOutput() {
    stop();
    close();
}

bool RtmpOutput::open(const std::string& rtmpUrl) {
    close();
    rtmpUrl_ = rtmpUrl;

    if (!parseRtmpUrl(rtmpUrl, rtmpHost_, rtmpPort_, rtmpApp_, rtmpStreamKey_)) {
        Logger::error("RtmpOutput: failed to parse RTMP URL");
        return false;
    }

    Logger::info("RtmpOutput: configured for " + rtmpHost_ + "/" + rtmpApp_);
    return true;
}

void RtmpOutput::close() {
    stop();
    disconnect();
    rtmpUrl_.clear();
}

bool RtmpOutput::connect() {
    disconnect();

    rtmp_ = RTMP_Alloc();
    if (!rtmp_) {
        Logger::error("RtmpOutput: RTMP_Alloc failed");
        return false;
    }
    RTMP_Init(rtmp_);

    char urlBuf[1024];
    std::strncpy(urlBuf, rtmpUrl_.c_str(), sizeof(urlBuf) - 1);
    urlBuf[sizeof(urlBuf) - 1] = '\0';

    if (!RTMP_SetupURL(rtmp_, urlBuf)) {
        Logger::error("RtmpOutput: RTMP_SetupURL failed");
        RTMP_Free(rtmp_);
        rtmp_ = nullptr;
        return false;
    }

    RTMP_EnableWrite(rtmp_);

    if (!RTMP_Connect(rtmp_, nullptr)) {
        Logger::error("RtmpOutput: RTMP_Connect failed");
        RTMP_Free(rtmp_);
        rtmp_ = nullptr;
        return false;
    }

    if (!RTMP_ConnectStream(rtmp_, 0)) {
        Logger::error("RtmpOutput: RTMP_ConnectStream failed");
        RTMP_Close(rtmp_);
        RTMP_Free(rtmp_);
        rtmp_ = nullptr;
        return false;
    }

    connected_ = true;
    Logger::info("RtmpOutput: connected to RTMP server");
    return true;
}

void RtmpOutput::disconnect() {
    connected_ = false;

    if (formatCtx_) {
        if (formatCtx_->pb) {
            avio_flush(formatCtx_->pb);
        }
        avformat_free_context(formatCtx_);
        formatCtx_ = nullptr;
    }
    avioCtx_ = nullptr;
    ioUserData_.reset();

    if (rtmp_) {
        RTMP_Close(rtmp_);
        RTMP_Free(rtmp_);
        rtmp_ = nullptr;
    }

    videoStreamIndex_ = -1;
    audioStreamIndex_ = -1;
    videoFrameCount_ = 0;
    audioFrameCount_ = 0;
}

bool RtmpOutput::writeHeader() {
    if (!videoCtx_ || !audioCtx_) {
        return false;
    }

    avformat_alloc_output_context2(&formatCtx_, nullptr, "flv", nullptr);
    if (!formatCtx_) {
        Logger::error("RtmpOutput: failed to create output context");
        return false;
    }

    AVStream* videoStream = avformat_new_stream(formatCtx_, nullptr);
    if (!videoStream) {
        Logger::error("RtmpOutput: failed to create video stream");
        return false;
    }
    videoStreamIndex_ = videoStream->index;
    avcodec_parameters_from_context(videoStream->codecpar, videoCtx_);
    videoStream->time_base = videoCtx_->time_base;

    AVStream* audioStream = avformat_new_stream(formatCtx_, nullptr);
    if (!audioStream) {
        Logger::error("RtmpOutput: failed to create audio stream");
        return false;
    }
    audioStreamIndex_ = audioStream->index;
    avcodec_parameters_from_context(audioStream->codecpar, audioCtx_);
    audioStream->time_base = audioCtx_->time_base;

    auto* ioCtx = new RtmpIoContext{rtmp_, &bytesSent_};
    ioUserData_.reset(ioCtx);
    const int avioBufSize = 64 * 1024;
    unsigned char* avioBuf = static_cast<unsigned char*>(av_malloc(avioBufSize));
    if (!avioBuf) {
        delete ioCtx;
        Logger::error("RtmpOutput: failed to allocate AVIO buffer");
        return false;
    }

    avioCtx_ = avio_alloc_context(avioBuf, avioBufSize, 1, ioCtx,
                                  nullptr, rtmpWriteCallback, nullptr);
    if (!avioCtx_) {
        av_free(avioBuf);
        delete ioCtx;
        Logger::error("RtmpOutput: failed to create AVIO context");
        return false;
    }

    formatCtx_->pb = avioCtx_;
    formatCtx_->flags |= AVFMT_FLAG_CUSTOM_IO;

    const int ret = avformat_write_header(formatCtx_, nullptr);
    if (ret < 0) {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        Logger::error(std::string("RtmpOutput: write_header failed: ") + errBuf);
        return false;
    }

    Logger::info("RtmpOutput: FLV header written");
    return true;
}

bool RtmpOutput::start(ThreadSafeQueue<EncodedPacket>* input,
                       AVCodecContext* videoCtx,
                       AVCodecContext* audioCtx) {
    if (running_.load() || !input || !videoCtx || !audioCtx) {
        return false;
    }

    input_ = input;
    videoCtx_ = videoCtx;
    audioCtx_ = audioCtx;
    stopRequested_ = false;
    running_ = true;
    bytesSent_ = 0;
    reconnectCount_ = 0;

    outputThread_ = std::make_unique<std::thread>(&RtmpOutput::outputThreadFunc, this);
    return true;
}

void RtmpOutput::stop() {
    stopRequested_ = true;
    if (input_) {
        input_->shutdown();
    }
    if (outputThread_ && outputThread_->joinable()) {
        outputThread_->join();
    }
    outputThread_.reset();
    disconnect();
    running_ = false;
    input_ = nullptr;
    videoCtx_ = nullptr;
    audioCtx_ = nullptr;
}

bool RtmpOutput::attemptReconnect() {
    Logger::warn("RtmpOutput: attempting reconnect...");
    disconnect();

    for (int attempt = 1; attempt <= 5 && !stopRequested_.load(); ++attempt) {
        std::this_thread::sleep_for(std::chrono::seconds(attempt * 2));
        if (connect() && writeHeader()) {
            reconnectCount_++;
            Logger::info("RtmpOutput: reconnected on attempt " + std::to_string(attempt));
            return true;
        }
    }

    Logger::error("RtmpOutput: reconnect failed after 5 attempts");
    return false;
}

bool RtmpOutput::writePacket(const EncodedPacket& packet) {
    if (!formatCtx_ || !connected_.load()) {
        return false;
    }

    AVPacket avPkt;
    av_init_packet(&avPkt);
    avPkt.data = const_cast<uint8_t*>(packet.data.data());
    avPkt.size = static_cast<int>(packet.data.size());
    avPkt.pts = packet.pts;
    avPkt.dts = packet.dts;

    if (packet.isVideo) {
        avPkt.stream_index = videoStreamIndex_;
        if (packet.isKeyFrame) {
            avPkt.flags |= AV_PKT_FLAG_KEY;
        }
        avPkt.duration = 1;
    } else {
        avPkt.stream_index = audioStreamIndex_;
    }

    const int ret = av_interleaved_write_frame(formatCtx_, &avPkt);
    if (ret < 0) {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        Logger::warn(std::string("RtmpOutput: write_frame failed: ") + errBuf);
        connected_ = false;
        return false;
    }

    return true;
}

void RtmpOutput::outputThreadFunc() {
    if (!connect()) {
        running_ = false;
        return;
    }

    if (!writeHeader()) {
        disconnect();
        running_ = false;
        return;
    }

    while (!stopRequested_.load()) {
        auto packetOpt = input_->pop(100);
        if (!packetOpt.has_value()) {
            continue;
        }

        if (!writePacket(*packetOpt)) {
            if (!attemptReconnect()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }

    if (formatCtx_ && connected_.load()) {
        av_write_trailer(formatCtx_);
    }

    disconnect();
    running_ = false;
}

} // namespace railshot
