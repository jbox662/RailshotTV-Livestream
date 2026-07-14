#include "ui/SettingsDialog.h"

#include "capture/WasapiAudioCapture.h"
#include "core/models/SceneManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
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
    setMinimumSize(520, 520);
    resize(560, 560);
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
        QStringLiteral("Monitoring plays the mixed program audio on speakers/headphones "
                       "(like OBS audio monitoring). Mic/Aux feeds the global mic strip.\n"
                       "Application (process) audio capture is Phase F4."),
        audioPage);
    audioHint->setWordWrap(true);
    audioHint->setObjectName(QStringLiteral("rsFieldLabel"));
    audioForm->addRow(audioHint);
    tabs->addTab(audioPage, QStringLiteral("Audio"));

    auto* streamPage = new QWidget(tabs);
    auto* streamForm = new QFormLayout(streamPage);
    rtmpEdit_ = new QLineEdit(streamPage);
    rtmpEdit_->setPlaceholderText(QStringLiteral("rtmp://… / stream key"));
    streamForm->addRow(QStringLiteral("Default RTMP URL"), rtmpEdit_);
    tabs->addTab(streamPage, QStringLiteral("Stream"));

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

void SettingsDialog::loadUi() {
    widthSpin_->setValue(original_.canvasWidth);
    heightSpin_->setValue(original_.canvasHeight);
    fpsSpin_->setValue(original_.fps);
    rtmpEdit_->setText(QString::fromStdString(original_.defaultRtmpUrl));
    collectionNameEdit_->setText(
        QString::fromStdString(SceneManager::instance().collection().name));
    monitorEnableCheck_->setChecked(original_.audioMonitoringEnabled);
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
    draft.hotkeys.transition = seqToString(hkTransition_).toStdString();
    draft.hotkeys.startStopStream = seqToString(hkStream_).toStdString();
    draft.hotkeys.record = seqToString(hkRecord_).toStdString();
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
