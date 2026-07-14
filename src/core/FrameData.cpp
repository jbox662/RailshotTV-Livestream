#include "core/FrameData.h"

#include <algorithm>
#include <cstring>

namespace railshot {

namespace {

size_t bytesForFormat(int width, int height, PixelFormat format) {
    switch (format) {
    case PixelFormat::NV12:
        return static_cast<size_t>(width) * static_cast<size_t>(height) * 3 / 2;
    case PixelFormat::YUY2:
        return static_cast<size_t>(width) * static_cast<size_t>(height) * 2;
    case PixelFormat::RGB24:
        return static_cast<size_t>(width) * static_cast<size_t>(height) * 3;
    case PixelFormat::RGBA32:
        return static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    }
    return 0;
}

} // namespace

VideoFrame::VideoFrame(int w, int h, PixelFormat fmt) {
    allocate(w, h, fmt);
}

VideoFrame::VideoFrame(const VideoFrame& other) = default;
VideoFrame& VideoFrame::operator=(const VideoFrame& other) = default;
VideoFrame::VideoFrame(VideoFrame&& other) noexcept = default;
VideoFrame& VideoFrame::operator=(VideoFrame&& other) noexcept = default;

void VideoFrame::allocate(int w, int h, PixelFormat fmt) {
    width = w;
    height = h;
    format = fmt;
    data.resize(bytesForFormat(w, h, fmt));
}

void VideoFrame::clear() {
    width = 0;
    height = 0;
    format = PixelFormat::NV12;
    timestampUs = 0;
    data.clear();
}

size_t VideoFrame::dataSize() const {
    return data.size();
}

} // namespace railshot
