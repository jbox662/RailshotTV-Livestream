#include "ui/ScoreboardPreviewWidget.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>

namespace railshot {

ScoreboardPreviewWidget::ScoreboardPreviewWidget(QWidget* parent)
    : QWidget(parent) {
    setMinimumHeight(160);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ScoreboardPreviewWidget::setScoreboardImage(const QImage& image) {
    scoreboardImage_ = image;
    update();
}

void ScoreboardPreviewWidget::setPreviewBackground(const QColor& color) {
    previewBackground_ = color;
    update();
}

void ScoreboardPreviewWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.fillRect(rect(), previewBackground_);

    // Simulated video frame behind the overlay.
    QLinearGradient videoBg(0, 0, width(), height());
    videoBg.setColorAt(0.0, previewBackground_.lighter(115));
    videoBg.setColorAt(1.0, previewBackground_.darker(115));
    painter.fillRect(rect(), videoBg);

    if (scoreboardImage_.isNull()) {
        painter.setPen(QColor(180, 180, 190));
        painter.drawText(rect(), Qt::AlignCenter, "Preview will appear here");
        return;
    }

    const int margin = 12;
    const QRect target = rect().adjusted(margin, margin, -margin, -margin);
    const QImage scaled =
        scoreboardImage_.scaled(target.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const int x = target.left() + (target.width() - scaled.width()) / 2;
    const int y = target.bottom() - scaled.height();
    painter.drawImage(x, y, scaled);

    painter.setPen(QColor(255, 255, 255, 40));
    painter.drawRect(target.adjusted(0, 0, -1, -1));
}

void ScoreboardPreviewWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    update();
}

} // namespace railshot
