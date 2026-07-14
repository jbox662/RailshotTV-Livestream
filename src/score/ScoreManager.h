#pragma once

#include "core/ProductionProfile.h"
#include "score/MatchState.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>

namespace railshot {

class ScoreManager {
public:
    static ScoreManager& instance();

    ScoreManager(const ScoreManager&) = delete;
    ScoreManager& operator=(const ScoreManager&) = delete;

    [[nodiscard]] ProductionProfile profile() const;
    void setProfile(ProductionProfile profile);

    [[nodiscard]] MatchState state() const;
    void setState(const MatchState& state);
    void resetScores();

    void adjustScore(int player, int delta);
    void setActivePlayer(int player);

    void applyBilliardPreset(GameType type);

    [[nodiscard]] uint64_t version() const { return version_.load(); }

    void setOnStateChanged(std::function<void()> callback);

private:
    ScoreManager() = default;
    void notifyChanged();

    mutable std::mutex mutex_;
    MatchState state_;
    ProductionProfile profile_ = ProductionProfile::General;
    std::atomic<uint64_t> version_{0};
    std::function<void()> onStateChanged_;
};

} // namespace railshot
