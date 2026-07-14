#pragma once

#include <QFrame>
#include <QHBoxLayout>
#include <QStyle>
#include <QVBoxLayout>

class QLabel;
class QToolButton;

namespace railshot {

// Bottom production panel chrome — RailShot branded.
class DockPanel : public QFrame {
    Q_OBJECT

public:
    explicit DockPanel(const QString& title, QWidget* parent = nullptr);

    [[nodiscard]] QVBoxLayout* contentLayout() const { return contentLayout_; }
    [[nodiscard]] QHBoxLayout* footerLayout() const { return footerLayout_; }
    void setFooterVisible(bool visible);

    QToolButton* addFooterButton(QStyle::StandardPixmap icon, const QString& tooltip);
    QToolButton* addFooterButton(const QString& text, const QString& tooltip);

private:
    QToolButton* insertFooterButton(QToolButton* button);

    QVBoxLayout* contentLayout_ = nullptr;
    QHBoxLayout* footerLayout_ = nullptr;
    QWidget* footer_ = nullptr;
};

} // namespace railshot
