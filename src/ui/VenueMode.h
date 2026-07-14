#pragma once

#include "core/StreamController.h"

#include <QMainWindow>

class QCheckBox;
class QGridLayout;
class QLineEdit;

namespace railshot {

class VenueModeWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit VenueModeWindow(StreamController* controller, QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onSceneGoLive();
    void onStop();

private:
    void rebuildSceneButtons();

    StreamController* controller_ = nullptr;
    QWidget* gridHost_ = nullptr;
    QGridLayout* grid_ = nullptr;
    QLineEdit* rtmpUrlEdit_ = nullptr;
    QCheckBox* billiardPresetCheck_ = nullptr;
};

} // namespace railshot
