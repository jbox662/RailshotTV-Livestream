#pragma once

#include "score/MatchState.h"
#include "score/ScoreboardStyle.h"

class QImage;
class QPainter;

namespace railshot {

void paintScoreboard(QPainter& painter, const MatchState& state, const ScoreboardStyle& style, int width,
                     int height);

[[nodiscard]] QImage renderScoreboardImage(const MatchState& state, const ScoreboardStyle& style, int width,
                                           int height);

[[nodiscard]] MatchState sampleMatchStateForPreview();

} // namespace railshot
