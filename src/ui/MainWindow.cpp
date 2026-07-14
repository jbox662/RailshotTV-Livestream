#include "ui/MainWindow.h"
#include "ui/PreviewWidget.h"
#include "ui/AudioMixer.h"
#include "ui/DockPanel.h"
#include "ui/StudioMode.h"
#include "ui/ScoreboardWidget.h"
#include "ui/AddSourceDialog.h"
#include "ui/BrowserSourceConfigDialog.h"
#include "ui/ColorSourceConfigDialog.h"
#include "ui/FiltersDialog.h"
#include "ui/MediaSourceConfigDialog.h"
#include "ui/ScoreboardConfigDialog.h"
#include "ui/SettingsDialog.h"
#include "ui/TextSourceConfigDialog.h"
#include "ui/VenueMode.h"

#include "capture/NdiFinder.h"
#include "capture/BrowserSource.h"
#include "capture/BrowserSourceSettings.h"
#include "capture/ColorSource.h"
#include "capture/MediaSource.h"
#include "capture/MediaSourceSettings.h"
#include "capture/DxgiMonitorCapture.h"
#include "capture/ScoreboardSource.h"
#include "capture/TextSource.h"
#include "capture/WasapiAudioCapture.h"
#include "capture/WindowBitbltCapture.h"
#include "core/AppSettings.h"
#include "core/ProductionProfile.h"
#include "core/RemoteCommandBus.h"
#include "core/models/SceneManager.h"
#include "core/models/SourceTypes.h"
#include "network/LocalWebServer.h"
#include "network/RemoteWebSocketServer.h"
#include "network/RailShotApiClient.h"
#include "score/ScoreManager.h"
#include "score/ScoreboardStyle.h"

#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonObject>
#include <QKeySequence>
#include <QListWidgetItem>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QShortcut>
#include <QSize>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <chrono>
#include <functional>
#include <utility>
#include <vector>

namespace railshot {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , controller_(std::make_unique<StreamController>()) {
    setWindowTitle("RailShot TV Broadcaster");
    resize(1280, 720);
    setMinimumSize(1024, 640);

    webServer_ = std::make_unique<LocalWebServer>(this);
    webServer_->start(8080);
    wsServer_ = std::make_unique<RemoteWebSocketServer>(this);
    wsServer_->start(4455);

    RemoteCommandBus::instance().attach(
        controller_.get(),
        [this]() {
            return rtmpUrlEdit_ ? rtmpUrlEdit_->text().trimmed().toStdString()
                                : AppSettings::instance().defaultRtmpUrl();
        },
        [this]() {
            QMetaObject::invokeMethod(this, "syncControlsFromController", Qt::QueuedConnection);
        });

    ScoreManager::instance().setOnStateChanged([this]() {
        QMetaObject::invokeMethod(this, "onScoreStateChanged", Qt::QueuedConnection);
    });

    setupUi();
    syncActiveCollectionFromSettings();
    refreshCollectionsCombo();
    restoreProductionProfile();
    assignDefaultWebcam();
    refreshUi();
    bindHotkeys();

    SceneManager::instance().setOnScenesChanged([this]() {
        QMetaObject::invokeMethod(this, "onScenesChanged", Qt::QueuedConnection);
    });

    connect(studioMode_, &StudioModeWidget::transitionClicked,
            this, &MainWindow::onTransition);

    connect(studioMode_->previewMonitor(), &PreviewWidget::sourceTransformChanged, this,
            [this](const std::string&) {
                studioMode_->previewMonitor()->update();
            });

    auto syncListFromPreview = [this](const std::string& sourceId) {
        if (!sourcesList_) {
            return;
        }
        sourcesList_->blockSignals(true);
        if (sourceId.empty()) {
            sourcesList_->clearSelection();
            sourcesList_->setCurrentRow(-1);
        } else {
            for (int i = 0; i < sourcesList_->count(); ++i) {
                auto* item = sourcesList_->item(i);
                if (item->data(Qt::UserRole).toString().toStdString() == sourceId) {
                    sourcesList_->setCurrentItem(item);
                    break;
                }
            }
        }
        sourcesList_->blockSignals(false);
        updateMonitorSelection(sourceId);
        updateSourceContextBar();
    };
    connect(studioMode_->singleMonitor(), &PreviewWidget::sourceSelected, this, syncListFromPreview);
    connect(studioMode_->previewMonitor(), &PreviewWidget::sourceSelected, this, syncListFromPreview);

    statsTimer_ = new QTimer(this);
    connect(statsTimer_, &QTimer::timeout, this, &MainWindow::updateStats);
    statsTimer_->start(1000);
}

MainWindow::~MainWindow() {
    controller_->stopStream();
    controller_->stopPreviewEngine();
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    if (!previewEngineRequested_) {
        previewEngineRequested_ = true;
        QTimer::singleShot(250, this, &MainWindow::startPreviewDeferred);
    }
}

void MainWindow::startPreviewDeferred() {
    controller_->ensurePreviewEngine();
    ScoreboardSource::refreshAll();
}

void MainWindow::setupUi() {
    auto* central = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    studioMode_ = new StudioModeWidget(controller_.get(), central);
    mainLayout->addWidget(studioMode_, 1);

    // Contextual strip above docks
    auto* contextBar = new QWidget(central);
    contextBar->setObjectName("rsContextBar");
    auto* contextLayout = new QHBoxLayout(contextBar);
    contextLayout->setContentsMargins(6, 4, 8, 4);
    contextLayout->setSpacing(8);
    sourceContextLabel_ = new QLabel("No source selected", contextBar);
    sourceContextLabel_->setObjectName("rsContextLabel");
    contextLayout->addWidget(sourceContextLabel_, 1);

    rtmpUrlEdit_ = new QLineEdit(contextBar);
    rtmpUrlEdit_->setPlaceholderText("RTMP URL / stream key");
    rtmpUrlEdit_->setText(QString::fromStdString(AppSettings::instance().defaultRtmpUrl()));
    rtmpUrlEdit_->setMinimumWidth(180);
    rtmpUrlEdit_->setMaximumWidth(320);
    contextLayout->addWidget(rtmpUrlEdit_, 1);
    connect(rtmpUrlEdit_, &QLineEdit::editingFinished, this, [this]() {
        AppSettings::instance().setDefaultRtmpUrl(rtmpUrlEdit_->text().trimmed().toStdString());
    });

    profileCombo_ = new QComboBox(contextBar);
    profileCombo_->addItem("General Production", static_cast<int>(ProductionProfile::General));
    profileCombo_->addItem("Sports / Billiards", static_cast<int>(ProductionProfile::SportsBilliards));
    profileCombo_->setMinimumWidth(140);
    connect(profileCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::onProfileChanged);
    contextLayout->addWidget(profileCombo_);

    sourcePropertiesBtn_ = new QPushButton("Properties", contextBar);
    sourcePropertiesBtn_->setObjectName("rsContextButton");
    sourcePropertiesBtn_->setEnabled(false);
    connect(sourcePropertiesBtn_, &QPushButton::clicked, this, &MainWindow::onConfigureSource);
    contextLayout->addWidget(sourcePropertiesBtn_);
    mainLayout->addWidget(contextBar);

    auto* bottomSplitter = new QSplitter(Qt::Horizontal, central);
    bottomSplitter_ = bottomSplitter;
    bottomSplitter->setObjectName("rsBottomDockStrip");
    bottomSplitter->setChildrenCollapsible(false);
    bottomSplitter->setHandleWidth(1);
    bottomSplitter->setMinimumHeight(220);
    bottomSplitter->setMaximumHeight(280);

    scenesDock_ = new DockPanel("Scenes", bottomSplitter);
    auto* scenesDock = scenesDock_;
    auto* scenesHost = new QWidget(scenesDock);
    auto* scenesLayout = new QVBoxLayout(scenesHost);
    scenesLayout->setContentsMargins(0, 0, 0, 0);
    scenesLayout->setSpacing(4);

    auto* collectionsRow = new QWidget(scenesHost);
    auto* collectionsLayout = new QHBoxLayout(collectionsRow);
    collectionsLayout->setContentsMargins(4, 4, 4, 0);
    collectionsLayout->setSpacing(4);
    collectionCombo_ = new QComboBox(collectionsRow);
    collectionCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    collectionsLayout->addWidget(collectionCombo_, 1);
    auto* newCollectionBtn = new QToolButton(collectionsRow);
    newCollectionBtn->setText("+");
    newCollectionBtn->setToolTip(QStringLiteral("New collection"));
    auto* renameCollectionBtn = new QToolButton(collectionsRow);
    renameCollectionBtn->setText(QStringLiteral("✎"));
    renameCollectionBtn->setToolTip(QStringLiteral("Rename collection"));
    auto* deleteCollectionBtn = new QToolButton(collectionsRow);
    deleteCollectionBtn->setText(QStringLiteral("−"));
    deleteCollectionBtn->setToolTip(QStringLiteral("Delete collection"));
    auto* dupCollectionBtn = new QToolButton(collectionsRow);
    dupCollectionBtn->setText(QStringLiteral("⧉"));
    dupCollectionBtn->setToolTip(QStringLiteral("Duplicate collection"));
    collectionsLayout->addWidget(newCollectionBtn);
    collectionsLayout->addWidget(renameCollectionBtn);
    collectionsLayout->addWidget(dupCollectionBtn);
    collectionsLayout->addWidget(deleteCollectionBtn);
    scenesLayout->addWidget(collectionsRow);

    scenesList_ = new QListWidget(scenesHost);
    scenesList_->setFrameShape(QFrame::NoFrame);
    scenesList_->setUniformItemSizes(true);
    connect(scenesList_, &QListWidget::currentRowChanged, this, &MainWindow::onSceneSelectionChanged);
    scenesLayout->addWidget(scenesList_, 1);
    scenesDock->contentLayout()->addWidget(scenesHost);
    connect(scenesDock->addFooterButton("+", "Add Scene"), &QToolButton::clicked, this,
            &MainWindow::onAddScene);
    connect(scenesDock->addFooterButton("−", "Remove Scene"), &QToolButton::clicked, this,
            &MainWindow::onRemoveScene);

    connect(collectionCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::onCollectionComboChanged);
    connect(newCollectionBtn, &QToolButton::clicked, this, &MainWindow::onNewCollection);
    connect(renameCollectionBtn, &QToolButton::clicked, this, &MainWindow::onRenameCollection);
    connect(dupCollectionBtn, &QToolButton::clicked, this, &MainWindow::onDuplicateCollection);
    connect(deleteCollectionBtn, &QToolButton::clicked, this, &MainWindow::onDeleteCollection);

    sourcesDock_ = new DockPanel("Sources", bottomSplitter);
    auto* sourcesDock = sourcesDock_;
    sourcesList_ = new QListWidget(sourcesDock);
    sourcesList_->setFrameShape(QFrame::NoFrame);
    sourcesList_->setUniformItemSizes(false);
    connect(sourcesList_, &QListWidget::currentRowChanged, this, &MainWindow::onSourceSelectionChanged);
    connect(sourcesList_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        onConfigureSource();
    });
    sourcesDock->contentLayout()->addWidget(sourcesList_);
    connect(sourcesDock->addFooterButton("+", "Add Source"), &QToolButton::clicked, this,
            &MainWindow::onAddSource);
    connect(sourcesDock->addFooterButton("−", "Remove Source"), &QToolButton::clicked, this,
            &MainWindow::onRemoveSource);
    connect(sourcesDock->addFooterButton("↑", "Move Source Up"), &QToolButton::clicked, this,
            &MainWindow::onMoveSourceUp);
    connect(sourcesDock->addFooterButton("↓", "Move Source Down"), &QToolButton::clicked, this,
            &MainWindow::onMoveSourceDown);
    connect(sourcesDock->addFooterButton("⚙", "Source Properties"), &QToolButton::clicked, this,
            &MainWindow::onConfigureSource);
    connect(sourcesDock->addFooterButton("ƒ", "Filters"), &QToolButton::clicked, this,
            &MainWindow::onEditFilters);

    audioMixer_ = new AudioMixerWidget(controller_.get(), bottomSplitter);

    transitionsDock_ = new DockPanel("Scene Transitions", bottomSplitter);
    auto* transitionsDock = transitionsDock_;
    auto* transitionBody = new QWidget(transitionsDock);
    auto* transitionLayout = new QVBoxLayout(transitionBody);
    transitionLayout->setContentsMargins(8, 8, 8, 8);
    transitionLayout->setSpacing(6);
    auto* transitionField = new QLabel("Transition", transitionBody);
    transitionField->setObjectName("rsFieldLabel");
    transitionCombo_ = new QComboBox(transitionBody);
    transitionCombo_->addItem("Fade", static_cast<int>(TransitionType::Fade));
    transitionCombo_->addItem("Cut", static_cast<int>(TransitionType::Cut));
    transitionCombo_->addItem("Slide", static_cast<int>(TransitionType::Slide));
    transitionCombo_->addItem("Fade to Black", static_cast<int>(TransitionType::FadeToBlack));
    transitionLayout->addWidget(transitionField);
    transitionLayout->addWidget(transitionCombo_);
    auto* durationField = new QLabel("Duration", transitionBody);
    durationField->setObjectName("rsFieldLabel");
    transitionDurationSpin_ = new QSpinBox(transitionBody);
    transitionDurationSpin_->setRange(0, 5000);
    transitionDurationSpin_->setSingleStep(50);
    transitionDurationSpin_->setSuffix(" ms");
    transitionDurationSpin_->setValue(SceneManager::kDefaultFadeDurationMs);
    transitionLayout->addWidget(durationField);
    transitionLayout->addWidget(transitionDurationSpin_);
    transitionLayout->addStretch();
    transitionsDock->contentLayout()->addWidget(transitionBody);
    connect(transitionCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                const auto type = static_cast<TransitionType>(transitionCombo_->itemData(index).toInt());
                Scene* scene = SceneManager::instance().sceneById(sourcesSceneId());
                if (scene) {
                    scene->transitionType = type;
                }
            });
    connect(transitionDurationSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [](int ms) { SceneManager::instance().setFadeDurationMs(ms); });

    scoreDock_ = new DockPanel("Scoreboard", bottomSplitter);
    auto* scoreDock = scoreDock_;
    scoreboard_ = new ScoreboardWidget(scoreDock);
    scoreDock->contentLayout()->addWidget(scoreboard_);

    controlsDock_ = new DockPanel("Controls", bottomSplitter);
    auto* controlsDock = controlsDock_;
    auto* controlsHost = new QWidget(controlsDock);
    auto* controlsLayout = new QVBoxLayout(controlsHost);
    controlsLayout->setContentsMargins(8, 6, 8, 6);
    controlsLayout->setSpacing(4);

    auto addControlButton = [&](QPushButton* btn) {
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        controlsLayout->addWidget(btn, 1);
    };

    startStopBtn_ = new QPushButton("Start Streaming", controlsHost);
    startStopBtn_->setObjectName("rsControlButton");
    connect(startStopBtn_, &QPushButton::clicked, this, &MainWindow::onStartStop);
    addControlButton(startStopBtn_);

    recordBtn_ = new QPushButton("Start Recording", controlsHost);
    recordBtn_->setObjectName("rsControlButton");
    connect(recordBtn_, &QPushButton::clicked, this, &MainWindow::onRecord);
    addControlButton(recordBtn_);

    virtualCamBtn_ = new QPushButton("Start Virtual Camera", controlsHost);
    virtualCamBtn_->setObjectName("rsControlButton");
    connect(virtualCamBtn_, &QPushButton::clicked, this, &MainWindow::onVirtualCamera);
    addControlButton(virtualCamBtn_);

    studioModeBtn_ = new QPushButton("Studio Mode", controlsHost);
    studioModeBtn_->setObjectName("rsControlButton");
    studioModeBtn_->setCheckable(true);
    connect(studioModeBtn_, &QPushButton::toggled, this, [this](bool checked) {
        studioMode_->setStudioModeEnabled(checked);
        controller_->setStudioModeEnabled(checked);
        onStudioModeToggled(checked);
    });
    addControlButton(studioModeBtn_);

    auto* venueBtn = new QPushButton("Venue Mode", controlsHost);
    venueBtn->setObjectName("rsControlButton");
    connect(venueBtn, &QPushButton::clicked, this, &MainWindow::onOpenVenueMode);
    addControlButton(venueBtn);

    auto* settingsBtn = new QPushButton("Settings…", controlsHost);
    settingsBtn->setObjectName("rsControlButton");
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::onOpenSettings);
    addControlButton(settingsBtn);

    controlsDock->contentLayout()->addWidget(controlsHost);
    controlsDock->setFooterVisible(false);

    remoteUrlLabel_ = new QLabel(this);
    remoteUrlLabel_->setObjectName("rsFieldLabel");
    if (webServer_->isRunning()) {
        QString remote = QStringLiteral("Remote: %1").arg(webServer_->baseUrl());
        if (wsServer_ && wsServer_->isRunning()) {
            remote += QStringLiteral("  ·  WS: %1").arg(wsServer_->baseUrl());
        }
        remoteUrlLabel_->setText(remote);
    } else {
        remoteUrlLabel_->setText(QStringLiteral("Remote: offline"));
    }
    statusBar()->addPermanentWidget(remoteUrlLabel_);

    bottomSplitter->addWidget(scenesDock);
    bottomSplitter->addWidget(sourcesDock);
    bottomSplitter->addWidget(audioMixer_);
    bottomSplitter->addWidget(transitionsDock);
    bottomSplitter->addWidget(scoreDock);
    bottomSplitter->addWidget(controlsDock);
    bottomSplitter->setStretchFactor(0, 2);
    bottomSplitter->setStretchFactor(1, 3);
    bottomSplitter->setStretchFactor(2, 4);
    bottomSplitter->setStretchFactor(3, 2);
    bottomSplitter->setStretchFactor(4, 2);
    bottomSplitter->setStretchFactor(5, 3);
    bottomSplitter->setSizes({150, 220, 280, 150, 180, 190});

    mainLayout->addWidget(bottomSplitter);
    setCentralWidget(central);

    setupMenuBar();
    statusBar()->showMessage("Ready");
}

void MainWindow::setupMenuBar() {
    auto* bar = menuBar();

    auto* fileMenu = bar->addMenu(QStringLiteral("&File"));
    fileMenu->addAction(QStringLiteral("&Settings…"), this, &MainWindow::onOpenSettings,
                        QKeySequence(QStringLiteral("Ctrl+,")));
    fileMenu->addAction(QStringLiteral("&Remux Recording…"), this, &MainWindow::onRemuxRecording);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("E&xit"), this, &QWidget::close, QKeySequence::Quit);

    auto* editMenu = bar->addMenu(QStringLiteral("&Edit"));
    editMenu->addAction(QStringLiteral("&Add Scene"), this, &MainWindow::onAddScene);
    editMenu->addAction(QStringLiteral("Add So&urce…"), this, &MainWindow::onAddSource);
    editMenu->addSeparator();
    editMenu->addAction(QStringLiteral("Source &Properties…"), this, &MainWindow::onConfigureSource);
    editMenu->addAction(QStringLiteral("Source &Filters…"), this, &MainWindow::onEditFilters);

    auto* viewMenu = bar->addMenu(QStringLiteral("&View"));
    studioModeAction_ = viewMenu->addAction(QStringLiteral("&Studio Mode"));
    studioModeAction_->setCheckable(true);
    connect(studioModeAction_, &QAction::toggled, this, [this](bool checked) {
        if (studioModeBtn_ && studioModeBtn_->isChecked() != checked) {
            studioModeBtn_->setChecked(checked);
        }
    });
    viewMenu->addAction(QStringLiteral("&Venue Mode"), this, &MainWindow::onOpenVenueMode);
    viewMenu->addSeparator();
    viewMenu->addAction(QStringLiteral("Studio &Transition"), this, &MainWindow::onTransition);

    auto* docksMenu = bar->addMenu(QStringLiteral("&Docks"));
    auto addDockToggle = [docksMenu](const QString& title, QWidget* dock) {
        auto* action = docksMenu->addAction(title);
        action->setCheckable(true);
        action->setChecked(true);
        connect(action, &QAction::toggled, dock, &QWidget::setVisible);
        connect(dock, &QWidget::destroyed, action, &QObject::deleteLater);
    };
    addDockToggle(QStringLiteral("&Scenes"), scenesDock_);
    addDockToggle(QStringLiteral("S&ources"), sourcesDock_);
    addDockToggle(QStringLiteral("&Audio Mixer"), audioMixer_);
    addDockToggle(QStringLiteral("Scene &Transitions"), transitionsDock_);
    addDockToggle(QStringLiteral("Score&board"), scoreDock_);
    addDockToggle(QStringLiteral("&Controls"), controlsDock_);

    auto* profileMenu = bar->addMenu(QStringLiteral("&Profile"));
    auto* profileGroup = new QActionGroup(this);
    profileGroup->setExclusive(true);
    profileGeneralAction_ = profileMenu->addAction(QStringLiteral("&General Production"));
    profileGeneralAction_->setCheckable(true);
    profileSportsAction_ = profileMenu->addAction(QStringLiteral("&Sports / Billiards"));
    profileSportsAction_->setCheckable(true);
    profileGroup->addAction(profileGeneralAction_);
    profileGroup->addAction(profileSportsAction_);
    profileGeneralAction_->setChecked(true);
    connect(profileGeneralAction_, &QAction::triggered, this, [this]() {
        if (profileCombo_) {
            profileCombo_->setCurrentIndex(0);
        }
    });
    connect(profileSportsAction_, &QAction::triggered, this, [this]() {
        if (profileCombo_ && profileCombo_->count() > 1) {
            profileCombo_->setCurrentIndex(1);
        }
    });

    collectionMenu_ = bar->addMenu(QStringLiteral("&Scene Collection"));
    refreshCollectionMenu();

    auto* helpMenu = bar->addMenu(QStringLiteral("&Help"));
    helpMenu->addAction(QStringLiteral("&About RailShot TV"), this, [this]() {
        QMessageBox::about(
            this, QStringLiteral("About RailShot TV"),
            QStringLiteral("RailShot TV Broadcaster\nVersion 1.0.0\n\n"
                           "Broadcast control for billiards venues."));
    });
}

void MainWindow::refreshCollectionMenu() {
    if (!collectionMenu_) {
        return;
    }
    collectionMenu_->clear();
    collectionMenu_->addAction(QStringLiteral("&New…"), this, &MainWindow::onNewCollection);
    collectionMenu_->addAction(QStringLiteral("&Rename…"), this, &MainWindow::onRenameCollection);
    collectionMenu_->addAction(QStringLiteral("&Duplicate…"), this, &MainWindow::onDuplicateCollection);
    collectionMenu_->addAction(QStringLiteral("De&lete…"), this, &MainWindow::onDeleteCollection);
    collectionMenu_->addSeparator();

    auto* group = new QActionGroup(collectionMenu_);
    group->setExclusive(true);
    const auto items = SceneManager::instance().listCollections();
    const std::string activeId = SceneManager::instance().currentCollectionId();
    for (const auto& info : items) {
        auto* action = collectionMenu_->addAction(QString::fromStdString(info.name));
        action->setCheckable(true);
        action->setData(QString::fromStdString(info.id));
        action->setChecked(info.id == activeId);
        group->addAction(action);
        connect(action, &QAction::triggered, this, [this, id = info.id]() {
            if (!collectionCombo_) {
                return;
            }
            for (int i = 0; i < collectionCombo_->count(); ++i) {
                if (collectionCombo_->itemData(i).toString().toStdString() == id) {
                    collectionCombo_->setCurrentIndex(i);
                    return;
                }
            }
        });
    }
}

void MainWindow::assignDefaultWebcam() {
    const auto devices = StreamController::enumerateVideoDevices();
    if (devices.empty()) {
        return;
    }

    auto& manager = SceneManager::instance();
    for (auto& scene : manager.collection().scenes) {
        for (auto& src : scene.sources) {
            if (src.type == SourceType::VideoDevice && src.pathOrDeviceId.empty()) {
                src.pathOrDeviceId = devices.front().id;
            }
        }
    }
}

void MainWindow::refreshScenesList() {
    scenesList_->blockSignals(true);
    scenesList_->clear();
    auto& manager = SceneManager::instance();
    const std::string activeId = manager.activeSceneId();
    const std::string previewId = manager.previewSceneId();
    const bool studio = studioMode_->isStudioModeEnabled();

    for (const auto& scene : manager.collection().scenes) {
        QString label = QString::fromStdString(scene.name);
        if (studio) {
            if (scene.id == activeId) {
                label += " (Program)";
            }
            if (scene.id == previewId) {
                label += " (Preview)";
            }
        } else if (scene.id == activeId) {
            label += " (Live)";
        }
        auto* item = new QListWidgetItem(label, scenesList_);
        item->setData(Qt::UserRole, QString::fromStdString(scene.id));
        const std::string selectedId = studio ? previewId : activeId;
        if (scene.id == selectedId) {
            item->setSelected(true);
        }
    }
    scenesList_->blockSignals(false);
}

std::string MainWindow::sourcesSceneId() const {
    auto& manager = SceneManager::instance();
    if (studioMode_->isStudioModeEnabled()) {
        return manager.previewSceneId();
    }
    return manager.activeSceneId();
}

void MainWindow::refreshSourcesList() {
    const QString selectedId = sourcesList_->currentItem()
                                   ? sourcesList_->currentItem()->data(Qt::UserRole).toString()
                                   : QString();

    sourcesList_->blockSignals(true);
    sourcesList_->clear();

    const Scene* scene = SceneManager::instance().sceneById(sourcesSceneId());
    if (!scene) {
        sourcesList_->blockSignals(false);
        updateSourceContextBar();
        return;
    }

    std::vector<Source> sorted = scene->sources;
    std::sort(sorted.begin(), sorted.end(),
              [](const Source& a, const Source& b) { return a.zOrder < b.zOrder; });

    for (const auto& src : sorted) {
        auto* item = new QListWidgetItem(sourcesList_);
        item->setData(Qt::UserRole, QString::fromStdString(src.id));
        item->setSizeHint(QSize(0, 30));
        sourcesList_->addItem(item);
        sourcesList_->setItemWidget(item, makeSourceRowWidget(src, sourcesList_));
        if (QString::fromStdString(src.id) == selectedId) {
            item->setSelected(true);
            sourcesList_->setCurrentItem(item);
        }
    }
    sourcesList_->blockSignals(false);
    updateSourceContextBar();
}

QWidget* MainWindow::makeSourceRowWidget(const Source& src, QListWidget* list) {
    auto* row = new QWidget(list);
    row->setObjectName("rsSourceRow");
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(6, 2, 4, 2);
    layout->setSpacing(2);

    QString typeLabel;
    switch (src.type) {
    case SourceType::VideoDevice: typeLabel = "Cam"; break;
    case SourceType::AudioDevice: typeLabel = "Aud"; break;
    case SourceType::Image: typeLabel = "Img"; break;
    case SourceType::MediaFile: typeLabel = "Media"; break;
    case SourceType::NDI: typeLabel = "NDI"; break;
    case SourceType::Browser: typeLabel = "Web"; break;
    case SourceType::Scoreboard: typeLabel = "Score"; break;
    case SourceType::DisplayCapture: typeLabel = "Disp"; break;
    case SourceType::WindowCapture: typeLabel = "Win"; break;
    case SourceType::Text: typeLabel = "Text"; break;
    case SourceType::Color: typeLabel = "Color"; break;
    case SourceType::DesktopAudio: typeLabel = "Desk"; break;
    case SourceType::ApplicationAudio: typeLabel = "App"; break;
    case SourceType::GameCapture: typeLabel = "Game"; break;
    }

    auto* name = new QLabel(
        QString("%1  ·  %2").arg(QString::fromStdString(src.name), typeLabel), row);
    name->setObjectName("rsSourceName");
    layout->addWidget(name, 1);

    auto* eye = new QToolButton(row);
    eye->setObjectName("rsSourceEye");
    eye->setCheckable(true);
    eye->setChecked(src.isVisible);
    eye->setText(src.isVisible ? QStringLiteral("◉") : QStringLiteral("◎"));
    eye->setToolTip(src.isVisible ? QStringLiteral("Hide") : QStringLiteral("Show"));
    eye->setProperty("sourceId", QString::fromStdString(src.id));
    connect(eye, &QToolButton::clicked, this, &MainWindow::onToggleSourceVisible);
    layout->addWidget(eye);

    auto* lock = new QToolButton(row);
    lock->setObjectName("rsSourceLock");
    lock->setCheckable(true);
    lock->setChecked(src.locked);
    lock->setText(src.locked ? QStringLiteral("🔒") : QStringLiteral("🔓"));
    lock->setToolTip(src.locked ? QStringLiteral("Unlock") : QStringLiteral("Lock"));
    lock->setProperty("sourceId", QString::fromStdString(src.id));
    connect(lock, &QToolButton::clicked, this, &MainWindow::onToggleSourceLocked);
    layout->addWidget(lock);

    return row;
}

void MainWindow::updateSourceContextBar() {
    if (!sourceContextLabel_ || !sourcePropertiesBtn_) {
        return;
    }
    auto* item = sourcesList_ ? sourcesList_->currentItem() : nullptr;
    if (!item) {
        sourceContextLabel_->setText(QStringLiteral("No source selected"));
        sourcePropertiesBtn_->setEnabled(false);
        return;
    }
    const Source* src = SceneManager::instance().sourceById(
        item->data(Qt::UserRole).toString().toStdString());
    if (!src) {
        sourceContextLabel_->setText(QStringLiteral("No source selected"));
        sourcePropertiesBtn_->setEnabled(false);
        return;
    }
    sourceContextLabel_->setText(QStringLiteral("Selected: %1")
                                     .arg(QString::fromStdString(src->name)));
    sourcePropertiesBtn_->setEnabled(true);
}

void MainWindow::onToggleSourceVisible() {
    auto* button = qobject_cast<QToolButton*>(sender());
    if (!button) {
        return;
    }
    Source* src = SceneManager::instance().sourceById(
        button->property("sourceId").toString().toStdString());
    if (!src) {
        return;
    }
    src->isVisible = !src->isVisible;
    button->setChecked(src->isVisible);
    button->setText(src->isVisible ? QStringLiteral("◉") : QStringLiteral("◎"));
    button->setToolTip(src->isVisible ? QStringLiteral("Hide") : QStringLiteral("Show"));
    controller_->onSceneCollectionChanged();
}

void MainWindow::onToggleSourceLocked() {
    auto* button = qobject_cast<QToolButton*>(sender());
    if (!button) {
        return;
    }
    Source* src = SceneManager::instance().sourceById(
        button->property("sourceId").toString().toStdString());
    if (!src) {
        return;
    }
    src->locked = !src->locked;
    button->setChecked(src->locked);
    button->setText(src->locked ? QStringLiteral("🔒") : QStringLiteral("🔓"));
    button->setToolTip(src->locked ? QStringLiteral("Unlock") : QStringLiteral("Lock"));
}

void MainWindow::onMoveSourceUp() {
    auto* item = sourcesList_->currentItem();
    if (!item) {
        return;
    }
    Scene* scene = SceneManager::instance().sceneById(sourcesSceneId());
    if (!scene) {
        return;
    }
    const std::string id = item->data(Qt::UserRole).toString().toStdString();
    for (size_t i = 1; i < scene->sources.size(); ++i) {
        if (scene->sources[i].id != id) {
            continue;
        }
        std::swap(scene->sources[i - 1].zOrder, scene->sources[i].zOrder);
        std::swap(scene->sources[i - 1], scene->sources[i]);
        refreshSourcesList();
        controller_->onSceneCollectionChanged();
        return;
    }
}

void MainWindow::onMoveSourceDown() {
    auto* item = sourcesList_->currentItem();
    if (!item) {
        return;
    }
    Scene* scene = SceneManager::instance().sceneById(sourcesSceneId());
    if (!item || !scene) {
        return;
    }
    const std::string id = item->data(Qt::UserRole).toString().toStdString();
    for (size_t i = 0; i + 1 < scene->sources.size(); ++i) {
        if (scene->sources[i].id != id) {
            continue;
        }
        std::swap(scene->sources[i].zOrder, scene->sources[i + 1].zOrder);
        std::swap(scene->sources[i], scene->sources[i + 1]);
        refreshSourcesList();
        controller_->onSceneCollectionChanged();
        return;
    }
}

void MainWindow::refreshUi() {
    refreshScenesList();
    refreshSourcesList();
    audioMixer_->rebuild();
}

void MainWindow::onScenesChanged() {
    refreshCollectionsCombo();
    refreshUi();
    controller_->onSceneCollectionChanged();
}

void MainWindow::onSceneSelectionChanged() {
    auto* item = scenesList_->currentItem();
    if (!item) {
        return;
    }
    const std::string sceneId = item->data(Qt::UserRole).toString().toStdString();
    if (studioMode_->isStudioModeEnabled()) {
        controller_->setPreviewScene(sceneId);
    } else {
        controller_->setActiveScene(sceneId);
    }
    refreshScenesList();
    refreshSourcesList();
    audioMixer_->rebuild();
}

void MainWindow::updateMonitorSelection(const std::string& sourceId) {
    auto& manager = SceneManager::instance();
    if (studioMode_->isStudioModeEnabled()) {
        studioMode_->previewMonitor()->setSelectedSourceId(sourceId);
        studioMode_->previewMonitor()->setEditSceneId(manager.previewSceneId());
        studioMode_->previewMonitor()->setInteractionEnabled(true);
        // Program is a frozen output monitor — never draw live selection overlays.
        studioMode_->programMonitor()->setSelectedSourceId({});
        studioMode_->programMonitor()->setEditSceneId({});
        studioMode_->programMonitor()->setInteractionEnabled(false);
    } else {
        studioMode_->singleMonitor()->setSelectedSourceId(sourceId);
        studioMode_->singleMonitor()->setEditSceneId({});
        studioMode_->singleMonitor()->setInteractionEnabled(true);
    }
}

void MainWindow::onSourceSelectionChanged() {
    auto* item = sourcesList_->currentItem();
    if (!item) {
        updateMonitorSelection({});
        updateSourceContextBar();
        return;
    }
    updateMonitorSelection(item->data(Qt::UserRole).toString().toStdString());
    updateSourceContextBar();
}

void MainWindow::onStudioModeToggled(bool enabled) {
    studioModeBtn_->blockSignals(true);
    studioModeBtn_->setChecked(enabled);
    studioModeBtn_->blockSignals(false);
    if (studioModeAction_) {
        studioModeAction_->blockSignals(true);
        studioModeAction_->setChecked(enabled);
        studioModeAction_->blockSignals(false);
    }
    refreshScenesList();
    refreshSourcesList();
    controller_->onSceneCollectionChanged();
    ScoreboardSource::refreshAll();
    if (auto* item = sourcesList_->currentItem()) {
        updateMonitorSelection(item->data(Qt::UserRole).toString().toStdString());
    } else {
        updateMonitorSelection({});
    }
}

void MainWindow::onProfileChanged(int index) {
    const auto profile = static_cast<ProductionProfile>(profileCombo_->itemData(index).toInt());
    ScoreManager::instance().setProfile(profile);
    scoreboard_->setBilliardOptionsVisible(profile == ProductionProfile::SportsBilliards);
    AppSettingsData data = AppSettings::instance().data();
    data.productionProfile = static_cast<int>(profile);
    AppSettings::instance().setData(data);
    if (profileGeneralAction_ && profileSportsAction_) {
        profileGeneralAction_->blockSignals(true);
        profileSportsAction_->blockSignals(true);
        profileGeneralAction_->setChecked(profile == ProductionProfile::General);
        profileSportsAction_->setChecked(profile == ProductionProfile::SportsBilliards);
        profileGeneralAction_->blockSignals(false);
        profileSportsAction_->blockSignals(false);
    }
}

void MainWindow::onOpenVenueMode() {
    if (!venueWindow_) {
        venueWindow_ = new VenueModeWindow(controller_.get(), this);
    }
    venueWindow_->show();
    venueWindow_->raise();
    venueWindow_->activateWindow();
}

void MainWindow::onScoreStateChanged() {
    scoreboard_->refreshFromState();
    RailShotApiClient::instance().onMatchStateChanged(ScoreManager::instance().state());
    ScoreboardSource::refreshAll();
    BrowserSource::refreshAll();
}

void MainWindow::onTransition() {
    RemoteCommandBus::instance().execute(QStringLiteral("studioTransition"));
    refreshScenesList();
    refreshSourcesList();
    audioMixer_->rebuild();
    if (auto* item = sourcesList_->currentItem()) {
        updateMonitorSelection(item->data(Qt::UserRole).toString().toStdString());
    }
    syncControlsFromController();
}

void MainWindow::onAddScene() {
    bool ok = false;
    const QString name = QInputDialog::getText(this, "Add Scene", "Scene name:",
                                               QLineEdit::Normal, "New Scene", &ok);
    if (!ok || name.isEmpty()) {
        return;
    }
    SceneManager::instance().addScene(name.toStdString());
    refreshUi();
}

void MainWindow::onRemoveScene() {
    auto* item = scenesList_->currentItem();
    if (!item) {
        return;
    }
    const std::string sceneId = item->data(Qt::UserRole).toString().toStdString();
    if (SceneManager::instance().collection().scenes.size() <= 1) {
        QMessageBox::information(this, "Remove Scene", "You must keep at least one scene.");
        return;
    }
    if (!SceneManager::instance().removeScene(sceneId)) {
        return;
    }
    refreshUi();
    controller_->onSceneCollectionChanged();
}

void MainWindow::onRemoveSource() {
    auto* item = sourcesList_->currentItem();
    if (!item) {
        return;
    }
    const std::string sourceId = item->data(Qt::UserRole).toString().toStdString();
    if (!SceneManager::instance().removeSource(sourcesSceneId(), sourceId)) {
        return;
    }
    refreshUi();
    controller_->onSceneCollectionChanged();
}

namespace {

SourceTransform centeredSourceTransform(float width, float height) {
    return {(1920.0f - width) * 0.5f, (1080.0f - height) * 0.5f, width, height};
}

} // namespace

void MainWindow::onAddSource() {
    if (!SceneManager::instance().sceneById(sourcesSceneId())) {
        return;
    }

    AddSourceDialog picker(this);
    if (picker.exec() != QDialog::Accepted) {
        return;
    }
    const QString choice = picker.selectedTypeId();
    if (choice.isEmpty()) {
        return;
    }
    const QString customName = picker.sourceName();

    Source src;
    src.name = customName.toStdString();
    if (choice == "video_device") {
        src.type = SourceType::VideoDevice;
        const auto devices = StreamController::enumerateVideoDevices();
        if (!devices.empty()) {
            src.pathOrDeviceId = devices.front().id;
            if (customName.isEmpty() || customName == QStringLiteral("Video Capture Device")) {
                src.name = devices.front().name;
            }
        }
    } else if (choice == "display_capture") {
        src.type = SourceType::DisplayCapture;
        src.pathOrDeviceId = "0";
        const auto monitors = DxgiMonitorCapture::enumerateMonitors();
        if (!monitors.empty()) {
            src.pathOrDeviceId = std::to_string(monitors.front().index);
            if (customName.isEmpty() || customName == QStringLiteral("Display Capture")) {
                src.name = monitors.front().name;
            }
            src.transform = centeredSourceTransform(
                static_cast<float>(monitors.front().width),
                static_cast<float>(monitors.front().height));
        } else {
            src.transform = centeredSourceTransform(1920.0f, 1080.0f);
        }
    } else if (choice == "window_capture") {
        src.type = SourceType::WindowCapture;
        src.transform = centeredSourceTransform(1280.0f, 720.0f);
    } else if (choice == "game_capture") {
        src.type = SourceType::GameCapture;
        src.transform = centeredSourceTransform(1280.0f, 720.0f);
    } else if (choice == "image") {
        src.type = SourceType::Image;
    } else if (choice == "media_file") {
        src.type = SourceType::MediaFile;
        src.loop = true;
    } else if (choice == "desktop_audio") {
        src.type = SourceType::DesktopAudio;
        src.transform = {0, 0, 1, 1};
    } else if (choice == "app_audio") {
        src.type = SourceType::ApplicationAudio;
        src.transform = {0, 0, 1, 1};
    } else if (choice == "audio_input") {
        src.type = SourceType::AudioDevice;
        src.transform = {0, 0, 1, 1};
    } else if (choice == "ndi") {
        src.type = SourceType::NDI;
        const auto ndiSources = NdiFinder::discoverSources();
        if (!ndiSources.empty()) {
            src.pathOrDeviceId = ndiSources.front().url;
            if (customName.isEmpty() || customName == QStringLiteral("NDI Source")) {
                src.name = ndiSources.front().name;
            }
        }
    } else if (choice == "browser") {
        src.type = SourceType::Browser;
        src.zOrder = 10;
        const BrowserSourceSettings browserDefaults = BrowserSourceSettings::defaults();
        browserDefaults.applyToSource(src);
        src.name = customName.toStdString();
        src.transform = browserDefaults.defaultSceneTransform();
    } else if (choice == "text") {
        src.type = SourceType::Text;
        const TextSourceSettings textDefaults;
        textDefaults.applyToSource(src);
        src.name = customName.toStdString();
        src.transform = centeredSourceTransform(static_cast<float>(textDefaults.width),
                                                static_cast<float>(textDefaults.height));
    } else if (choice == "color") {
        src.type = SourceType::Color;
        const ColorSourceSettings colorDefaults;
        colorDefaults.applyToSource(src);
        src.name = customName.toStdString();
        src.transform = centeredSourceTransform(static_cast<float>(colorDefaults.width),
                                                static_cast<float>(colorDefaults.height));
    } else if (choice == "scoreboard") {
        src.type = SourceType::Scoreboard;
        src.transform = {0, 0, 1920, 110};
        src.zOrder = 100;
        src.overlaySettings = ScoreboardStyle::defaults().toJson();
    } else {
        // audio_file and fallback
        src.type = SourceType::MediaFile;
        src.loop = true;
    }

    if (choice != "browser" && choice != "scoreboard" && choice != "text"
        && choice != "color" && choice != "display_capture"
        && choice != "window_capture" && choice != "game_capture"
        && choice != "desktop_audio" && choice != "app_audio"
        && choice != "audio_input") {
        src.transform = centeredSourceTransform(960.0f, 540.0f);
    }

    Source& added = SceneManager::instance().addSource(sourcesSceneId(), src);
    refreshUi();
    controller_->onSceneCollectionChanged();

    if (choice == "browser") {
        BrowserSourceConfigDialog dlg(added, this);
        connect(&dlg, &BrowserSourceConfigDialog::settingsChanged, this,
                [&added](const BrowserSourceSettings& settings) {
                    // Live URL/CSS/render updates must NOT resize the scene item.
                    // (Spins can briefly report 8x8 while the dialog is polishing.)
                    settings.applyConfigToSource(added);
                    BrowserSource::applyLiveConfig(added);
                });
        if (dlg.exec() == QDialog::Accepted) {
            dlg.applyToSource(added);
        } else {
            // Keep default 800×600 centered even if Cancel.
            const BrowserSourceSettings defaults = BrowserSourceSettings::fromSource(added);
            added.transform = defaults.defaultSceneTransform();
        }
        // Final safeguard against a speck-sized source on the canvas.
        if (added.transform.width < 64.0f || added.transform.height < 64.0f) {
            added.transform = BrowserSourceSettings::fromSource(added).defaultSceneTransform();
        }
        BrowserSource::reloadAll();
        controller_->onSceneCollectionChanged();
        refreshUi();
    } else if (choice == "display_capture" || choice == "window_capture"
               || choice == "game_capture" || choice == "app_audio" || choice == "text"
               || choice == "color") {
        // Select camera / file / picker immediately so the source is not blank.
        for (int i = 0; i < sourcesList_->count(); ++i) {
            auto* item = sourcesList_->item(i);
            if (item && item->data(Qt::UserRole).toString().toStdString() == added.id) {
                sourcesList_->setCurrentItem(item);
                break;
            }
        }
        onConfigureSource();
    }
}

void MainWindow::onConfigureSource() {
    auto* item = sourcesList_->currentItem();
    if (!item) {
        return;
    }
    Source* src = SceneManager::instance().sourceById(
        item->data(Qt::UserRole).toString().toStdString());
    if (!src) {
        return;
    }

    if (src->type == SourceType::Image) {
        const QString path = QFileDialog::getOpenFileName(
            this, "Select Image", {}, "Images (*.png *.jpg *.jpeg)");
        if (!path.isEmpty()) {
            src->pathOrDeviceId = path.toStdString();
        }
    } else if (src->type == SourceType::MediaFile) {
        MediaSourceConfigDialog dlg(*src, this);
        connect(&dlg, &MediaSourceConfigDialog::settingsChanged, this,
                [src](const MediaSourceSettings& settings) {
                    settings.applyToSource(*src);
                    MediaSource::applyLiveConfig(*src);
                });
        if (dlg.exec() != QDialog::Accepted) {
            return;
        }
        dlg.applyToSource(*src);
        MediaSource::applyLiveConfig(*src);
    } else if (src->type == SourceType::AudioDevice) {
        const auto devices = WasapiAudioCapture::enumerateInputDevices();
        QStringList names;
        for (const auto& d : devices) {
            names << QString::fromStdString(d.name);
        }
        if (names.isEmpty()) {
            names << QStringLiteral("Default input");
        }
        bool ok = false;
        const QString choice = QInputDialog::getItem(this, QStringLiteral("Audio Input"),
                                                     QStringLiteral("Device:"), names, 0, false, &ok);
        if (ok && !devices.empty()) {
            const int idx = names.indexOf(choice);
            if (idx >= 0) {
                src->pathOrDeviceId = devices[static_cast<size_t>(idx)].id;
                src->name = devices[static_cast<size_t>(idx)].name;
            }
        } else if (ok) {
            src->pathOrDeviceId.clear();
        }
    } else if (src->type == SourceType::VideoDevice) {
        const auto devices = StreamController::enumerateVideoDevices();
        QStringList names;
        for (const auto& d : devices) {
            names << QString::fromStdString(d.name);
        }
        bool ok = false;
        const QString choice = QInputDialog::getItem(this, "Select Camera", "Device:",
                                                     names, 0, false, &ok);
        if (ok && !devices.empty()) {
            const int idx = names.indexOf(choice);
            if (idx >= 0) {
                src->pathOrDeviceId = devices[static_cast<size_t>(idx)].id;
                src->name = devices[static_cast<size_t>(idx)].name;
            }
        }
        const auto isoReply = QMessageBox::question(
            this, "ISO Recording",
            "Record this camera to a separate ISO file while streaming?",
            QMessageBox::Yes | QMessageBox::No);
        src->isoRecording = (isoReply == QMessageBox::Yes);
    } else if (src->type == SourceType::NDI) {
        const auto ndiSources = NdiFinder::discoverSources();
        QStringList names;
        for (const auto& s : ndiSources) {
            names << QString::fromStdString(s.name);
        }
        if (names.isEmpty()) {
            names << "No NDI sources found (install SDK in vendor/ndi)";
        }
        bool ok = false;
        const QString choice = QInputDialog::getItem(this, "Select NDI Source", "Source:",
                                                       names, 0, false, &ok);
        if (ok && !ndiSources.empty()) {
            const int idx = names.indexOf(choice);
            if (idx >= 0) {
                src->pathOrDeviceId = ndiSources[static_cast<size_t>(idx)].url;
                src->name = ndiSources[static_cast<size_t>(idx)].name;
            }
        }
        const auto isoReply = QMessageBox::question(
            this, "ISO Recording",
            "Record this NDI source to a separate ISO file while streaming?",
            QMessageBox::Yes | QMessageBox::No);
        src->isoRecording = (isoReply == QMessageBox::Yes);
    } else if (src->type == SourceType::Browser) {
        const Source backup = *src;
        BrowserSourceConfigDialog dlg(*src, this);
        connect(&dlg, &BrowserSourceConfigDialog::settingsChanged, this,
                [src](const BrowserSourceSettings& settings) {
                    // Live edits update the browser only — keep canvas size from handles.
                    settings.applyConfigToSource(*src);
                    BrowserSource::applyLiveConfig(*src);
                });
        if (dlg.exec() != QDialog::Accepted) {
            *src = backup;
            BrowserSource::applyLiveConfig(*src);
            return;
        }
        // On OK: update config; resize canvas only if Width/Height were changed.
        const float x = src->transform.x;
        const float y = src->transform.y;
        const float oldW = src->transform.width;
        const float oldH = src->transform.height;
        dlg.settings().applyConfigToSource(*src);
        if (dlg.sizeChangedFromOpen()) {
            dlg.settings().applySceneSizeToSource(*src);
            src->transform.x = x;
            src->transform.y = y;
        } else {
            src->transform.width = oldW;
            src->transform.height = oldH;
        }
        if (src->transform.width < 64.0f || src->transform.height < 64.0f) {
            src->transform = BrowserSourceSettings::fromSource(*src).defaultSceneTransform();
        }
        BrowserSource::reloadAll();
    } else if (src->type == SourceType::Scoreboard) {
        const Source backup = *src;
        ScoreboardConfigDialog dlg(*src, this);
        connect(&dlg, &ScoreboardConfigDialog::liveStyleChanged, this, [src](const ScoreboardStyle& style) {
            src->overlaySettings = style.toJson();
            src->transform.width = static_cast<float>(style.barWidth > 0 ? style.barWidth : 1920);
            src->transform.height = static_cast<float>(style.barHeight);
            ScoreboardSource::applyLiveConfig(*src);
        });
        if (dlg.exec() != QDialog::Accepted) {
            *src = backup;
            ScoreboardSource::applyLiveConfig(*src);
            return;
        }
        dlg.applyToSource(*src);
        ScoreboardSource::refreshAll();
    } else if (src->type == SourceType::DisplayCapture) {
        const auto monitors = DxgiMonitorCapture::enumerateMonitors();
        if (monitors.empty()) {
            QMessageBox::warning(this, "Display Capture",
                                 "No monitors were found for capture.");
            return;
        }
        QStringList names;
        int current = 0;
        for (int i = 0; i < static_cast<int>(monitors.size()); ++i) {
            const auto& m = monitors[static_cast<size_t>(i)];
            names << QString("%1 (%2x%3)")
                         .arg(QString::fromStdString(m.name))
                         .arg(m.width)
                         .arg(m.height);
            if (std::to_string(m.index) == src->pathOrDeviceId) {
                current = i;
            }
        }
        bool pickOk = false;
        const QString choice = QInputDialog::getItem(this, "Display Capture", "Monitor:",
                                                     names, current, false, &pickOk);
        if (pickOk) {
            const int idx = names.indexOf(choice);
            if (idx >= 0) {
                const auto& m = monitors[static_cast<size_t>(idx)];
                src->pathOrDeviceId = std::to_string(m.index);
                src->name = m.name;
                src->transform.width = static_cast<float>(m.width);
                src->transform.height = static_cast<float>(m.height);
            }
        }
    } else if (src->type == SourceType::WindowCapture || src->type == SourceType::GameCapture) {
        const auto windows = WindowBitbltCapture::enumerateWindows();
        if (windows.empty()) {
            QMessageBox::warning(this, "Window Capture",
                                 "No visible windows were found.");
            return;
        }
        QStringList names;
        int current = 0;
        for (int i = 0; i < static_cast<int>(windows.size()); ++i) {
            const auto& w = windows[static_cast<size_t>(i)];
            names << QString::fromStdString(w.title);
            if (std::to_string(w.hwnd) == src->pathOrDeviceId) {
                current = i;
            }
        }
        const QString title = src->type == SourceType::GameCapture ? QStringLiteral("Game Capture")
                                                                   : QStringLiteral("Window Capture");
        bool pickOk = false;
        const QString choice = QInputDialog::getItem(this, title, QStringLiteral("Window:"),
                                                     names, current, false, &pickOk);
        if (pickOk) {
            const int idx = names.indexOf(choice);
            if (idx >= 0) {
                const auto& w = windows[static_cast<size_t>(idx)];
                src->pathOrDeviceId = std::to_string(w.hwnd);
                src->name = w.title;
            }
        }
    } else if (src->type == SourceType::ApplicationAudio) {
        const auto procs = WasapiAudioCapture::enumerateProcesses();
        if (procs.empty()) {
            QMessageBox::warning(this, "Application Audio", "No processes found.");
            return;
        }
        QStringList names;
        int current = 0;
        for (int i = 0; i < static_cast<int>(procs.size()); ++i) {
            const auto& p = procs[static_cast<size_t>(i)];
            names << QStringLiteral("%1 (%2)").arg(QString::fromStdString(p.name)).arg(p.pid);
            if (std::to_string(p.pid) == src->pathOrDeviceId) {
                current = i;
            }
        }
        bool pickOk = false;
        const QString choice = QInputDialog::getItem(this, QStringLiteral("Application Audio"),
                                                     QStringLiteral("Process:"), names, current,
                                                     false, &pickOk);
        if (pickOk) {
            const int idx = names.indexOf(choice);
            if (idx >= 0) {
                const auto& p = procs[static_cast<size_t>(idx)];
                src->pathOrDeviceId = std::to_string(p.pid);
                src->name = p.name + " Audio";
            }
        }
    } else if (src->type == SourceType::Text) {
        TextSourceConfigDialog dlg(*src, this);
        if (dlg.exec() != QDialog::Accepted) {
            return;
        }
        dlg.applyToSource(*src);
    } else if (src->type == SourceType::Color) {
        ColorSourceConfigDialog dlg(*src, this);
        if (dlg.exec() != QDialog::Accepted) {
            return;
        }
        dlg.applyToSource(*src);
    } else if (src->type == SourceType::DesktopAudio) {
        const auto devices = WasapiAudioCapture::enumerateOutputDevices();
        QStringList names;
        for (const auto& d : devices) {
            names << QString::fromStdString(d.name);
        }
        if (names.isEmpty()) {
            names << QStringLiteral("Default output (loopback)");
        }
        bool ok = false;
        const QString choice = QInputDialog::getItem(
            this, QStringLiteral("Desktop Audio"), QStringLiteral("Playback device to capture:"),
            names, 0, false, &ok);
        if (ok && !devices.empty()) {
            const int idx = names.indexOf(choice);
            if (idx >= 0) {
                src->pathOrDeviceId = devices[static_cast<size_t>(idx)].id;
                src->name = "Desktop Audio (" + devices[static_cast<size_t>(idx)].name + ")";
            }
        } else if (ok) {
            src->pathOrDeviceId.clear();
        }
    }

    controller_->onSceneCollectionChanged();
    refreshUi();
}

void MainWindow::onEditFilters() {
    auto* item = sourcesList_->currentItem();
    if (!item) {
        return;
    }
    Source* src = SceneManager::instance().sourceById(
        item->data(Qt::UserRole).toString().toStdString());
    if (!src) {
        return;
    }
    FiltersDialog dlg(*src, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    dlg.applyToSource(*src);
    controller_->onSceneCollectionChanged();
    refreshUi();
}

void MainWindow::onOpenSettings() {
    SettingsDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    AppSettingsData settings = dlg.resultSettings();
    settings.productionProfile = profileCombo_->currentData().toInt();
    settings.activeCollectionId = SceneManager::instance().currentCollectionId();

    const QString rename = dlg.activeCollectionRename();
    if (!rename.isEmpty()) {
        SceneManager::instance().renameCollection(
            SceneManager::instance().currentCollectionId(), rename.toStdString());
    }

    if (dlg.videoSettingsChanged() && (controller_->isStreaming() || controller_->isEncoding())) {
        QMessageBox::information(this, "Settings",
                                 "Stop streaming/recording/replay before applying canvas size or FPS changes.");
        AppSettingsData keep = AppSettings::instance().data();
        keep.defaultRtmpUrl = settings.defaultRtmpUrl;
        keep.hotkeys = settings.hotkeys;
        keep.productionProfile = settings.productionProfile;
        keep.activeCollectionId = settings.activeCollectionId;
        keep.audioMonitoringEnabled = settings.audioMonitoringEnabled;
        keep.monitoringDeviceId = settings.monitoringDeviceId;
        keep.micDeviceId = settings.micDeviceId;
        keep.videoEncoder = settings.videoEncoder;
        keep.encoderPreset = settings.encoderPreset;
        keep.videoBitrateKbps = settings.videoBitrateKbps;
        keep.audioBitrateKbps = settings.audioBitrateKbps;
        keep.recordingFormat = settings.recordingFormat;
        keep.recordingDirectory = settings.recordingDirectory;
        keep.recordingBitrateKbps = settings.recordingBitrateKbps;
        keep.replayBufferEnabled = settings.replayBufferEnabled;
        keep.replayBufferSeconds = settings.replayBufferSeconds;
        keep.streamService = settings.streamService;
        keep.streamServer = settings.streamServer;
        keep.streamKey = settings.streamKey;
        keep.micVolume = settings.micVolume;
        keep.micMuted = settings.micMuted;
        keep.micSyncDelayMs = settings.micSyncDelayMs;
        AppSettings::instance().setData(keep);
        controller_->applyAudioSettings();
        rtmpUrlEdit_->setText(QString::fromStdString(keep.defaultRtmpUrl));
        bindHotkeys();
        refreshCollectionsCombo();
        return;
    }
    AppSettings::instance().setData(settings);
    rtmpUrlEdit_->setText(QString::fromStdString(settings.defaultRtmpUrl));
    bindHotkeys();
    controller_->applyAudioSettings();
    if (dlg.videoSettingsChanged()) {
        controller_->applyVideoSettings();
    }
    refreshCollectionsCombo();
}

void MainWindow::bindHotkeys() {
    for (QShortcut* sc : hotkeyShortcuts_) {
        if (sc) {
            sc->deleteLater();
        }
    }
    hotkeyShortcuts_.clear();

    const HotkeyBindings hk = AppSettings::instance().hotkeys();
    auto add = [this](const std::string& seq, const std::function<void()>& slot) {
        if (seq.empty()) {
            return;
        }
        const QKeySequence key(QString::fromStdString(seq));
        if (key.isEmpty()) {
            return;
        }
        auto* shortcut = new QShortcut(key, this);
        shortcut->setContext(Qt::ApplicationShortcut);
        connect(shortcut, &QShortcut::activated, this, slot);
        hotkeyShortcuts_.push_back(shortcut);
    };

    add(hk.transition, [this]() { onTransition(); });
    add(hk.startStopStream, [this]() { onStartStop(); });
    add(hk.record, [this]() { onRecord(); });
    add(hk.saveReplay, [this]() { onSaveReplay(); });
    add(hk.scene1, [this]() {
        RemoteCommandBus::instance().execute(QStringLiteral("selectSceneByIndex"),
                                             QJsonObject{{QStringLiteral("index"), 0}});
        refreshUi();
    });
    add(hk.scene2, [this]() {
        RemoteCommandBus::instance().execute(QStringLiteral("selectSceneByIndex"),
                                             QJsonObject{{QStringLiteral("index"), 1}});
        refreshUi();
    });
    add(hk.scene3, [this]() {
        RemoteCommandBus::instance().execute(QStringLiteral("selectSceneByIndex"),
                                             QJsonObject{{QStringLiteral("index"), 2}});
        refreshUi();
    });
    add(hk.scene4, [this]() {
        RemoteCommandBus::instance().execute(QStringLiteral("selectSceneByIndex"),
                                             QJsonObject{{QStringLiteral("index"), 3}});
        refreshUi();
    });
    add(hk.scoreP1Plus, []() {
        RemoteCommandBus::instance().execute(
            QStringLiteral("adjustScore"),
            QJsonObject{{QStringLiteral("player"), 1}, {QStringLiteral("delta"), 1}});
    });
    add(hk.scoreP1Minus, []() {
        RemoteCommandBus::instance().execute(
            QStringLiteral("adjustScore"),
            QJsonObject{{QStringLiteral("player"), 1}, {QStringLiteral("delta"), -1}});
    });
    add(hk.scoreP2Plus, []() {
        RemoteCommandBus::instance().execute(
            QStringLiteral("adjustScore"),
            QJsonObject{{QStringLiteral("player"), 2}, {QStringLiteral("delta"), 1}});
    });
    add(hk.scoreP2Minus, []() {
        RemoteCommandBus::instance().execute(
            QStringLiteral("adjustScore"),
            QJsonObject{{QStringLiteral("player"), 2}, {QStringLiteral("delta"), -1}});
    });
}

void MainWindow::syncActiveCollectionFromSettings() {
    const std::string id = AppSettings::instance().activeCollectionId();
    if (!id.empty() && id != SceneManager::instance().currentCollectionId()) {
        SceneManager::instance().switchCollection(id);
    }
    persistActiveCollectionId();
}

void MainWindow::persistActiveCollectionId() {
    AppSettings::instance().setActiveCollectionId(SceneManager::instance().currentCollectionId());
}

bool MainWindow::collectionSwitchBlocked(QString* reason) const {
    if (!controller_) {
        return false;
    }
    if (controller_->isStreaming()) {
        if (reason) {
            *reason = QStringLiteral("Stop streaming before switching collections.");
        }
        return true;
    }
    if (controller_->isRecording()) {
        if (reason) {
            *reason = QStringLiteral("Stop recording before switching collections.");
        }
        return true;
    }
    if (controller_->isVirtualCameraActive()) {
        if (reason) {
            *reason = QStringLiteral("Stop the virtual camera before switching collections.");
        }
        return true;
    }
    return false;
}

void MainWindow::refreshCollectionsCombo() {
    if (!collectionCombo_) {
        return;
    }
    collectionCombo_->blockSignals(true);
    collectionCombo_->clear();
    const auto items = SceneManager::instance().listCollections();
    const std::string activeId = SceneManager::instance().currentCollectionId();
    int activeIndex = 0;
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        collectionCombo_->addItem(QString::fromStdString(items[static_cast<size_t>(i)].name),
                                  QString::fromStdString(items[static_cast<size_t>(i)].id));
        if (items[static_cast<size_t>(i)].id == activeId) {
            activeIndex = i;
        }
    }
    if (collectionCombo_->count() > 0) {
        collectionCombo_->setCurrentIndex(activeIndex);
    }
    collectionCombo_->blockSignals(false);
    refreshCollectionMenu();
}

void MainWindow::onCollectionComboChanged(int index) {
    if (index < 0 || !collectionCombo_) {
        return;
    }
    const std::string id = collectionCombo_->itemData(index).toString().toStdString();
    if (id.empty() || id == SceneManager::instance().currentCollectionId()) {
        return;
    }
    QString reason;
    if (collectionSwitchBlocked(&reason)) {
        QMessageBox::information(this, QStringLiteral("Collections"), reason);
        refreshCollectionsCombo();
        return;
    }
    if (!SceneManager::instance().switchCollection(id)) {
        QMessageBox::warning(this, QStringLiteral("Collections"),
                             QStringLiteral("Could not switch collection."));
        refreshCollectionsCombo();
        return;
    }
    persistActiveCollectionId();
    controller_->onSceneCollectionChanged();
    refreshUi();
}

void MainWindow::onNewCollection() {
    QString reason;
    if (collectionSwitchBlocked(&reason)) {
        QMessageBox::information(this, QStringLiteral("Collections"), reason);
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("New Collection"),
                                               QStringLiteral("Name:"), QLineEdit::Normal,
                                               QStringLiteral("New Collection"), &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }
    SceneManager::instance().createCollection(name.trimmed().toStdString());
    persistActiveCollectionId();
    controller_->onSceneCollectionChanged();
    refreshCollectionsCombo();
    refreshUi();
}

void MainWindow::onRenameCollection() {
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, QStringLiteral("Rename Collection"), QStringLiteral("Name:"), QLineEdit::Normal,
        QString::fromStdString(SceneManager::instance().collection().name), &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }
    SceneManager::instance().renameCollection(SceneManager::instance().currentCollectionId(),
                                              name.trimmed().toStdString());
    refreshCollectionsCombo();
}

void MainWindow::onDuplicateCollection() {
    QString reason;
    if (collectionSwitchBlocked(&reason)) {
        QMessageBox::information(this, QStringLiteral("Collections"), reason);
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, QStringLiteral("Duplicate Collection"), QStringLiteral("Name:"), QLineEdit::Normal,
        QString::fromStdString(SceneManager::instance().collection().name + " Copy"), &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }
    SceneManager::instance().duplicateCollection(name.trimmed().toStdString());
    persistActiveCollectionId();
    controller_->onSceneCollectionChanged();
    refreshCollectionsCombo();
    refreshUi();
}

void MainWindow::onDeleteCollection() {
    QString reason;
    if (collectionSwitchBlocked(&reason)) {
        QMessageBox::information(this, QStringLiteral("Collections"), reason);
        return;
    }
    if (SceneManager::instance().listCollections().size() <= 1) {
        QMessageBox::information(this, QStringLiteral("Collections"),
                                 QStringLiteral("Cannot delete the last collection."));
        return;
    }
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Delete Collection"),
        QStringLiteral("Delete \"%1\"?")
            .arg(QString::fromStdString(SceneManager::instance().collection().name)));
    if (answer != QMessageBox::Yes) {
        return;
    }
    if (!SceneManager::instance().deleteCollection(SceneManager::instance().currentCollectionId())) {
        QMessageBox::warning(this, QStringLiteral("Collections"),
                             QStringLiteral("Could not delete collection."));
        return;
    }
    persistActiveCollectionId();
    controller_->onSceneCollectionChanged();
    refreshCollectionsCombo();
    refreshUi();
}

void MainWindow::restoreProductionProfile() {
    const int profile = AppSettings::instance().data().productionProfile;
    for (int i = 0; i < profileCombo_->count(); ++i) {
        if (profileCombo_->itemData(i).toInt() == profile) {
            profileCombo_->blockSignals(true);
            profileCombo_->setCurrentIndex(i);
            profileCombo_->blockSignals(false);
            onProfileChanged(i);
            return;
        }
    }
}

void MainWindow::syncControlsFromController() {
    if (!controller_) {
        return;
    }
    const bool streaming = controller_->isStreaming();
    const bool recording = controller_->isRecording();
    startStopBtn_->setText(streaming ? QStringLiteral("Stop Streaming")
                                     : QStringLiteral("Start Streaming"));
    startStopBtn_->setObjectName(streaming ? QStringLiteral("rsControlButtonLive")
                                           : QStringLiteral("rsControlButton"));
    startStopBtn_->style()->unpolish(startStopBtn_);
    startStopBtn_->style()->polish(startStopBtn_);
    rtmpUrlEdit_->setEnabled(!streaming);
    recordBtn_->setText(recording ? QStringLiteral("Stop Recording")
                                  : QStringLiteral("Start Recording"));
    const bool vcam = controller_->isVirtualCameraActive();
    virtualCamBtn_->setText(vcam ? QStringLiteral("Stop Virtual Camera")
                                 : QStringLiteral("Start Virtual Camera"));
    refreshUi();
}

void MainWindow::onVirtualCamera() {
    if (!controller_) {
        return;
    }
    if (controller_->isVirtualCameraActive()) {
        controller_->stopVirtualCamera();
        virtualCamBtn_->setText(QStringLiteral("Start Virtual Camera"));
        return;
    }

    if (!controller_->startVirtualCamera()) {
        QMessageBox::critical(
            this, QStringLiteral("Virtual Camera"),
            QStringLiteral(
                "Could not start the virtual camera.\n\n"
                "Register the filter once (as Administrator):\n"
                "scripts\\install-virtualcam.bat"));
        return;
    }
    virtualCamBtn_->setText(QStringLiteral("Stop Virtual Camera"));
}

void MainWindow::onRecord() {
    if (controller_->isRecording()) {
        controller_->stopRecording();
        recordBtn_->setText("Start Recording");
        syncControlsFromController();
        return;
    }

    if (!controller_->startRecording()) {
        QMessageBox::critical(this, "Recording Failed",
                              "Could not start program recording. Add at least one source to the active scene.");
        return;
    }

    recordBtn_->setText("Stop Recording");
    syncControlsFromController();
}

void MainWindow::onSaveReplay() {
    if (!controller_) {
        return;
    }
    if (!controller_->isReplayBufferActive()) {
        if (!controller_->startReplayBuffer()) {
            QMessageBox::warning(this, QStringLiteral("Replay Buffer"),
                                 QStringLiteral("Could not start the replay buffer. "
                                                "Add a source and try again."));
            return;
        }
        statusBar()->showMessage(
            QStringLiteral("Replay buffer started — press Save Replay again after a few seconds."),
            4000);
        return;
    }
    if (!controller_->saveReplayBuffer()) {
        QMessageBox::warning(this, QStringLiteral("Save Replay"),
                             QStringLiteral("Replay buffer is empty or save failed."));
        return;
    }
    const auto path = QString::fromStdString(controller_->stats().lastReplayPath);
    statusBar()->showMessage(QStringLiteral("Replay saved: %1").arg(path), 6000);
}

void MainWindow::onRemuxRecording() {
    if (!controller_) {
        return;
    }
    const QString input = QFileDialog::getOpenFileName(
        this, QStringLiteral("Remux recording"), QString(),
        QStringLiteral("Media (*.mp4 *.mkv *.mov *.flv);;All files (*.*)"));
    if (input.isEmpty()) {
        return;
    }
    QString suggested = input;
    if (suggested.endsWith(QStringLiteral(".mkv"), Qt::CaseInsensitive)) {
        suggested.replace(suggested.size() - 3, 3, QStringLiteral("mp4"));
    } else {
        suggested += QStringLiteral(".mp4");
    }
    const QString output = QFileDialog::getSaveFileName(
        this, QStringLiteral("Remux output"), suggested,
        QStringLiteral("MP4 (*.mp4);;MKV (*.mkv);;MOV (*.mov)"));
    if (output.isEmpty()) {
        return;
    }
    if (!controller_->remuxRecording(input.toStdString(), output.toStdString())) {
        QMessageBox::critical(this, QStringLiteral("Remux Failed"),
                              QStringLiteral("Could not remux the selected file."));
        return;
    }
    QMessageBox::information(this, QStringLiteral("Remux"),
                             QStringLiteral("Wrote:\n%1").arg(output));
}

void MainWindow::onStartStop() {
    if (controller_->isStreaming()) {
        controller_->stopStream();
        syncControlsFromController();
        return;
    }

    const QString url = rtmpUrlEdit_->text().trimmed();
    if (url.isEmpty()) {
        QMessageBox::warning(this, "RTMP URL Required",
                             "Enter your RTMP stream URL before starting.");
        return;
    }

    if (!controller_->setRtmpUrl(url.toStdString())) {
        QMessageBox::critical(this, "Error", "Invalid RTMP URL.");
        return;
    }

    if (!controller_->startStream()) {
        QMessageBox::critical(this, "Error",
                              "Failed to start stream. Configure scene sources first.");
        return;
    }

    syncControlsFromController();
}

void MainWindow::updateStats() {
    const StreamStats s = controller_->stats();

    QString status;
    if (s.isStreaming) {
        const auto elapsed = std::chrono::steady_clock::now() - s.streamStartTime;
        const int seconds = static_cast<int>(std::chrono::duration<double>(elapsed).count());
        const int mins = seconds / 60;
        const int secs = seconds % 60;

        status = QString("LIVE | Encoder: %1 | FPS: %2 | Frames: %3 | Dropped: %4 (%5%) | "
                         "Connected: %6 | Bytes: %7 | Reconnects: %8 | Duration: %9:%10%11%12")
                     .arg(QString::fromStdString(s.encoderName))
                     .arg(s.encodedFps, 0, 'f', 1)
                     .arg(s.totalFrames)
                     .arg(s.droppedFrames)
                     .arg(s.dropRate, 0, 'f', 3)
                     .arg(s.isConnected ? "Yes" : "No")
                     .arg(s.bytesSent)
                     .arg(s.reconnectCount)
                     .arg(mins, 2, 10, QChar('0'))
                     .arg(secs, 2, 10, QChar('0'))
                     .arg(s.isRecording ? " | REC" : "")
                     .arg(s.isReplayBufferActive ? " | REPLAY" : "");
    } else if (s.isRecording || s.isReplayBufferActive) {
        status = QString("Encoding (%1)%2%3")
                     .arg(QString::fromStdString(s.encoderName.empty() ? "…" : s.encoderName))
                     .arg(s.isRecording ? " | REC" : "")
                     .arg(s.isReplayBufferActive ? " | REPLAY" : "");
    } else {
        status = "Ready — configure scenes and sources, enter RTMP URL, then Start Streaming";
    }

    statusBar()->showMessage(status);
}

} // namespace railshot
