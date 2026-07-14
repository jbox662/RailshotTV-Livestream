#pragma once

#include "core/StreamController.h"
#include "core/models/SourceTypes.h"
#include "network/LocalWebServer.h"
#include "network/RemoteWebSocketServer.h"

#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>

#include <memory>
#include <vector>

class QShortcut;

namespace railshot {

class StudioModeWidget;
class AudioMixerWidget;
class ScoreboardWidget;
class VenueModeWindow;
class DockPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void startPreviewDeferred();
    void onStartStop();
    void onRecord();
    void onVirtualCamera();
    void onCollectionComboChanged(int index);
    void onNewCollection();
    void onRenameCollection();
    void onDuplicateCollection();
    void onDeleteCollection();
    void updateStats();
    void refreshScenesList();
    void refreshSourcesList();
    void onSceneSelectionChanged();
    void onSourceSelectionChanged();
    void onAddScene();
    void onAddSource();
    void onConfigureSource();
    void onEditFilters();
    void onOpenSettings();
    void onRemoveScene();
    void onRemoveSource();
    void onScenesChanged();
    void onStudioModeToggled(bool enabled);
    void onTransition();
    void onProfileChanged(int index);
    void onOpenVenueMode();
    void onScoreStateChanged();
    void onToggleSourceVisible();
    void onToggleSourceLocked();
    void onMoveSourceUp();
    void onMoveSourceDown();
    void syncControlsFromController();

private:
    void setupUi();
    void setupMenuBar();
    void refreshCollectionMenu();
    void assignDefaultWebcam();
    void refreshUi();
    void updateMonitorSelection(const std::string& sourceId);
    void updateSourceContextBar();
    void bindHotkeys();
    void syncActiveCollectionFromSettings();
    void refreshCollectionsCombo();
    [[nodiscard]] bool collectionSwitchBlocked(QString* reason = nullptr) const;
    void persistActiveCollectionId();
    void restoreProductionProfile();
    [[nodiscard]] std::string sourcesSceneId() const;
    QWidget* makeSourceRowWidget(const Source& src, QListWidget* list);

    std::unique_ptr<StreamController> controller_;
    std::unique_ptr<LocalWebServer> webServer_;
    std::unique_ptr<RemoteWebSocketServer> wsServer_;
    VenueModeWindow* venueWindow_ = nullptr;
    StudioModeWidget* studioMode_ = nullptr;
    AudioMixerWidget* audioMixer_ = nullptr;
    ScoreboardWidget* scoreboard_ = nullptr;
    QSplitter* bottomSplitter_ = nullptr;
    DockPanel* scenesDock_ = nullptr;
    DockPanel* sourcesDock_ = nullptr;
    DockPanel* transitionsDock_ = nullptr;
    DockPanel* scoreDock_ = nullptr;
    DockPanel* controlsDock_ = nullptr;
    QListWidget* scenesList_ = nullptr;
    QListWidget* sourcesList_ = nullptr;
    QComboBox* collectionCombo_ = nullptr;
    QComboBox* profileCombo_ = nullptr;
    QLabel* remoteUrlLabel_ = nullptr;
    QLabel* sourceContextLabel_ = nullptr;
    QPushButton* sourcePropertiesBtn_ = nullptr;
    QLineEdit* rtmpUrlEdit_ = nullptr;
    QPushButton* startStopBtn_ = nullptr;
    QPushButton* recordBtn_ = nullptr;
    QPushButton* virtualCamBtn_ = nullptr;
    QPushButton* studioModeBtn_ = nullptr;
    QComboBox* transitionCombo_ = nullptr;
    QSpinBox* transitionDurationSpin_ = nullptr;
    QMenu* collectionMenu_ = nullptr;
    QAction* studioModeAction_ = nullptr;
    QAction* profileGeneralAction_ = nullptr;
    QAction* profileSportsAction_ = nullptr;
    QTimer* statsTimer_ = nullptr;
    std::vector<QShortcut*> hotkeyShortcuts_;
    bool previewEngineRequested_ = false;
};

} // namespace railshot
