#include "ui/PreviewWidget.h"

#include "core/AppSettings.h"
#include "core/models/SceneManager.h"

#include <QCursor>
#include <QEvent>
#include <QFont>
#include <QMouseEvent>
#include <QOpenGLFunctions>
#include <QPainter>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace railshot {

namespace {

int canvasWidth() {
    return AppSettings::instance().canvasWidth();
}

int canvasHeight() {
    return AppSettings::instance().canvasHeight();
}

constexpr float kMinSourceSize = 32.0f;
constexpr double kHandleDrawSize = 8.0;
constexpr double kHandleHitSize = 12.0;

QRectF contentRectForWidget(int widgetW, int widgetH) {
    const double contentAspect = static_cast<double>(canvasWidth()) / static_cast<double>(canvasHeight());
    const double widgetAspect = static_cast<double>(widgetW) / static_cast<double>(widgetH);

    if (widgetAspect > contentAspect) {
        const double drawW = widgetH * contentAspect;
        const double x = (widgetW - drawW) * 0.5;
        return QRectF(x, 0.0, drawW, static_cast<double>(widgetH));
    }

    const double drawH = widgetW / contentAspect;
    const double y = (widgetH - drawH) * 0.5;
    return QRectF(0.0, y, static_cast<double>(widgetW), drawH);
}

} // namespace

bool PreviewWidget::isCornerHandle(EditHandle handle) {
    switch (handle) {
    case EditHandle::TopLeft:
    case EditHandle::TopRight:
    case EditHandle::BottomLeft:
    case EditHandle::BottomRight:
        return true;
    default:
        return false;
    }
}

QPointF PreviewWidget::handleCenter(const QRectF& box, EditHandle handle) {
    switch (handle) {
    case EditHandle::TopLeft:
        return box.topLeft();
    case EditHandle::Top:
        return QPointF(box.center().x(), box.top());
    case EditHandle::TopRight:
        return box.topRight();
    case EditHandle::Left:
        return QPointF(box.left(), box.center().y());
    case EditHandle::Right:
        return QPointF(box.right(), box.center().y());
    case EditHandle::BottomLeft:
        return box.bottomLeft();
    case EditHandle::Bottom:
        return QPointF(box.center().x(), box.bottom());
    case EditHandle::BottomRight:
        return box.bottomRight();
    default:
        return box.center();
    }
}

PreviewWidget::PreviewWidget(QWidget* parent)
    : QOpenGLWidget(parent) {
    setMouseTracking(true);
    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &PreviewWidget::pollFrame);
    timer->start(33);
}

PreviewWidget::~PreviewWidget() = default;

void PreviewWidget::setFrameQueue(ThreadSafeQueue<VideoFrame>* queue) {
    frameQueue_ = queue;
    {
        QMutexLocker lock(&imageMutex_);
        currentImage_ = QImage();
    }
    update();
}

void PreviewWidget::setSelectedSourceId(const std::string& sourceId) {
    selectedSourceId_ = sourceId;
    update();
}

void PreviewWidget::setEditSceneId(const std::string& sceneId) {
    editSceneId_ = sceneId;
}

void PreviewWidget::setInteractionEnabled(bool enabled) {
    interactionEnabled_ = enabled;
    if (!enabled) {
        interacting_ = false;
        activeHandle_ = EditHandle::None;
        unsetCursor();
    }
}

Source* PreviewWidget::selectedSource() const {
    if (selectedSourceId_.empty()) {
        return nullptr;
    }
    if (!editSceneId_.empty()) {
        return SceneManager::instance().sourceInScene(editSceneId_, selectedSourceId_);
    }
    return SceneManager::instance().sourceById(selectedSourceId_);
}

Scene* PreviewWidget::editScene() const {
    auto& manager = SceneManager::instance();
    if (!editSceneId_.empty()) {
        return manager.sceneById(editSceneId_);
    }
    return manager.activeScene();
}

Source* PreviewWidget::hitTestSourceAt(const QPointF& widgetPos) const {
    Scene* scene = editScene();
    if (!scene) {
        return nullptr;
    }

    // Top-most first (highest zOrder wins), like OBS.
    std::vector<Source*> ordered;
    ordered.reserve(scene->sources.size());
    for (auto& src : scene->sources) {
        if (src.isVisible) {
            ordered.push_back(&src);
        }
    }
    std::sort(ordered.begin(), ordered.end(),
              [](const Source* a, const Source* b) { return a->zOrder > b->zOrder; });

    for (Source* src : ordered) {
        if (sourceBoxInWidget(src->transform).contains(widgetPos)) {
            return src;
        }
    }
    return nullptr;
}

QRectF PreviewWidget::sourceBoxInWidget(const SourceTransform& transform) const {
    const QRectF content = previewContentRect();
    const double sx = content.width() / canvasWidth();
    const double sy = content.height() / canvasHeight();
    return {content.left() + transform.x * sx, content.top() + transform.y * sy,
            transform.width * sx, transform.height * sy};
}

PreviewWidget::EditHandle PreviewWidget::hitTestHandle(const QRectF& box,
                                                       const QPointF& widgetPos) const {
    const double half = kHandleHitSize * 0.5;
    const EditHandle handles[] = {
        EditHandle::TopLeft,    EditHandle::Top,    EditHandle::TopRight, EditHandle::Left,
        EditHandle::Right,      EditHandle::BottomLeft, EditHandle::Bottom, EditHandle::BottomRight,
    };

    for (const EditHandle handle : handles) {
        const QPointF center = handleCenter(box, handle);
        const QRectF hitRect(center.x() - half, center.y() - half, kHandleHitSize, kHandleHitSize);
        if (hitRect.contains(widgetPos)) {
            return handle;
        }
    }

    if (box.contains(widgetPos)) {
        return EditHandle::Move;
    }
    return EditHandle::None;
}

void PreviewWidget::applyHandleCursor(EditHandle handle) {
    switch (handle) {
    case EditHandle::TopLeft:
    case EditHandle::BottomRight:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case EditHandle::TopRight:
    case EditHandle::BottomLeft:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case EditHandle::Top:
    case EditHandle::Bottom:
        setCursor(Qt::SizeVerCursor);
        break;
    case EditHandle::Left:
    case EditHandle::Right:
        setCursor(Qt::SizeHorCursor);
        break;
    case EditHandle::Move:
        setCursor(Qt::SizeAllCursor);
        break;
    default:
        unsetCursor();
        break;
    }
}

SourceTransform PreviewWidget::resizeFromHandle(const SourceTransform& start, EditHandle handle,
                                                float dx, float dy, bool lockAspect) const {
    SourceTransform t = start;
    const float aspect = start.height > 0.0f ? start.width / start.height : 1.0f;

    auto applyAspectCorner = [&](float& w, float& h) {
        if (!lockAspect || aspect <= 0.0f) {
            return;
        }
        if (std::fabs(dx) >= std::fabs(dy * aspect)) {
            h = w / aspect;
        } else {
            w = h * aspect;
        }
    };

    switch (handle) {
    case EditHandle::Move:
        t.x = start.x + dx;
        t.y = start.y + dy;
        break;

    case EditHandle::BottomRight: {
        float w = std::max(kMinSourceSize, start.width + dx);
        float h = std::max(kMinSourceSize, start.height + dy);
        applyAspectCorner(w, h);
        t.width = w;
        t.height = h;
        break;
    }
    case EditHandle::BottomLeft: {
        const float right = start.x + start.width;
        float w = std::max(kMinSourceSize, start.width - dx);
        float h = std::max(kMinSourceSize, start.height + dy);
        applyAspectCorner(w, h);
        t.x = right - w;
        t.width = w;
        t.height = h;
        break;
    }
    case EditHandle::TopRight: {
        const float bottom = start.y + start.height;
        float w = std::max(kMinSourceSize, start.width + dx);
        float h = std::max(kMinSourceSize, start.height - dy);
        applyAspectCorner(w, h);
        t.y = bottom - h;
        t.width = w;
        t.height = h;
        break;
    }
    case EditHandle::TopLeft: {
        const float right = start.x + start.width;
        const float bottom = start.y + start.height;
        float w = std::max(kMinSourceSize, start.width - dx);
        float h = std::max(kMinSourceSize, start.height - dy);
        applyAspectCorner(w, h);
        t.x = right - w;
        t.y = bottom - h;
        t.width = w;
        t.height = h;
        break;
    }
    case EditHandle::Right: {
        float w = std::max(kMinSourceSize, start.width + dx);
        t.width = w;
        if (lockAspect) {
            t.height = std::max(kMinSourceSize, w / aspect);
        }
        break;
    }
    case EditHandle::Left: {
        const float right = start.x + start.width;
        float w = std::max(kMinSourceSize, start.width - dx);
        t.x = right - w;
        t.width = w;
        if (lockAspect) {
            t.height = std::max(kMinSourceSize, w / aspect);
        }
        break;
    }
    case EditHandle::Bottom: {
        float h = std::max(kMinSourceSize, start.height + dy);
        t.height = h;
        if (lockAspect) {
            t.width = std::max(kMinSourceSize, h * aspect);
        }
        break;
    }
    case EditHandle::Top: {
        const float bottom = start.y + start.height;
        float h = std::max(kMinSourceSize, start.height - dy);
        t.y = bottom - h;
        t.height = h;
        if (lockAspect) {
            t.width = std::max(kMinSourceSize, h * aspect);
        }
        break;
    }
    default:
        break;
    }

    return t;
}

void PreviewWidget::initializeGL() {
    QOpenGLFunctions* gl = QOpenGLWidget::context()->functions();
    gl->glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
}

void PreviewWidget::pollFrame() {
    if (!frameQueue_) {
        return;
    }

    VideoFrame latest;
    bool gotFrame = false;
    while (auto frame = frameQueue_->pop(0)) {
        latest = std::move(*frame);
        gotFrame = true;
    }

    if (gotFrame && latest.isValid()) {
        uploadNv12Frame(latest);
        update();
    }
}

void PreviewWidget::uploadNv12Frame(const VideoFrame& frame) {
    const int w = frame.width;
    const int h = frame.height;
    QImage image(w, h, QImage::Format_RGB888);

    const auto* src = frame.data.data();
    const int ySize = w * h;
    const auto* uvPlane = src + ySize;

    for (int row = 0; row < h; ++row) {
        auto* dst = image.scanLine(row);
        for (int col = 0; col < w; ++col) {
            const int y = src[row * w + col];
            const int uvIndex = (row / 2) * w + (col & ~1);
            const int u = uvPlane[uvIndex] - 128;
            const int v = uvPlane[uvIndex + 1] - 128;

            const int r = std::clamp(y + ((359 * v) >> 8), 0, 255);
            const int g = std::clamp(y - ((88 * u + 183 * v) >> 8), 0, 255);
            const int b = std::clamp(y + ((454 * u) >> 8), 0, 255);

            dst[col * 3 + 0] = static_cast<uchar>(r);
            dst[col * 3 + 1] = static_cast<uchar>(g);
            dst[col * 3 + 2] = static_cast<uchar>(b);
        }
    }

    QMutexLocker lock(&imageMutex_);
    currentImage_ = std::move(image);
}

QRectF PreviewWidget::previewContentRect() const {
    return contentRectForWidget(std::max(width(), 1), std::max(height(), 1));
}

QPointF PreviewWidget::mapToCanvas(const QPoint& pos) const {
    const QRectF content = previewContentRect();
    const double x = (pos.x() - content.left()) / content.width() * canvasWidth();
    const double y = (pos.y() - content.top()) / content.height() * canvasHeight();
    return {x, y};
}

void PreviewWidget::mousePressEvent(QMouseEvent* event) {
    if (!interactionEnabled_ || event->button() != Qt::LeftButton) {
        return;
    }

    // Click selects the topmost source under the cursor (OBS behavior).
    Source* hit = hitTestSourceAt(event->position());
    if (!hit) {
        if (!selectedSourceId_.empty()) {
            selectedSourceId_.clear();
            emit sourceSelected({});
            update();
        }
        return;
    }

    if (hit->id != selectedSourceId_) {
        selectedSourceId_ = hit->id;
        emit sourceSelected(selectedSourceId_);
        update();
    }

    if (hit->locked) {
        return;
    }

    const QRectF box = sourceBoxInWidget(hit->transform);
    const EditHandle handle = hitTestHandle(box, event->position());
    if (handle == EditHandle::None) {
        return;
    }

    interacting_ = true;
    activeHandle_ = handle;
    dragStartMousePos_ = event->pos();
    dragStartTransform_ = hit->transform;
}

void PreviewWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!interactionEnabled_) {
        return;
    }

    Source* src = selectedSource();
    if (!interacting_) {
        // Prefer the currently selected source for handle cursors; otherwise whatever is under the mouse.
        Source* hoverSrc = src;
        if (!hoverSrc || !sourceBoxInWidget(hoverSrc->transform).contains(event->position())) {
            hoverSrc = hitTestSourceAt(event->position());
        }
        if (!hoverSrc || hoverSrc->locked) {
            if (hoverHandle_ != EditHandle::None) {
                hoverHandle_ = EditHandle::None;
                unsetCursor();
            }
            return;
        }

        const EditHandle handle = hitTestHandle(sourceBoxInWidget(hoverSrc->transform), event->position());
        if (handle != hoverHandle_) {
            hoverHandle_ = handle;
            applyHandleCursor(handle);
        }
        return;
    }

    if (!src || src->locked) {
        return;
    }

    const QPointF startCanvas = mapToCanvas(dragStartMousePos_);
    const QPointF currentCanvas = mapToCanvas(event->pos());
    const float dx = static_cast<float>(currentCanvas.x() - startCanvas.x());
    const float dy = static_cast<float>(currentCanvas.y() - startCanvas.y());
    const bool lockAspect =
        (event->modifiers() & Qt::ShiftModifier) && isCornerHandle(activeHandle_);

    src->transform =
        resizeFromHandle(dragStartTransform_, activeHandle_, dx, dy, lockAspect);

    update();
    emit sourceTransformChanged(selectedSourceId_);
}

void PreviewWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        interacting_ = false;
        activeHandle_ = EditHandle::None;
        dragStartTransform_ = {};
    }
}

void PreviewWidget::leaveEvent(QEvent* /*event*/) {
    hoverHandle_ = EditHandle::None;
    unsetCursor();
}

void PreviewWidget::drawSelectionHandles(QPainter& painter, const QRectF& box) const {
    const EditHandle handles[] = {
        EditHandle::TopLeft,    EditHandle::Top,    EditHandle::TopRight, EditHandle::Left,
        EditHandle::Right,      EditHandle::BottomLeft, EditHandle::Bottom, EditHandle::BottomRight,
    };

    const double half = kHandleDrawSize * 0.5;
    painter.setPen(QPen(QColor(59, 130, 246), 1));
    painter.setBrush(Qt::white);

    for (const EditHandle handle : handles) {
        const QPointF center = handleCenter(box, handle);
        painter.drawRect(QRectF(center.x() - half, center.y() - half, kHandleDrawSize, kHandleDrawSize));
    }
}

void PreviewWidget::paintGL() {
    QOpenGLFunctions* gl = context()->functions();
    gl->glClear(GL_COLOR_BUFFER_BIT);

    QImage imageCopy;
    {
        QMutexLocker lock(&imageMutex_);
        imageCopy = currentImage_;
    }

    QPainter painter(this);
    painter.fillRect(rect(), QColor(12, 13, 18));

    if (imageCopy.isNull()) {
        painter.setPen(QColor(90, 98, 112));
        QFont font = painter.font();
        font.setPointSize(15);
        font.setWeight(QFont::DemiBold);
        painter.setFont(font);
        painter.drawText(rect(), Qt::AlignCenter,
                         "No Signal\nAdd a source and start the stream to preview");
        return;
    }

    const QRectF content = previewContentRect();
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(content, imageCopy);

    if (!interactionEnabled_) {
        return;
    }

    Source* src = selectedSource();
    if (!src) {
        return;
    }

    const QRectF box = sourceBoxInWidget(src->transform);
    painter.setPen(QPen(QColor(59, 130, 246), 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(box);
    drawSelectionHandles(painter, box);
}

} // namespace railshot
