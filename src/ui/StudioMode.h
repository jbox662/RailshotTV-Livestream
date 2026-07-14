#pragma once

#include "core/StreamController.h"

#include <QPushButton>
#include <QWidget>

namespace railshot {

class PreviewWidget;

class StudioModeWidget : public QWidget {
    Q_OBJECT

public:
    explicit StudioModeWidget(StreamController* controller, QWidget* parent = nullptr);

    void setStudioModeEnabled(bool enabled);
    [[nodiscard]] bool isStudioModeEnabled() const { return studioModeEnabled_; }

    void initializeMonitors();

    PreviewWidget* previewMonitor() const { return previewMonitor_; }
    PreviewWidget* programMonitor() const { return programMonitor_; }
    PreviewWidget* singleMonitor() const { return singleMonitor_; }

signals:
    void studioModeToggled(bool enabled);
    void transitionClicked();

private slots:
    void onTransition();

private:
    void rebuildLayout();

    StreamController* controller_ = nullptr;
    PreviewWidget* previewMonitor_ = nullptr;
    PreviewWidget* programMonitor_ = nullptr;
    PreviewWidget* singleMonitor_ = nullptr;
    QPushButton* transitionBtn_ = nullptr;
    QWidget* dualContainer_ = nullptr;
    QWidget* singleContainer_ = nullptr;
    bool studioModeEnabled_ = false;
};

} // namespace railshot
