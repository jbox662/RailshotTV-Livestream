#include "ui/AudioMixer.h"

#include "core/AudioMixer.h"
#include "ui/DockPanel.h"

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace railshot {

AudioMixerWidget::AudioMixerWidget(StreamController* controller, QWidget* parent)
    : DockPanel("Audio Mixer", parent)
    , controller_(controller) {
    channelsLayout_ = new QHBoxLayout();
    channelsLayout_->setSpacing(0);
    channelsLayout_->setContentsMargins(0, 0, 0, 0);

    auto* channelsHost = new QWidget(this);
    channelsHost->setObjectName("rsMixerChannels");
    channelsHost->setLayout(channelsLayout_);
    contentLayout()->addWidget(channelsHost, 1);

    meterTimer_ = new QTimer(this);
    meterTimer_->setInterval(40);
    connect(meterTimer_, &QTimer::timeout, this, &AudioMixerWidget::onMeterTick);
    meterTimer_->start();

    rebuild();
}

QWidget* AudioMixerWidget::makeChannelStrip(const std::string& id, const std::string& name,
                                            int volume, bool muted) {
    auto* strip = new QFrame(this);
    strip->setObjectName("rsMixerStrip");
    strip->setMinimumWidth(120);
    strip->setMaximumWidth(180);

    auto* stripLayout = new QVBoxLayout(strip);
    stripLayout->setContentsMargins(8, 8, 8, 8);
    stripLayout->setSpacing(6);

    auto* nameLabel = new QLabel(QString::fromStdString(name), strip);
    nameLabel->setObjectName("rsMixerName");
    nameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    stripLayout->addWidget(nameLabel);

    auto* meter = new QProgressBar(strip);
    meter->setObjectName("rsVolumeMeter");
    meter->setRange(0, 100);
    meter->setValue(0);
    meter->setTextVisible(false);
    stripLayout->addWidget(meter);

    auto* slider = new QSlider(Qt::Horizontal, strip);
    slider->setObjectName("rsVolumeSlider");
    slider->setRange(0, 100);
    slider->setValue(volume);
    stripLayout->addWidget(slider);

    auto* bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(4);
    auto* mute = new QCheckBox(QStringLiteral("Mute"), strip);
    mute->setObjectName("rsMuteToggle");
    mute->setChecked(muted);
    mute->setToolTip("Mute");
    bottomRow->addWidget(mute);
    bottomRow->addStretch();
    stripLayout->addLayout(bottomRow);
    stripLayout->addStretch();

    const QString key = QString::fromStdString(id);
    controls_[key] = {slider, meter, mute, id, 0.0f};

    connect(slider, &QSlider::valueChanged, this, &AudioMixerWidget::onVolumeChanged);
    connect(mute, &QCheckBox::toggled, this, &AudioMixerWidget::onMuteToggled);

    return strip;
}

void AudioMixerWidget::rebuild() {
    while (channelsLayout_->count() > 0) {
        QLayoutItem* item = channelsLayout_->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    controls_.clear();

    auto& manager = SceneManager::instance();
    const Scene* scene = manager.activeScene();
    if (!scene) {
        return;
    }

    channelsLayout_->addWidget(makeChannelStrip("__mic__", "Mic/Aux", 100, false));

    for (const auto& src : scene->sources) {
        if (src.type == SourceType::MediaFile || src.type == SourceType::AudioDevice
            || src.type == SourceType::DesktopAudio) {
            channelsLayout_->addWidget(makeChannelStrip(src.id, src.name, src.volume, src.muted));
        }
    }
    channelsLayout_->addStretch();
}

void AudioMixerWidget::onMeterTick() {
    const auto peaks = AudioMixer::snapshotPeaks();
    for (auto it = controls_.begin(); it != controls_.end(); ++it) {
        float target = 0.0f;
        const auto found = peaks.find(it->sourceId);
        if (found != peaks.end()) {
            target = found->second;
        }
        // Visual decay for hold/falloff.
        it->displayLevel = std::max(target, it->displayLevel * 0.88f);
        if (it->meter) {
            it->meter->setValue(static_cast<int>(std::clamp(it->displayLevel * 100.0f, 0.0f, 100.0f)));
        }
    }
}

void AudioMixerWidget::onVolumeChanged(int value) {
    auto* slider = qobject_cast<QSlider*>(sender());
    if (!slider) {
        return;
    }

    for (auto it = controls_.begin(); it != controls_.end(); ++it) {
        if (it->slider != slider) {
            continue;
        }
        if (it->sourceId == "__mic__") {
            return;
        }
        if (Source* src = SceneManager::instance().sourceById(it->sourceId)) {
            src->volume = value;
        }
        break;
    }
}

void AudioMixerWidget::onMuteToggled(bool checked) {
    auto* mute = qobject_cast<QCheckBox*>(sender());
    if (!mute) {
        return;
    }

    for (auto it = controls_.begin(); it != controls_.end(); ++it) {
        if (it->mute != mute) {
            continue;
        }
        if (it->sourceId == "__mic__") {
            return;
        }
        if (Source* src = SceneManager::instance().sourceById(it->sourceId)) {
            src->muted = checked;
        }
        break;
    }
}

} // namespace railshot
