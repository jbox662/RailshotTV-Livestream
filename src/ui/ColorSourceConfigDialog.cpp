#include "ui/ColorSourceConfigDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

namespace railshot {

ColorSourceConfigDialog::ColorSourceConfigDialog(Source& source, QWidget* parent)
    : QDialog(parent)
    , settings_(ColorSourceSettings::fromSource(source)) {
    setWindowTitle(QStringLiteral("Properties for '%1'").arg(QString::fromStdString(source.name)));
    resize(360, 200);
    auto* root = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    widthSpin_ = new QSpinBox(this);
    widthSpin_->setRange(8, 8192);
    heightSpin_ = new QSpinBox(this);
    heightSpin_->setRange(8, 8192);
    colorEdit_ = new QLineEdit(this);
    colorEdit_->setPlaceholderText(QStringLiteral("#AARRGGBB"));
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

void ColorSourceConfigDialog::loadUi() {
    widthSpin_->setValue(settings_.width);
    heightSpin_->setValue(settings_.height);
    colorEdit_->setText(QStringLiteral("#%1").arg(settings_.color, 8, 16, QLatin1Char('0')).toUpper());
}

void ColorSourceConfigDialog::applyToSource(Source& source) const {
    ColorSourceSettings settings = settings_;
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
