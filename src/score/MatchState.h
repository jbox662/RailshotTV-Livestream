#pragma once

#include <string>

namespace railshot {

enum class GameType {
    Generic,
    EightBall,
    NineBall
};

struct MatchState {
    std::string player1Name = "Player 1";
    std::string player2Name = "Player 2";
    int player1Score = 0;
    int player2Score = 0;
    int raceTo = 7;
    GameType gameType = GameType::Generic;
    int activePlayer = 1; // 1 or 2
};

} // namespace railshot
