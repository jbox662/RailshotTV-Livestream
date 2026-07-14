#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;

namespace railshot {

class ScoreboardWidget : public QWidget {
    Q_OBJECT

public:
    explicit ScoreboardWidget(QWidget* parent = nullptr);

    void refreshFromState();
    void setBilliardOptionsVisible(bool visible);

private slots:
    void onAdjustScore();
    void onStateFieldChanged();

private:
    void bindStateToUi();

    QLineEdit* player1Name_ = nullptr;
    QLineEdit* player2Name_ = nullptr;
    QSpinBox* raceTo_ = nullptr;
    QComboBox* gameType_ = nullptr;
    QComboBox* activePlayer_ = nullptr;
    QLabel* p1ScoreLabel_ = nullptr;
    QLabel* p2ScoreLabel_ = nullptr;
    bool updatingUi_ = false;
};

} // namespace railshot
