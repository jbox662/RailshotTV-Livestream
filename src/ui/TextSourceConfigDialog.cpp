#include "ui/TextSourceConfigDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QVBoxLayout>

namespace railshot {

TextSourceConfigDialog::TextSourceConfigDialog(Source& source, QWidget* parent)
    : QDialog(parent)
    , settings_(TextSourceSettings::fromSource(source)) {
    setWindowTitle(QStringLiteral("Properties for '%1'").arg(QString::fromStdString(source.name)));
    resize(420, 360);
    auto* root = new QVBoxLayout(this);
    textEdit_ = new QPlainTextEdit(this);
    textEdit_->setMinimumHeight(100);
    root->addWidget(textEdit_);
    auto* form = new QFormLayout();
    fontEdit_ = new QLineEdit(this);
    fontSizeSpin_ = new QSpinBox(this);
    fontSizeSpin_->setRange(8, 400);
    widthSpin_ = new QSpinBox(this);
    widthSpin_->setRange(8, 8192);
    heightSpin_ = new QSpinBox(this);
    heightSpin_->setRange(8, 8192);
    colorEdit_ = new QLineEdit(this);
    colorEdit_->setPlaceholderText(QStringLiteral("#AARRGGBB"));
    form->addRow(QStringLiteral("Font"), fontEdit_);
    form->addRow(QStringLiteral("Font size"), fontSizeSpin_);
    form->addRow(QStringLiteral("Width"), widthSpin_);
    form->addRow(QStringLiteral("Height"), heightSpin_);
    form->addRow(QStringLiteral("Color"), colorEdit_);
    root->addLayout(form);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
    loadUi();
}

void TextSourceConfigDialog::loadUi() {
    textEdit_->setPlainText(settings_.text);
    fontEdit_->setText(settings_.fontFamily);
    fontSizeSpin_->setValue(settings_.fontSize);
    widthSpin_->setValue(settings_.width);
    heightSpin_->setValue(settings_.height);
    colorEdit_->setText(QStringLiteral("#%1").arg(settings_.color, 8, 16, QLatin1Char('0')).toUpper());
}

void TextSourceConfigDialog::applyToSource(Source& source) const {
    TextSourceSettings settings = settings_;
    settings.text = textEdit_->toPlainText();
    settings.fontFamily = fontEdit_->text().trimmed();
    if (settings.fontFamily.isEmpty()) {
        settings.fontFamily = QStringLiteral("Bahnschrift");
    }
    settings.fontSize = fontSizeSpin_->value();
    settings.width = widthSpin_->value();
    settings.height = heightSpin_->value();
    QString hex = colorEdit_->text().trimmed();
    if (hex.startsWith(QLatin1Char('#'))) {
        hex = hex.mid(1);
    }
    bool ok = false;
    const auto value = hex.toUInt(&ok, 16);
    if (ok) {
        settings.color = value;
        if (hex.size() <= 6) {
            settings.color |= 0xFF000000u;
        }
    }
    const float x = source.transform.x;
    const float y = source.transform.y;
    settings.applyToSource(source);
    source.transform.x = x;
    source.transform.y = y;
}

} // namespace railshot
