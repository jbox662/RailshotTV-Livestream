#include "ui/DockPanel.h"

#include <QCursor>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace railshot {

DockPanel::DockPanel(const QString& title, QWidget* parent)
    : QFrame(parent) {
    setObjectName("rsDockPanel");
    setFrameShape(QFrame::NoFrame);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* header = new QWidget(this);
    header->setObjectName("rsDockHeader");
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 8, 10, 6);
    headerLayout->setSpacing(6);

    auto* accent = new QFrame(header);
    accent->setObjectName("rsDockAccent");
    accent->setFixedSize(3, 14);
    headerLayout->addWidget(accent);

    auto* titleLabel = new QLabel(title, header);
    titleLabel->setObjectName("rsDockTitle");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    auto* body = new QWidget(this);
    body->setObjectName("rsDockBody");
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);
    contentLayout_ = new QVBoxLayout();
    contentLayout_->setContentsMargins(0, 0, 0, 0);
    contentLayout_->setSpacing(0);
    bodyLayout->addLayout(contentLayout_, 1);

    auto* footer = new QWidget(this);
    footer_ = footer;
    footer->setObjectName("rsDockFooter");
    footerLayout_ = new QHBoxLayout(footer);
    footerLayout_->setContentsMargins(8, 4, 8, 6);
    footerLayout_->setSpacing(2);
    footerLayout_->addStretch();

    root->addWidget(header);
    root->addWidget(body, 1);
    root->addWidget(footer);
}

void DockPanel::setFooterVisible(bool visible) {
    if (footer_) {
        footer_->setVisible(visible);
    }
}

QToolButton* DockPanel::insertFooterButton(QToolButton* button) {
    button->setObjectName("rsDockToolButton");
    button->setAutoRaise(false);
    button->setFixedSize(28, 28);
    button->setCursor(Qt::PointingHandCursor);
    footerLayout_->insertWidget(footerLayout_->count() - 1, button);
    return button;
}

QToolButton* DockPanel::addFooterButton(QStyle::StandardPixmap icon, const QString& tooltip) {
    auto* button = new QToolButton(this);
    button->setIcon(style()->standardIcon(icon));
    button->setToolTip(tooltip);
    return insertFooterButton(button);
}

QToolButton* DockPanel::addFooterButton(const QString& text, const QString& tooltip) {
    auto* button = new QToolButton(this);
    button->setText(text);
    button->setToolTip(tooltip);
    return insertFooterButton(button);
}

} // namespace railshot
