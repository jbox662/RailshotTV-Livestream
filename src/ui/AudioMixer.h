#pragma once

#include "core/StreamController.h"
#include "core/models/SceneManager.h"
#include "ui/DockPanel.h"

#include <QCheckBox>
#include <QHash>
#include <QHBoxLayout>
#include <QProgressBar>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>

namespace railshot {

class AudioMixerWidget : public DockPanel {
    Q_OBJECT

public:
    explicit AudioMixerWidget(StreamController* controller, QWidget* parent = nullptr);

    void rebuild();

private slots:
    void onVolumeChanged(int value);
    void onMuteToggled(bool checked);
    void onDelayChanged(int value);
    void onMeterTick();

private:
    struct SourceControls {
        QSlider* slider = nullptr;
        QProgressBar* meter = nullptr;
        QCheckBox* mute = nullptr;
        QSpinBox* delaySpin = nullptr;
        std::string sourceId;
        float displayLevel = 0.0f;
    };

    QWidget* makeChannelStrip(const std::string& id, const std::string& name, int volume,
                              bool muted, int syncDelayMs);

    StreamController* controller_ = nullptr;
    QHBoxLayout* channelsLayout_ = nullptr;
    QHash<QString, SourceControls> controls_;
    QTimer* meterTimer_ = nullptr;
};

} // namespace railshot
