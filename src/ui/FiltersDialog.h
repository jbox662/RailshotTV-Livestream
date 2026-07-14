#pragma once

#include "core/models/SourceTypes.h"

#include <QDialog>

class QListWidget;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QStackedWidget;
class QLineEdit;
class QComboBox;

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
    QWidget* emptyPage_ = nullptr;
    QWidget* opacityPage_ = nullptr;
    QWidget* colorPage_ = nullptr;
    QWidget* gainPage_ = nullptr;
    QWidget* compressorPage_ = nullptr;
    QWidget* gatePage_ = nullptr;
    QWidget* suppressPage_ = nullptr;
    QWidget* cropPage_ = nullptr;
    QWidget* keyPage_ = nullptr;
    QWidget* maskPage_ = nullptr;
    QWidget* gradePage_ = nullptr;
    QWidget* scalePage_ = nullptr;
    QWidget* scrollPage_ = nullptr;
    QWidget* sharpPage_ = nullptr;
    QWidget* delayPage_ = nullptr;
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
    QDoubleSpinBox* cropL_ = nullptr;
    QDoubleSpinBox* cropT_ = nullptr;
    QDoubleSpinBox* cropR_ = nullptr;
    QDoubleSpinBox* cropB_ = nullptr;
    QComboBox* keyTypeCombo_ = nullptr;
    QDoubleSpinBox* keySimSpin_ = nullptr;
    QDoubleSpinBox* keySmoothSpin_ = nullptr;
    QDoubleSpinBox* keySpillSpin_ = nullptr;
    QLineEdit* maskPathEdit_ = nullptr;
    QDoubleSpinBox* maskOpacitySpin_ = nullptr;
    QDoubleSpinBox* gradeAmountSpin_ = nullptr;
    QDoubleSpinBox* liftSpin_ = nullptr;
    QDoubleSpinBox* gammaSpin_ = nullptr;
    QDoubleSpinBox* gainSpin_ = nullptr;
    QLineEdit* lutPathEdit_ = nullptr;
    QDoubleSpinBox* scaleSpin_ = nullptr;
    QDoubleSpinBox* scrollXSpin_ = nullptr;
    QDoubleSpinBox* scrollYSpin_ = nullptr;
    QCheckBox* scrollLoopCheck_ = nullptr;
    QDoubleSpinBox* sharpSpin_ = nullptr;
    QSpinBox* delaySpin_ = nullptr;
    int selectedIndex_ = -1;
    FilterType editingKeyType_ = FilterType::ChromaKey;
};

} // namespace railshot
