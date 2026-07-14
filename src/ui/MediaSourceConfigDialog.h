#pragma once

#include "capture/MediaSourceSettings.h"
#include "core/models/SourceTypes.h"

#include <QDialog>

class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;
class QPushButton;

namespace railshot {

class MediaSourceConfigDialog : public QDialog {
    Q_OBJECT

public:
    explicit MediaSourceConfigDialog(Source& source, QWidget* parent = nullptr);

    void applyToSource(Source& source) const;

signals:
    void settingsChanged(const MediaSourceSettings& settings);

private slots:
    void onBrowse();
    void onRestartPlayback();
    void onTogglePause();

private:
    void buildUi();
    void loadFromSource();
    void syncSettingsFromUi();
    void emitLiveUpdate();

    Source& source_;
    MediaSourceSettings settings_;
    bool pausedUi_ = false;

    QLineEdit* pathEdit_ = nullptr;
    QPushButton* browseBtn_ = nullptr;
    QCheckBox* loopCheck_ = nullptr;
    QCheckBox* restartCheck_ = nullptr;
    QCheckBox* hwDecodeCheck_ = nullptr;
    QDoubleSpinBox* speedSpin_ = nullptr;
    QPushButton* pauseBtn_ = nullptr;
    QPushButton* restartBtn_ = nullptr;
};

} // namespace railshot
