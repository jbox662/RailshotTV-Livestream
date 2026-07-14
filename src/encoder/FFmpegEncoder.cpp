#include "encoder/FFmpegEncoder.h"

#include "core/AppSettings.h"
#include "core/Logger.h"

#include <chrono>
#include <cstring>
#include <algorithm>

namespace railshot {

FFmpegEncoder::FFmpegEncoder() = default;

FFmpegEncoder::~FFmpegEncoder() {
    stop();
    shutdown();
}

bool FFmpegEncoder::initialize() {
    if (!initVideoEncoder()) {
        return false;
    }
    if (!initAudioEncoder()) {
        cleanup();
        return false;
    }

    videoFrame_ = av_frame_alloc();
    audioFrame_ = av_frame_alloc();
    packet_ = av_packet_alloc();

    if (!videoFrame_ || !audioFrame_ || !packet_) {
        Logger::error("FFmpegEncoder: failed to allocate frames/packet");
        cleanup();
        return false;
    }

    videoFrame_->format = videoCtx_->pix_fmt;
    videoFrame_->width = videoCtx_->width;
    videoFrame_->height = videoCtx_->height;
    if (av_frame_get_buffer(videoFrame_, 0) < 0) {
        Logger::error("FFmpegEncoder: failed to allocate video frame buffer");
        cleanup();
        return false;
    }

    audioFrame_->format = audioCtx_->sample_fmt;
    audioFrame_->ch_layout = audioCtx_->ch_layout;
    audioFrame_->sample_rate = audioCtx_->sample_rate;
    audioFrame_->nb_samples = audioCtx_->frame_size;
    if (av_frame_get_buffer(audioFrame_, 0) < 0) {
        Logger::error("FFmpegEncoder: failed to allocate audio frame buffer");
        cleanup();
        return false;
    }

    Logger::info("FFmpegEncoder: initialized with " + videoCodecName_ + " + AAC");
    return true;
}

void FFmpegEncoder::shutdown() {
    stop();
    cleanup();
}

bool FFmpegEncoder::tryOpenVideoEncoder(const char* codecName, bool hardware) {
    const AVCodec* codec = avcodec_find_encoder_by_name(codecName);
    if (!codec) {
        return false;
    }

    AVCodecContext* ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        return false;
    }

    const int width = AppSettings::instance().canvasWidth();
    const int height = AppSettings::instance().canvasHeight();
    const int fps = AppSettings::instance().fps();

    ctx->width = width;
    ctx->height = height;
    ctx->time_base = {1, fps};
    ctx->framerate = {fps, 1};
    ctx->gop_size = fps * 2;
    ctx->max_b_frames = 0;
    ctx->bit_rate = 6'000'000;

    if (hardware) {
        ctx->pix_fmt = AV_PIX_FMT_NV12;
    } else {
        ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    }

    AVDictionary* opts = nullptr;
    if (std::string(codecName) == "libx264") {
        av_dict_set(&opts, "preset", "veryfast", 0);
        av_dict_set(&opts, "tune", "zerolatency", 0);
    } else if (std::string(codecName) == "h264_nvenc") {
        av_dict_set(&opts, "preset", "p4", 0);
        av_dict_set(&opts, "tune", "ll", 0);
        av_dict_set(&opts, "rc", "cbr", 0);
    } else if (std::string(codecName) == "h264_amf") {
        av_dict_set(&opts, "quality", "balanced", 0);
        av_dict_set(&opts, "rc", "cbr", 0);
    } else if (std::string(codecName) == "h264_qsv") {
        av_dict_set(&opts, "preset", "medium", 0);
    }

    const int ret = avcodec_open2(ctx, codec, &opts);
    av_dict_free(&opts);

    if (ret < 0) {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        Logger::debug(std::string("FFmpegEncoder: ") + codecName + " unavailable: " + errBuf);
        avcodec_free_context(&ctx);
        return false;
    }

    videoCtx_ = ctx;
    videoCodecName_ = codecName;
    Logger::info(std::string("FFmpegEncoder: using video encoder ") + codecName);
    return true;
}

bool FFmpegEncoder::initVideoEncoder() {
    const char* hwCodecs[] = {"h264_nvenc", "h264_amf", "h264_qsv"};
    for (const char* codec : hwCodecs) {
        if (tryOpenVideoEncoder(codec, true)) {
            return true;
        }
    }
    return tryOpenVideoEncoder("libx264", false);
}

bool FFmpegEncoder::initAudioEncoder() {
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!codec) {
        Logger::error("FFmpegEncoder: AAC encoder not found");
        return false;
    }

    audioCtx_ = avcodec_alloc_context3(codec);
    if (!audioCtx_) {
        return false;
    }

    av_channel_layout_default(&audioCtx_->ch_layout, kAudioChannels);
    audioCtx_->sample_rate = kAudioSampleRate;
    audioCtx_->sample_fmt = AV_SAMPLE_FMT_FLTP;
    audioCtx_->bit_rate = 128'000;
    audioCtx_->time_base = {1, kAudioSampleRate};

    const int ret = avcodec_open2(audioCtx_, codec, nullptr);
    if (ret < 0) {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        Logger::error(std::string("FFmpegEncoder: failed to open AAC encoder: ") + errBuf);
        return false;
    }

    swrCtx_ = swr_alloc();
    AVChannelLayout inLayout;
    av_channel_layout_default(&inLayout, kAudioChannels);
    av_opt_set_chlayout(swrCtx_, "in_chlayout", &inLayout, 0);
    av_opt_set_chlayout(swrCtx_, "out_chlayout", &audioCtx_->ch_layout, 0);
    av_opt_set_int(swrCtx_, "in_sample_rate", kAudioSampleRate, 0);
    av_opt_set_int(swrCtx_, "out_sample_rate", kAudioSampleRate, 0);
    av_opt_set_sample_fmt(swrCtx_, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
    av_opt_set_sample_fmt(swrCtx_, "out_sample_fmt", audioCtx_->sample_fmt, 0);

    if (swr_init(swrCtx_) < 0) {
        Logger::error("FFmpegEncoder: failed to init resampler");
        return false;
    }

    return true;
}

bool FFmpegEncoder::start(ThreadSafeQueue<VideoFrame>* videoInput,
                          ThreadSafeQueue<AudioFrame>* audioInput,
                          ThreadSafeQueue<EncodedPacket>* output) {
    if (running_.load() || !videoInput || !output) {
        return false;
    }

    videoInput_ = videoInput;
    audioInput_ = audioInput;
    output_ = output;
    stopRequested_ = false;
    running_ = true;
    videoPts_ = 0;
    audioPts_ = 0;
    lastVideoTimestampUs_ = 0;
    encodedFrames_ = 0;
    droppedFrames_ = 0;

    encoderThread_ = std::make_unique<std::thread>(&FFmpegEncoder::encoderThreadFunc, this);
    return true;
}

void FFmpegEncoder::stop() {
    stopRequested_ = true;
    if (videoInput_) videoInput_->shutdown();
    if (audioInput_) audioInput_->shutdown();
    if (encoderThread_ && encoderThread_->joinable()) {
        encoderThread_->join();
    }
    encoderThread_.reset();
    running_ = false;
    videoInput_ = nullptr;
    audioInput_ = nullptr;
    output_ = nullptr;
}

void FFmpegEncoder::encodeVideoFrame(const VideoFrame& frame) {
    if (!videoCtx_ || !videoFrame_ || !packet_) {
        return;
    }

    if (av_frame_make_writable(videoFrame_) < 0) {
        droppedFrames_++;
        return;
    }

    if (videoCtx_->pix_fmt == AV_PIX_FMT_NV12) {
        const int ySize = frame.width * frame.height;
        std::memcpy(videoFrame_->data[0], frame.data.data(), static_cast<size_t>(ySize));
        std::memcpy(videoFrame_->data[1], frame.data.data() + ySize,
                    static_cast<size_t>(ySize / 2));
    } else {
        // YUV420P from NV12
        const int w = frame.width;
        const int h = frame.height;
        const auto* src = frame.data.data();
        const int ySize = w * h;

        std::memcpy(videoFrame_->data[0], src, ySize);

        auto* uvSrc = src + ySize;
        auto* uDst = videoFrame_->data[1];
        auto* vDst = videoFrame_->data[2];
        const int chromaW = w / 2;
        const int chromaH = h / 2;
        for (int row = 0; row < chromaH; ++row) {
            for (int col = 0; col < chromaW; ++col) {
                const int idx = row * w + col * 2;
                uDst[row * chromaW + col] = uvSrc[idx];
                vDst[row * chromaW + col] = uvSrc[idx + 1];
            }
        }
    }

    videoFrame_->pts = videoPts_++;
    lastVideoTimestampUs_ = frame.timestampUs;

    int ret = avcodec_send_frame(videoCtx_, videoFrame_);
    if (ret < 0) {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        Logger::warn(std::string("FFmpegEncoder: send_frame failed: ") + errBuf);
        droppedFrames_++;
        return;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(videoCtx_, packet_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            droppedFrames_++;
            break;
        }

        EncodedPacket enc;
        enc.isVideo = true;
        enc.pts = packet_->pts;
        enc.dts = packet_->dts;
        enc.isKeyFrame = (packet_->flags & AV_PKT_FLAG_KEY) != 0;
        enc.data.assign(packet_->data, packet_->data + packet_->size);

        if (output_) {
            output_->push(std::move(enc));
        }
        encodedFrames_++;
        av_packet_unref(packet_);
    }
}

void FFmpegEncoder::encodeAudioFrame(const AudioFrame& frame) {
    if (!audioCtx_ || !audioFrame_ || !packet_ || !swrCtx_) {
        return;
    }

    const int numSamples = frame.numSamples();
    if (numSamples <= 0) {
        return;
    }

    if (av_frame_make_writable(audioFrame_) < 0) {
        return;
    }

    const uint8_t* inData[] = {frame.data.data()};
    const int converted = swr_convert(swrCtx_,
                                      audioFrame_->data,
                                      audioFrame_->nb_samples,
                                      inData,
                                      numSamples);
    if (converted < 0) {
        return;
    }

    audioFrame_->nb_samples = converted;
    audioFrame_->pts = audioPts_;
    audioPts_ += converted;

    int ret = avcodec_send_frame(audioCtx_, audioFrame_);
    if (ret < 0) {
        return;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(audioCtx_, packet_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            break;
        }

        EncodedPacket enc;
        enc.isVideo = false;
        enc.pts = packet_->pts;
        enc.dts = packet_->dts;
        enc.data.assign(packet_->data, packet_->data + packet_->size);

        if (output_) {
            output_->push(std::move(enc));
        }
        av_packet_unref(packet_);
    }
}

void FFmpegEncoder::flushEncoders() {
    if (videoCtx_) {
        avcodec_send_frame(videoCtx_, nullptr);
        int ret = 0;
        while (ret >= 0) {
            ret = avcodec_receive_packet(videoCtx_, packet_);
            if (ret >= 0 && output_) {
                EncodedPacket enc;
                enc.isVideo = true;
                enc.pts = packet_->pts;
                enc.dts = packet_->dts;
                enc.isKeyFrame = (packet_->flags & AV_PKT_FLAG_KEY) != 0;
                enc.data.assign(packet_->data, packet_->data + packet_->size);
                output_->push(std::move(enc));
                av_packet_unref(packet_);
            }
        }
    }

    if (audioCtx_) {
        avcodec_send_frame(audioCtx_, nullptr);
        int ret = 0;
        while (ret >= 0) {
            ret = avcodec_receive_packet(audioCtx_, packet_);
            if (ret >= 0 && output_) {
                EncodedPacket enc;
                enc.isVideo = false;
                enc.pts = packet_->pts;
                enc.dts = packet_->dts;
                enc.data.assign(packet_->data, packet_->data + packet_->size);
                output_->push(std::move(enc));
                av_packet_unref(packet_);
            }
        }
    }
}

void FFmpegEncoder::encoderThreadFunc() {
    const int fps = std::max(1, AppSettings::instance().fps());
    const auto frameInterval = std::chrono::microseconds(1'000'000 / fps);
    auto nextFrameTime = std::chrono::steady_clock::now();

    while (!stopRequested_.load()) {
        auto videoOpt = videoInput_->pop(10);
        if (videoOpt.has_value()) {
            encodeVideoFrame(*videoOpt);
        }

        auto audioOpt = audioInput_ ? audioInput_->pop(0) : std::nullopt;
        while (audioOpt.has_value()) {
            encodeAudioFrame(*audioOpt);
            audioOpt = audioInput_ ? audioInput_->pop(0) : std::nullopt;
        }

        nextFrameTime += frameInterval;
        std::this_thread::sleep_until(nextFrameTime);
    }

    flushEncoders();
    running_ = false;
}

void FFmpegEncoder::cleanup() {
    if (packet_) {
        av_packet_free(&packet_);
    }
    if (videoFrame_) {
        av_frame_free(&videoFrame_);
    }
    if (audioFrame_) {
        av_frame_free(&audioFrame_);
    }
    if (swrCtx_) {
        swr_free(&swrCtx_);
    }
    if (videoCtx_) {
        avcodec_free_context(&videoCtx_);
    }
    if (audioCtx_) {
        avcodec_free_context(&audioCtx_);
    }
}

} // namespace railshot
