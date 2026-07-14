#pragma once

#include "core/models/SourceTypes.h"

#include <QDialog>

class QListWidget;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QStackedWidget;
class QPushButton;

namespace railshot {

class FiltersDialog : public QDialog {
    Q_OBJECT
public:
    explicit FiltersDialog(Source& source, QWidget* parent = nullptr);
    void applyToSource(Source& source) const;

private:
    void rebuildList();
    void onSelectionChanged();
    void onAddFilter(FilterType type);
    void onRemove();
    void syncEditorFromSelection();
    void syncSelectionFromEditor();

    Source draft_;
    QListWidget* list_ = nullptr;
    QStackedWidget* editorStack_ = nullptr;
    QWidget* opacityPage_ = nullptr;
    QWidget* colorPage_ = nullptr;
    QWidget* gainPage_ = nullptr;
    QWidget* compressorPage_ = nullptr;
    QWidget* gatePage_ = nullptr;
    QWidget* suppressPage_ = nullptr;
    QWidget* emptyPage_ = nullptr;
    QCheckBox* enabledCheck_ = nullptr;
    QDoubleSpinBox* opacitySpin_ = nullptr;
    QDoubleSpinBox* brightnessSpin_ = nullptr;
    QDoubleSpinBox* contrastSpin_ = nullptr;
    QDoubleSpinBox* saturationSpin_ = nullptr;
    QDoubleSpinBox* gainDbSpin_ = nullptr;
    QDoubleSpinBox* compRatioSpin_ = nullptr;
    QDoubleSpinBox* compThresholdSpin_ = nullptr;
    QDoubleSpinBox* compAttackSpin_ = nullptr;
    QDoubleSpinBox* compReleaseSpin_ = nullptr;
    QDoubleSpinBox* compOutputSpin_ = nullptr;
    QDoubleSpinBox* gateOpenSpin_ = nullptr;
    QDoubleSpinBox* gateCloseSpin_ = nullptr;
    QDoubleSpinBox* gateAttackSpin_ = nullptr;
    QDoubleSpinBox* gateHoldSpin_ = nullptr;
    QDoubleSpinBox* gateReleaseSpin_ = nullptr;
    QDoubleSpinBox* suppressSpin_ = nullptr;
    int selectedIndex_ = -1;
};

} // namespace railshot
