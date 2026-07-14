#pragma once

#include "core/models/SourceTypes.h"

#include <QString>

namespace railshot {

struct MediaSourceSettings {
    static constexpr double kDefaultSpeed = 1.0;

    QString localFile;
    bool isLocalFile = true;
    bool looping = true;
    bool restartOnActivate = true;
    bool hardwareDecode = true;
    double speedPercent = 100.0; // 1–200 (OBS-like percent)

    [[nodiscard]] static MediaSourceSettings defaults();
    [[nodiscard]] static MediaSourceSettings fromSource(const Source& source);
    [[nodiscard]] static MediaSourceSettings fromJson(const std::string& json);
    [[nodiscard]] std::string toJson() const;

    [[nodiscard]] double playbackRate() const;

    void applyToSource(Source& source) const;
};

} // namespace railshot
