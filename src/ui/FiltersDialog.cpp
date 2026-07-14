#include "ui/FiltersDialog.h"

#include "core/models/SceneManager.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace railshot {
namespace {

QString filterLabel(const SourceFilter& filter) {
    QString type;
    switch (filter.type) {
    case FilterType::ColorCorrection:
        type = QStringLiteral("Color Correction");
        break;
    case FilterType::Gain:
        type = QStringLiteral("Gain");
        break;
    case FilterType::Compressor:
        type = QStringLiteral("Compressor");
        break;
    case FilterType::NoiseGate:
        type = QStringLiteral("Noise Gate");
        break;
    case FilterType::NoiseSuppress:
        type = QStringLiteral("Noise Suppress");
        break;
    case FilterType::Opacity:
    default:
        type = QStringLiteral("Opacity");
        break;
    }
    return filter.enabled ? type : (type + QStringLiteral(" (off)"));
}

std::string defaultParams(FilterType type) {
    QJsonObject obj;
    switch (type) {
    case FilterType::Opacity:
        obj[QStringLiteral("opacity")] = 1.0;
        break;
    case FilterType::ColorCorrection:
        obj[QStringLiteral("brightness")] = 0.0;
        obj[QStringLiteral("contrast")] = 1.0;
        obj[QStringLiteral("saturation")] = 1.0;
        break;
    case FilterType::Gain:
        obj[QStringLiteral("db")] = 0.0;
        break;
    case FilterType::Compressor:
        obj[QStringLiteral("ratio")] = 10.0;
        obj[QStringLiteral("threshold")] = -18.0;
        obj[QStringLiteral("attack_time")] = 6.0;
        obj[QStringLiteral("release_time")] = 60.0;
        obj[QStringLiteral("output_gain")] = 0.0;
        break;
    case FilterType::NoiseGate:
        obj[QStringLiteral("open_threshold")] = -26.0;
        obj[QStringLiteral("close_threshold")] = -32.0;
        obj[QStringLiteral("attack_time")] = 25.0;
        obj[QStringLiteral("hold_time")] = 200.0;
        obj[QStringLiteral("release_time")] = 150.0;
        break;
    case FilterType::NoiseSuppress:
        obj[QStringLiteral("suppress_level")] = -30.0;
        break;
    }
    return QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString();
}

} // namespace

FiltersDialog::FiltersDialog(Source& source, QWidget* parent)
    : QDialog(parent)
    , draft_(source) {
    setWindowTitle(QStringLiteral("Filters for '%1'").arg(QString::fromStdString(source.name)));
    resize(560, 420);

    auto* root = new QHBoxLayout(this);
    auto* left = new QVBoxLayout();
    list_ = new QListWidget(this);
    left->addWidget(list_, 1);
    auto* addRow = new QHBoxLayout();
    auto* addOpacity = new QPushButton(QStringLiteral("+ Opacity"), this);
    auto* addColor = new QPushButton(QStringLiteral("+ Color"), this);
    connect(addOpacity, &QPushButton::clicked, this,
            [this]() { onAddFilter(FilterType::Opacity); });
    connect(addColor, &QPushButton::clicked, this,
            [this]() { onAddFilter(FilterType::ColorCorrection); });
    addRow->addWidget(addOpacity);
    addRow->addWidget(addColor);
    auto* addGain = new QPushButton(QStringLiteral("+ Gain"), this);
    auto* addComp = new QPushButton(QStringLiteral("+ Comp"), this);
    auto* addGate = new QPushButton(QStringLiteral("+ Gate"), this);
    auto* addSuppress = new QPushButton(QStringLiteral("+ Suppress"), this);
    auto* remove = new QPushButton(QStringLiteral("Remove"), this);
    connect(addGain, &QPushButton::clicked, this, [this]() { onAddFilter(FilterType::Gain); });
    connect(addComp, &QPushButton::clicked, this,
            [this]() { onAddFilter(FilterType::Compressor); });
    connect(addGate, &QPushButton::clicked, this, [this]() { onAddFilter(FilterType::NoiseGate); });
    connect(addSuppress, &QPushButton::clicked, this,
            [this]() { onAddFilter(FilterType::NoiseSuppress); });
    connect(remove, &QPushButton::clicked, this, &FiltersDialog::onRemove);
    addRow->addWidget(addGain);
    addRow->addWidget(addComp);
    addRow->addWidget(addGate);
    addRow->addWidget(addSuppress);
    addRow->addWidget(remove);
    left->addLayout(addRow);
    root->addLayout(left, 1);

    auto* right = new QVBoxLayout();
    enabledCheck_ = new QCheckBox(QStringLiteral("Filter enabled"), this);
    right->addWidget(enabledCheck_);
    editorStack_ = new QStackedWidget(this);
    emptyPage_ = new QWidget(editorStack_);
    auto* emptyLayout = new QVBoxLayout(emptyPage_);
    emptyLayout->addWidget(new QLabel(QStringLiteral("Select a filter to edit."), emptyPage_));
    emptyLayout->addStretch();

    opacityPage_ = new QWidget(editorStack_);
    auto* opacityForm = new QFormLayout(opacityPage_);
    opacitySpin_ = new QDoubleSpinBox(opacityPage_);
    opacitySpin_->setRange(0.0, 1.0);
    opacitySpin_->setSingleStep(0.05);
    opacitySpin_->setDecimals(2);
    opacityForm->addRow(QStringLiteral("Opacity"), opacitySpin_);

    colorPage_ = new QWidget(editorStack_);
    auto* colorForm = new QFormLayout(colorPage_);
    brightnessSpin_ = new QDoubleSpinBox(colorPage_);
    brightnessSpin_->setRange(-1.0, 1.0);
    brightnessSpin_->setSingleStep(0.05);
    brightnessSpin_->setDecimals(2);
    contrastSpin_ = new QDoubleSpinBox(colorPage_);
    contrastSpin_->setRange(0.0, 2.0);
    contrastSpin_->setSingleStep(0.05);
    contrastSpin_->setDecimals(2);
    saturationSpin_ = new QDoubleSpinBox(colorPage_);
    saturationSpin_->setRange(0.0, 2.0);
    saturationSpin_->setSingleStep(0.05);
    saturationSpin_->setDecimals(2);
    colorForm->addRow(QStringLiteral("Brightness"), brightnessSpin_);
    colorForm->addRow(QStringLiteral("Contrast"), contrastSpin_);
    colorForm->addRow(QStringLiteral("Saturation"), saturationSpin_);

    gainPage_ = new QWidget(editorStack_);
    auto* gainForm = new QFormLayout(gainPage_);
    gainDbSpin_ = new QDoubleSpinBox(gainPage_);
    gainDbSpin_->setRange(-30.0, 30.0);
    gainDbSpin_->setSuffix(QStringLiteral(" dB"));
    gainForm->addRow(QStringLiteral("Gain"), gainDbSpin_);

    compressorPage_ = new QWidget(editorStack_);
    auto* compForm = new QFormLayout(compressorPage_);
    compRatioSpin_ = new QDoubleSpinBox(compressorPage_);
    compRatioSpin_->setRange(1.0, 32.0);
    compThresholdSpin_ = new QDoubleSpinBox(compressorPage_);
    compThresholdSpin_->setRange(-60.0, 0.0);
    compThresholdSpin_->setSuffix(QStringLiteral(" dB"));
    compAttackSpin_ = new QDoubleSpinBox(compressorPage_);
    compAttackSpin_->setRange(1.0, 500.0);
    compAttackSpin_->setSuffix(QStringLiteral(" ms"));
    compReleaseSpin_ = new QDoubleSpinBox(compressorPage_);
    compReleaseSpin_->setRange(1.0, 1000.0);
    compReleaseSpin_->setSuffix(QStringLiteral(" ms"));
    compOutputSpin_ = new QDoubleSpinBox(compressorPage_);
    compOutputSpin_->setRange(-30.0, 30.0);
    compOutputSpin_->setSuffix(QStringLiteral(" dB"));
    compForm->addRow(QStringLiteral("Ratio"), compRatioSpin_);
    compForm->addRow(QStringLiteral("Threshold"), compThresholdSpin_);
    compForm->addRow(QStringLiteral("Attack"), compAttackSpin_);
    compForm->addRow(QStringLiteral("Release"), compReleaseSpin_);
    compForm->addRow(QStringLiteral("Output gain"), compOutputSpin_);

    gatePage_ = new QWidget(editorStack_);
    auto* gateForm = new QFormLayout(gatePage_);
    gateOpenSpin_ = new QDoubleSpinBox(gatePage_);
    gateOpenSpin_->setRange(-60.0, 0.0);
    gateOpenSpin_->setSuffix(QStringLiteral(" dB"));
    gateCloseSpin_ = new QDoubleSpinBox(gatePage_);
    gateCloseSpin_->setRange(-60.0, 0.0);
    gateCloseSpin_->setSuffix(QStringLiteral(" dB"));
    gateAttackSpin_ = new QDoubleSpinBox(gatePage_);
    gateAttackSpin_->setRange(1.0, 500.0);
    gateAttackSpin_->setSuffix(QStringLiteral(" ms"));
    gateHoldSpin_ = new QDoubleSpinBox(gatePage_);
    gateHoldSpin_->setRange(0.0, 2000.0);
    gateHoldSpin_->setSuffix(QStringLiteral(" ms"));
    gateReleaseSpin_ = new QDoubleSpinBox(gatePage_);
    gateReleaseSpin_->setRange(1.0, 2000.0);
    gateReleaseSpin_->setSuffix(QStringLiteral(" ms"));
    gateForm->addRow(QStringLiteral("Open threshold"), gateOpenSpin_);
    gateForm->addRow(QStringLiteral("Close threshold"), gateCloseSpin_);
    gateForm->addRow(QStringLiteral("Attack"), gateAttackSpin_);
    gateForm->addRow(QStringLiteral("Hold"), gateHoldSpin_);
    gateForm->addRow(QStringLiteral("Release"), gateReleaseSpin_);

    suppressPage_ = new QWidget(editorStack_);
    auto* suppressForm = new QFormLayout(suppressPage_);
    suppressSpin_ = new QDoubleSpinBox(suppressPage_);
    suppressSpin_->setRange(-60.0, -5.0);
    suppressSpin_->setSuffix(QStringLiteral(" dB"));
    suppressForm->addRow(QStringLiteral("Suppress level"), suppressSpin_);

    editorStack_->addWidget(emptyPage_);
    editorStack_->addWidget(opacityPage_);
    editorStack_->addWidget(colorPage_);
    editorStack_->addWidget(gainPage_);
    editorStack_->addWidget(compressorPage_);
    editorStack_->addWidget(gatePage_);
    editorStack_->addWidget(suppressPage_);
    right->addWidget(editorStack_, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    right->addWidget(buttons);
    root->addLayout(right, 1);

    connect(list_, &QListWidget::currentRowChanged, this, &FiltersDialog::onSelectionChanged);
    connect(enabledCheck_, &QCheckBox::toggled, this, [this](bool) { syncSelectionFromEditor(); });
    auto bindSpin = [this](QDoubleSpinBox* spin) {
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this](double) { syncSelectionFromEditor(); });
    };
    bindSpin(opacitySpin_);
    bindSpin(brightnessSpin_);
    bindSpin(contrastSpin_);
    bindSpin(saturationSpin_);
    bindSpin(gainDbSpin_);
    bindSpin(compRatioSpin_);
    bindSpin(compThresholdSpin_);
    bindSpin(compAttackSpin_);
    bindSpin(compReleaseSpin_);
    bindSpin(compOutputSpin_);
    bindSpin(gateOpenSpin_);
    bindSpin(gateCloseSpin_);
    bindSpin(gateAttackSpin_);
    bindSpin(gateHoldSpin_);
    bindSpin(gateReleaseSpin_);
    bindSpin(suppressSpin_);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    rebuildList();
}

void FiltersDialog::rebuildList() {
    list_->clear();
    for (const auto& filter : draft_.filters) {
        list_->addItem(filterLabel(filter));
    }
    if (!draft_.filters.empty()) {
        list_->setCurrentRow(std::clamp(selectedIndex_, 0, static_cast<int>(draft_.filters.size()) - 1));
    } else {
        selectedIndex_ = -1;
        editorStack_->setCurrentWidget(emptyPage_);
    }
}

void FiltersDialog::onSelectionChanged() {
    selectedIndex_ = list_->currentRow();
    syncEditorFromSelection();
}

void FiltersDialog::onAddFilter(FilterType type) {
    SourceFilter filter;
    filter.id = SceneManager::generateId();
    filter.type = type;
    filter.enabled = true;
    filter.paramsJson = defaultParams(type);
    draft_.filters.push_back(filter);
    selectedIndex_ = static_cast<int>(draft_.filters.size()) - 1;
    rebuildList();
}

void FiltersDialog::onRemove() {
    if (selectedIndex_ < 0 || selectedIndex_ >= static_cast<int>(draft_.filters.size())) {
        return;
    }
    draft_.filters.erase(draft_.filters.begin() + selectedIndex_);
    selectedIndex_ = std::min(selectedIndex_, static_cast<int>(draft_.filters.size()) - 1);
    rebuildList();
}

void FiltersDialog::syncEditorFromSelection() {
    if (selectedIndex_ < 0 || selectedIndex_ >= static_cast<int>(draft_.filters.size())) {
        editorStack_->setCurrentWidget(emptyPage_);
        return;
    }
    const SourceFilter& filter = draft_.filters[static_cast<size_t>(selectedIndex_)];
    enabledCheck_->blockSignals(true);
    enabledCheck_->setChecked(filter.enabled);
    enabledCheck_->blockSignals(false);

    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(filter.paramsJson));
    const QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject{};

    auto setSpin = [](QDoubleSpinBox* spin, double v) {
        spin->blockSignals(true);
        spin->setValue(v);
        spin->blockSignals(false);
    };

    switch (filter.type) {
    case FilterType::Opacity:
        editorStack_->setCurrentWidget(opacityPage_);
        setSpin(opacitySpin_, obj.value(QStringLiteral("opacity")).toDouble(1.0));
        break;
    case FilterType::ColorCorrection:
        editorStack_->setCurrentWidget(colorPage_);
        setSpin(brightnessSpin_, obj.value(QStringLiteral("brightness")).toDouble(0.0));
        setSpin(contrastSpin_, obj.value(QStringLiteral("contrast")).toDouble(1.0));
        setSpin(saturationSpin_, obj.value(QStringLiteral("saturation")).toDouble(1.0));
        break;
    case FilterType::Gain:
        editorStack_->setCurrentWidget(gainPage_);
        setSpin(gainDbSpin_, obj.value(QStringLiteral("db")).toDouble(0.0));
        break;
    case FilterType::Compressor:
        editorStack_->setCurrentWidget(compressorPage_);
        setSpin(compRatioSpin_, obj.value(QStringLiteral("ratio")).toDouble(10.0));
        setSpin(compThresholdSpin_, obj.value(QStringLiteral("threshold")).toDouble(-18.0));
        setSpin(compAttackSpin_, obj.value(QStringLiteral("attack_time")).toDouble(6.0));
        setSpin(compReleaseSpin_, obj.value(QStringLiteral("release_time")).toDouble(60.0));
        setSpin(compOutputSpin_, obj.value(QStringLiteral("output_gain")).toDouble(0.0));
        break;
    case FilterType::NoiseGate:
        editorStack_->setCurrentWidget(gatePage_);
        setSpin(gateOpenSpin_, obj.value(QStringLiteral("open_threshold")).toDouble(-26.0));
        setSpin(gateCloseSpin_, obj.value(QStringLiteral("close_threshold")).toDouble(-32.0));
        setSpin(gateAttackSpin_, obj.value(QStringLiteral("attack_time")).toDouble(25.0));
        setSpin(gateHoldSpin_, obj.value(QStringLiteral("hold_time")).toDouble(200.0));
        setSpin(gateReleaseSpin_, obj.value(QStringLiteral("release_time")).toDouble(150.0));
        break;
    case FilterType::NoiseSuppress:
        editorStack_->setCurrentWidget(suppressPage_);
        setSpin(suppressSpin_, obj.value(QStringLiteral("suppress_level")).toDouble(-30.0));
        break;
    }
}

void FiltersDialog::syncSelectionFromEditor() {
    if (selectedIndex_ < 0 || selectedIndex_ >= static_cast<int>(draft_.filters.size())) {
        return;
    }
    SourceFilter& filter = draft_.filters[static_cast<size_t>(selectedIndex_)];
    filter.enabled = enabledCheck_->isChecked();
    QJsonObject obj;
    switch (filter.type) {
    case FilterType::Opacity:
        obj[QStringLiteral("opacity")] = opacitySpin_->value();
        break;
    case FilterType::ColorCorrection:
        obj[QStringLiteral("brightness")] = brightnessSpin_->value();
        obj[QStringLiteral("contrast")] = contrastSpin_->value();
        obj[QStringLiteral("saturation")] = saturationSpin_->value();
        break;
    case FilterType::Gain:
        obj[QStringLiteral("db")] = gainDbSpin_->value();
        break;
    case FilterType::Compressor:
        obj[QStringLiteral("ratio")] = compRatioSpin_->value();
        obj[QStringLiteral("threshold")] = compThresholdSpin_->value();
        obj[QStringLiteral("attack_time")] = compAttackSpin_->value();
        obj[QStringLiteral("release_time")] = compReleaseSpin_->value();
        obj[QStringLiteral("output_gain")] = compOutputSpin_->value();
        break;
    case FilterType::NoiseGate:
        obj[QStringLiteral("open_threshold")] = gateOpenSpin_->value();
        obj[QStringLiteral("close_threshold")] = gateCloseSpin_->value();
        obj[QStringLiteral("attack_time")] = gateAttackSpin_->value();
        obj[QStringLiteral("hold_time")] = gateHoldSpin_->value();
        obj[QStringLiteral("release_time")] = gateReleaseSpin_->value();
        break;
    case FilterType::NoiseSuppress:
        obj[QStringLiteral("suppress_level")] = suppressSpin_->value();
        break;
    }
    filter.paramsJson = QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString();
    if (auto* item = list_->item(selectedIndex_)) {
        item->setText(filterLabel(filter));
    }
}

void FiltersDialog::applyToSource(Source& source) const {
    source.filters = draft_.filters;
}

} // namespace railshot
