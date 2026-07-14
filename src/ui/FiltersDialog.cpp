#include "ui/FiltersDialog.h"

#include "core/models/SceneManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace railshot {
namespace {

QString filterLabel(const SourceFilter& filter) {
    QString type;
    switch (filter.type) {
    case FilterType::ColorCorrection: type = QStringLiteral("Color Correction"); break;
    case FilterType::Gain: type = QStringLiteral("Gain"); break;
    case FilterType::Compressor: type = QStringLiteral("Compressor"); break;
    case FilterType::NoiseGate: type = QStringLiteral("Noise Gate"); break;
    case FilterType::NoiseSuppress: type = QStringLiteral("Noise Suppress"); break;
    case FilterType::Crop: type = QStringLiteral("Crop"); break;
    case FilterType::ChromaKey: type = QStringLiteral("Chroma Key"); break;
    case FilterType::ColorKey: type = QStringLiteral("Color Key"); break;
    case FilterType::ImageMask: type = QStringLiteral("Image Mask"); break;
    case FilterType::ColorGrade: type = QStringLiteral("Color Grade / LUT"); break;
    case FilterType::Scale: type = QStringLiteral("Scale"); break;
    case FilterType::Scroll: type = QStringLiteral("Scroll"); break;
    case FilterType::Sharpness: type = QStringLiteral("Sharpness"); break;
    case FilterType::RenderDelay: type = QStringLiteral("Render Delay"); break;
    case FilterType::Opacity:
    default: type = QStringLiteral("Opacity"); break;
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
    case FilterType::Crop:
        obj[QStringLiteral("relative")] = true;
        obj[QStringLiteral("left")] = 0.0;
        obj[QStringLiteral("top")] = 0.0;
        obj[QStringLiteral("right")] = 0.0;
        obj[QStringLiteral("bottom")] = 0.0;
        break;
    case FilterType::ChromaKey:
        obj[QStringLiteral("key_color_type")] = QStringLiteral("green");
        obj[QStringLiteral("similarity")] = 400.0;
        obj[QStringLiteral("smoothness")] = 80.0;
        obj[QStringLiteral("spill")] = 100.0;
        break;
    case FilterType::ColorKey:
        obj[QStringLiteral("key_color_type")] = QStringLiteral("green");
        obj[QStringLiteral("similarity")] = 80.0;
        obj[QStringLiteral("smoothness")] = 50.0;
        break;
    case FilterType::ImageMask:
        obj[QStringLiteral("image_path")] = QString();
        obj[QStringLiteral("opacity")] = 1.0;
        break;
    case FilterType::ColorGrade:
        obj[QStringLiteral("clut_amount")] = 1.0;
        obj[QStringLiteral("lift")] = 0.0;
        obj[QStringLiteral("gamma")] = 1.0;
        obj[QStringLiteral("gain")] = 1.0;
        obj[QStringLiteral("image_path")] = QString();
        break;
    case FilterType::Scale:
        obj[QStringLiteral("scale")] = 1.0;
        break;
    case FilterType::Scroll:
        obj[QStringLiteral("speed_x")] = 0.0;
        obj[QStringLiteral("speed_y")] = 0.05;
        obj[QStringLiteral("loop")] = true;
        break;
    case FilterType::Sharpness:
        obj[QStringLiteral("sharpness")] = 0.08;
        break;
    case FilterType::RenderDelay:
        obj[QStringLiteral("delay_ms")] = 0;
        break;
    }
    return QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString();
}

QDoubleSpinBox* makeSpin(QWidget* parent, double minV, double maxV, double step = 0.05) {
    auto* spin = new QDoubleSpinBox(parent);
    spin->setRange(minV, maxV);
    spin->setSingleStep(step);
    spin->setDecimals(3);
    return spin;
}

} // namespace

FiltersDialog::FiltersDialog(Source& source, QWidget* parent)
    : QDialog(parent)
    , draft_(source) {
    setWindowTitle(QStringLiteral("Filters for '%1'").arg(QString::fromStdString(source.name)));
    resize(640, 460);

    auto* root = new QHBoxLayout(this);
    auto* left = new QVBoxLayout();
    list_ = new QListWidget(this);
    left->addWidget(list_, 1);

    auto addBtn = [this](const QString& label, FilterType type, QHBoxLayout* row) {
        auto* btn = new QPushButton(label, this);
        connect(btn, &QPushButton::clicked, this, [this, type]() { onAddFilter(type); });
        row->addWidget(btn);
    };
    auto* row1 = new QHBoxLayout();
    addBtn(QStringLiteral("+ Opacity"), FilterType::Opacity, row1);
    addBtn(QStringLiteral("+ Color"), FilterType::ColorCorrection, row1);
    addBtn(QStringLiteral("+ Crop"), FilterType::Crop, row1);
    addBtn(QStringLiteral("+ Key"), FilterType::ChromaKey, row1);
    left->addLayout(row1);
    auto* row2 = new QHBoxLayout();
    addBtn(QStringLiteral("+ ColorKey"), FilterType::ColorKey, row2);
    addBtn(QStringLiteral("+ Mask"), FilterType::ImageMask, row2);
    addBtn(QStringLiteral("+ Grade"), FilterType::ColorGrade, row2);
    addBtn(QStringLiteral("+ Scale"), FilterType::Scale, row2);
    left->addLayout(row2);
    auto* row3 = new QHBoxLayout();
    addBtn(QStringLiteral("+ Scroll"), FilterType::Scroll, row3);
    addBtn(QStringLiteral("+ Sharp"), FilterType::Sharpness, row3);
    addBtn(QStringLiteral("+ Delay"), FilterType::RenderDelay, row3);
    addBtn(QStringLiteral("+ Gain"), FilterType::Gain, row3);
    left->addLayout(row3);
    auto* row4 = new QHBoxLayout();
    addBtn(QStringLiteral("+ Comp"), FilterType::Compressor, row4);
    addBtn(QStringLiteral("+ Gate"), FilterType::NoiseGate, row4);
    addBtn(QStringLiteral("+ Suppress"), FilterType::NoiseSuppress, row4);
    auto* remove = new QPushButton(QStringLiteral("Remove"), this);
    connect(remove, &QPushButton::clicked, this, &FiltersDialog::onRemove);
    row4->addWidget(remove);
    left->addLayout(row4);
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
    opacitySpin_ = makeSpin(opacityPage_, 0.0, 1.0);
    opacityForm->addRow(QStringLiteral("Opacity"), opacitySpin_);

    colorPage_ = new QWidget(editorStack_);
    auto* colorForm = new QFormLayout(colorPage_);
    brightnessSpin_ = makeSpin(colorPage_, -1.0, 1.0);
    contrastSpin_ = makeSpin(colorPage_, 0.0, 2.0);
    saturationSpin_ = makeSpin(colorPage_, 0.0, 2.0);
    colorForm->addRow(QStringLiteral("Brightness"), brightnessSpin_);
    colorForm->addRow(QStringLiteral("Contrast"), contrastSpin_);
    colorForm->addRow(QStringLiteral("Saturation"), saturationSpin_);

    gainPage_ = new QWidget(editorStack_);
    auto* gainForm = new QFormLayout(gainPage_);
    gainDbSpin_ = makeSpin(gainPage_, -30.0, 30.0, 0.5);
    gainDbSpin_->setSuffix(QStringLiteral(" dB"));
    gainForm->addRow(QStringLiteral("Gain"), gainDbSpin_);

    compressorPage_ = new QWidget(editorStack_);
    auto* compForm = new QFormLayout(compressorPage_);
    compRatioSpin_ = makeSpin(compressorPage_, 1.0, 32.0, 0.1);
    compThresholdSpin_ = makeSpin(compressorPage_, -60.0, 0.0, 0.5);
    compAttackSpin_ = makeSpin(compressorPage_, 1.0, 500.0, 1.0);
    compReleaseSpin_ = makeSpin(compressorPage_, 1.0, 1000.0, 1.0);
    compOutputSpin_ = makeSpin(compressorPage_, -30.0, 30.0, 0.5);
    compForm->addRow(QStringLiteral("Ratio"), compRatioSpin_);
    compForm->addRow(QStringLiteral("Threshold"), compThresholdSpin_);
    compForm->addRow(QStringLiteral("Attack"), compAttackSpin_);
    compForm->addRow(QStringLiteral("Release"), compReleaseSpin_);
    compForm->addRow(QStringLiteral("Output gain"), compOutputSpin_);

    gatePage_ = new QWidget(editorStack_);
    auto* gateForm = new QFormLayout(gatePage_);
    gateOpenSpin_ = makeSpin(gatePage_, -60.0, 0.0, 0.5);
    gateCloseSpin_ = makeSpin(gatePage_, -60.0, 0.0, 0.5);
    gateAttackSpin_ = makeSpin(gatePage_, 1.0, 500.0, 1.0);
    gateHoldSpin_ = makeSpin(gatePage_, 0.0, 2000.0, 1.0);
    gateReleaseSpin_ = makeSpin(gatePage_, 1.0, 2000.0, 1.0);
    gateForm->addRow(QStringLiteral("Open"), gateOpenSpin_);
    gateForm->addRow(QStringLiteral("Close"), gateCloseSpin_);
    gateForm->addRow(QStringLiteral("Attack"), gateAttackSpin_);
    gateForm->addRow(QStringLiteral("Hold"), gateHoldSpin_);
    gateForm->addRow(QStringLiteral("Release"), gateReleaseSpin_);

    suppressPage_ = new QWidget(editorStack_);
    auto* suppressForm = new QFormLayout(suppressPage_);
    suppressSpin_ = makeSpin(suppressPage_, -60.0, -5.0, 0.5);
    suppressForm->addRow(QStringLiteral("Suppress level"), suppressSpin_);

    cropPage_ = new QWidget(editorStack_);
    auto* cropForm = new QFormLayout(cropPage_);
    cropL_ = makeSpin(cropPage_, 0.0, 0.49, 0.01);
    cropT_ = makeSpin(cropPage_, 0.0, 0.49, 0.01);
    cropR_ = makeSpin(cropPage_, 0.0, 0.49, 0.01);
    cropB_ = makeSpin(cropPage_, 0.0, 0.49, 0.01);
    cropForm->addRow(QStringLiteral("Left"), cropL_);
    cropForm->addRow(QStringLiteral("Top"), cropT_);
    cropForm->addRow(QStringLiteral("Right"), cropR_);
    cropForm->addRow(QStringLiteral("Bottom"), cropB_);

    keyPage_ = new QWidget(editorStack_);
    auto* keyForm = new QFormLayout(keyPage_);
    keyTypeCombo_ = new QComboBox(keyPage_);
    keyTypeCombo_->addItems({QStringLiteral("green"), QStringLiteral("blue"), QStringLiteral("magenta"),
                             QStringLiteral("red"), QStringLiteral("custom")});
    keySimSpin_ = makeSpin(keyPage_, 1.0, 1000.0, 1.0);
    keySmoothSpin_ = makeSpin(keyPage_, 1.0, 1000.0, 1.0);
    keySpillSpin_ = makeSpin(keyPage_, 0.0, 1000.0, 1.0);
    keyForm->addRow(QStringLiteral("Key color"), keyTypeCombo_);
    keyForm->addRow(QStringLiteral("Similarity"), keySimSpin_);
    keyForm->addRow(QStringLiteral("Smoothness"), keySmoothSpin_);
    keyForm->addRow(QStringLiteral("Spill (chroma)"), keySpillSpin_);

    maskPage_ = new QWidget(editorStack_);
    auto* maskForm = new QFormLayout(maskPage_);
    maskPathEdit_ = new QLineEdit(maskPage_);
    maskOpacitySpin_ = makeSpin(maskPage_, 0.0, 1.0);
    maskForm->addRow(QStringLiteral("Image path"), maskPathEdit_);
    maskForm->addRow(QStringLiteral("Opacity"), maskOpacitySpin_);

    gradePage_ = new QWidget(editorStack_);
    auto* gradeForm = new QFormLayout(gradePage_);
    gradeAmountSpin_ = makeSpin(gradePage_, 0.0, 1.0);
    liftSpin_ = makeSpin(gradePage_, -1.0, 1.0);
    gammaSpin_ = makeSpin(gradePage_, 0.01, 4.0);
    gainSpin_ = makeSpin(gradePage_, 0.0, 4.0);
    lutPathEdit_ = new QLineEdit(gradePage_);
    gradeForm->addRow(QStringLiteral("Amount"), gradeAmountSpin_);
    gradeForm->addRow(QStringLiteral("Lift"), liftSpin_);
    gradeForm->addRow(QStringLiteral("Gamma"), gammaSpin_);
    gradeForm->addRow(QStringLiteral("Gain"), gainSpin_);
    gradeForm->addRow(QStringLiteral("LUT image path"), lutPathEdit_);

    scalePage_ = new QWidget(editorStack_);
    auto* scaleForm = new QFormLayout(scalePage_);
    scaleSpin_ = makeSpin(scalePage_, 0.05, 8.0, 0.05);
    scaleForm->addRow(QStringLiteral("Scale"), scaleSpin_);

    scrollPage_ = new QWidget(editorStack_);
    auto* scrollForm = new QFormLayout(scrollPage_);
    scrollXSpin_ = makeSpin(scrollPage_, -5.0, 5.0, 0.01);
    scrollYSpin_ = makeSpin(scrollPage_, -5.0, 5.0, 0.01);
    scrollLoopCheck_ = new QCheckBox(QStringLiteral("Loop"), scrollPage_);
    scrollForm->addRow(QStringLiteral("Speed X"), scrollXSpin_);
    scrollForm->addRow(QStringLiteral("Speed Y"), scrollYSpin_);
    scrollForm->addRow(QString(), scrollLoopCheck_);

    sharpPage_ = new QWidget(editorStack_);
    auto* sharpForm = new QFormLayout(sharpPage_);
    sharpSpin_ = makeSpin(sharpPage_, 0.0, 1.0, 0.01);
    sharpForm->addRow(QStringLiteral("Sharpness"), sharpSpin_);

    delayPage_ = new QWidget(editorStack_);
    auto* delayForm = new QFormLayout(delayPage_);
    delaySpin_ = new QSpinBox(delayPage_);
    delaySpin_->setRange(0, 5000);
    delaySpin_->setSuffix(QStringLiteral(" ms"));
    delayForm->addRow(QStringLiteral("Delay"), delaySpin_);

    for (QWidget* page : {emptyPage_, opacityPage_, colorPage_, gainPage_, compressorPage_, gatePage_,
                          suppressPage_, cropPage_, keyPage_, maskPage_, gradePage_, scalePage_,
                          scrollPage_, sharpPage_, delayPage_}) {
        editorStack_->addWidget(page);
    }
    right->addWidget(editorStack_, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    right->addWidget(buttons);
    root->addLayout(right, 1);

    connect(list_, &QListWidget::currentRowChanged, this, &FiltersDialog::onSelectionChanged);
    connect(enabledCheck_, &QCheckBox::toggled, this, [this](bool) { syncSelectionFromEditor(); });
    auto bindD = [this](QDoubleSpinBox* s) {
        connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this](double) { syncSelectionFromEditor(); });
    };
    for (QDoubleSpinBox* s :
         {opacitySpin_, brightnessSpin_, contrastSpin_, saturationSpin_, gainDbSpin_,
          compRatioSpin_, compThresholdSpin_, compAttackSpin_, compReleaseSpin_, compOutputSpin_,
          gateOpenSpin_, gateCloseSpin_, gateAttackSpin_, gateHoldSpin_, gateReleaseSpin_,
          suppressSpin_, cropL_, cropT_, cropR_, cropB_, keySimSpin_, keySmoothSpin_, keySpillSpin_,
          maskOpacitySpin_, gradeAmountSpin_, liftSpin_, gammaSpin_, gainSpin_, scaleSpin_,
          scrollXSpin_, scrollYSpin_, sharpSpin_}) {
        bindD(s);
    }
    connect(delaySpin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int) { syncSelectionFromEditor(); });
    connect(keyTypeCombo_, &QComboBox::currentTextChanged, this,
            [this](const QString&) { syncSelectionFromEditor(); });
    connect(maskPathEdit_, &QLineEdit::textChanged, this, [this](const QString&) {
        syncSelectionFromEditor();
    });
    connect(lutPathEdit_, &QLineEdit::textChanged, this, [this](const QString&) {
        syncSelectionFromEditor();
    });
    connect(scrollLoopCheck_, &QCheckBox::toggled, this, [this](bool) { syncSelectionFromEditor(); });
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
    editingKeyType_ = filter.type;
    enabledCheck_->blockSignals(true);
    enabledCheck_->setChecked(filter.enabled);
    enabledCheck_->blockSignals(false);

    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(filter.paramsJson));
    const QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject{};
    auto setD = [](QDoubleSpinBox* s, double v) {
        s->blockSignals(true);
        s->setValue(v);
        s->blockSignals(false);
    };

    switch (filter.type) {
    case FilterType::Opacity:
        editorStack_->setCurrentWidget(opacityPage_);
        setD(opacitySpin_, obj.value(QStringLiteral("opacity")).toDouble(1.0));
        break;
    case FilterType::ColorCorrection:
        editorStack_->setCurrentWidget(colorPage_);
        setD(brightnessSpin_, obj.value(QStringLiteral("brightness")).toDouble(0.0));
        setD(contrastSpin_, obj.value(QStringLiteral("contrast")).toDouble(1.0));
        setD(saturationSpin_, obj.value(QStringLiteral("saturation")).toDouble(1.0));
        break;
    case FilterType::Gain:
        editorStack_->setCurrentWidget(gainPage_);
        setD(gainDbSpin_, obj.value(QStringLiteral("db")).toDouble(0.0));
        break;
    case FilterType::Compressor:
        editorStack_->setCurrentWidget(compressorPage_);
        setD(compRatioSpin_, obj.value(QStringLiteral("ratio")).toDouble(10.0));
        setD(compThresholdSpin_, obj.value(QStringLiteral("threshold")).toDouble(-18.0));
        setD(compAttackSpin_, obj.value(QStringLiteral("attack_time")).toDouble(6.0));
        setD(compReleaseSpin_, obj.value(QStringLiteral("release_time")).toDouble(60.0));
        setD(compOutputSpin_, obj.value(QStringLiteral("output_gain")).toDouble(0.0));
        break;
    case FilterType::NoiseGate:
        editorStack_->setCurrentWidget(gatePage_);
        setD(gateOpenSpin_, obj.value(QStringLiteral("open_threshold")).toDouble(-26.0));
        setD(gateCloseSpin_, obj.value(QStringLiteral("close_threshold")).toDouble(-32.0));
        setD(gateAttackSpin_, obj.value(QStringLiteral("attack_time")).toDouble(25.0));
        setD(gateHoldSpin_, obj.value(QStringLiteral("hold_time")).toDouble(200.0));
        setD(gateReleaseSpin_, obj.value(QStringLiteral("release_time")).toDouble(150.0));
        break;
    case FilterType::NoiseSuppress:
        editorStack_->setCurrentWidget(suppressPage_);
        setD(suppressSpin_, obj.value(QStringLiteral("suppress_level")).toDouble(-30.0));
        break;
    case FilterType::Crop:
        editorStack_->setCurrentWidget(cropPage_);
        setD(cropL_, obj.value(QStringLiteral("left")).toDouble(0.0));
        setD(cropT_, obj.value(QStringLiteral("top")).toDouble(0.0));
        setD(cropR_, obj.value(QStringLiteral("right")).toDouble(0.0));
        setD(cropB_, obj.value(QStringLiteral("bottom")).toDouble(0.0));
        break;
    case FilterType::ChromaKey:
    case FilterType::ColorKey: {
        editorStack_->setCurrentWidget(keyPage_);
        const QString type = obj.value(QStringLiteral("key_color_type")).toString(QStringLiteral("green"));
        keyTypeCombo_->blockSignals(true);
        keyTypeCombo_->setCurrentText(type);
        keyTypeCombo_->blockSignals(false);
        setD(keySimSpin_,
             obj.value(QStringLiteral("similarity"))
                 .toDouble(filter.type == FilterType::ChromaKey ? 400.0 : 80.0));
        setD(keySmoothSpin_,
             obj.value(QStringLiteral("smoothness"))
                 .toDouble(filter.type == FilterType::ChromaKey ? 80.0 : 50.0));
        setD(keySpillSpin_, obj.value(QStringLiteral("spill")).toDouble(100.0));
        keySpillSpin_->setEnabled(filter.type == FilterType::ChromaKey);
        break;
    }
    case FilterType::ImageMask:
        editorStack_->setCurrentWidget(maskPage_);
        maskPathEdit_->blockSignals(true);
        maskPathEdit_->setText(obj.value(QStringLiteral("image_path")).toString());
        maskPathEdit_->blockSignals(false);
        setD(maskOpacitySpin_, obj.value(QStringLiteral("opacity")).toDouble(1.0));
        break;
    case FilterType::ColorGrade:
        editorStack_->setCurrentWidget(gradePage_);
        setD(gradeAmountSpin_, obj.value(QStringLiteral("clut_amount")).toDouble(1.0));
        setD(liftSpin_, obj.value(QStringLiteral("lift")).toDouble(0.0));
        setD(gammaSpin_, obj.value(QStringLiteral("gamma")).toDouble(1.0));
        setD(gainSpin_, obj.value(QStringLiteral("gain")).toDouble(1.0));
        lutPathEdit_->blockSignals(true);
        lutPathEdit_->setText(obj.value(QStringLiteral("image_path")).toString());
        lutPathEdit_->blockSignals(false);
        break;
    case FilterType::Scale:
        editorStack_->setCurrentWidget(scalePage_);
        setD(scaleSpin_, obj.value(QStringLiteral("scale")).toDouble(1.0));
        break;
    case FilterType::Scroll:
        editorStack_->setCurrentWidget(scrollPage_);
        setD(scrollXSpin_, obj.value(QStringLiteral("speed_x")).toDouble(0.0));
        setD(scrollYSpin_, obj.value(QStringLiteral("speed_y")).toDouble(0.0));
        scrollLoopCheck_->blockSignals(true);
        scrollLoopCheck_->setChecked(obj.value(QStringLiteral("loop")).toBool(true));
        scrollLoopCheck_->blockSignals(false);
        break;
    case FilterType::Sharpness:
        editorStack_->setCurrentWidget(sharpPage_);
        setD(sharpSpin_, obj.value(QStringLiteral("sharpness")).toDouble(0.08));
        break;
    case FilterType::RenderDelay:
        editorStack_->setCurrentWidget(delayPage_);
        delaySpin_->blockSignals(true);
        delaySpin_->setValue(obj.value(QStringLiteral("delay_ms")).toInt(0));
        delaySpin_->blockSignals(false);
        break;
    default:
        editorStack_->setCurrentWidget(emptyPage_);
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
    case FilterType::Crop:
        obj[QStringLiteral("relative")] = true;
        obj[QStringLiteral("left")] = cropL_->value();
        obj[QStringLiteral("top")] = cropT_->value();
        obj[QStringLiteral("right")] = cropR_->value();
        obj[QStringLiteral("bottom")] = cropB_->value();
        break;
    case FilterType::ChromaKey:
    case FilterType::ColorKey:
        obj[QStringLiteral("key_color_type")] = keyTypeCombo_->currentText();
        obj[QStringLiteral("similarity")] = keySimSpin_->value();
        obj[QStringLiteral("smoothness")] = keySmoothSpin_->value();
        obj[QStringLiteral("spill")] = keySpillSpin_->value();
        break;
    case FilterType::ImageMask:
        obj[QStringLiteral("image_path")] = maskPathEdit_->text().trimmed();
        obj[QStringLiteral("opacity")] = maskOpacitySpin_->value();
        break;
    case FilterType::ColorGrade:
        obj[QStringLiteral("clut_amount")] = gradeAmountSpin_->value();
        obj[QStringLiteral("lift")] = liftSpin_->value();
        obj[QStringLiteral("gamma")] = gammaSpin_->value();
        obj[QStringLiteral("gain")] = gainSpin_->value();
        obj[QStringLiteral("image_path")] = lutPathEdit_->text().trimmed();
        break;
    case FilterType::Scale:
        obj[QStringLiteral("scale")] = scaleSpin_->value();
        break;
    case FilterType::Scroll:
        obj[QStringLiteral("speed_x")] = scrollXSpin_->value();
        obj[QStringLiteral("speed_y")] = scrollYSpin_->value();
        obj[QStringLiteral("loop")] = scrollLoopCheck_->isChecked();
        break;
    case FilterType::Sharpness:
        obj[QStringLiteral("sharpness")] = sharpSpin_->value();
        break;
    case FilterType::RenderDelay:
        obj[QStringLiteral("delay_ms")] = delaySpin_->value();
        break;
    default:
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
