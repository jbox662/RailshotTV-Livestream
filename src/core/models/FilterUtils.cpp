#include "core/models/SourceTypes.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace railshot {

FilterRenderParams resolveFilters(const Source& source) {
    FilterRenderParams params;
    for (const auto& filter : source.filters) {
        if (!filter.enabled) {
            continue;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(filter.paramsJson));
        const QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject{};
        if (filter.type == FilterType::Opacity) {
            const float opacity = static_cast<float>(obj.value(QStringLiteral("opacity")).toDouble(1.0));
            params.opacity *= std::clamp(opacity, 0.0f, 1.0f);
        } else if (filter.type == FilterType::ColorCorrection) {
            params.brightness = static_cast<float>(obj.value(QStringLiteral("brightness")).toDouble(0.0));
            params.contrast = static_cast<float>(obj.value(QStringLiteral("contrast")).toDouble(1.0));
            params.saturation = static_cast<float>(obj.value(QStringLiteral("saturation")).toDouble(1.0));
            params.brightness = std::clamp(params.brightness, -1.0f, 1.0f);
            params.contrast = std::clamp(params.contrast, 0.0f, 2.0f);
            params.saturation = std::clamp(params.saturation, 0.0f, 2.0f);
        }
    }
    return params;
}

} // namespace railshot
