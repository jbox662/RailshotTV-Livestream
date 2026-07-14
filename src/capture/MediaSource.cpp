#include "capture/MediaSource.h"

#include "core/Logger.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <vector>

namespace railshot {
namespace {

std::mutex g_mediaInstancesMutex;
std::vector<MediaSource*> g_mediaInstances;

enum AVPixelFormat mediaGetHwFormat(AVCodecContext* ctx, const enum AVPixelFormat* pixFmts) {
    const auto target = static_cast<AVPixelFormat>(reinterpret_cast<intptr_t>(ctx->opaque));
    for (const enum AVPixelFormat* p = pixFmts; *p != AV_PIX_FMT_NONE; ++p) {
        if (*p == target) {
            return *p;
        }
    }
    return AV_PIX_FMT_NONE;
}

} // namespace

MediaSource::MediaSource(Source config)
    : config_(std::move(config)) {
    registerInstance();
    const MediaSourceSettings s = settings();
    playbackRate_.store(s.playbackRate());
}

MediaSource::~MediaSource() {
    stop();
    unregisterInstance();
}

void MediaSource::registerInstance() {
    std::lock_guard lock(g_mediaInstancesMutex);
    g_mediaInstances.push_back(this);
}

void MediaSource::unregisterInstance() {
    std::lock_guard lock(g_mediaInstancesMutex);
    g_mediaInstances.erase(
        std::remove(g_mediaInstances.begin(), g_mediaInstances.end(), this), g_mediaInstances.end());
}

MediaSourceSettings MediaSource::settings() const {
    return MediaSourceSettings::fromSource(config_);
}

void MediaSource::applyLiveConfig(const Source& source) {
    std::vector<MediaSource*> copy;
    {
        std::lock_guard lock(g_mediaInstancesMutex);
        copy = g_mediaInstances;
    }
    for (MediaSource* provider : copy) {
        if (!provider || provider->config_.id != source.id) {
            continue;
        }
        provider->updateConfig(source);
    }
}

void MediaSource::restartPlayback(const std::string& sourceId) {
    std::vector<MediaSource*> copy;
    {
        std::lock_guard lock(g_mediaInstancesMutex);
        copy = g_mediaInstances;
    }
    for (MediaSource* provider : copy) {
        if (!provider || provider->config_.id != sourceId) {
            continue;
        }
        provider->paused_.store(false);
        provider->requestSeekToStart();
    }
}

void MediaSource::setPaused(const std::string& sourceId, bool paused) {
    std::vector<MediaSource*> copy;
    {
        std::lock_guard lock(g_mediaInstancesMutex);
        copy = g_mediaInstances;
    }
    for (MediaSource* provider : copy) {
        if (!provider || provider->config_.id != sourceId) {
            continue;
        }
        provider->paused_.store(paused);
    }
}

bool MediaSource::hasVideo() const {
    return hasVideoStream_;
}

bool MediaSource::hasAudio() const {
    return hasAudioStream_;
}

void MediaSource::requestSeekToStart() {
    seekToStart_.store(true);
}

void MediaSource::flushCodecs() {
    if (videoCtx_) {
        avcodec_flush_buffers(videoCtx_);
    }
    if (audioCtx_) {
        avcodec_flush_buffers(audioCtx_);
    }
}

bool MediaSource::openMedia() {
    closeMedia();

    if (config_.pathOrDeviceId.empty()) {
        Logger::error("MediaSource: no file path for " + config_.name);
        return false;
    }

    if (avformat_open_input(&formatCtx_, config_.pathOrDeviceId.c_str(), nullptr, nullptr) < 0) {
        Logger::error("MediaSource: failed to open " + config_.pathOrDeviceId);
        return false;
    }

    if (avformat_find_stream_info(formatCtx_, nullptr) < 0) {
        Logger::error("MediaSource: failed to find stream info");
        closeMedia();
        return false;
    }

    const MediaSourceSettings mediaSettings = settings();
    playbackRate_.store(mediaSettings.playbackRate());

    for (unsigned int i = 0; i < formatCtx_->nb_streams; ++i) {
        AVCodecParameters* par = formatCtx_->streams[i]->codecpar;
        if (par->codec_type == AVMEDIA_TYPE_VIDEO && videoStreamIndex_ < 0) {
            const AVCodec* codec = avcodec_find_decoder(par->codec_id);
            if (!codec) {
                continue;
            }
            videoCtx_ = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(videoCtx_, par);

            if (mediaSettings.hardwareDecode) {
                const AVHWDeviceType candidates[] = {
                    AV_HWDEVICE_TYPE_D3D11VA,
                    AV_HWDEVICE_TYPE_DXVA2,
                    AV_HWDEVICE_TYPE_NONE,
                };
                for (int c = 0; candidates[c] != AV_HWDEVICE_TYPE_NONE; ++c) {
                    const AVHWDeviceType type = candidates[c];
                    for (int j = 0;; ++j) {
                        const AVCodecHWConfig* config = avcodec_get_hw_config(codec, j);
                        if (!config) {
                            break;
                        }
                        if (!(config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)
                            || config->device_type != type) {
                            continue;
                        }
                        if (av_hwdevice_ctx_create(&hwDeviceCtx_, type, nullptr, nullptr, 0) < 0) {
                            hwDeviceCtx_ = nullptr;
                            continue;
                        }
                        videoCtx_->hw_device_ctx = av_buffer_ref(hwDeviceCtx_);
                        videoCtx_->opaque = reinterpret_cast<void*>(static_cast<intptr_t>(config->pix_fmt));
                        videoCtx_->get_format = mediaGetHwFormat;
                        usingHwDecode_ = true;
                        break;
                    }
                    if (usingHwDecode_) {
                        break;
                    }
                }
            }

            if (avcodec_open2(videoCtx_, codec, nullptr) < 0) {
                if (hwDeviceCtx_) {
                    av_buffer_unref(&hwDeviceCtx_);
                }
                videoCtx_->hw_device_ctx = nullptr;
                videoCtx_->get_format = nullptr;
                videoCtx_->opaque = nullptr;
                usingHwDecode_ = false;
                if (avcodec_open2(videoCtx_, codec, nullptr) < 0) {
                    avcodec_free_context(&videoCtx_);
                    continue;
                }
            }
            videoStreamIndex_ = static_cast<int>(i);
            hasVideoStream_ = true;
            swsSrcFmt_ = videoCtx_->pix_fmt;
        } else if (par->codec_type == AVMEDIA_TYPE_AUDIO && audioStreamIndex_ < 0) {
            const AVCodec* codec = avcodec_find_decoder(par->codec_id);
            if (!codec) {
                continue;
            }
            audioCtx_ = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(audioCtx_, par);
            if (avcodec_open2(audioCtx_, codec, nullptr) < 0) {
                avcodec_free_context(&audioCtx_);
                continue;
            }
            audioStreamIndex_ = static_cast<int>(i);
            hasAudioStream_ = true;

            swrCtx_ = swr_alloc();
            AVChannelLayout outLayout;
            av_channel_layout_default(&outLayout, 2);
            av_opt_set_chlayout(swrCtx_, "out_chlayout", &outLayout, 0);
            av_opt_set_int(swrCtx_, "out_sample_rate", 48000, 0);
            av_opt_set_sample_fmt(swrCtx_, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
            av_opt_set_chlayout(swrCtx_, "in_chlayout", &audioCtx_->ch_layout, 0);
            av_opt_set_int(swrCtx_, "in_sample_rate", audioCtx_->sample_rate, 0);
            av_opt_set_sample_fmt(swrCtx_, "in_sample_fmt", audioCtx_->sample_fmt, 0);
            swr_init(swrCtx_);
        }
    }

    if (!hasVideoStream_ && !hasAudioStream_) {
        Logger::error("MediaSource: no decodable streams in " + config_.pathOrDeviceId);
        closeMedia();
        return false;
    }

    Logger::info(std::string("MediaSource: opened ") + config_.pathOrDeviceId
                 + (usingHwDecode_ ? " (hw decode)" : " (software decode)"));
    return true;
}

void MediaSource::closeMedia() {
    if (swsCtx_) {
        sws_freeContext(swsCtx_);
        swsCtx_ = nullptr;
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
    if (hwDeviceCtx_) {
        av_buffer_unref(&hwDeviceCtx_);
    }
    if (formatCtx_) {
        avformat_close_input(&formatCtx_);
    }
    videoStreamIndex_ = -1;
    audioStreamIndex_ = -1;
    hasVideoStream_ = false;
    hasAudioStream_ = false;
    usingHwDecode_ = false;
    swsSrcFmt_ = AV_PIX_FMT_NONE;
}

int MediaSource::frameDelayMs() const {
    double fps = 30.0;
    if (formatCtx_ && videoStreamIndex_ >= 0) {
        const AVRational rate = formatCtx_->streams[videoStreamIndex_]->avg_frame_rate;
        if (rate.num > 0 && rate.den > 0) {
            fps = static_cast<double>(rate.num) / static_cast<double>(rate.den);
        }
    }
    const double rate = std::max(0.01, playbackRate_.load());
    const int ms = static_cast<int>(1000.0 / (fps * rate));
    return std::clamp(ms, 1, 200);
}

bool MediaSource::decodeNext() {
    if (!formatCtx_) {
        return false;
    }

    if (seekToStart_.exchange(false)) {
        av_seek_frame(formatCtx_, -1, 0, AVSEEK_FLAG_BACKWARD);
        flushCodecs();
    }

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    AVFrame* swFrame = nullptr;
    bool gotFrame = false;

    while (av_read_frame(formatCtx_, packet) >= 0) {
        if (packet->stream_index == videoStreamIndex_ && videoCtx_) {
            if (avcodec_send_packet(videoCtx_, packet) >= 0) {
                while (avcodec_receive_frame(videoCtx_, frame) >= 0) {
                    AVFrame* source = frame;
                    if (frame->format == AV_PIX_FMT_D3D11 || frame->format == AV_PIX_FMT_DXVA2_VLD
                        || (usingHwDecode_ && frame->hw_frames_ctx)) {
                        if (!swFrame) {
                            swFrame = av_frame_alloc();
                        }
                        if (av_hwframe_transfer_data(swFrame, frame, 0) < 0) {
                            continue;
                        }
                        source = swFrame;
                    }

                    const AVPixelFormat srcFmt = static_cast<AVPixelFormat>(source->format);
                    if (!swsCtx_ || swsSrcFmt_ != srcFmt) {
                        if (swsCtx_) {
                            sws_freeContext(swsCtx_);
                        }
                        swsCtx_ = sws_getContext(source->width, source->height, srcFmt, source->width,
                                                 source->height, AV_PIX_FMT_NV12, SWS_BILINEAR, nullptr,
                                                 nullptr, nullptr);
                        swsSrcFmt_ = srcFmt;
                    }
                    if (!swsCtx_) {
                        continue;
                    }

                    VideoFrame vf;
                    vf.width = source->width;
                    vf.height = source->height;
                    vf.format = PixelFormat::NV12;
                    vf.allocate(vf.width, vf.height, PixelFormat::NV12);

                    uint8_t* dstData[4] = {vf.data.data(), vf.data.data() + vf.width * vf.height, nullptr,
                                          nullptr};
                    int dstLinesize[4] = {vf.width, vf.width, 0, 0};
                    sws_scale(swsCtx_, source->data, source->linesize, 0, source->height, dstData,
                              dstLinesize);

                    const auto now = std::chrono::steady_clock::now().time_since_epoch();
                    vf.timestampUs =
                        std::chrono::duration_cast<std::chrono::microseconds>(now).count();

                    {
                        std::lock_guard lock(videoMutex_);
                        latestVideo_ = std::move(vf);
                    }
                    gotFrame = true;
                }
            }
        } else if (packet->stream_index == audioStreamIndex_ && audioCtx_) {
            if (avcodec_send_packet(audioCtx_, packet) >= 0) {
                while (avcodec_receive_frame(audioCtx_, frame) >= 0) {
                    AudioFrame af;
                    af.sampleRate = 48000;
                    af.channels = 2;
                    af.bytesPerSample = 2;

                    const int outSamples = swr_get_out_samples(swrCtx_, frame->nb_samples);
                    std::vector<uint8_t> buffer(static_cast<size_t>(outSamples) * 2 * 2);
                    uint8_t* outData = buffer.data();
                    const int converted = swr_convert(swrCtx_, &outData, outSamples,
                                                      const_cast<const uint8_t**>(frame->data),
                                                      frame->nb_samples);
                    if (converted > 0) {
                        buffer.resize(static_cast<size_t>(converted) * 2 * 2);
                        af.data = std::move(buffer);
                        const auto now = std::chrono::steady_clock::now().time_since_epoch();
                        af.timestampUs =
                            std::chrono::duration_cast<std::chrono::microseconds>(now).count();
                        std::lock_guard lock(audioMutex_);
                        latestAudio_ = std::move(af);
                        gotFrame = true;
                    }
                }
            }
        }
        av_packet_unref(packet);
        if (gotFrame) {
            break;
        }
    }

    if (!gotFrame && config_.loop) {
        av_seek_frame(formatCtx_, -1, 0, AVSEEK_FLAG_BACKWARD);
        flushCodecs();
    }

    if (swFrame) {
        av_frame_free(&swFrame);
    }
    av_frame_free(&frame);
    av_packet_free(&packet);
    return gotFrame;
}

bool MediaSource::start() {
    if (running_.load()) {
        return true;
    }
    if (!openMedia()) {
        return false;
    }
    stopRequested_ = false;
    paused_ = false;
    seekToStart_ = false;
    running_ = true;
    decodeThread_ = std::make_unique<std::thread>(&MediaSource::decodeThreadFunc, this);
    return true;
}

void MediaSource::stop() {
    stopRequested_ = true;
    if (decodeThread_ && decodeThread_->joinable()) {
        decodeThread_->join();
    }
    decodeThread_.reset();
    closeMedia();
    running_ = false;
}

bool MediaSource::isRunning() const {
    return running_.load();
}

void MediaSource::updateConfig(const Source& source) {
    const MediaSourceSettings before = settings();
    const bool wasVisible = config_.isVisible;
    const std::string oldPath = config_.pathOrDeviceId;
    config_ = source;
    const MediaSourceSettings after = settings();
    playbackRate_.store(after.playbackRate());

    if (!running_.load()) {
        return;
    }

    // Restart-on-show (OBS media source behavior).
    if (after.restartOnActivate && !wasVisible && config_.isVisible) {
        paused_.store(false);
        requestSeekToStart();
    }

    const bool needReopen = oldPath != config_.pathOrDeviceId
                            || before.hardwareDecode != after.hardwareDecode;
    if (!needReopen) {
        return;
    }

    stopRequested_ = true;
    if (decodeThread_ && decodeThread_->joinable()) {
        decodeThread_->join();
    }
    decodeThread_.reset();
    closeMedia();
    stopRequested_ = false;
    if (!openMedia()) {
        running_ = false;
        return;
    }
    decodeThread_ = std::make_unique<std::thread>(&MediaSource::decodeThreadFunc, this);
}

void MediaSource::decodeThreadFunc() {
    while (!stopRequested_.load()) {
        if (paused_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
        if (!decodeNext()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(frameDelayMs()));
        }
    }
}

std::optional<VideoFrame> MediaSource::latestVideoFrame() {
    std::lock_guard lock(videoMutex_);
    if (!latestVideo_.isValid()) {
        return std::nullopt;
    }
    return latestVideo_;
}

std::optional<AudioFrame> MediaSource::latestAudioFrame() {
    std::lock_guard lock(audioMutex_);
    if (!latestAudio_.isValid()) {
        return std::nullopt;
    }
    return latestAudio_;
}

} // namespace railshot
