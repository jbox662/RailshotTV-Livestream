#include "output/Remux.h"

#include "core/Logger.h"

#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

namespace railshot {

bool remuxFile(const std::string& inputPath, const std::string& outputPath) {
    if (inputPath.empty() || outputPath.empty() || inputPath == outputPath) {
        return false;
    }

    AVFormatContext* inputCtx = nullptr;
    if (avformat_open_input(&inputCtx, inputPath.c_str(), nullptr, nullptr) < 0) {
        Logger::error("Remux: failed to open input " + inputPath);
        return false;
    }
    if (avformat_find_stream_info(inputCtx, nullptr) < 0) {
        Logger::error("Remux: failed to find stream info");
        avformat_close_input(&inputCtx);
        return false;
    }

    AVFormatContext* outputCtx = nullptr;
    if (avformat_alloc_output_context2(&outputCtx, nullptr, nullptr, outputPath.c_str()) < 0
        || !outputCtx) {
        Logger::error("Remux: failed to create output context");
        avformat_close_input(&inputCtx);
        return false;
    }

    std::vector<int> streamMap(inputCtx->nb_streams, -1);
    int outIndex = 0;
    for (unsigned i = 0; i < inputCtx->nb_streams; ++i) {
        AVStream* inStream = inputCtx->streams[i];
        const AVMediaType type = inStream->codecpar->codec_type;
        if (type != AVMEDIA_TYPE_VIDEO && type != AVMEDIA_TYPE_AUDIO
            && type != AVMEDIA_TYPE_SUBTITLE) {
            continue;
        }
        AVStream* outStream = avformat_new_stream(outputCtx, nullptr);
        if (!outStream) {
            Logger::error("Remux: failed to create output stream");
            avformat_free_context(outputCtx);
            avformat_close_input(&inputCtx);
            return false;
        }
        if (avcodec_parameters_copy(outStream->codecpar, inStream->codecpar) < 0) {
            Logger::error("Remux: failed to copy codec parameters");
            avformat_free_context(outputCtx);
            avformat_close_input(&inputCtx);
            return false;
        }
        outStream->codecpar->codec_tag = 0;
        outStream->time_base = inStream->time_base;
        streamMap[i] = outIndex++;
    }

    if (!(outputCtx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&outputCtx->pb, outputPath.c_str(), AVIO_FLAG_WRITE) < 0) {
            Logger::error("Remux: failed to open output " + outputPath);
            avformat_free_context(outputCtx);
            avformat_close_input(&inputCtx);
            return false;
        }
    }

    if (avformat_write_header(outputCtx, nullptr) < 0) {
        Logger::error("Remux: write_header failed");
        if (outputCtx->pb) {
            avio_closep(&outputCtx->pb);
        }
        avformat_free_context(outputCtx);
        avformat_close_input(&inputCtx);
        return false;
    }

    AVPacket packet;
    av_init_packet(&packet);
    while (av_read_frame(inputCtx, &packet) >= 0) {
        if (packet.stream_index < 0
            || packet.stream_index >= static_cast<int>(streamMap.size())
            || streamMap[packet.stream_index] < 0) {
            av_packet_unref(&packet);
            continue;
        }

        AVStream* inStream = inputCtx->streams[packet.stream_index];
        AVStream* outStream = outputCtx->streams[streamMap[packet.stream_index]];
        packet.stream_index = streamMap[packet.stream_index];
        packet.pts = av_rescale_q_rnd(packet.pts, inStream->time_base, outStream->time_base,
                                      static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        packet.dts = av_rescale_q_rnd(packet.dts, inStream->time_base, outStream->time_base,
                                      static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        packet.duration = av_rescale_q(packet.duration, inStream->time_base, outStream->time_base);
        packet.pos = -1;

        if (av_interleaved_write_frame(outputCtx, &packet) < 0) {
            Logger::warn("Remux: write_frame failed");
            av_packet_unref(&packet);
            break;
        }
        av_packet_unref(&packet);
    }

    av_write_trailer(outputCtx);
    if (outputCtx->pb) {
        avio_closep(&outputCtx->pb);
    }
    avformat_free_context(outputCtx);
    avformat_close_input(&inputCtx);
    Logger::info("Remux: wrote " + outputPath);
    return true;
}

} // namespace railshot
