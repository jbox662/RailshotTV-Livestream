#pragma once

#include "capture/ColorSource.h"
#include "core/models/SourceTypes.h"

#include <QDialog>

class QSpinBox;
class QLineEdit;

namespace railshot {

class ColorSourceConfigDialog : public QDialog {
    Q_OBJECT
public:
    explicit ColorSourceConfigDialog(Source& source, QWidget* parent = nullptr);
    void applyToSource(Source& source) const;

private:
    void loadUi();
    ColorSourceSettings settings_;
    QSpinBox* widthSpin_ = nullptr;
    QSpinBox* heightSpin_ = nullptr;
    QLineEdit* colorEdit_ = nullptr;
};

} // namespace railshot
