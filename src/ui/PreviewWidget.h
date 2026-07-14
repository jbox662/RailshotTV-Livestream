#pragma once

#include "core/FrameData.h"
#include "core/ThreadSafeQueue.h"
#include "core/models/SourceTypes.h"

#include <QOpenGLWidget>
#include <QImage>
#include <QMutex>
#include <QPoint>
#include <QRectF>

class QPainter;
class QEvent;

namespace railshot {

class PreviewWidget : public QOpenGLWidget {
    Q_OBJECT

public:
    explicit PreviewWidget(QWidget* parent = nullptr);
    ~PreviewWidget() override;

    void setFrameQueue(ThreadSafeQueue<VideoFrame>* queue);
    void setSelectedSourceId(const std::string& sourceId);
    void setEditSceneId(const std::string& sceneId);
    void setInteractionEnabled(bool enabled);

public slots:
    void pollFrame();

signals:
    void sourceTransformChanged(const std::string& sourceId);
    void sourceSelected(const std::string& sourceId);

protected:
    void paintGL() override;
    void initializeGL() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    enum class EditHandle {
        None,
        Move,
        TopLeft,
        Top,
        TopRight,
        Left,
        Right,
        BottomLeft,
        Bottom,
        BottomRight
    };

    Source* selectedSource() const;
    Scene* editScene() const;
    Source* hitTestSourceAt(const QPointF& widgetPos) const;
    QRectF sourceBoxInWidget(const SourceTransform& transform) const;
    EditHandle hitTestHandle(const QRectF& box, const QPointF& widgetPos) const;
    void applyHandleCursor(EditHandle handle);
    SourceTransform resizeFromHandle(const SourceTransform& start, EditHandle handle, float dx,
                                     float dy, bool lockAspect) const;
    static bool isCornerHandle(EditHandle handle);
    static QPointF handleCenter(const QRectF& box, EditHandle handle);
    void uploadNv12Frame(const VideoFrame& frame);
    QPointF mapToCanvas(const QPoint& pos) const;
    QRectF previewContentRect() const;
    void drawSelectionHandles(QPainter& painter, const QRectF& box) const;

    ThreadSafeQueue<VideoFrame>* frameQueue_ = nullptr;
    QImage currentImage_;
    QMutex imageMutex_;

    std::string selectedSourceId_;
    std::string editSceneId_;
    bool interactionEnabled_ = true;
    bool interacting_ = false;
    EditHandle activeHandle_ = EditHandle::None;
    EditHandle hoverHandle_ = EditHandle::None;
    QPoint dragStartMousePos_;
    SourceTransform dragStartTransform_;
};

} // namespace railshot
