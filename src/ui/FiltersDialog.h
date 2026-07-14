#pragma once

#include "core/models/SourceTypes.h"

#include <QDialog>

class QListWidget;
class QDoubleSpinBox;
class QCheckBox;
class QStackedWidget;
class QLabel;

namespace railshot {

class FiltersDialog : public QDialog {
    Q_OBJECT
public:
    explicit FiltersDialog(Source& source, QWidget* parent = nullptr);
    void applyToSource(Source& source) const;

private:
    void rebuildList();
    void onSelectionChanged();
    void onAddOpacity();
    void onAddColor();
    void onRemove();
    void syncEditorFromSelection();
    void syncSelectionFromEditor();

    Source draft_;
    QListWidget* list_ = nullptr;
    QStackedWidget* editorStack_ = nullptr;
    QWidget* opacityPage_ = nullptr;
    QWidget* colorPage_ = nullptr;
    QWidget* emptyPage_ = nullptr;
    QCheckBox* enabledCheck_ = nullptr;
    QDoubleSpinBox* opacitySpin_ = nullptr;
    QDoubleSpinBox* brightnessSpin_ = nullptr;
    QDoubleSpinBox* contrastSpin_ = nullptr;
    QDoubleSpinBox* saturationSpin_ = nullptr;
    int selectedIndex_ = -1;
};

} // namespace railshot
