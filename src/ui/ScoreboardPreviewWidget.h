#pragma once

#include <QWidget>

class QImage;

namespace railshot {

class ScoreboardPreviewWidget : public QWidget {
    Q_OBJECT

public:
    explicit ScoreboardPreviewWidget(QWidget* parent = nullptr);

    void setScoreboardImage(const QImage& image);
    void setPreviewBackground(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QImage scoreboardImage_;
    QColor previewBackground_{0x1a, 0x1a, 0x2e};
};

} // namespace railshot
