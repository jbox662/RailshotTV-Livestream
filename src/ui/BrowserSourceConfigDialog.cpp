#include "ui/BrowserSourceConfigDialog.h"

#include "capture/BrowserSource.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

namespace railshot {

BrowserSourceConfigDialog::BrowserSourceConfigDialog(Source& source, QWidget* parent)
    : QDialog(parent)
    , source_(source)
    , settings_(BrowserSourceSettings::fromSource(source))
    , openedWidth_(settings_.width)
    , openedHeight_(settings_.height) {
    setWindowTitle(QStringLiteral("Properties for '%1'")
                       .arg(QString::fromStdString(source.name)));
    resize(580, 720);
    buildUi();
    loadFromSource();

    previewTimer_ = new QTimer(this);
    connect(previewTimer_, &QTimer::timeout, this, &BrowserSourceConfigDialog::updatePreview);
    previewTimer_->start(500);
    updatePreview();
}

void BrowserSourceConfigDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    QTimer::singleShot(0, this, [this]() {
        if (!widthSpin_ || !heightSpin_) {
            return;
        }
        if (widthSpin_->value() < 64 || heightSpin_->value() < 64) {
            widthSpin_->blockSignals(true);
            heightSpin_->blockSignals(true);
            widthSpin_->setValue(std::max(openedWidth_, BrowserSourceSettings::kDefaultWidth));
            heightSpin_->setValue(std::max(openedHeight_, BrowserSourceSettings::kDefaultHeight));
            widthSpin_->blockSignals(false);
            heightSpin_->blockSignals(false);
            syncSettingsFromUi();
        }
    });
}

void BrowserSourceConfigDialog::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(8);
    root->setContentsMargins(10, 10, 10, 10);

    previewLabel_ = new QLabel(this);
    previewLabel_->setObjectName("rsBrowserPreview");
    previewLabel_->setMinimumHeight(200);
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setFrameShape(QFrame::StyledPanel);
    previewLabel_->setText(QStringLiteral("Preview"));
    root->addWidget(previewLabel_);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* formHost = new QWidget(scroll);
    auto* form = new QVBoxLayout(formHost);
    form->setSpacing(8);

    localFileCheck_ = new QCheckBox(QStringLiteral("Local file"), formHost);
    form->addWidget(localFileCheck_);

    auto* urlRow = new QHBoxLayout();
    urlEdit_ = new QLineEdit(formHost);
    browseBtn_ = new QPushButton(QStringLiteral("Browse…"), formHost);
    browseBtn_->setFixedWidth(80);
    urlRow->addWidget(new QLabel(QStringLiteral("URL"), formHost));
    urlRow->addWidget(urlEdit_, 1);
    urlRow->addWidget(browseBtn_);
    form->addLayout(urlRow);

    auto* sizeGrid = new QGridLayout();
    widthSpin_ = new QSpinBox(formHost);
    widthSpin_->setRange(8, 8192);
    widthSpin_->setValue(BrowserSourceSettings::kDefaultWidth);
    heightSpin_ = new QSpinBox(formHost);
    heightSpin_->setRange(8, 8192);
    heightSpin_->setValue(BrowserSourceSettings::kDefaultHeight);
    sizeGrid->addWidget(new QLabel(QStringLiteral("Width"), formHost), 0, 0);
    sizeGrid->addWidget(widthSpin_, 0, 1);
    sizeGrid->addWidget(new QLabel(QStringLiteral("Height"), formHost), 1, 0);
    sizeGrid->addWidget(heightSpin_, 1, 1);
    form->addLayout(sizeGrid);

    rerouteAudioCheck_ = new QCheckBox(QStringLiteral("Control audio via RailShot"), formHost);
    form->addWidget(rerouteAudioCheck_);

    fpsCustomCheck_ = new QCheckBox(QStringLiteral("Use custom frame rate"), formHost);
    form->addWidget(fpsCustomCheck_);

    auto* fpsRow = new QHBoxLayout();
    fpsSpin_ = new QSpinBox(formHost);
    fpsSpin_->setRange(1, 60);
    fpsSpin_->setValue(BrowserSourceSettings::kDefaultFps);
    fpsRow->addWidget(new QLabel(QStringLiteral("FPS"), formHost));
    fpsRow->addWidget(fpsSpin_);
    fpsRow->addStretch(1);
    form->addLayout(fpsRow);

    auto* fpsHint = new QLabel(
        QStringLiteral("When custom FPS is off, the browser capture rate follows Settings → Canvas FPS."),
        formHost);
    fpsHint->setWordWrap(true);
    fpsHint->setObjectName(QStringLiteral("rsMutedHint"));
    form->addWidget(fpsHint);

    auto* cssHeader = new QHBoxLayout();
    cssHeader->addWidget(new QLabel(QStringLiteral("Custom CSS"), formHost));
    cssHeader->addStretch(1);
    auto* resetCssBtn = new QPushButton(QStringLiteral("Transparent default"), formHost);
    resetCssBtn->setToolTip(
        QStringLiteral("Reset CSS so the page body is transparent (typical for Scoreholio overlays)."));
    cssHeader->addWidget(resetCssBtn);
    form->addLayout(cssHeader);
    cssEdit_ = new QPlainTextEdit(formHost);
    cssEdit_->setMinimumHeight(90);
    cssEdit_->setPlaceholderText(
        QStringLiteral("body { background-color: rgba(0, 0, 0, 0); margin: 0px auto; overflow: hidden; }"));
    form->addWidget(cssEdit_);

    shutdownCheck_ = new QCheckBox(QStringLiteral("Shutdown source when not visible"), formHost);
    shutdownCheck_->setToolTip(
        QStringLiteral("Stops WebView2 capture while this source is hidden to reduce CPU/GPU use."));
    form->addWidget(shutdownCheck_);

    refreshActiveCheck_ =
        new QCheckBox(QStringLiteral("Refresh browser when scene becomes active"), formHost);
    refreshActiveCheck_->setToolTip(
        QStringLiteral("Reloads the page when the source becomes visible again (after shutdown or scene switch)."));
    form->addWidget(refreshActiveCheck_);

    auto* permRow = new QHBoxLayout();
    permissionsCombo_ = new QComboBox(formHost);
    permissionsCombo_->addItem(QStringLiteral("No access to RailShot"),
                               static_cast<int>(BrowserPagePermission::None));
    permissionsCombo_->addItem(QStringLiteral("Read access to RailShot status information"),
                               static_cast<int>(BrowserPagePermission::ReadApp));
    permissionsCombo_->addItem(
        QStringLiteral("Read access to user information (current scene collection)"),
        static_cast<int>(BrowserPagePermission::ReadUser));
    permissionsCombo_->addItem(QStringLiteral("Basic access to RailShot (Save replay buffer, etc.)"),
                               static_cast<int>(BrowserPagePermission::Basic));
    permissionsCombo_->addItem(
        QStringLiteral("Advanced access to RailShot (Change scenes, start/stop replay buffer)"),
        static_cast<int>(BrowserPagePermission::Advanced));
    permissionsCombo_->addItem(
        QStringLiteral("Full access to RailShot (Start/stop stream without warning)"),
        static_cast<int>(BrowserPagePermission::All));
    permRow->addWidget(new QLabel(QStringLiteral("Page permissions"), formHost));
    permRow->addWidget(permissionsCombo_, 1);
    form->addLayout(permRow);

    refreshCacheBtn_ = new QPushButton(QStringLiteral("Refresh cache of current page"), formHost);
    form->addWidget(refreshCacheBtn_);
    form->addStretch(1);

    scroll->setWidget(formHost);
    root->addWidget(scroll, 1);

    auto* buttonRow = new QHBoxLayout();
    auto* defaultsBtn = new QPushButton(QStringLiteral("Defaults"), this);
    connect(defaultsBtn, &QPushButton::clicked, this, &BrowserSourceConfigDialog::onRestoreDefaults);
    buttonRow->addWidget(defaultsBtn);
    buttonRow->addStretch(1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    buttonRow->addWidget(buttons);
    root->addLayout(buttonRow);

    connect(localFileCheck_, &QCheckBox::toggled, this, [this](bool checked) {
        browseBtn_->setEnabled(checked);
        emitLiveUpdate();
    });
    connect(urlEdit_, &QLineEdit::textChanged, this, [this]() { emitLiveUpdate(); });
    connect(widthSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int) { emitLiveUpdate(); });
    connect(heightSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int) { emitLiveUpdate(); });
    connect(rerouteAudioCheck_, &QCheckBox::toggled, this, [this](bool) { emitLiveUpdate(); });
    connect(fpsCustomCheck_, &QCheckBox::toggled, this, [this](bool) {
        updateDependentVisibility();
        emitLiveUpdate();
    });
    connect(fpsSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int) { emitLiveUpdate(); });
    connect(cssEdit_, &QPlainTextEdit::textChanged, this, [this]() { emitLiveUpdate(); });
    connect(shutdownCheck_, &QCheckBox::toggled, this, [this](bool) { emitLiveUpdate(); });
    connect(refreshActiveCheck_, &QCheckBox::toggled, this, [this](bool) { emitLiveUpdate(); });
    connect(permissionsCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { emitLiveUpdate(); });
    connect(browseBtn_, &QPushButton::clicked, this, &BrowserSourceConfigDialog::onBrowseLocalFile);
    connect(refreshCacheBtn_, &QPushButton::clicked, this, &BrowserSourceConfigDialog::onRefreshCache);
    connect(resetCssBtn, &QPushButton::clicked, this, [this]() {
        cssEdit_->setPlainText(QString::fromUtf8(BrowserSourceSettings::kDefaultCss));
        emitLiveUpdate();
    });
}

void BrowserSourceConfigDialog::updateDependentVisibility() {
    const bool customFps = fpsCustomCheck_ && fpsCustomCheck_->isChecked();
    if (fpsSpin_) {
        fpsSpin_->setEnabled(customFps);
        fpsSpin_->setVisible(customFps);
    }
}

void BrowserSourceConfigDialog::loadFromSource() {
    localFileCheck_->blockSignals(true);
    urlEdit_->blockSignals(true);
    widthSpin_->blockSignals(true);
    heightSpin_->blockSignals(true);
    rerouteAudioCheck_->blockSignals(true);
    fpsCustomCheck_->blockSignals(true);
    fpsSpin_->blockSignals(true);
    cssEdit_->blockSignals(true);
    shutdownCheck_->blockSignals(true);
    refreshActiveCheck_->blockSignals(true);
    permissionsCombo_->blockSignals(true);

    localFileCheck_->setChecked(settings_.isLocalFile);
    urlEdit_->setText(settings_.url);
    widthSpin_->setValue(settings_.width > 0 ? settings_.width : BrowserSourceSettings::kDefaultWidth);
    heightSpin_->setValue(settings_.height > 0 ? settings_.height
                                               : BrowserSourceSettings::kDefaultHeight);
    rerouteAudioCheck_->setChecked(settings_.rerouteAudio);
    fpsCustomCheck_->setChecked(settings_.fpsCustom);
    fpsSpin_->setValue(settings_.fps);
    cssEdit_->setPlainText(settings_.customCss);
    shutdownCheck_->setChecked(settings_.shutdownWhenNotVisible);
    refreshActiveCheck_->setChecked(settings_.refreshWhenActive);
    const int permIndex =
        permissionsCombo_->findData(static_cast<int>(settings_.pagePermissions));
    permissionsCombo_->setCurrentIndex(permIndex >= 0 ? permIndex : 1);
    browseBtn_->setEnabled(settings_.isLocalFile);
    updateDependentVisibility();

    localFileCheck_->blockSignals(false);
    urlEdit_->blockSignals(false);
    widthSpin_->blockSignals(false);
    heightSpin_->blockSignals(false);
    rerouteAudioCheck_->blockSignals(false);
    fpsCustomCheck_->blockSignals(false);
    fpsSpin_->blockSignals(false);
    cssEdit_->blockSignals(false);
    shutdownCheck_->blockSignals(false);
    refreshActiveCheck_->blockSignals(false);
    permissionsCombo_->blockSignals(false);

    syncSettingsFromUi();
}

BrowserSourceSettings BrowserSourceConfigDialog::settings() const {
    return settings_;
}

void BrowserSourceConfigDialog::syncSettingsFromUi() {
    settings_.isLocalFile = localFileCheck_->isChecked();
    settings_.url = urlEdit_->text().trimmed();
    settings_.width = widthSpin_->value();
    settings_.height = heightSpin_->value();
    settings_.rerouteAudio = rerouteAudioCheck_->isChecked();
    settings_.fpsCustom = fpsCustomCheck_->isChecked();
    settings_.fps = fpsSpin_->value();
    settings_.customCss = cssEdit_->toPlainText();
    settings_.shutdownWhenNotVisible = shutdownCheck_->isChecked();
    settings_.refreshWhenActive = refreshActiveCheck_->isChecked();
    settings_.pagePermissions = static_cast<BrowserPagePermission>(
        permissionsCombo_->currentData().toInt());
    settings_.clampDimensions();
    settings_.fps = std::clamp(settings_.fps, 1, 60);
}

void BrowserSourceConfigDialog::emitLiveUpdate() {
    syncSettingsFromUi();
    emit settingsChanged(settings_);
}

void BrowserSourceConfigDialog::applyToSource(Source& source) const {
    BrowserSourceSettings copy = settings_;
    copy.clampDimensions();
    if (copy.width < 64) {
        copy.width = BrowserSourceSettings::kDefaultWidth;
    }
    if (copy.height < 64) {
        copy.height = BrowserSourceSettings::kDefaultHeight;
    }

    const float x = source.transform.x;
    const float y = source.transform.y;
    copy.applyToSource(source);
    source.transform.x = x;
    source.transform.y = y;
}

bool BrowserSourceConfigDialog::sizeChangedFromOpen() const {
    return settings_.width != openedWidth_ || settings_.height != openedHeight_;
}

void BrowserSourceConfigDialog::onBrowseLocalFile() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Select local HTML file"), {},
        QStringLiteral("HTML files (*.html *.htm);;All files (*.*)"));
    if (path.isEmpty()) {
        return;
    }
    urlEdit_->setText(path);
    localFileCheck_->setChecked(true);
    emitLiveUpdate();
}

void BrowserSourceConfigDialog::onRestoreDefaults() {
    settings_ = BrowserSourceSettings::defaults();
    openedWidth_ = settings_.width;
    openedHeight_ = settings_.height;
    loadFromSource();
    emitLiveUpdate();
}

void BrowserSourceConfigDialog::onRefreshCache() {
    BrowserSource::reloadAll();
    emitLiveUpdate();
}

void BrowserSourceConfigDialog::updatePreview() {
    previewLabel_->setText(
        QStringLiteral("Preview (%1 × %2)\nUpdates live in the canvas while this dialog is open")
            .arg(settings_.width)
            .arg(settings_.height));
}

} // namespace railshot
