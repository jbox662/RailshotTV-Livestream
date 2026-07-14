#include "ui/ScoreboardConfigDialog.h"

#include "score/ScoreManager.h"
#include "score/ScoreboardRenderer.h"
#include "ui/ScoreboardPreviewWidget.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

namespace railshot {

namespace {

QColor colorFromHex(const std::string& hex) {
    QString value = QString::fromStdString(hex);
    if (value.startsWith('#')) {
        value = value.mid(1);
    }
    if (value.length() == 6) {
        value += "FF";
    }
    bool ok = false;
    const QRgb rgb = value.toUInt(&ok, 16);
    return ok ? QColor::fromRgba(rgb) : QColor(Qt::white);
}

QString colorToHex(const QColor& color) {
    return QString("#%1%2%3%4")
        .arg(color.red(), 2, 16, QChar('0'))
        .arg(color.green(), 2, 16, QChar('0'))
        .arg(color.blue(), 2, 16, QChar('0'))
        .arg(color.alpha(), 2, 16, QChar('0'));
}

void setColorButton(QPushButton* button, const std::string& hex) {
    const QColor color = colorFromHex(hex);
    button->setStyleSheet(QString(
                              "QPushButton { background-color: %1; border: 1px solid #555; min-width: 72px; }")
                              .arg(color.name(QColor::HexArgb)));
    button->setText(colorToHex(color));
}

QPushButton* makeColorButton(QWidget* parent) {
    auto* button = new QPushButton(parent);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

MatchState previewMatchState() {
    MatchState state = ScoreManager::instance().state();
    if (state.player1Name.empty() && state.player2Name.empty()) {
        return sampleMatchStateForPreview();
    }
    if (state.player1Name.empty()) {
        state.player1Name = "Player 1";
    }
    if (state.player2Name.empty()) {
        state.player2Name = "Player 2";
    }
    if (state.activePlayer != 1 && state.activePlayer != 2) {
        state.activePlayer = 1;
    }
    return state;
}

} // namespace

ScoreboardConfigDialog::ScoreboardConfigDialog(const Source& source, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Configure Scoreboard");
    resize(980, 680);
    setMinimumSize(860, 560);

    style_ = ScoreboardStyle::fromJson(source.overlaySettings);
    if (style_.barHeight <= 0) {
        style_.barHeight = static_cast<int>(source.transform.height);
    }
    if (style_.barWidth <= 0) {
        style_.barWidth = static_cast<int>(source.transform.width > 0 ? source.transform.width : 1920);
    }

    auto* mainLayout = new QVBoxLayout(this);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(splitter, 1);

    auto* optionsHost = new QWidget(splitter);
    auto* optionsLayout = new QVBoxLayout(optionsHost);
    optionsLayout->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea(optionsHost);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* scrollContent = new QWidget(scroll);
    auto* scrollLayout = new QVBoxLayout(scrollContent);

    auto* intro = new QLabel(
        "Changes update the preview instantly. The program preview also updates live — Cancel restores "
        "your previous look.",
        scrollContent);
    intro->setWordWrap(true);
    scrollLayout->addWidget(intro);

    auto* designGroup = new QGroupBox("Design", scrollContent);
    auto* designForm = new QFormLayout(designGroup);

    presetCombo_ = new QComboBox(designGroup);
    presetCombo_->addItem("Broadcast (default)", static_cast<int>(ScoreboardDesign::Broadcast));
    presetCombo_->addItem("Minimal", static_cast<int>(ScoreboardDesign::Minimal));
    presetCombo_->addItem("Billiards Classic", static_cast<int>(ScoreboardDesign::Billiards));
    presetCombo_->addItem("Light", static_cast<int>(ScoreboardDesign::Light));
    presetCombo_->addItem("High Contrast", static_cast<int>(ScoreboardDesign::HighContrast));
    presetCombo_->addItem("Neon", static_cast<int>(ScoreboardDesign::Neon));
    presetCombo_->addItem("Retro", static_cast<int>(ScoreboardDesign::Retro));
    designForm->addRow("Preset:", presetCombo_);

    fontCombo_ = new QFontComboBox(designGroup);
    fontCombo_->setCurrentFont(QFont(QString::fromStdString(style_.fontFamily)));
    designForm->addRow("Font:", fontCombo_);

    vsLabelEdit_ = new QLineEdit(QString::fromStdString(style_.vsLabel), designGroup);
    designForm->addRow("Center label:", vsLabelEdit_);

    barHeightSpin_ = new QSpinBox(designGroup);
    barHeightSpin_->setRange(64, 240);
    barHeightSpin_->setSuffix(" px");
    designForm->addRow("Bar height:", barHeightSpin_);

    barWidthSpin_ = new QSpinBox(designGroup);
    barWidthSpin_->setRange(640, 3840);
    barWidthSpin_->setSuffix(" px");
    designForm->addRow("Bar width:", barWidthSpin_);

    scrollLayout->addWidget(designGroup);

    auto* typographyGroup = new QGroupBox("Typography", scrollContent);
    auto* typographyForm = new QFormLayout(typographyGroup);

    nameFontSpin_ = new QDoubleSpinBox(typographyGroup);
    nameFontSpin_->setRange(0, 72);
    nameFontSpin_->setDecimals(1);
    nameFontSpin_->setSpecialValueText("Auto");
    typographyForm->addRow("Name size:", nameFontSpin_);

    scoreFontSpin_ = new QDoubleSpinBox(typographyGroup);
    scoreFontSpin_->setRange(0, 96);
    scoreFontSpin_->setDecimals(1);
    scoreFontSpin_->setSpecialValueText("Auto");
    typographyForm->addRow("Score size:", scoreFontSpin_);

    metaFontSpin_ = new QDoubleSpinBox(typographyGroup);
    metaFontSpin_->setRange(0, 48);
    metaFontSpin_->setDecimals(1);
    metaFontSpin_->setSpecialValueText("Auto");
    typographyForm->addRow("Meta size:", metaFontSpin_);

    vsFontSpin_ = new QDoubleSpinBox(typographyGroup);
    vsFontSpin_->setRange(0, 48);
    vsFontSpin_->setDecimals(1);
    vsFontSpin_->setSpecialValueText("Auto");
    typographyForm->addRow("Center label size:", vsFontSpin_);

    nameBold_ = new QCheckBox("Bold player names", typographyGroup);
    scoreBold_ = new QCheckBox("Bold score numbers", typographyGroup);
    uppercaseNames_ = new QCheckBox("Uppercase player names", typographyGroup);
    typographyForm->addRow(nameBold_);
    typographyForm->addRow(scoreBold_);
    typographyForm->addRow(uppercaseNames_);

    scrollLayout->addWidget(typographyGroup);

    auto* colorGroup = new QGroupBox("Colors", scrollContent);
    auto* colorForm = new QFormLayout(colorGroup);

    plateTopBtn_ = makeColorButton(colorGroup);
    plateBottomBtn_ = makeColorButton(colorGroup);
    accentBtn_ = makeColorButton(colorGroup);
    activeBtn_ = makeColorButton(colorGroup);
    nameTextBtn_ = makeColorButton(colorGroup);
    scoreTextBtn_ = makeColorButton(colorGroup);
    metaTextBtn_ = makeColorButton(colorGroup);
    badgeBgBtn_ = makeColorButton(colorGroup);
    borderColorBtn_ = makeColorButton(colorGroup);
    previewBgBtn_ = makeColorButton(colorGroup);

    colorForm->addRow("Plate top:", plateTopBtn_);
    colorForm->addRow("Plate bottom:", plateBottomBtn_);
    colorForm->addRow("Accent stripe:", accentBtn_);
    colorForm->addRow("Active player:", activeBtn_);
    colorForm->addRow("Name text:", nameTextBtn_);
    colorForm->addRow("Score text:", scoreTextBtn_);
    colorForm->addRow("Meta text:", metaTextBtn_);
    colorForm->addRow("Score badge:", badgeBgBtn_);
    colorForm->addRow("Border:", borderColorBtn_);
    colorForm->addRow("Preview background:", previewBgBtn_);

    scrollLayout->addWidget(colorGroup);

    auto* layoutGroup = new QGroupBox("Layout & Effects", scrollContent);
    auto* layoutForm = new QFormLayout(layoutGroup);

    badgeWidthSpin_ = new QSpinBox(layoutGroup);
    badgeWidthSpin_->setRange(0, 200);
    badgeWidthSpin_->setSpecialValueText("Auto");
    layoutForm->addRow("Badge width:", badgeWidthSpin_);

    badgeHeightSpin_ = new QSpinBox(layoutGroup);
    badgeHeightSpin_->setRange(0, 160);
    badgeHeightSpin_->setSpecialValueText("Auto");
    layoutForm->addRow("Badge height:", badgeHeightSpin_);

    badgeRadiusSpin_ = new QSpinBox(layoutGroup);
    badgeRadiusSpin_->setRange(0, 40);
    badgeRadiusSpin_->setSpecialValueText("Auto");
    layoutForm->addRow("Badge corners:", badgeRadiusSpin_);

    accentStripeSpin_ = new QSpinBox(layoutGroup);
    accentStripeSpin_->setRange(0, 20);
    accentStripeSpin_->setSpecialValueText("Auto");
    layoutForm->addRow("Accent height:", accentStripeSpin_);

    shadowOffsetSpin_ = new QSpinBox(layoutGroup);
    shadowOffsetSpin_->setRange(0, 24);
    shadowOffsetSpin_->setSpecialValueText("Auto");
    layoutForm->addRow("Shadow offset:", shadowOffsetSpin_);

    shadowOpacitySpin_ = new QSpinBox(layoutGroup);
    shadowOpacitySpin_->setRange(0, 255);
    layoutForm->addRow("Shadow opacity:", shadowOpacitySpin_);

    plateRadiusSpin_ = new QSpinBox(layoutGroup);
    plateRadiusSpin_->setRange(0, 40);
    layoutForm->addRow("Plate corners:", plateRadiusSpin_);

    borderWidthSpin_ = new QSpinBox(layoutGroup);
    borderWidthSpin_->setRange(0, 8);
    layoutForm->addRow("Border width:", borderWidthSpin_);

    paddingSpin_ = new QSpinBox(layoutGroup);
    paddingSpin_->setRange(0, 120);
    paddingSpin_->setSpecialValueText("Auto");
    layoutForm->addRow("Side padding:", paddingSpin_);

    scrollLayout->addWidget(layoutGroup);

    auto* visibilityGroup = new QGroupBox("Visibility", scrollContent);
    auto* visibilityLayout = new QVBoxLayout(visibilityGroup);

    showGameType_ = new QCheckBox("Show game type header", visibilityGroup);
    showVs_ = new QCheckBox("Show center label", visibilityGroup);
    showRaceTo_ = new QCheckBox("Show race-to line", visibilityGroup);
    showAccentStripe_ = new QCheckBox("Show accent stripe", visibilityGroup);
    showShadow_ = new QCheckBox("Show drop shadow", visibilityGroup);
    showPlayerNames_ = new QCheckBox("Show player names", visibilityGroup);
    showActiveBar_ = new QCheckBox("Show active player bar", visibilityGroup);
    useGradientPlate_ = new QCheckBox("Gradient plate background", visibilityGroup);

    visibilityLayout->addWidget(showGameType_);
    visibilityLayout->addWidget(showVs_);
    visibilityLayout->addWidget(showRaceTo_);
    visibilityLayout->addWidget(showAccentStripe_);
    visibilityLayout->addWidget(showShadow_);
    visibilityLayout->addWidget(showPlayerNames_);
    visibilityLayout->addWidget(showActiveBar_);
    visibilityLayout->addWidget(useGradientPlate_);

    scrollLayout->addWidget(visibilityGroup);
    scrollLayout->addStretch(1);
    scroll->setWidget(scrollContent);
    optionsLayout->addWidget(scroll);

    auto* previewHost = new QWidget(splitter);
    auto* previewLayout = new QVBoxLayout(previewHost);

    auto* previewTitle = new QLabel("Live Preview", previewHost);
    QFont titleFont = previewTitle->font();
    titleFont.setBold(true);
    previewTitle->setFont(titleFont);
    previewLayout->addWidget(previewTitle);

    previewWidget_ = new ScoreboardPreviewWidget(previewHost);
    previewLayout->addWidget(previewWidget_, 1);

    auto* previewHint = new QLabel("Uses your current match scores when available.", previewHost);
    previewHint->setWordWrap(true);
    previewLayout->addWidget(previewHint);

    splitter->addWidget(optionsHost);
    splitter->addWidget(previewHost);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 4);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);

    connect(presetCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ScoreboardConfigDialog::onPresetChanged);

    connect(plateTopBtn_, &QPushButton::clicked, this,
            [this]() { pickColor(plateTopBtn_, &ScoreboardStyle::plateTop); });
    connect(plateBottomBtn_, &QPushButton::clicked, this,
            [this]() { pickColor(plateBottomBtn_, &ScoreboardStyle::plateBottom); });
    connect(accentBtn_, &QPushButton::clicked, this,
            [this]() { pickColor(accentBtn_, &ScoreboardStyle::accent); });
    connect(activeBtn_, &QPushButton::clicked, this,
            [this]() { pickColor(activeBtn_, &ScoreboardStyle::activeColor); });
    connect(nameTextBtn_, &QPushButton::clicked, this,
            [this]() { pickColor(nameTextBtn_, &ScoreboardStyle::nameText); });
    connect(scoreTextBtn_, &QPushButton::clicked, this,
            [this]() { pickColor(scoreTextBtn_, &ScoreboardStyle::scoreText); });
    connect(metaTextBtn_, &QPushButton::clicked, this,
            [this]() { pickColor(metaTextBtn_, &ScoreboardStyle::metaText); });
    connect(badgeBgBtn_, &QPushButton::clicked, this,
            [this]() { pickColor(badgeBgBtn_, &ScoreboardStyle::badgeBackground); });
    connect(borderColorBtn_, &QPushButton::clicked, this,
            [this]() { pickColor(borderColorBtn_, &ScoreboardStyle::borderColor); });
    connect(previewBgBtn_, &QPushButton::clicked, this,
            [this]() { pickColor(previewBgBtn_, &ScoreboardStyle::previewBackground); });

    const auto hook = [this]() { onStyleEdited(); };
    connect(fontCombo_, &QFontComboBox::currentFontChanged, this, hook);
    connect(vsLabelEdit_, &QLineEdit::textChanged, this, hook);
    connect(barHeightSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, hook);
    connect(barWidthSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, hook);
    connect(nameFontSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, hook);
    connect(scoreFontSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, hook);
    connect(metaFontSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, hook);
    connect(vsFontSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, hook);
    connect(badgeWidthSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, hook);
    connect(badgeHeightSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, hook);
    connect(badgeRadiusSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, hook);
    connect(accentStripeSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, hook);
    connect(shadowOffsetSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, hook);
    connect(shadowOpacitySpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, hook);
    connect(plateRadiusSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, hook);
    connect(borderWidthSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, hook);
    connect(paddingSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, hook);
    connect(showGameType_, &QCheckBox::toggled, this, hook);
    connect(showVs_, &QCheckBox::toggled, this, hook);
    connect(showRaceTo_, &QCheckBox::toggled, this, hook);
    connect(showAccentStripe_, &QCheckBox::toggled, this, hook);
    connect(showShadow_, &QCheckBox::toggled, this, hook);
    connect(showPlayerNames_, &QCheckBox::toggled, this, hook);
    connect(showActiveBar_, &QCheckBox::toggled, this, hook);
    connect(nameBold_, &QCheckBox::toggled, this, hook);
    connect(scoreBold_, &QCheckBox::toggled, this, hook);
    connect(uppercaseNames_, &QCheckBox::toggled, this, hook);
    connect(useGradientPlate_, &QCheckBox::toggled, this, hook);

    auto* scoreTimer = new QTimer(this);
    connect(scoreTimer, &QTimer::timeout, this, [this]() {
        const uint64_t version = ScoreManager::instance().version();
        if (version != lastScoreVersion_) {
            lastScoreVersion_ = version;
            updatePreview();
        }
    });
    scoreTimer->start(250);

    loadFromStyle(style_);
    updatePreview();
}

void ScoreboardConfigDialog::onPresetChanged(int index) {
    const int designValue = presetCombo_->itemData(index).toInt();
    style_.applyPreset(static_cast<ScoreboardDesign>(designValue));
    loadFromStyle(style_);
    onStyleEdited();
}

void ScoreboardConfigDialog::pickColor(QPushButton* button, std::string ScoreboardStyle::* field) {
    QColorDialog dlg(colorFromHex(style_.*field), this);
    dlg.setOption(QColorDialog::ShowAlphaChannel);
    dlg.setOption(QColorDialog::DontUseNativeDialog);
    connect(&dlg, &QColorDialog::currentColorChanged, this, [this, button, field](const QColor& color) {
        if (!color.isValid()) {
            return;
        }
        style_.*field = colorToHex(color).toStdString();
        setColorButton(button, style_.*field);
        onStyleEdited();
    });
    if (dlg.exec() == QDialog::Accepted) {
        style_.*field = colorToHex(dlg.selectedColor()).toStdString();
        setColorButton(button, style_.*field);
        onStyleEdited();
    }
}

void ScoreboardConfigDialog::loadFromStyle(const ScoreboardStyle& style) {
    style_ = style;

    const int presetIndex = presetCombo_->findData(static_cast<int>(style.design));
    if (presetIndex >= 0) {
        presetCombo_->blockSignals(true);
        presetCombo_->setCurrentIndex(presetIndex);
        presetCombo_->blockSignals(false);
    }

    if (!style.fontFamily.empty()) {
        fontCombo_->setCurrentFont(QFont(QString::fromStdString(style.fontFamily)));
    } else {
        fontCombo_->setCurrentFont(QFontDatabase::systemFont(QFontDatabase::GeneralFont));
    }

    vsLabelEdit_->setText(QString::fromStdString(style.vsLabel));
    barHeightSpin_->setValue(style.barHeight);
    barWidthSpin_->setValue(style.barWidth > 0 ? style.barWidth : 1920);
    nameFontSpin_->setValue(style.nameFontSize);
    scoreFontSpin_->setValue(style.scoreFontSize);
    metaFontSpin_->setValue(style.metaFontSize);
    vsFontSpin_->setValue(style.vsFontSize);
    badgeWidthSpin_->setValue(style.badgeWidth);
    badgeHeightSpin_->setValue(style.badgeHeight);
    badgeRadiusSpin_->setValue(style.badgeCornerRadius);
    accentStripeSpin_->setValue(style.accentStripeHeight);
    shadowOffsetSpin_->setValue(style.shadowOffset);
    shadowOpacitySpin_->setValue(style.shadowOpacity);
    plateRadiusSpin_->setValue(style.plateCornerRadius);
    borderWidthSpin_->setValue(style.borderWidth);
    paddingSpin_->setValue(style.horizontalPadding);

    showGameType_->setChecked(style.showGameType);
    showVs_->setChecked(style.showVs);
    showRaceTo_->setChecked(style.showRaceTo);
    showAccentStripe_->setChecked(style.showAccentStripe);
    showShadow_->setChecked(style.showShadow);
    showPlayerNames_->setChecked(style.showPlayerNames);
    showActiveBar_->setChecked(style.showActiveBar);
    nameBold_->setChecked(style.nameBold);
    scoreBold_->setChecked(style.scoreBold);
    uppercaseNames_->setChecked(style.uppercaseNames);
    useGradientPlate_->setChecked(style.useGradientPlate);

    setColorButton(plateTopBtn_, style.plateTop);
    setColorButton(plateBottomBtn_, style.plateBottom);
    setColorButton(accentBtn_, style.accent);
    setColorButton(activeBtn_, style.activeColor);
    setColorButton(nameTextBtn_, style.nameText);
    setColorButton(scoreTextBtn_, style.scoreText);
    setColorButton(metaTextBtn_, style.metaText);
    setColorButton(badgeBgBtn_, style.badgeBackground);
    setColorButton(borderColorBtn_, style.borderColor);
    setColorButton(previewBgBtn_, style.previewBackground);
}

ScoreboardStyle ScoreboardConfigDialog::currentStyle() const {
    ScoreboardStyle style = style_;
    style.design = static_cast<ScoreboardDesign>(presetCombo_->currentData().toInt());
    style.fontFamily = fontCombo_->currentFont().family().toStdString();
    style.vsLabel = vsLabelEdit_->text().trimmed().toStdString();
    if (style.vsLabel.empty()) {
        style.vsLabel = "VS";
    }
    style.barHeight = barHeightSpin_->value();
    style.barWidth = barWidthSpin_->value();
    style.nameFontSize = nameFontSpin_->value();
    style.scoreFontSize = scoreFontSpin_->value();
    style.metaFontSize = metaFontSpin_->value();
    style.vsFontSize = vsFontSpin_->value();
    style.badgeWidth = badgeWidthSpin_->value();
    style.badgeHeight = badgeHeightSpin_->value();
    style.badgeCornerRadius = badgeRadiusSpin_->value();
    style.accentStripeHeight = accentStripeSpin_->value();
    style.shadowOffset = shadowOffsetSpin_->value();
    style.shadowOpacity = shadowOpacitySpin_->value();
    style.plateCornerRadius = plateRadiusSpin_->value();
    style.borderWidth = borderWidthSpin_->value();
    style.horizontalPadding = paddingSpin_->value();
    style.showGameType = showGameType_->isChecked();
    style.showVs = showVs_->isChecked();
    style.showRaceTo = showRaceTo_->isChecked();
    style.showAccentStripe = showAccentStripe_->isChecked();
    style.showShadow = showShadow_->isChecked();
    style.showPlayerNames = showPlayerNames_->isChecked();
    style.showActiveBar = showActiveBar_->isChecked();
    style.nameBold = nameBold_->isChecked();
    style.scoreBold = scoreBold_->isChecked();
    style.uppercaseNames = uppercaseNames_->isChecked();
    style.useGradientPlate = useGradientPlate_->isChecked();
    return style;
}

void ScoreboardConfigDialog::onStyleEdited() {
    style_ = currentStyle();
    updatePreview();
    emit liveStyleChanged(style_);
}

void ScoreboardConfigDialog::updatePreview() {
    const ScoreboardStyle style = currentStyle();
    const MatchState state = previewMatchState();
    const int width = style.barWidth > 0 ? style.barWidth : 1920;
    const int height = style.barHeight > 0 ? style.barHeight : 110;
    const QImage image = renderScoreboardImage(state, style, width, height);
    previewWidget_->setPreviewBackground(colorFromHex(style.previewBackground));
    previewWidget_->setScoreboardImage(image);
}

void ScoreboardConfigDialog::applyToSource(Source& source) const {
    const ScoreboardStyle style = currentStyle();
    source.overlaySettings = style.toJson();
    source.transform.width = static_cast<float>(style.barWidth > 0 ? style.barWidth : 1920);
    source.transform.height = static_cast<float>(style.barHeight);
}

} // namespace railshot
