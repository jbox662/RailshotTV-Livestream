#include "ui/SettingsDialog.h"

#include "capture/WasapiAudioCapture.h"
#include "core/StreamServicePresets.h"
#include "core/models/SceneManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace railshot {
namespace {

QKeySequenceEdit* makeHotkeyEdit(QWidget* parent, const QString& sequence) {
    auto* edit = new QKeySequenceEdit(parent);
    edit->setKeySequence(QKeySequence(sequence));
    return edit;
}

QString seqToString(QKeySequenceEdit* edit) {
    if (!edit) {
        return {};
    }
    return edit->keySequence().toString(QKeySequence::PortableText);
}

void fillDeviceCombo(QComboBox* combo, const std::vector<AudioDeviceInfo>& devices,
                     const std::string& selectedId) {
    combo->clear();
    combo->addItem(QStringLiteral("Default"), QString());
    int select = 0;
    for (const auto& d : devices) {
        QString label = QString::fromStdString(d.name);
        if (d.isDefault) {
            label += QStringLiteral(" (Default)");
        }
        combo->addItem(label, QString::fromStdString(d.id));
        if (!selectedId.empty() && d.id == selectedId) {
            select = combo->count() - 1;
        }
    }
    combo->setCurrentIndex(select);
}

} // namespace

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
    , original_(AppSettings::instance().data())
    , draft_(original_) {
    setWindowTitle(QStringLiteral("Settings"));
    setMinimumSize(560, 600);
    resize(620, 640);
    setObjectName(QStringLiteral("rsSettingsDialog"));

    auto* root = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);

    auto* videoPage = new QWidget(tabs);
    auto* videoForm = new QFormLayout(videoPage);
    widthSpin_ = new QSpinBox(videoPage);
    widthSpin_->setRange(320, 7680);
    widthSpin_->setSingleStep(2);
    heightSpin_ = new QSpinBox(videoPage);
    heightSpin_->setRange(240, 4320);
    heightSpin_->setSingleStep(2);
    fpsSpin_ = new QSpinBox(videoPage);
    fpsSpin_->setRange(15, 60);
    collectionNameEdit_ = new QLineEdit(videoPage);
    collectionNameEdit_->setPlaceholderText(QStringLiteral("Default Collection"));
    videoForm->addRow(QStringLiteral("Canvas width"), widthSpin_);
    videoForm->addRow(QStringLiteral("Canvas height"), heightSpin_);
    videoForm->addRow(QStringLiteral("FPS"), fpsSpin_);
    videoForm->addRow(QStringLiteral("Active collection name"), collectionNameEdit_);
    auto* videoHint = new QLabel(
        QStringLiteral("Video changes apply when the preview/encoder is restarted. "
                       "Stop streaming and virtual camera before changing canvas size.\n"
                       "Renaming here updates the active scene collection. "
                       "Create/switch collections from the Scenes dock.\n"
                       "Register Virtual Camera once (admin) via scripts\\install-virtualcam.bat."),
        videoPage);
    videoHint->setWordWrap(true);
    videoHint->setObjectName(QStringLiteral("rsFieldLabel"));
    videoForm->addRow(videoHint);
    tabs->addTab(videoPage, QStringLiteral("Video"));

    auto* audioPage = new QWidget(tabs);
    auto* audioForm = new QFormLayout(audioPage);
    monitorEnableCheck_ = new QCheckBox(QStringLiteral("Monitor program mix (hear audio locally)"),
                                        audioPage);
    audioForm->addRow(monitorEnableCheck_);
    monitorDeviceCombo_ = new QComboBox(audioPage);
    micDeviceCombo_ = new QComboBox(audioPage);
    fillDeviceCombo(monitorDeviceCombo_, WasapiAudioCapture::enumerateOutputDevices(),
                    original_.monitoringDeviceId);
    fillDeviceCombo(micDeviceCombo_, WasapiAudioCapture::enumerateInputDevices(),
                    original_.micDeviceId);
    audioForm->addRow(QStringLiteral("Monitoring device"), monitorDeviceCombo_);
    audioForm->addRow(QStringLiteral("Mic/Aux device"), micDeviceCombo_);
    auto* audioHint = new QLabel(
        QStringLiteral("Monitoring plays the mixed program audio on speakers/headphones. "
                       "Mic/Aux feeds the global mic strip. Application audio is available "
                       "as an Application Audio source."),
        audioPage);
    audioHint->setWordWrap(true);
    audioHint->setObjectName(QStringLiteral("rsFieldLabel"));
    audioForm->addRow(audioHint);
    tabs->addTab(audioPage, QStringLiteral("Audio"));

    auto* outputPage = new QWidget(tabs);
    auto* outputForm = new QFormLayout(outputPage);
    encoderCombo_ = new QComboBox(outputPage);
    encoderCombo_->addItem(QStringLiteral("Auto (prefer hardware)"), QStringLiteral("auto"));
    encoderCombo_->addItem(QStringLiteral("NVIDIA NVENC (H.264)"), QStringLiteral("h264_nvenc"));
    encoderCombo_->addItem(QStringLiteral("Intel Quick Sync (H.264)"), QStringLiteral("h264_qsv"));
    encoderCombo_->addItem(QStringLiteral("AMD AMF (H.264)"), QStringLiteral("h264_amf"));
    encoderCombo_->addItem(QStringLiteral("x264 (software)"), QStringLiteral("libx264"));
    encoderPresetCombo_ = new QComboBox(outputPage);
    encoderPresetCombo_->addItem(QStringLiteral("Default"), QStringLiteral("default"));
    encoderPresetCombo_->addItem(QStringLiteral("ultrafast / p1"), QStringLiteral("ultrafast"));
    encoderPresetCombo_->addItem(QStringLiteral("veryfast / p4"), QStringLiteral("veryfast"));
    encoderPresetCombo_->addItem(QStringLiteral("medium / p5"), QStringLiteral("medium"));
    encoderPresetCombo_->addItem(QStringLiteral("slow / p7"), QStringLiteral("slow"));
    videoBitrateSpin_ = new QSpinBox(outputPage);
    videoBitrateSpin_->setRange(500, 100000);
    videoBitrateSpin_->setSuffix(QStringLiteral(" kbps"));
    audioBitrateSpin_ = new QSpinBox(outputPage);
    audioBitrateSpin_->setRange(64, 512);
    audioBitrateSpin_->setSuffix(QStringLiteral(" kbps"));
    recordingFormatCombo_ = new QComboBox(outputPage);
    recordingFormatCombo_->addItem(QStringLiteral("MP4"), QStringLiteral("mp4"));
    recordingFormatCombo_->addItem(QStringLiteral("MKV"), QStringLiteral("mkv"));
    recordingFormatCombo_->addItem(QStringLiteral("MOV"), QStringLiteral("mov"));
    recordingDirEdit_ = new QLineEdit(outputPage);
    recordingDirEdit_->setPlaceholderText(QStringLiteral("Movies/RailShot/Recordings (default)"));
    auto* dirRow = new QWidget(outputPage);
    auto* dirLayout = new QHBoxLayout(dirRow);
    dirLayout->setContentsMargins(0, 0, 0, 0);
    dirLayout->addWidget(recordingDirEdit_, 1);
    auto* browseBtn = new QPushButton(QStringLiteral("Browse…"), dirRow);
    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(
            this, QStringLiteral("Recording folder"), recordingDirEdit_->text());
        if (!dir.isEmpty()) {
            recordingDirEdit_->setText(dir);
        }
    });
    dirLayout->addWidget(browseBtn);
    replayEnableCheck_ = new QCheckBox(QStringLiteral("Enable replay buffer (auto-start with stream)"),
                                       outputPage);
    replaySecondsSpin_ = new QSpinBox(outputPage);
    replaySecondsSpin_->setRange(5, 300);
    replaySecondsSpin_->setSuffix(QStringLiteral(" s"));
    outputForm->addRow(QStringLiteral("Video encoder"), encoderCombo_);
    outputForm->addRow(QStringLiteral("Encoder preset"), encoderPresetCombo_);
    outputForm->addRow(QStringLiteral("Video bitrate"), videoBitrateSpin_);
    outputForm->addRow(QStringLiteral("Audio bitrate"), audioBitrateSpin_);
    outputForm->addRow(QStringLiteral("Recording format"), recordingFormatCombo_);
    outputForm->addRow(QStringLiteral("Recording folder"), dirRow);
    outputForm->addRow(replayEnableCheck_);
    outputForm->addRow(QStringLiteral("Replay length"), replaySecondsSpin_);
    auto* outputHint = new QLabel(
        QStringLiteral("Encoder changes apply the next time encoding starts "
                       "(stream, record, or replay). Recording can run without streaming. "
                       "Save Replay uses the bound hotkey (default F12). "
                       "Remux is available from File → Remux Recording…."),
        outputPage);
    outputHint->setWordWrap(true);
    outputHint->setObjectName(QStringLiteral("rsFieldLabel"));
    outputForm->addRow(outputHint);
    tabs->addTab(outputPage, QStringLiteral("Output"));

    auto* streamPage = new QWidget(tabs);
    auto* streamForm = new QFormLayout(streamPage);
    streamServiceCombo_ = new QComboBox(streamPage);
    for (const auto& preset : streamServicePresets()) {
        streamServiceCombo_->addItem(QString::fromStdString(preset.displayName),
                                     QString::fromStdString(preset.id));
    }
    streamServerEdit_ = new QLineEdit(streamPage);
    streamKeyEdit_ = new QLineEdit(streamPage);
    streamKeyEdit_->setEchoMode(QLineEdit::Password);
    rtmpEdit_ = new QLineEdit(streamPage);
    rtmpEdit_->setPlaceholderText(QStringLiteral("rtmp://… / stream key"));
    streamForm->addRow(QStringLiteral("Service"), streamServiceCombo_);
    streamForm->addRow(QStringLiteral("Server"), streamServerEdit_);
    streamForm->addRow(QStringLiteral("Stream key"), streamKeyEdit_);
    streamForm->addRow(QStringLiteral("Combined RTMP URL"), rtmpEdit_);
    websocketPasswordEdit_ = new QLineEdit(streamPage);
    websocketPasswordEdit_->setEchoMode(QLineEdit::Password);
    websocketPasswordEdit_->setPlaceholderText(QStringLiteral("Empty = no auth (local tools)"));
    streamForm->addRow(QStringLiteral("obs-websocket password"), websocketPasswordEdit_);
    auto* streamHint = new QLabel(
        QStringLiteral("Pick a service to fill the ingest server, then paste your stream key. "
                       "Combined URL is used by the main bar and Start Streaming.\n"
                       "WebSocket remote listens on port 4455 with obs-websocket v5-compatible "
                       "Hello/Identify/Request. Leave password empty for unauthenticated local use."),
        streamPage);
    streamHint->setWordWrap(true);
    streamHint->setObjectName(QStringLiteral("rsFieldLabel"));
    streamForm->addRow(streamHint);
    tabs->addTab(streamPage, QStringLiteral("Stream"));

    connect(streamServiceCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { syncStreamFieldsFromService(); });
    auto refreshCombined = [this]() {
        const QString server = streamServerEdit_->text().trimmed();
        const QString key = streamKeyEdit_->text().trimmed();
        if (streamServiceCombo_->currentData().toString() == QStringLiteral("Custom") && server.isEmpty()) {
            return;
        }
        rtmpEdit_->setText(QString::fromStdString(
            combineRtmpUrl(server.toStdString(), key.toStdString())));
    };
    connect(streamServerEdit_, &QLineEdit::editingFinished, this, refreshCombined);
    connect(streamKeyEdit_, &QLineEdit::editingFinished, this, refreshCombined);

    auto* hotkeysPage = new QWidget(tabs);
    auto* hotkeysForm = new QFormLayout(hotkeysPage);
    auto* hotkeysHint = new QLabel(
        QStringLiteral("Shortcuts work while RailShot is focused. Click a field and press keys."),
        hotkeysPage);
    hotkeysHint->setWordWrap(true);
    hotkeysHint->setObjectName(QStringLiteral("rsFieldLabel"));
    hotkeysForm->addRow(hotkeysHint);
    hkTransition_ = makeHotkeyEdit(hotkeysPage, QString::fromStdString(original_.hotkeys.transition));
    hkStream_ = makeHotkeyEdit(hotkeysPage, QString::fromStdString(original_.hotkeys.startStopStream));
    hkRecord_ = makeHotkeyEdit(hotkeysPage, QString::fromStdString(original_.hotkeys.record));
    hkSaveReplay_ = makeHotkeyEdit(hotkeysPage, QString::fromStdString(original_.hotkeys.saveReplay));
    hkScene1_ = makeHotkeyEdit(hotkeysPage, QString::fromStdString(original_.hotkeys.scene1));
    hkScene2_ = makeHotkeyEdit(hotkeysPage, QString::fromStdString(original_.hotkeys.scene2));
    hkScene3_ = makeHotkeyEdit(hotkeysPage, QString::fromStdString(original_.hotkeys.scene3));
    hkScene4_ = makeHotkeyEdit(hotkeysPage, QString::fromStdString(original_.hotkeys.scene4));
    hkP1Plus_ = makeHotkeyEdit(hotkeysPage, QString::fromStdString(original_.hotkeys.scoreP1Plus));
    hkP1Minus_ = makeHotkeyEdit(hotkeysPage, QString::fromStdString(original_.hotkeys.scoreP1Minus));
    hkP2Plus_ = makeHotkeyEdit(hotkeysPage, QString::fromStdString(original_.hotkeys.scoreP2Plus));
    hkP2Minus_ = makeHotkeyEdit(hotkeysPage, QString::fromStdString(original_.hotkeys.scoreP2Minus));
    hotkeysForm->addRow(QStringLiteral("Studio transition"), hkTransition_);
    hotkeysForm->addRow(QStringLiteral("Start / stop stream"), hkStream_);
    hotkeysForm->addRow(QStringLiteral("Start / stop record"), hkRecord_);
    hotkeysForm->addRow(QStringLiteral("Save replay"), hkSaveReplay_);
    hotkeysForm->addRow(QStringLiteral("Scene 1"), hkScene1_);
    hotkeysForm->addRow(QStringLiteral("Scene 2"), hkScene2_);
    hotkeysForm->addRow(QStringLiteral("Scene 3"), hkScene3_);
    hotkeysForm->addRow(QStringLiteral("Scene 4"), hkScene4_);
    hotkeysForm->addRow(QStringLiteral("Player 1 +1"), hkP1Plus_);
    hotkeysForm->addRow(QStringLiteral("Player 1 -1"), hkP1Minus_);
    hotkeysForm->addRow(QStringLiteral("Player 2 +1"), hkP2Plus_);
    hotkeysForm->addRow(QStringLiteral("Player 2 -1"), hkP2Minus_);
    tabs->addTab(hotkeysPage, QStringLiteral("Hotkeys"));

    root->addWidget(tabs);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        draft_ = collectDraft();
        videoChanged_ = draft_.canvasWidth != original_.canvasWidth
                        || draft_.canvasHeight != original_.canvasHeight
                        || draft_.fps != original_.fps;
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    loadUi();
}

void SettingsDialog::syncStreamFieldsFromService() {
    const QString id = streamServiceCombo_->currentData().toString();
    if (const auto* preset = findStreamService(id.toStdString())) {
        if (!preset->serverUrl.empty()) {
            streamServerEdit_->setText(QString::fromStdString(preset->serverUrl));
        }
        if (id != QStringLiteral("Custom")) {
            rtmpEdit_->setText(QString::fromStdString(
                combineRtmpUrl(streamServerEdit_->text().trimmed().toStdString(),
                               streamKeyEdit_->text().trimmed().toStdString())));
        }
    }
}

void SettingsDialog::loadUi() {
    widthSpin_->setValue(original_.canvasWidth);
    heightSpin_->setValue(original_.canvasHeight);
    fpsSpin_->setValue(original_.fps);
    rtmpEdit_->setText(QString::fromStdString(original_.defaultRtmpUrl));
    collectionNameEdit_->setText(
        QString::fromStdString(SceneManager::instance().collection().name));
    monitorEnableCheck_->setChecked(original_.audioMonitoringEnabled);

    const int encIdx = encoderCombo_->findData(QString::fromStdString(original_.videoEncoder));
    encoderCombo_->setCurrentIndex(encIdx >= 0 ? encIdx : 0);
    const int presetIdx =
        encoderPresetCombo_->findData(QString::fromStdString(original_.encoderPreset));
    encoderPresetCombo_->setCurrentIndex(presetIdx >= 0 ? presetIdx : 0);
    videoBitrateSpin_->setValue(original_.videoBitrateKbps);
    audioBitrateSpin_->setValue(original_.audioBitrateKbps);
    const int fmtIdx =
        recordingFormatCombo_->findData(QString::fromStdString(original_.recordingFormat));
    recordingFormatCombo_->setCurrentIndex(fmtIdx >= 0 ? fmtIdx : 0);
    recordingDirEdit_->setText(QString::fromStdString(original_.recordingDirectory));
    replayEnableCheck_->setChecked(original_.replayBufferEnabled);
    replaySecondsSpin_->setValue(original_.replayBufferSeconds);

    const int svcIdx =
        streamServiceCombo_->findData(QString::fromStdString(original_.streamService));
    streamServiceCombo_->blockSignals(true);
    streamServiceCombo_->setCurrentIndex(svcIdx >= 0 ? svcIdx : 0);
    streamServiceCombo_->blockSignals(false);
    streamServerEdit_->setText(QString::fromStdString(original_.streamServer));
    streamKeyEdit_->setText(QString::fromStdString(original_.streamKey));
    websocketPasswordEdit_->setText(QString::fromStdString(original_.websocketPassword));
}

AppSettingsData SettingsDialog::collectDraft() const {
    AppSettingsData draft = original_;
    draft.canvasWidth = widthSpin_->value();
    draft.canvasHeight = heightSpin_->value();
    draft.fps = fpsSpin_->value();
    draft.defaultRtmpUrl = rtmpEdit_->text().trimmed().toStdString();
    draft.activeCollectionId = SceneManager::instance().currentCollectionId();
    draft.audioMonitoringEnabled = monitorEnableCheck_->isChecked();
    draft.monitoringDeviceId = monitorDeviceCombo_->currentData().toString().toStdString();
    draft.micDeviceId = micDeviceCombo_->currentData().toString().toStdString();

    draft.videoEncoder = encoderCombo_->currentData().toString().toStdString();
    draft.encoderPreset = encoderPresetCombo_->currentData().toString().toStdString();
    draft.videoBitrateKbps = videoBitrateSpin_->value();
    draft.audioBitrateKbps = audioBitrateSpin_->value();
    draft.recordingFormat = recordingFormatCombo_->currentData().toString().toStdString();
    draft.recordingDirectory = recordingDirEdit_->text().trimmed().toStdString();
    draft.replayBufferEnabled = replayEnableCheck_->isChecked();
    draft.replayBufferSeconds = replaySecondsSpin_->value();
    draft.streamService = streamServiceCombo_->currentData().toString().toStdString();
    draft.streamServer = streamServerEdit_->text().trimmed().toStdString();
    draft.streamKey = streamKeyEdit_->text().trimmed().toStdString();
    draft.websocketPassword = websocketPasswordEdit_->text().toStdString();
    if (draft.streamService != "Custom") {
        draft.defaultRtmpUrl = combineRtmpUrl(draft.streamServer, draft.streamKey);
    }

    draft.hotkeys.transition = seqToString(hkTransition_).toStdString();
    draft.hotkeys.startStopStream = seqToString(hkStream_).toStdString();
    draft.hotkeys.record = seqToString(hkRecord_).toStdString();
    draft.hotkeys.saveReplay = seqToString(hkSaveReplay_).toStdString();
    draft.hotkeys.scene1 = seqToString(hkScene1_).toStdString();
    draft.hotkeys.scene2 = seqToString(hkScene2_).toStdString();
    draft.hotkeys.scene3 = seqToString(hkScene3_).toStdString();
    draft.hotkeys.scene4 = seqToString(hkScene4_).toStdString();
    draft.hotkeys.scoreP1Plus = seqToString(hkP1Plus_).toStdString();
    draft.hotkeys.scoreP1Minus = seqToString(hkP1Minus_).toStdString();
    draft.hotkeys.scoreP2Plus = seqToString(hkP2Plus_).toStdString();
    draft.hotkeys.scoreP2Minus = seqToString(hkP2Minus_).toStdString();
    return draft;
}

AppSettingsData SettingsDialog::resultSettings() const {
    return draft_;
}

QString SettingsDialog::activeCollectionRename() const {
    return collectionNameEdit_ ? collectionNameEdit_->text().trimmed() : QString();
}

} // namespace railshot
