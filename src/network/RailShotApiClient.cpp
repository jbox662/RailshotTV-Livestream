#include "network/RailShotApiClient.h"

#include "core/Logger.h"

namespace railshot {

RailShotApiClient& RailShotApiClient::instance() {
    static RailShotApiClient client;
    return client;
}

void RailShotApiClient::setEnabled(bool enabled) {
    enabled_ = enabled;
    if (enabled) {
        Logger::info("RailShotApiClient: cloud sync enabled (configure API URL + token in settings)");
    }
}

void RailShotApiClient::configure(const std::string& apiBaseUrl, const std::string& accessToken) {
    apiBaseUrl_ = apiBaseUrl;
    accessToken_ = accessToken;
}

void RailShotApiClient::onMatchStateChanged(const MatchState& state) {
    if (!enabled_.load() || apiBaseUrl_.empty()) {
        return;
    }
    Logger::info("RailShotApiClient: would POST score — "
                 + state.player1Name + " " + std::to_string(state.player1Score) + " vs "
                 + state.player2Name + " " + std::to_string(state.player2Score));
}

} // namespace railshot
