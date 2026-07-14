#pragma once

#include "capture/TextSource.h"
#include "core/models/SourceTypes.h"

#include <QDialog>

class QSpinBox;
class QLineEdit;
class QPlainTextEdit;

namespace railshot {

class TextSourceConfigDialog : public QDialog {
    Q_OBJECT
public:
    explicit TextSourceConfigDialog(Source& source, QWidget* parent = nullptr);
    void applyToSource(Source& source) const;

private:
    void loadUi();
    TextSourceSettings settings_;
    QPlainTextEdit* textEdit_ = nullptr;
    QLineEdit* fontEdit_ = nullptr;
    QSpinBox* fontSizeSpin_ = nullptr;
    QSpinBox* widthSpin_ = nullptr;
    QSpinBox* heightSpin_ = nullptr;
    QLineEdit* colorEdit_ = nullptr;
};

} // namespace railshot
