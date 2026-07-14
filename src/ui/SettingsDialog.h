#pragma once

#include "core/AppSettings.h"

#include <QDialog>
#include <QString>

class QSpinBox;
class QLineEdit;
class QKeySequenceEdit;
class QComboBox;
class QCheckBox;

namespace railshot {

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    [[nodiscard]] AppSettingsData resultSettings() const;
    [[nodiscard]] bool videoSettingsChanged() const { return videoChanged_; }
    [[nodiscard]] QString activeCollectionRename() const;

private:
    void loadUi();
    void syncStreamFieldsFromService();
    AppSettingsData collectDraft() const;

    AppSettingsData original_;
    AppSettingsData draft_;
    bool videoChanged_ = false;

    QSpinBox* widthSpin_ = nullptr;
    QSpinBox* heightSpin_ = nullptr;
    QSpinBox* fpsSpin_ = nullptr;
    QLineEdit* rtmpEdit_ = nullptr;
    QLineEdit* collectionNameEdit_ = nullptr;

    QCheckBox* monitorEnableCheck_ = nullptr;
    QComboBox* monitorDeviceCombo_ = nullptr;
    QComboBox* micDeviceCombo_ = nullptr;

    QComboBox* encoderCombo_ = nullptr;
    QComboBox* encoderPresetCombo_ = nullptr;
    QSpinBox* videoBitrateSpin_ = nullptr;
    QSpinBox* audioBitrateSpin_ = nullptr;

    QComboBox* recordingFormatCombo_ = nullptr;
    QLineEdit* recordingDirEdit_ = nullptr;
    QCheckBox* replayEnableCheck_ = nullptr;
    QSpinBox* replaySecondsSpin_ = nullptr;

    QComboBox* streamServiceCombo_ = nullptr;
    QLineEdit* streamServerEdit_ = nullptr;
    QLineEdit* streamKeyEdit_ = nullptr;
    QLineEdit* websocketPasswordEdit_ = nullptr;

    QKeySequenceEdit* hkTransition_ = nullptr;
    QKeySequenceEdit* hkStream_ = nullptr;
    QKeySequenceEdit* hkRecord_ = nullptr;
    QKeySequenceEdit* hkSaveReplay_ = nullptr;
    QKeySequenceEdit* hkScene1_ = nullptr;
    QKeySequenceEdit* hkScene2_ = nullptr;
    QKeySequenceEdit* hkScene3_ = nullptr;
    QKeySequenceEdit* hkScene4_ = nullptr;
    QKeySequenceEdit* hkP1Plus_ = nullptr;
    QKeySequenceEdit* hkP1Minus_ = nullptr;
    QKeySequenceEdit* hkP2Plus_ = nullptr;
    QKeySequenceEdit* hkP2Minus_ = nullptr;
};

} // namespace railshot
