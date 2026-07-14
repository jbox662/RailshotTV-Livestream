#include "ui/VenueMode.h"

#include "core/ProductionProfile.h"
#include "core/models/SceneManager.h"
#include "score/ScoreManager.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace railshot {

VenueModeWindow::VenueModeWindow(StreamController* controller, QWidget* parent)
    : QMainWindow(parent)
    , controller_(controller) {
    setWindowTitle("RailShot — Venue Mode");
    resize(900, 600);

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(16);

    auto* title = new QLabel("Venue Mode", central);
    title->setStyleSheet("font-size: 22px; font-weight: 700;");
    root->addWidget(title);

    auto* subtitle = new QLabel("Select a scene to go live. Optional billiard score presets apply on switch.",
                                central);
    subtitle->setWordWrap(true);
    subtitle->setObjectName("panelTitle");
    root->addWidget(subtitle);

    auto* urlRow = new QHBoxLayout();
    urlRow->addWidget(new QLabel("RTMP URL", central));
    rtmpUrlEdit_ = new QLineEdit(central);
    rtmpUrlEdit_->setPlaceholderText("rtmp://a.rtmp.youtube.com/live2/YOUR_STREAM_KEY");
    urlRow->addWidget(rtmpUrlEdit_, 1);
    root->addLayout(urlRow);

    billiardPresetCheck_ = new QCheckBox("Apply billiard score preset when switching scenes", central);
    root->addWidget(billiardPresetCheck_);

    gridHost_ = new QWidget(central);
    grid_ = new QGridLayout(gridHost_);
    grid_->setSpacing(12);
    root->addWidget(gridHost_, 1);

    auto* stopBtn = new QPushButton("Stop Stream & Exit Venue Mode", central);
    stopBtn->setObjectName("liveButton");
    connect(stopBtn, &QPushButton::clicked, this, &VenueModeWindow::onStop);
    root->addWidget(stopBtn);

    setCentralWidget(central);
    rebuildSceneButtons();
}

void VenueModeWindow::rebuildSceneButtons() {
    while (QLayoutItem* item = grid_->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    auto& manager = SceneManager::instance();
    int col = 0;
    int row = 0;
    const int columns = 3;

    for (const auto& scene : manager.collection().scenes) {
        auto* btn = new QPushButton(QString::fromStdString(scene.name), gridHost_);
        btn->setMinimumHeight(80);
        btn->setProperty("sceneId", QString::fromStdString(scene.id));
        connect(btn, &QPushButton::clicked, this, &VenueModeWindow::onSceneGoLive);
        grid_->addWidget(btn, row, col);
        col++;
        if (col >= columns) {
            col = 0;
            row++;
        }
    }
}

void VenueModeWindow::onSceneGoLive() {
    const auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn || !controller_) {
        return;
    }

    const std::string sceneId = btn->property("sceneId").toString().toStdString();
    controller_->setActiveScene(sceneId);

    if (billiardPresetCheck_->isChecked()) {
        ScoreManager::instance().applyBilliardPreset(GameType::NineBall);
        ScoreManager::instance().setProfile(ProductionProfile::SportsBilliards);
    }

    const QString url = rtmpUrlEdit_->text().trimmed();
    if (url.isEmpty()) {
        QMessageBox::warning(this, "RTMP URL", "Enter an RTMP URL before going live.");
        return;
    }

    if (controller_->isStreaming()) {
        controller_->stopStream();
    }

    if (!controller_->setRtmpUrl(url.toStdString())) {
        QMessageBox::critical(this, "Error", "Invalid RTMP URL.");
        return;
    }

    if (!controller_->startStream()) {
        QMessageBox::critical(this, "Error", "Failed to start stream.");
        return;
    }
}

void VenueModeWindow::onStop() {
    if (controller_) {
        controller_->stopStream();
    }
    close();
}

void VenueModeWindow::closeEvent(QCloseEvent* event) {
    if (controller_ && controller_->isStreaming()) {
        controller_->stopStream();
    }
    QMainWindow::closeEvent(event);
}

} // namespace railshot
