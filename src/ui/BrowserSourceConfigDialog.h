#pragma once

#include "capture/BrowserSourceSettings.h"
#include "core/models/SourceTypes.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QShowEvent;
class QSpinBox;
class QTimer;

namespace railshot {

class BrowserSourceConfigDialog : public QDialog {
    Q_OBJECT

public:
    explicit BrowserSourceConfigDialog(Source& source, QWidget* parent = nullptr);

    [[nodiscard]] BrowserSourceSettings settings() const;
    void applyToSource(Source& source) const;
    [[nodiscard]] bool sizeChangedFromOpen() const;

signals:
    void settingsChanged(const BrowserSourceSettings& settings);

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    void onBrowseLocalFile();
    void onRestoreDefaults();
    void onRefreshCache();
    void updatePreview();

private:
    void buildUi();
    void loadFromSource();
    void syncSettingsFromUi();
    void emitLiveUpdate();
    void updateDependentVisibility();

    Source& source_;
    BrowserSourceSettings settings_;
    int openedWidth_ = BrowserSourceSettings::kDefaultWidth;
    int openedHeight_ = BrowserSourceSettings::kDefaultHeight;

    QLabel* previewLabel_ = nullptr;
    QCheckBox* localFileCheck_ = nullptr;
    QLineEdit* urlEdit_ = nullptr;
    QPushButton* browseBtn_ = nullptr;
    QSpinBox* widthSpin_ = nullptr;
    QSpinBox* heightSpin_ = nullptr;
    QCheckBox* rerouteAudioCheck_ = nullptr;
    QCheckBox* fpsCustomCheck_ = nullptr;
    QSpinBox* fpsSpin_ = nullptr;
    QPlainTextEdit* cssEdit_ = nullptr;
    QCheckBox* shutdownCheck_ = nullptr;
    QCheckBox* refreshActiveCheck_ = nullptr;
    QComboBox* permissionsCombo_ = nullptr;
    QPushButton* refreshCacheBtn_ = nullptr;
    QTimer* previewTimer_ = nullptr;
};

} // namespace railshot
