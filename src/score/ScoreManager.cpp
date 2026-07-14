#include "score/ScoreManager.h"

#include <algorithm>

namespace railshot {

ScoreManager& ScoreManager::instance() {
    static ScoreManager manager;
    return manager;
}

ProductionProfile ScoreManager::profile() const {
    std::lock_guard lock(mutex_);
    return profile_;
}

void ScoreManager::setProfile(ProductionProfile profile) {
    std::lock_guard lock(mutex_);
    if (profile_ == profile) {
        return;
    }
    profile_ = profile;
    notifyChanged();
}

MatchState ScoreManager::state() const {
    std::lock_guard lock(mutex_);
    return state_;
}

void ScoreManager::setState(const MatchState& state) {
    std::lock_guard lock(mutex_);
    state_ = state;
    state_.player1Score = std::max(0, state_.player1Score);
    state_.player2Score = std::max(0, state_.player2Score);
    if (state_.activePlayer != 1 && state_.activePlayer != 2) {
        state_.activePlayer = 1;
    }
    notifyChanged();
}

void ScoreManager::resetScores() {
    std::lock_guard lock(mutex_);
    state_.player1Score = 0;
    state_.player2Score = 0;
    notifyChanged();
}

void ScoreManager::adjustScore(int player, int delta) {
    std::lock_guard lock(mutex_);
    if (player == 1) {
        state_.player1Score = std::max(0, state_.player1Score + delta);
    } else if (player == 2) {
        state_.player2Score = std::max(0, state_.player2Score + delta);
    }
    notifyChanged();
}

void ScoreManager::setActivePlayer(int player) {
    std::lock_guard lock(mutex_);
    if (player == 1 || player == 2) {
        state_.activePlayer = player;
        notifyChanged();
    }
}

void ScoreManager::applyBilliardPreset(GameType type) {
    std::lock_guard lock(mutex_);
    state_.gameType = type;
    state_.player1Score = 0;
    state_.player2Score = 0;
    state_.activePlayer = 1;
    if (type == GameType::EightBall) {
        state_.raceTo = 1;
        state_.player1Name = "Player 1";
        state_.player2Name = "Player 2";
    } else if (type == GameType::NineBall) {
        state_.raceTo = 9;
        state_.player1Name = "Player 1";
        state_.player2Name = "Player 2";
    }
    notifyChanged();
}

void ScoreManager::setOnStateChanged(std::function<void()> callback) {
    std::lock_guard lock(mutex_);
    onStateChanged_ = std::move(callback);
}

void ScoreManager::notifyChanged() {
    version_.fetch_add(1);
    if (onStateChanged_) {
        onStateChanged_();
    }
}

} // namespace railshot
