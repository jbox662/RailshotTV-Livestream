#include "ui/MediaSourceConfigDialog.h"

#include "capture/MediaSource.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace railshot {

MediaSourceConfigDialog::MediaSourceConfigDialog(Source& source, QWidget* parent)
    : QDialog(parent)
    , source_(source)
    , settings_(MediaSourceSettings::fromSource(source)) {
    setWindowTitle(QStringLiteral("Properties for '%1'")
                       .arg(QString::fromStdString(source.name)));
    resize(520, 320);
    buildUi();
    loadFromSource();
}

void MediaSourceConfigDialog::buildUi() {
    auto* root = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    auto* pathRow = new QHBoxLayout();
    pathEdit_ = new QLineEdit(this);
    browseBtn_ = new QPushButton(QStringLiteral("Browse…"), this);
    browseBtn_->setFixedWidth(80);
    pathRow->addWidget(pathEdit_, 1);
    pathRow->addWidget(browseBtn_);
    form->addRow(QStringLiteral("Local file"), pathRow);

    loopCheck_ = new QCheckBox(QStringLiteral("Loop"), this);
    form->addRow(QString(), loopCheck_);

    restartCheck_ = new QCheckBox(QStringLiteral("Restart playback when source becomes active"), this);
    restartCheck_->setToolTip(
        QStringLiteral("Seeks to the start when the source is shown again (OBS media default)."));
    form->addRow(QString(), restartCheck_);

    hwDecodeCheck_ = new QCheckBox(QStringLiteral("Use hardware decoding when available"), this);
    form->addRow(QString(), hwDecodeCheck_);

    speedSpin_ = new QDoubleSpinBox(this);
    speedSpin_->setRange(1.0, 200.0);
    speedSpin_->setDecimals(0);
    speedSpin_->setSuffix(QStringLiteral(" %"));
    speedSpin_->setValue(100.0);
    form->addRow(QStringLiteral("Speed"), speedSpin_);

    root->addLayout(form);

    auto* transport = new QHBoxLayout();
    pauseBtn_ = new QPushButton(QStringLiteral("Pause"), this);
    restartBtn_ = new QPushButton(QStringLiteral("Restart"), this);
    transport->addWidget(pauseBtn_);
    transport->addWidget(restartBtn_);
    transport->addStretch(1);
    root->addLayout(transport);

    auto* hint = new QLabel(
        QStringLiteral("Pause / Restart apply live to the active media source."), this);
    hint->setWordWrap(true);
    hint->setObjectName(QStringLiteral("rsMutedHint"));
    root->addWidget(hint);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    connect(browseBtn_, &QPushButton::clicked, this, &MediaSourceConfigDialog::onBrowse);
    connect(pathEdit_, &QLineEdit::textChanged, this, [this]() { emitLiveUpdate(); });
    connect(loopCheck_, &QCheckBox::toggled, this, [this](bool) { emitLiveUpdate(); });
    connect(restartCheck_, &QCheckBox::toggled, this, [this](bool) { emitLiveUpdate(); });
    connect(hwDecodeCheck_, &QCheckBox::toggled, this, [this](bool) { emitLiveUpdate(); });
    connect(speedSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double) { emitLiveUpdate(); });
    connect(pauseBtn_, &QPushButton::clicked, this, &MediaSourceConfigDialog::onTogglePause);
    connect(restartBtn_, &QPushButton::clicked, this, &MediaSourceConfigDialog::onRestartPlayback);
}

void MediaSourceConfigDialog::loadFromSource() {
    pathEdit_->blockSignals(true);
    loopCheck_->blockSignals(true);
    restartCheck_->blockSignals(true);
    hwDecodeCheck_->blockSignals(true);
    speedSpin_->blockSignals(true);

    pathEdit_->setText(settings_.localFile);
    loopCheck_->setChecked(settings_.looping);
    restartCheck_->setChecked(settings_.restartOnActivate);
    hwDecodeCheck_->setChecked(settings_.hardwareDecode);
    speedSpin_->setValue(settings_.speedPercent);

    pathEdit_->blockSignals(false);
    loopCheck_->blockSignals(false);
    restartCheck_->blockSignals(false);
    hwDecodeCheck_->blockSignals(false);
    speedSpin_->blockSignals(false);

    syncSettingsFromUi();
}

void MediaSourceConfigDialog::syncSettingsFromUi() {
    settings_.localFile = pathEdit_->text().trimmed();
    settings_.isLocalFile = true;
    settings_.looping = loopCheck_->isChecked();
    settings_.restartOnActivate = restartCheck_->isChecked();
    settings_.hardwareDecode = hwDecodeCheck_->isChecked();
    settings_.speedPercent = speedSpin_->value();
}

void MediaSourceConfigDialog::emitLiveUpdate() {
    syncSettingsFromUi();
    emit settingsChanged(settings_);
}

void MediaSourceConfigDialog::applyToSource(Source& source) const {
    MediaSourceSettings copy = settings_;
    copy.localFile = pathEdit_->text().trimmed();
    copy.looping = loopCheck_->isChecked();
    copy.restartOnActivate = restartCheck_->isChecked();
    copy.hardwareDecode = hwDecodeCheck_->isChecked();
    copy.speedPercent = speedSpin_->value();
    copy.applyToSource(source);
}

void MediaSourceConfigDialog::onBrowse() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Select Media"), {},
        QStringLiteral("Media (*.mp4 *.mkv *.mov *.webm *.avi *.mp3 *.wav *.flac *.aac);;All files (*.*)"));
    if (path.isEmpty()) {
        return;
    }
    pathEdit_->setText(path);
    emitLiveUpdate();
}

void MediaSourceConfigDialog::onRestartPlayback() {
    syncSettingsFromUi();
    settings_.applyToSource(source_);
    MediaSource::restartPlayback(source_.id);
}

void MediaSourceConfigDialog::onTogglePause() {
    pausedUi_ = !pausedUi_;
    pauseBtn_->setText(pausedUi_ ? QStringLiteral("Play") : QStringLiteral("Pause"));
    MediaSource::setPaused(source_.id, pausedUi_);
}

} // namespace railshot
