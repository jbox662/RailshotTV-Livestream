#pragma once

#include "core/StreamController.h"
#include "core/models/SceneManager.h"
#include "ui/DockPanel.h"

#include <QCheckBox>
#include <QHash>
#include <QHBoxLayout>
#include <QProgressBar>
#include <QSlider>
#include <QTimer>

#include <unordered_map>

namespace railshot {

class AudioMixerWidget : public DockPanel {
    Q_OBJECT

public:
    explicit AudioMixerWidget(StreamController* controller, QWidget* parent = nullptr);

    void rebuild();

private slots:
    void onVolumeChanged(int value);
    void onMuteToggled(bool checked);
    void onMeterTick();

private:
    struct SourceControls {
        QSlider* slider = nullptr;
        QProgressBar* meter = nullptr;
        QCheckBox* mute = nullptr;
        std::string sourceId;
        float displayLevel = 0.0f;
    };

    QWidget* makeChannelStrip(const std::string& id, const std::string& name, int volume,
                              bool muted);

    StreamController* controller_ = nullptr;
    QHBoxLayout* channelsLayout_ = nullptr;
    QHash<QString, SourceControls> controls_;
    QTimer* meterTimer_ = nullptr;
};

} // namespace railshot
