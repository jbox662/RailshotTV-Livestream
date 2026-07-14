#include "ui/StudioMode.h"
#include "ui/PreviewWidget.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace railshot {

namespace {

QWidget* wrapPreview(const QString& labelText, PreviewWidget* monitor, QWidget* parent) {
    auto* column = new QWidget(parent);
    auto* layout = new QVBoxLayout(column);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    auto* label = new QLabel(labelText, column);
    label->setObjectName("rsPreviewLabel");
    layout->addWidget(label);

    auto* frame = new QFrame(column);
    frame->setObjectName("rsPreviewFrame");
    frame->setFrameShape(QFrame::StyledPanel);
    auto* frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(1, 1, 1, 1);
    frameLayout->setSpacing(0);
    monitor->setMinimumHeight(320);
    frameLayout->addWidget(monitor, 1);
    layout->addWidget(frame, 1);

    return column;
}

} // namespace

StudioModeWidget::StudioModeWidget(StreamController* controller, QWidget* parent)
    : QWidget(parent)
    , controller_(controller) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(4);

    singleContainer_ = new QWidget(this);
    auto* singleLayout = new QVBoxLayout(singleContainer_);
    singleLayout->setContentsMargins(0, 0, 0, 0);
    singleLayout->setSpacing(2);

    auto* singleLabel = new QLabel("Preview", singleContainer_);
    singleLabel->setObjectName("rsPreviewLabel");
    singleLayout->addWidget(singleLabel);

    auto* singleFrame = new QFrame(singleContainer_);
    singleFrame->setObjectName("rsPreviewFrame");
    singleFrame->setFrameShape(QFrame::StyledPanel);
    auto* singleFrameLayout = new QVBoxLayout(singleFrame);
    singleFrameLayout->setContentsMargins(1, 1, 1, 1);
    singleMonitor_ = new PreviewWidget(singleFrame);
    singleMonitor_->setMinimumHeight(360);
    singleFrameLayout->addWidget(singleMonitor_, 1);
    singleLayout->addWidget(singleFrame, 1);

    auto* singleBar = new QHBoxLayout();
    singleBar->setContentsMargins(4, 0, 4, 0);
    auto* scaleLabel = new QLabel("Scale:", singleContainer_);
    scaleLabel->setObjectName("rsPreviewMeta");
    singleBar->addWidget(scaleLabel);
    auto* scaleCombo = new QComboBox(singleContainer_);
    scaleCombo->setObjectName("rsPreviewScale");
    scaleCombo->addItems({"Scale to Window", "50%", "75%", "100%"});
    scaleCombo->setCurrentIndex(0);
    scaleCombo->setMaximumWidth(160);
    singleBar->addWidget(scaleCombo);
    singleBar->addStretch();
    singleLayout->addLayout(singleBar);

    root->addWidget(singleContainer_, 1);

    dualContainer_ = new QWidget(this);
    auto* dualLayout = new QHBoxLayout(dualContainer_);
    dualLayout->setContentsMargins(0, 0, 0, 0);
    dualLayout->setSpacing(6);

    previewMonitor_ = new PreviewWidget(dualContainer_);
    programMonitor_ = new PreviewWidget(dualContainer_);
    dualLayout->addWidget(wrapPreview("Preview", previewMonitor_, dualContainer_), 1);

    auto* transitionCol = new QVBoxLayout();
    transitionCol->addStretch();
    transitionBtn_ = new QPushButton("Transition", dualContainer_);
    transitionBtn_->setObjectName("rsTransitionButton");
    transitionBtn_->setMinimumWidth(96);
    transitionBtn_->setMinimumHeight(48);
    connect(transitionBtn_, &QPushButton::clicked, this, &StudioModeWidget::onTransition);
    transitionCol->addWidget(transitionBtn_);
    transitionCol->addStretch();
    dualLayout->addLayout(transitionCol);

    dualLayout->addWidget(wrapPreview("Program", programMonitor_, dualContainer_), 1);

    dualContainer_->hide();
    root->addWidget(dualContainer_, 1);

    initializeMonitors();
}

void StudioModeWidget::initializeMonitors() {
    if (!controller_) {
        return;
    }
    singleMonitor_->setFrameQueue(&controller_->programPreviewQueue());
    previewMonitor_->setFrameQueue(&controller_->studioPreviewQueue());
    programMonitor_->setFrameQueue(&controller_->programPreviewQueue());
}

void StudioModeWidget::setStudioModeEnabled(bool enabled) {
    if (studioModeEnabled_ == enabled) {
        return;
    }
    studioModeEnabled_ = enabled;
    rebuildLayout();
}

void StudioModeWidget::rebuildLayout() {
    if (!controller_) {
        return;
    }

    controller_->ensurePreviewEngine();

    if (studioModeEnabled_) {
        singleContainer_->hide();
        dualContainer_->show();
        previewMonitor_->setFrameQueue(&controller_->studioPreviewQueue());
        programMonitor_->setFrameQueue(&controller_->programPreviewQueue());
        previewMonitor_->setInteractionEnabled(true);
        programMonitor_->setInteractionEnabled(false);
    } else {
        dualContainer_->hide();
        singleContainer_->show();
        singleMonitor_->setFrameQueue(&controller_->programPreviewQueue());
    }
}

void StudioModeWidget::onTransition() {
    if (controller_) {
        controller_->transitionPreviewToProgram();
    }
    emit transitionClicked();
}

} // namespace railshot
