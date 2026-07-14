#include "core/GLCompositor.h"

#include "core/AppSettings.h"
#include "core/Logger.h"
#include "output/VirtualCamOutput.h"

#include <QImage>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QPainter>
#include <QSurfaceFormat>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <optional>
#include <vector>

namespace railshot {

namespace {

const char* kVertexShader = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
out vec2 vTexCoord;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
)";

const char* kNv12FragmentShader = R"(
#version 330 core
in vec2 vTexCoord;
out vec4 fragColor;
uniform sampler2D yTexture;
uniform sampler2D uvTexture;
uniform float uAlpha;
uniform float uBrightness;
uniform float uContrast;
uniform float uSaturation;
uniform vec4 uCrop;          // L T R B insets 0..1
uniform vec2 uScroll;        // UV offset
uniform float uScale;        // UV zoom
uniform float uSharpness;    // 0..1
uniform vec2 uTexelSize;
uniform int uKeyMode;        // 0 none, 1 color, 2 chroma
uniform vec3 uKeyColor;
uniform float uKeySimilarity;
uniform float uKeySmoothness;
uniform float uKeySpill;
uniform float uGradeAmount;
uniform float uLift;
uniform float uGamma;
uniform float uGain;
uniform int uScrollLoop;

vec2 mapUv(vec2 uv) {
    vec2 local = uv;
    if (uScrollLoop == 1) {
        local = fract(local + uScroll);
    } else {
        local = clamp(local + uScroll, 0.0, 1.0);
    }
    // Scale about center of remaining crop window.
    vec2 minUv = vec2(uCrop.x, uCrop.y);
    vec2 maxUv = vec2(1.0 - uCrop.z, 1.0 - uCrop.w);
    vec2 center = 0.5 * (minUv + maxUv);
    vec2 span = max(maxUv - minUv, vec2(0.001));
    vec2 mapped = center + (local - 0.5) * span / max(uScale, 0.05);
    return clamp(mapped, minUv, maxUv);
}

vec3 sampleRgb(vec2 uv) {
    float y = texture(yTexture, uv).r;
    vec2 chroma = texture(uvTexture, uv).rg - vec2(0.5, 0.5);
    float r = y + 1.402 * chroma.y;
    float g = y - 0.344 * chroma.x - 0.714 * chroma.y;
    float b = y + 1.772 * chroma.x;
    return vec3(r, g, b);
}

void main() {
    vec2 uv = mapUv(vTexCoord);
    vec3 color = sampleRgb(uv);

    if (uSharpness > 0.001) {
        vec3 blur = sampleRgb(uv + vec2(uTexelSize.x, 0.0))
                  + sampleRgb(uv - vec2(uTexelSize.x, 0.0))
                  + sampleRgb(uv + vec2(0.0, uTexelSize.y))
                  + sampleRgb(uv - vec2(0.0, uTexelSize.y));
        blur *= 0.25;
        color = mix(color, color + (color - blur), uSharpness);
    }

    color = (color - 0.5) * uContrast + 0.5 + uBrightness;
    float luma = dot(color, vec3(0.299, 0.587, 0.114));
    color = mix(vec3(luma), color, uSaturation);

    if (uGradeAmount > 0.001) {
        vec3 graded = color + uLift;
        graded = pow(max(graded, vec3(0.0)), vec3(1.0 / max(uGamma, 0.01)));
        graded *= uGain;
        color = mix(color, graded, uGradeAmount);
    }

    float alpha = uAlpha;
    if (uKeyMode > 0) {
        float dist = distance(color, uKeyColor);
        float edge0 = max(uKeySimilarity - uKeySmoothness, 0.0);
        float edge1 = uKeySimilarity + uKeySmoothness;
        float mask = smoothstep(edge0, edge1, dist);
        alpha *= mask;
        if (uKeyMode == 2 && uKeySpill > 0.0) {
            float spill = (1.0 - mask) * uKeySpill;
            float keyedLuma = dot(color, vec3(0.299, 0.587, 0.114));
            color = mix(color, vec3(keyedLuma), spill);
        }
    }

    fragColor = vec4(clamp(color, 0.0, 1.0), clamp(alpha, 0.0, 1.0));
}
)";

const char* kRgbaFragmentShader = R"(
#version 330 core
in vec2 vTexCoord;
out vec4 fragColor;
uniform sampler2D rgbaTexture;
uniform float uAlpha;
uniform vec4 uCrop;
uniform vec2 uScroll;
uniform float uScale;
uniform int uScrollLoop;
void main() {
    vec2 local = vTexCoord;
    if (uScrollLoop == 1) {
        local = fract(local + uScroll);
    } else {
        local = clamp(local + uScroll, 0.0, 1.0);
    }
    vec2 minUv = vec2(uCrop.x, uCrop.y);
    vec2 maxUv = vec2(1.0 - uCrop.z, 1.0 - uCrop.w);
    vec2 center = 0.5 * (minUv + maxUv);
    vec2 span = max(maxUv - minUv, vec2(0.001));
    vec2 uv = clamp(center + (local - 0.5) * span / max(uScale, 0.05), minUv, maxUv);
    vec4 c = texture(rgbaTexture, uv);
    fragColor = vec4(c.rgb, c.a * uAlpha);
}
)";

std::vector<Source> sortedSources(const Scene& scene) {
    auto sources = scene.sources;
    std::sort(sources.begin(), sources.end(),
              [](const Source& a, const Source& b) { return a.zOrder < b.zOrder; });
    return sources;
}

QImage readFboAsImage(QOpenGLFunctions* gl, int width, int height) {
    std::vector<uint8_t> rgba(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    gl->glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());

    QImage image(width, height, QImage::Format_ARGB32);
    for (int row = 0; row < height; ++row) {
        const int srcRow = height - 1 - row;
        const uint8_t* src = rgba.data() + static_cast<size_t>(srcRow) * static_cast<size_t>(width) * 4;
        auto* dst = reinterpret_cast<QRgb*>(image.scanLine(row));
        for (int col = 0; col < width; ++col) {
            const int idx = col * 4;
            dst[col] = qRgba(src[idx], src[idx + 1], src[idx + 2], src[idx + 3]);
        }
    }
    return image;
}

QImage rgbaFrameToImage(const VideoFrame& frame) {
    QImage image(frame.width, frame.height, QImage::Format_ARGB32);
    for (int row = 0; row < frame.height; ++row) {
        std::memcpy(image.scanLine(row),
                    frame.data.data() + static_cast<size_t>(row) * static_cast<size_t>(frame.width) * 4,
                    static_cast<size_t>(frame.width) * 4);
    }
    return image;
}

void argbImageToNv12(const QImage& image, VideoFrame& output) {
    QImage argb = image.format() == QImage::Format_ARGB32
                      ? image
                      : image.convertToFormat(QImage::Format_ARGB32);
    const int w = argb.width();
    const int h = argb.height();
    output.allocate(w, h, PixelFormat::NV12);

    auto* yPlane = output.data.data();
    auto* uvPlane = output.data.data() + static_cast<size_t>(w) * static_cast<size_t>(h);

    for (int row = 0; row < h; ++row) {
        const auto* line = reinterpret_cast<const QRgb*>(argb.constScanLine(row));
        for (int col = 0; col < w; ++col) {
            const QRgb pixel = line[col];
            const int r = qRed(pixel);
            const int g = qGreen(pixel);
            const int b = qBlue(pixel);

            const int y = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
            yPlane[row * w + col] = static_cast<uint8_t>(std::clamp(y, 0, 255));

            if ((row & 1) == 0 && (col & 1) == 0) {
                const int u = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
                const int v = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
                const int uvIndex = (row / 2) * w + col;
                uvPlane[uvIndex] = static_cast<uint8_t>(std::clamp(u, 0, 255));
                uvPlane[uvIndex + 1] = static_cast<uint8_t>(std::clamp(v, 0, 255));
            }
        }
    }

    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    output.timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
}

void blendImageOnto(QImage& canvas, const QImage& overlay, const QRect& targetRect, float opacity) {
    if (overlay.isNull() || opacity <= 0.0f || targetRect.width() <= 0 || targetRect.height() <= 0) {
        return;
    }

    QImage src = overlay.format() == QImage::Format_ARGB32
                     ? overlay
                     : overlay.convertToFormat(QImage::Format_ARGB32);

    if (src.size() != targetRect.size()) {
        src = src.scaled(targetRect.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    QPainter painter(&canvas);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setOpacity(opacity);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(targetRect.topLeft(), src);
    painter.end();
}

void applyColorCorrection(QImage& image, const FilterRenderParams& filters) {
    for (int y = 0; y < image.height(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgb p = line[x];
            float r = qRed(p) / 255.0f;
            float g = qGreen(p) / 255.0f;
            float b = qBlue(p) / 255.0f;
            int a = qAlpha(p);

            r = (r - 0.5f) * filters.contrast + 0.5f + filters.brightness;
            g = (g - 0.5f) * filters.contrast + 0.5f + filters.brightness;
            b = (b - 0.5f) * filters.contrast + 0.5f + filters.brightness;
            const float luma = 0.299f * r + 0.587f * g + 0.114f * b;
            r = luma + (r - luma) * filters.saturation;
            g = luma + (g - luma) * filters.saturation;
            b = luma + (b - luma) * filters.saturation;

            if (filters.gradeAmount > 0.001f) {
                auto grade = [&](float c) {
                    c = c + filters.lift;
                    c = std::pow(std::max(0.0f, c), 1.0f / std::max(0.01f, filters.gamma));
                    return c * filters.gain;
                };
                r = r + (grade(r) - r) * filters.gradeAmount;
                g = g + (grade(g) - g) * filters.gradeAmount;
                b = b + (grade(b) - b) * filters.gradeAmount;
            }

            if (filters.keyMode > 0) {
                const float dr = r - filters.keyR;
                const float dg = g - filters.keyG;
                const float db = b - filters.keyB;
                const float dist = std::sqrt(dr * dr + dg * dg + db * db);
                const float edge0 = std::max(filters.keySimilarity - filters.keySmoothness, 0.0f);
                const float edge1 = filters.keySimilarity + filters.keySmoothness;
                float t = 0.0f;
                if (edge1 > edge0) {
                    t = std::clamp((dist - edge0) / (edge1 - edge0), 0.0f, 1.0f);
                } else {
                    t = dist >= filters.keySimilarity ? 1.0f : 0.0f;
                }
                a = static_cast<int>(a * t);
                if (filters.keyMode == 2 && filters.keySpill > 0.0f) {
                    const float spill = (1.0f - t) * filters.keySpill;
                    const float keyedLuma = 0.299f * r + 0.587f * g + 0.114f * b;
                    r = r + (keyedLuma - r) * spill;
                    g = g + (keyedLuma - g) * spill;
                    b = b + (keyedLuma - b) * spill;
                }
            }

            line[x] = qRgba(std::clamp(static_cast<int>(r * 255.0f), 0, 255),
                            std::clamp(static_cast<int>(g * 255.0f), 0, 255),
                            std::clamp(static_cast<int>(b * 255.0f), 0, 255),
                            std::clamp(a, 0, 255));
        }
    }
}

QImage applyCropScaleScroll(const QImage& src, const FilterRenderParams& filters, double clockSec) {
    if (src.isNull()) {
        return src;
    }
    const int w = src.width();
    const int h = src.height();
    const int left = static_cast<int>(filters.cropLeft * w);
    const int top = static_cast<int>(filters.cropTop * h);
    const int right = static_cast<int>(filters.cropRight * w);
    const int bottom = static_cast<int>(filters.cropBottom * h);
    QRect region(left, top, std::max(1, w - left - right), std::max(1, h - top - bottom));
    QImage cropped = src.copy(region);
    if (std::abs(filters.scale - 1.0f) > 0.001f) {
        const int nw = std::max(1, static_cast<int>(cropped.width() * filters.scale));
        const int nh = std::max(1, static_cast<int>(cropped.height() * filters.scale));
        cropped = cropped.scaled(nw, nh, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    if (std::abs(filters.scrollSpeedX) > 0.0001f || std::abs(filters.scrollSpeedY) > 0.0001f) {
        QImage out(cropped.size(), QImage::Format_ARGB32);
        out.fill(Qt::transparent);
        const int ox = static_cast<int>(std::fmod(filters.scrollSpeedX * clockSec, 1.0) * cropped.width());
        const int oy = static_cast<int>(std::fmod(filters.scrollSpeedY * clockSec, 1.0) * cropped.height());
        QPainter p(&out);
        if (filters.scrollLoop) {
            p.drawImage(ox, oy, cropped);
            p.drawImage(ox - cropped.width(), oy, cropped);
            p.drawImage(ox, oy - cropped.height(), cropped);
            p.drawImage(ox - cropped.width(), oy - cropped.height(), cropped);
        } else {
            p.drawImage(ox, oy, cropped);
        }
        p.end();
        return out;
    }
    return cropped;
}

} // namespace

GLCompositor::GLCompositor() = default;

GLCompositor::~GLCompositor() {
    stop();
}

bool GLCompositor::start(SourceRegistry* registry,
                         ThreadSafeQueue<VideoFrame>* outputQueue,
                         ThreadSafeQueue<VideoFrame>* programPreviewQueue,
                         ThreadSafeQueue<VideoFrame>* studioPreviewQueue) {
    if (running_.load() || !registry) {
        return false;
    }

    registry_ = registry;
    outputQueue_ = outputQueue;
    programPreviewQueue_ = programPreviewQueue;
    studioPreviewQueue_ = studioPreviewQueue;
    outputWidth_ = AppSettings::instance().canvasWidth();
    outputHeight_ = AppSettings::instance().canvasHeight();
    fps_ = AppSettings::instance().fps();
    stopRequested_ = false;
    running_ = true;

    compositorThread_ = std::make_unique<std::thread>(&GLCompositor::compositorThreadFunc, this);
    return true;
}

void GLCompositor::stop() {
    stopRequested_ = true;
    if (compositorThread_ && compositorThread_->joinable()) {
        compositorThread_->join();
    }
    compositorThread_.reset();
    running_ = false;
    registry_ = nullptr;
    outputQueue_ = nullptr;
    programPreviewQueue_ = nullptr;
    studioPreviewQueue_ = nullptr;
    virtualCamOutput_ = nullptr;
}

void GLCompositor::setStudioModeEnabled(bool enabled) {
    studioModeEnabled_ = enabled;
}

void GLCompositor::setPreviewQueues(ThreadSafeQueue<VideoFrame>* programPreviewQueue,
                                    ThreadSafeQueue<VideoFrame>* studioPreviewQueue) {
    programPreviewQueue_ = programPreviewQueue;
    studioPreviewQueue_ = studioPreviewQueue;
}

void GLCompositor::setOutputQueue(ThreadSafeQueue<VideoFrame>* outputQueue) {
    outputQueue_ = outputQueue;
}

void GLCompositor::setVirtualCamOutput(VirtualCamOutput* output) {
    virtualCamOutput_ = output;
}

bool GLCompositor::initGl() {
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setVersion(3, 3);

    surface_ = std::make_unique<QOffscreenSurface>();
    surface_->setFormat(format);
    surface_->create();

    context_ = std::make_unique<QOpenGLContext>();
    context_->setFormat(format);
    if (!context_->create() || !context_->makeCurrent(surface_.get())) {
        Logger::error("Compositor: failed to create OpenGL context");
        return false;
    }

    const int w = outputWidth_.load();
    const int h = outputHeight_.load();
    fbo_ = std::make_unique<QOpenGLFramebufferObject>(
        w, h, QOpenGLFramebufferObject::CombinedDepthStencil);
    if (!fbo_->isValid()) {
        Logger::error("Compositor: failed to create framebuffer");
        return false;
    }

    nv12Shader_ = std::make_unique<QOpenGLShaderProgram>();
    nv12Shader_->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader);
    nv12Shader_->addShaderFromSourceCode(QOpenGLShader::Fragment, kNv12FragmentShader);
    if (!nv12Shader_->link()) {
        Logger::error("Compositor: NV12 shader link failed: "
                      + nv12Shader_->log().toStdString());
        return false;
    }

    rgbaShader_ = std::make_unique<QOpenGLShaderProgram>();
    rgbaShader_->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader);
    rgbaShader_->addShaderFromSourceCode(QOpenGLShader::Fragment, kRgbaFragmentShader);
    if (!rgbaShader_->link()) {
        Logger::warn("Compositor: RGBA shader link failed: " + rgbaShader_->log().toStdString());
    }

    context_->functions()->glEnable(GL_BLEND);
    context_->functions()->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Logger::info("Compositor: scene compositor initialized "
                 + std::to_string(w) + "x" + std::to_string(h) + "@"
                 + std::to_string(fps_.load()));
    return true;
}

void GLCompositor::cleanupGl() {
    rgbaShader_.reset();
    nv12Shader_.reset();
    fbo_.reset();
    if (context_) {
        context_->doneCurrent();
    }
    context_.reset();
    surface_.reset();
}

void GLCompositor::drawLayer(const VideoFrame& frame, const SourceTransform& transform, float alpha,
                             float xOffset) {
    if (!frame.isValid() || alpha <= 0.0f || frame.format == PixelFormat::RGBA32) {
        return;
    }

    const float outW = static_cast<float>(outputWidth_.load());
    const float outH = static_cast<float>(outputHeight_.load());
    const float left = ((transform.x + xOffset) / outW) * 2.0f - 1.0f;
    const float right = ((transform.x + xOffset + transform.width) / outW) * 2.0f - 1.0f;
    const float top = 1.0f - (transform.y / outH) * 2.0f;
    const float bottom = 1.0f - ((transform.y + transform.height) / outH) * 2.0f;

    const float vertices[] = {
        left,  bottom, 0.0f, 1.0f,
        right, bottom, 1.0f, 1.0f,
        left,  top,    0.0f, 0.0f,
        right, top,    1.0f, 0.0f,
    };

    QOpenGLTexture yTex(QOpenGLTexture::Target2D);
    yTex.setSize(frame.width, frame.height);
    yTex.setFormat(QOpenGLTexture::R8_UNorm);
    yTex.allocateStorage();
    yTex.setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, frame.data.data());

    QOpenGLTexture uvTex(QOpenGLTexture::Target2D);
    uvTex.setSize(frame.width / 2, frame.height / 2);
    uvTex.setFormat(QOpenGLTexture::RG8_UNorm);
    uvTex.allocateStorage();
    const size_t ySize = static_cast<size_t>(frame.width) * static_cast<size_t>(frame.height);
    uvTex.setData(QOpenGLTexture::RG, QOpenGLTexture::UInt8, frame.data.data() + ySize);

    nv12Shader_->bind();
    nv12Shader_->setUniformValue("yTexture", 0);
    nv12Shader_->setUniformValue("uvTexture", 1);
    nv12Shader_->setUniformValue("uAlpha", alpha);
    nv12Shader_->setUniformValue("uBrightness", 0.0f);
    nv12Shader_->setUniformValue("uContrast", 1.0f);
    nv12Shader_->setUniformValue("uSaturation", 1.0f);
    nv12Shader_->setUniformValue("uCrop", QVector4D(0, 0, 0, 0));
    nv12Shader_->setUniformValue("uScroll", QVector2D(0, 0));
    nv12Shader_->setUniformValue("uScale", 1.0f);
    nv12Shader_->setUniformValue("uSharpness", 0.0f);
    nv12Shader_->setUniformValue("uTexelSize", QVector2D(1.0f / frame.width, 1.0f / frame.height));
    nv12Shader_->setUniformValue("uKeyMode", 0);
    nv12Shader_->setUniformValue("uKeyColor", QVector3D(0, 1, 0));
    nv12Shader_->setUniformValue("uKeySimilarity", 0.4f);
    nv12Shader_->setUniformValue("uKeySmoothness", 0.08f);
    nv12Shader_->setUniformValue("uKeySpill", 0.0f);
    nv12Shader_->setUniformValue("uGradeAmount", 0.0f);
    nv12Shader_->setUniformValue("uLift", 0.0f);
    nv12Shader_->setUniformValue("uGamma", 1.0f);
    nv12Shader_->setUniformValue("uGain", 1.0f);
    nv12Shader_->setUniformValue("uScrollLoop", 1);
    yTex.bind(0);
    uvTex.bind(1);

    nv12Shader_->enableAttributeArray(0);
    nv12Shader_->enableAttributeArray(1);
    nv12Shader_->setAttributeArray(0, GL_FLOAT, vertices, 4, 0);
    nv12Shader_->setAttributeArray(1, GL_FLOAT, vertices + 2, 4, sizeof(float) * 4);
    context_->functions()->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    nv12Shader_->disableAttributeArray(0);
    nv12Shader_->disableAttributeArray(1);
    yTex.release();
    uvTex.release();
    nv12Shader_->release();
}

std::optional<VideoFrame> GLCompositor::delayedFrame(const std::string& sourceId, VideoFrame frame,
                                                     int delayMs) {
    if (delayMs <= 0) {
        delayQueues_.erase(sourceId);
        return frame;
    }
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const int64_t nowUs = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
    auto& queue = delayQueues_[sourceId];
    queue.push_back({nowUs, std::move(frame)});
    const int64_t delayUs = static_cast<int64_t>(delayMs) * 1000;
    std::optional<VideoFrame> ready;
    while (!queue.empty() && (nowUs - queue.front().first) >= delayUs) {
        ready = std::move(queue.front().second);
        queue.pop_front();
    }
    // Cap memory (~delay + 250ms at 60fps ≈ delayMs/16 + 15 frames).
    const size_t maxFrames =
        static_cast<size_t>(std::max(2, delayMs / 16 + 20));
    while (queue.size() > maxFrames) {
        queue.pop_front();
    }
    return ready;
}

void GLCompositor::renderScene(const Scene& scene, float alpha, float xOffset) {
    if (!registry_ || alpha <= 0.0f) {
        return;
    }

    for (const auto& src : sortedSources(scene)) {
        if (!src.isVisible) {
            continue;
        }
        ISourceProvider* provider = registry_->providerForSource(src.id);
        if (!provider || !provider->hasVideo()) {
            continue;
        }
        auto frameOpt = provider->latestVideoFrame();
        if (!frameOpt.has_value() || frameOpt->format == PixelFormat::RGBA32) {
            continue;
        }
        const FilterRenderParams filters = resolveFilters(src);
        auto delayed = delayedFrame(src.id, std::move(*frameOpt), filters.renderDelayMs);
        if (!delayed.has_value()) {
            continue;
        }
        const VideoFrame& frame = *delayed;

        const float layerAlpha = alpha * filters.opacity;
        if (layerAlpha <= 0.0f) {
            continue;
        }

        const float outW = static_cast<float>(outputWidth_.load());
        const float outH = static_cast<float>(outputHeight_.load());
        const float left = ((src.transform.x + xOffset) / outW) * 2.0f - 1.0f;
        const float right = ((src.transform.x + xOffset + src.transform.width) / outW) * 2.0f - 1.0f;
        const float top = 1.0f - (src.transform.y / outH) * 2.0f;
        const float bottom = 1.0f - ((src.transform.y + src.transform.height) / outH) * 2.0f;
        const float vertices[] = {
            left,  bottom, 0.0f, 1.0f,
            right, bottom, 1.0f, 1.0f,
            left,  top,    0.0f, 0.0f,
            right, top,    1.0f, 0.0f,
        };

        QOpenGLTexture yTex(QOpenGLTexture::Target2D);
        yTex.setSize(frame.width, frame.height);
        yTex.setFormat(QOpenGLTexture::R8_UNorm);
        yTex.allocateStorage();
        yTex.setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, frame.data.data());

        QOpenGLTexture uvTex(QOpenGLTexture::Target2D);
        uvTex.setSize(frame.width / 2, frame.height / 2);
        uvTex.setFormat(QOpenGLTexture::RG8_UNorm);
        uvTex.allocateStorage();
        const size_t ySize = static_cast<size_t>(frame.width) * static_cast<size_t>(frame.height);
        uvTex.setData(QOpenGLTexture::RG, QOpenGLTexture::UInt8, frame.data.data() + ySize);

        const float scrollX = static_cast<float>(filters.scrollSpeedX * scrollClockSec_);
        const float scrollY = static_cast<float>(filters.scrollSpeedY * scrollClockSec_);

        nv12Shader_->bind();
        nv12Shader_->setUniformValue("yTexture", 0);
        nv12Shader_->setUniformValue("uvTexture", 1);
        nv12Shader_->setUniformValue("uAlpha", layerAlpha);
        nv12Shader_->setUniformValue("uBrightness", filters.brightness);
        nv12Shader_->setUniformValue("uContrast", filters.contrast);
        nv12Shader_->setUniformValue("uSaturation", filters.saturation);
        nv12Shader_->setUniformValue(
            "uCrop", QVector4D(filters.cropLeft, filters.cropTop, filters.cropRight,
                               filters.cropBottom));
        nv12Shader_->setUniformValue("uScroll", QVector2D(scrollX, scrollY));
        nv12Shader_->setUniformValue("uScale", filters.scale);
        nv12Shader_->setUniformValue("uSharpness", filters.sharpness);
        nv12Shader_->setUniformValue(
            "uTexelSize", QVector2D(1.0f / std::max(1, frame.width), 1.0f / std::max(1, frame.height)));
        nv12Shader_->setUniformValue("uKeyMode", filters.keyMode);
        nv12Shader_->setUniformValue("uKeyColor",
                                     QVector3D(filters.keyR, filters.keyG, filters.keyB));
        nv12Shader_->setUniformValue("uKeySimilarity", filters.keySimilarity);
        nv12Shader_->setUniformValue("uKeySmoothness", filters.keySmoothness);
        nv12Shader_->setUniformValue("uKeySpill", filters.keySpill);
        nv12Shader_->setUniformValue("uGradeAmount", filters.gradeAmount);
        nv12Shader_->setUniformValue("uLift", filters.lift);
        nv12Shader_->setUniformValue("uGamma", filters.gamma);
        nv12Shader_->setUniformValue("uGain", filters.gain);
        nv12Shader_->setUniformValue("uScrollLoop", filters.scrollLoop ? 1 : 0);
        yTex.bind(0);
        uvTex.bind(1);
        nv12Shader_->enableAttributeArray(0);
        nv12Shader_->enableAttributeArray(1);
        nv12Shader_->setAttributeArray(0, GL_FLOAT, vertices, 4, 0);
        nv12Shader_->setAttributeArray(1, GL_FLOAT, vertices + 2, 4, sizeof(float) * 4);
        context_->functions()->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        nv12Shader_->disableAttributeArray(0);
        nv12Shader_->disableAttributeArray(1);
        yTex.release();
        uvTex.release();
        nv12Shader_->release();
    }
}

void GLCompositor::compositeRgbaOverlays(const Scene& scene, float alpha, QImage& canvas,
                                         float xOffset) {
    if (!registry_ || alpha <= 0.0f) {
        return;
    }

    for (const auto& src : sortedSources(scene)) {
        if (!src.isVisible) {
            continue;
        }
        ISourceProvider* provider = registry_->providerForSource(src.id);
        if (!provider || !provider->hasVideo()) {
            continue;
        }
        auto frame = provider->latestVideoFrame();
        if (!frame.has_value() || frame->format != PixelFormat::RGBA32) {
            continue;
        }

        const FilterRenderParams filters = resolveFilters(src);
        auto delayed = delayedFrame(src.id + ":rgba", std::move(*frame), filters.renderDelayMs);
        if (!delayed.has_value()) {
            continue;
        }

        const float opacity = alpha * filters.opacity;
        if (opacity <= 0.0f) {
            continue;
        }

        QImage overlay = applyCropScaleScroll(rgbaFrameToImage(*delayed), filters, scrollClockSec_);
        if (filters.maskEnabled && !filters.maskPath.empty()) {
            QImage mask(QString::fromStdString(filters.maskPath));
            if (!mask.isNull()) {
                mask = mask.convertToFormat(QImage::Format_ARGB32)
                           .scaled(overlay.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                for (int y = 0; y < overlay.height(); ++y) {
                    auto* dst = reinterpret_cast<QRgb*>(overlay.scanLine(y));
                    const auto* m = reinterpret_cast<const QRgb*>(mask.constScanLine(y));
                    for (int x = 0; x < overlay.width(); ++x) {
                        const int ma = static_cast<int>(qAlpha(m[x]) * filters.maskOpacity);
                        dst[x] = qRgba(qRed(dst[x]), qGreen(dst[x]), qBlue(dst[x]),
                                       (qAlpha(dst[x]) * ma) / 255);
                    }
                }
            }
        }
        applyColorCorrection(overlay, filters);
        const QRect target(static_cast<int>(src.transform.x + xOffset),
                           static_cast<int>(src.transform.y),
                           static_cast<int>(src.transform.width),
                           static_cast<int>(src.transform.height));
        blendImageOnto(canvas, overlay, target, opacity);
    }
}

void GLCompositor::readbackNv12(const Scene* sceneA, float alphaA, const Scene* sceneB,
                                float alphaB, VideoFrame& output, float offsetA, float offsetB) {
    QImage canvas = readFboAsImage(context_->functions(), outputWidth_.load(), outputHeight_.load());
    if (sceneA && alphaA > 0.0f) {
        compositeRgbaOverlays(*sceneA, alphaA, canvas, offsetA);
    }
    if (sceneB && alphaB > 0.0f) {
        compositeRgbaOverlays(*sceneB, alphaB, canvas, offsetB);
    }
    argbImageToNv12(canvas, output);
}

void GLCompositor::renderAndReadback(const Scene& scene, float alpha, VideoFrame& output) {
    context_->makeCurrent(surface_.get());
    fbo_->bind();

    auto* gl = context_->functions();
    const int w = outputWidth_.load();
    const int h = outputHeight_.load();
    gl->glViewport(0, 0, w, h);
    gl->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl->glClear(GL_COLOR_BUFFER_BIT);

    renderScene(scene, alpha);
    readbackNv12(&scene, alpha, nullptr, 0.0f, output);
    fbo_->release();
    gl->glFinish();
}

void GLCompositor::compositorThreadFunc() {
    if (!initGl()) {
        running_ = false;
        return;
    }

    auto& sceneManager = SceneManager::instance();
    const int fps = std::max(1, fps_.load());
    const auto frameInterval = std::chrono::microseconds(1'000'000 / fps);
    auto nextFrameTime = std::chrono::steady_clock::now();
    const auto clockStart = nextFrameTime;

    while (!stopRequested_.load()) {
        scrollClockSec_ = std::chrono::duration<double>(std::chrono::steady_clock::now() - clockStart)
                              .count();
        sceneManager.updateTransition();
        sceneManager.tickAutoSave();

        const bool studio =
            studioModeEnabled_.load() && sceneManager.isStudioModeEnabled();

        std::optional<Scene> program;
        if (studio && !sceneManager.isTransitioning()) {
            program = sceneManager.programSceneSnapshotForRender();
        } else {
            program = sceneManager.activeSceneSnapshot();
        }
        if (program.has_value()) {
            VideoFrame programFrame;
            if (sceneManager.isTransitioning()) {
                const float blend = sceneManager.transitionBlend();
                const TransitionType ttype = sceneManager.activeTransitionType();
                context_->makeCurrent(surface_.get());
                fbo_->bind();
                auto* gl = context_->functions();
                const int w = outputWidth_.load();
                gl->glViewport(0, 0, w, outputHeight_.load());
                gl->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                gl->glClear(GL_COLOR_BUFFER_BIT);

                std::optional<Scene> outgoingScene;
                if (studio) {
                    outgoingScene = sceneManager.programSceneSnapshotForRender();
                } else {
                    outgoingScene = sceneManager.sceneSnapshot(sceneManager.outgoingSceneId());
                }
                const Scene* outgoingPtr =
                    outgoingScene.has_value() ? &outgoingScene.value() : nullptr;

                float outAlpha = 1.0f - blend;
                float inAlpha = blend;
                float outOffset = 0.0f;
                float inOffset = 0.0f;

                if (ttype == TransitionType::FadeToBlack) {
                    if (blend < 0.5f) {
                        outAlpha = 1.0f - blend * 2.0f;
                        inAlpha = 0.0f;
                    } else {
                        outAlpha = 0.0f;
                        inAlpha = (blend - 0.5f) * 2.0f;
                    }
                } else if (ttype == TransitionType::Slide) {
                    outAlpha = 1.0f;
                    inAlpha = 1.0f;
                    outOffset = -blend * static_cast<float>(w);
                    inOffset = (1.0f - blend) * static_cast<float>(w);
                }

                if (outgoingScene.has_value()) {
                    renderScene(*outgoingScene, outAlpha, outOffset);
                }
                renderScene(*program, inAlpha, inOffset);
                readbackNv12(outgoingPtr, outgoingScene.has_value() ? outAlpha : 0.0f,
                             &program.value(), inAlpha, programFrame, outOffset, inOffset);
                fbo_->release();
                gl->glFinish();
            } else {
                renderAndReadback(*program, 1.0f, programFrame);
            }

            if (programFrame.isValid()) {
                if (virtualCamOutput_) {
                    virtualCamOutput_->pushFrame(programFrame);
                }
                if (programPreviewQueue_) {
                    programPreviewQueue_->push(programFrame);
                }
                if (outputQueue_) {
                    outputQueue_->push(std::move(programFrame));
                }
            }
        }

        if (studioModeEnabled_.load() && studioPreviewQueue_) {
            if (auto preview = sceneManager.previewSceneSnapshot()) {
                VideoFrame previewFrame;
                renderAndReadback(*preview, 1.0f, previewFrame);
                if (previewFrame.isValid()) {
                    studioPreviewQueue_->push(std::move(previewFrame));
                }
            }
        }

        nextFrameTime += frameInterval;
        std::this_thread::sleep_until(nextFrameTime);
    }

    cleanupGl();
    running_ = false;
}

} // namespace railshot
