#pragma once

#include "score/MatchState.h"

#include <atomic>
#include <string>

namespace railshot {

class RailShotApiClient {
public:
    static RailShotApiClient& instance();

    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const { return enabled_.load(); }

    void configure(const std::string& apiBaseUrl, const std::string& accessToken);

    void onMatchStateChanged(const MatchState& state);

private:
    RailShotApiClient() = default;

    std::atomic<bool> enabled_{false};
    std::string apiBaseUrl_;
    std::string accessToken_;
};

} // namespace railshot
