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
    const QString type = filter.type == FilterType::ColorCorrection ? QStringLiteral("Color Correction")
                                                                    : QStringLiteral("Opacity");
    return filter.enabled ? type : (type + QStringLiteral(" (off)"));
}

std::string defaultParams(FilterType type) {
    QJsonObject obj;
    if (type == FilterType::Opacity) {
        obj[QStringLiteral("opacity")] = 1.0;
    } else {
        obj[QStringLiteral("brightness")] = 0.0;
        obj[QStringLiteral("contrast")] = 1.0;
        obj[QStringLiteral("saturation")] = 1.0;
    }
    return QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString();
}

} // namespace

FiltersDialog::FiltersDialog(Source& source, QWidget* parent)
    : QDialog(parent)
    , draft_(source) {
    setWindowTitle(QStringLiteral("Filters for '%1'").arg(QString::fromStdString(source.name)));
    resize(520, 380);

    auto* root = new QHBoxLayout(this);
    auto* left = new QVBoxLayout();
    list_ = new QListWidget(this);
    left->addWidget(list_, 1);
    auto* addRow = new QHBoxLayout();
    auto* addOpacity = new QPushButton(QStringLiteral("+ Opacity"), this);
    auto* addColor = new QPushButton(QStringLiteral("+ Color"), this);
    auto* remove = new QPushButton(QStringLiteral("Remove"), this);
    addRow->addWidget(addOpacity);
    addRow->addWidget(addColor);
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

    editorStack_->addWidget(emptyPage_);
    editorStack_->addWidget(opacityPage_);
    editorStack_->addWidget(colorPage_);
    right->addWidget(editorStack_, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    right->addWidget(buttons);
    root->addLayout(right, 1);

    connect(addOpacity, &QPushButton::clicked, this, &FiltersDialog::onAddOpacity);
    connect(addColor, &QPushButton::clicked, this, &FiltersDialog::onAddColor);
    connect(remove, &QPushButton::clicked, this, &FiltersDialog::onRemove);
    connect(list_, &QListWidget::currentRowChanged, this, &FiltersDialog::onSelectionChanged);
    connect(enabledCheck_, &QCheckBox::toggled, this, [this](bool) { syncSelectionFromEditor(); });
    connect(opacitySpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double) { syncSelectionFromEditor(); });
    connect(brightnessSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double) { syncSelectionFromEditor(); });
    connect(contrastSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double) { syncSelectionFromEditor(); });
    connect(saturationSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double) { syncSelectionFromEditor(); });
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

void FiltersDialog::onAddOpacity() {
    SourceFilter filter;
    filter.id = SceneManager::generateId();
    filter.type = FilterType::Opacity;
    filter.enabled = true;
    filter.paramsJson = defaultParams(FilterType::Opacity);
    draft_.filters.push_back(filter);
    selectedIndex_ = static_cast<int>(draft_.filters.size()) - 1;
    rebuildList();
}

void FiltersDialog::onAddColor() {
    SourceFilter filter;
    filter.id = SceneManager::generateId();
    filter.type = FilterType::ColorCorrection;
    filter.enabled = true;
    filter.paramsJson = defaultParams(FilterType::ColorCorrection);
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

    if (filter.type == FilterType::Opacity) {
        editorStack_->setCurrentWidget(opacityPage_);
        opacitySpin_->blockSignals(true);
        opacitySpin_->setValue(obj.value(QStringLiteral("opacity")).toDouble(1.0));
        opacitySpin_->blockSignals(false);
    } else {
        editorStack_->setCurrentWidget(colorPage_);
        brightnessSpin_->blockSignals(true);
        contrastSpin_->blockSignals(true);
        saturationSpin_->blockSignals(true);
        brightnessSpin_->setValue(obj.value(QStringLiteral("brightness")).toDouble(0.0));
        contrastSpin_->setValue(obj.value(QStringLiteral("contrast")).toDouble(1.0));
        saturationSpin_->setValue(obj.value(QStringLiteral("saturation")).toDouble(1.0));
        brightnessSpin_->blockSignals(false);
        contrastSpin_->blockSignals(false);
        saturationSpin_->blockSignals(false);
    }
}

void FiltersDialog::syncSelectionFromEditor() {
    if (selectedIndex_ < 0 || selectedIndex_ >= static_cast<int>(draft_.filters.size())) {
        return;
    }
    SourceFilter& filter = draft_.filters[static_cast<size_t>(selectedIndex_)];
    filter.enabled = enabledCheck_->isChecked();
    QJsonObject obj;
    if (filter.type == FilterType::Opacity) {
        obj[QStringLiteral("opacity")] = opacitySpin_->value();
    } else {
        obj[QStringLiteral("brightness")] = brightnessSpin_->value();
        obj[QStringLiteral("contrast")] = contrastSpin_->value();
        obj[QStringLiteral("saturation")] = saturationSpin_->value();
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
