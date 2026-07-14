#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace railshot {

enum class PixelFormat {
    NV12,
    YUY2,
    RGB24,
    RGBA32
};

class VideoFrame {
public:
    VideoFrame() = default;

    VideoFrame(int width, int height, PixelFormat format);

    VideoFrame(const VideoFrame& other);
    VideoFrame& operator=(const VideoFrame& other);
    VideoFrame(VideoFrame&& other) noexcept;
    VideoFrame& operator=(VideoFrame&& other) noexcept;

    void allocate(int width, int height, PixelFormat format);
    void clear();

    [[nodiscard]] size_t dataSize() const;
    [[nodiscard]] bool isValid() const { return !data.empty() && width > 0 && height > 0; }

    int width = 0;
    int height = 0;
    PixelFormat format = PixelFormat::NV12;
    int64_t timestampUs = 0;
    std::vector<uint8_t> data;
};

struct AudioFrame {
    std::vector<uint8_t> data;
    int sampleRate = 48000;
    int channels = 2;
    int bytesPerSample = 2;
    int64_t timestampUs = 0;

    [[nodiscard]] int numSamples() const {
        if (bytesPerSample <= 0 || channels <= 0) {
            return 0;
        }
        return static_cast<int>(data.size()) / (bytesPerSample * channels);
    }

    [[nodiscard]] bool isValid() const { return !data.empty() && numSamples() > 0; }
};

} // namespace railshot
