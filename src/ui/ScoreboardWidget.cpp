#include "ui/ScoreboardWidget.h"

#include "score/ScoreManager.h"

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace railshot {

ScoreboardWidget::ScoreboardWidget(QWidget* parent)
    : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(8);

    auto* namesRow = new QGridLayout();
    player1Name_ = new QLineEdit(this);
    player2Name_ = new QLineEdit(this);
    namesRow->addWidget(new QLabel("Player 1", this), 0, 0);
    namesRow->addWidget(player1Name_, 0, 1);
    namesRow->addWidget(new QLabel("Player 2", this), 1, 0);
    namesRow->addWidget(player2Name_, 1, 1);
    root->addLayout(namesRow);

    auto* scoreRow = new QHBoxLayout();
    p1ScoreLabel_ = new QLabel("0", this);
    p2ScoreLabel_ = new QLabel("0", this);
    p1ScoreLabel_->setAlignment(Qt::AlignCenter);
    p2ScoreLabel_->setAlignment(Qt::AlignCenter);
    p1ScoreLabel_->setStyleSheet("font-size: 28px; font-weight: 700;");
    p2ScoreLabel_->setStyleSheet("font-size: 28px; font-weight: 700;");

    auto* p1Minus = new QPushButton("-1", this);
    auto* p1Plus = new QPushButton("+1", this);
    auto* p2Minus = new QPushButton("-1", this);
    auto* p2Plus = new QPushButton("+1", this);
    p1Minus->setProperty("player", 1);
    p1Minus->setProperty("delta", -1);
    p1Plus->setProperty("player", 1);
    p1Plus->setProperty("delta", 1);
    p2Minus->setProperty("player", 2);
    p2Minus->setProperty("delta", -1);
    p2Plus->setProperty("player", 2);
    p2Plus->setProperty("delta", 1);
    connect(p1Minus, &QPushButton::clicked, this, &ScoreboardWidget::onAdjustScore);
    connect(p1Plus, &QPushButton::clicked, this, &ScoreboardWidget::onAdjustScore);
    connect(p2Minus, &QPushButton::clicked, this, &ScoreboardWidget::onAdjustScore);
    connect(p2Plus, &QPushButton::clicked, this, &ScoreboardWidget::onAdjustScore);

    scoreRow->addWidget(p1Minus);
    scoreRow->addWidget(p1ScoreLabel_, 1);
    scoreRow->addWidget(p1Plus);
    scoreRow->addSpacing(12);
    scoreRow->addWidget(p2Minus);
    scoreRow->addWidget(p2ScoreLabel_, 1);
    scoreRow->addWidget(p2Plus);
    root->addLayout(scoreRow);

    auto* optionsRow = new QGridLayout();
    raceTo_ = new QSpinBox(this);
    raceTo_->setRange(1, 99);
    gameType_ = new QComboBox(this);
    gameType_->addItem("Generic", static_cast<int>(GameType::Generic));
    gameType_->addItem("8-Ball", static_cast<int>(GameType::EightBall));
    gameType_->addItem("9-Ball", static_cast<int>(GameType::NineBall));
    activePlayer_ = new QComboBox(this);
    activePlayer_->addItem("Player 1", 1);
    activePlayer_->addItem("Player 2", 2);

    optionsRow->addWidget(new QLabel("Race to", this), 0, 0);
    optionsRow->addWidget(raceTo_, 0, 1);
    optionsRow->addWidget(new QLabel("Game type", this), 1, 0);
    optionsRow->addWidget(gameType_, 1, 1);
    optionsRow->addWidget(new QLabel("Active", this), 2, 0);
    optionsRow->addWidget(activePlayer_, 2, 1);
    root->addLayout(optionsRow);

    connect(player1Name_, &QLineEdit::editingFinished, this, &ScoreboardWidget::onStateFieldChanged);
    connect(player2Name_, &QLineEdit::editingFinished, this, &ScoreboardWidget::onStateFieldChanged);
    connect(raceTo_, QOverload<int>::of(&QSpinBox::valueChanged), this, &ScoreboardWidget::onStateFieldChanged);
    connect(gameType_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ScoreboardWidget::onStateFieldChanged);
    connect(activePlayer_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ScoreboardWidget::onStateFieldChanged);

    refreshFromState();
}

void ScoreboardWidget::setBilliardOptionsVisible(bool visible) {
    raceTo_->setVisible(visible);
    gameType_->setVisible(visible);
    raceTo_->parentWidget();
    for (auto* label : findChildren<QLabel*>()) {
        const QString t = label->text();
        if (t == "Race to" || t == "Game type") {
            label->setVisible(visible);
        }
    }
}

void ScoreboardWidget::refreshFromState() {
    updatingUi_ = true;
    const MatchState state = ScoreManager::instance().state();
    player1Name_->setText(QString::fromStdString(state.player1Name));
    player2Name_->setText(QString::fromStdString(state.player2Name));
    p1ScoreLabel_->setText(QString::number(state.player1Score));
    p2ScoreLabel_->setText(QString::number(state.player2Score));
    raceTo_->setValue(state.raceTo);
    const int gtIndex = gameType_->findData(static_cast<int>(state.gameType));
    if (gtIndex >= 0) {
        gameType_->setCurrentIndex(gtIndex);
    }
    activePlayer_->setCurrentIndex(state.activePlayer == 2 ? 1 : 0);
    updatingUi_ = false;
}

void ScoreboardWidget::onAdjustScore() {
    const auto* btn = qobject_cast<QPushButton*>(sender());
    if (!btn) {
        return;
    }
    const int player = btn->property("player").toInt();
    const int delta = btn->property("delta").toInt();
    ScoreManager::instance().adjustScore(player, delta);
    refreshFromState();
}

void ScoreboardWidget::onStateFieldChanged() {
    if (updatingUi_) {
        return;
    }
    MatchState state = ScoreManager::instance().state();
    state.player1Name = player1Name_->text().toStdString();
    state.player2Name = player2Name_->text().toStdString();
    state.raceTo = raceTo_->value();
    state.gameType = static_cast<GameType>(gameType_->currentData().toInt());
    state.activePlayer = activePlayer_->currentData().toInt();
    ScoreManager::instance().setState(state);
    refreshFromState();
}

} // namespace railshot
