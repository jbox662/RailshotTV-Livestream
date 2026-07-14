#include "score/ScoreboardStyle.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace railshot {

namespace {

int designToInt(ScoreboardDesign design) {
    return static_cast<int>(design);
}

ScoreboardDesign designFromInt(int value) {
    switch (value) {
    case 1: return ScoreboardDesign::Minimal;
    case 2: return ScoreboardDesign::Billiards;
    case 3: return ScoreboardDesign::Light;
    case 4: return ScoreboardDesign::HighContrast;
    case 5: return ScoreboardDesign::Neon;
    case 6: return ScoreboardDesign::Retro;
    default: return ScoreboardDesign::Broadcast;
    }
}

std::string jsonString(const QJsonObject& obj, const char* key, const std::string& fallback) {
    return obj.value(key).toString(QString::fromStdString(fallback)).toStdString();
}

} // namespace

ScoreboardStyle ScoreboardStyle::defaults() {
    return ScoreboardStyle{};
}

ScoreboardStyle ScoreboardStyle::fromJson(const std::string& json) {
    ScoreboardStyle style = defaults();
    if (json.empty()) {
        return style;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(json));
    if (!doc.isObject()) {
        return style;
    }

    const QJsonObject obj = doc.object();
    style.design = designFromInt(obj.value("design").toInt(designToInt(style.design)));
    style.plateTop = jsonString(obj, "plateTop", style.plateTop);
    style.plateBottom = jsonString(obj, "plateBottom", style.plateBottom);
    style.accent = jsonString(obj, "accent", style.accent);
    style.activeColor = jsonString(obj, "activeColor", style.activeColor);
    style.nameText = jsonString(obj, "nameText", style.nameText);
    style.scoreText = jsonString(obj, "scoreText", style.scoreText);
    style.metaText = jsonString(obj, "metaText", style.metaText);
    style.badgeBackground = jsonString(obj, "badgeBackground", style.badgeBackground);
    style.borderColor = jsonString(obj, "borderColor", style.borderColor);
    style.previewBackground = jsonString(obj, "previewBackground", style.previewBackground);
    style.fontFamily = jsonString(obj, "fontFamily", style.fontFamily);
    style.vsLabel = jsonString(obj, "vsLabel", style.vsLabel);

    style.nameFontSize = obj.value("nameFontSize").toDouble(style.nameFontSize);
    style.scoreFontSize = obj.value("scoreFontSize").toDouble(style.scoreFontSize);
    style.metaFontSize = obj.value("metaFontSize").toDouble(style.metaFontSize);
    style.vsFontSize = obj.value("vsFontSize").toDouble(style.vsFontSize);

    style.showGameType = obj.value("showGameType").toBool(style.showGameType);
    style.showVs = obj.value("showVs").toBool(style.showVs);
    style.showRaceTo = obj.value("showRaceTo").toBool(style.showRaceTo);
    style.showAccentStripe = obj.value("showAccentStripe").toBool(style.showAccentStripe);
    style.showShadow = obj.value("showShadow").toBool(style.showShadow);
    style.showPlayerNames = obj.value("showPlayerNames").toBool(style.showPlayerNames);
    style.showActiveBar = obj.value("showActiveBar").toBool(style.showActiveBar);
    style.nameBold = obj.value("nameBold").toBool(style.nameBold);
    style.scoreBold = obj.value("scoreBold").toBool(style.scoreBold);
    style.uppercaseNames = obj.value("uppercaseNames").toBool(style.uppercaseNames);
    style.useGradientPlate = obj.value("useGradientPlate").toBool(style.useGradientPlate);

    style.barHeight = obj.value("barHeight").toInt(style.barHeight);
    style.barWidth = obj.value("barWidth").toInt(style.barWidth);
    style.badgeCornerRadius = obj.value("badgeCornerRadius").toInt(style.badgeCornerRadius);
    style.badgeWidth = obj.value("badgeWidth").toInt(style.badgeWidth);
    style.badgeHeight = obj.value("badgeHeight").toInt(style.badgeHeight);
    style.accentStripeHeight = obj.value("accentStripeHeight").toInt(style.accentStripeHeight);
    style.shadowOffset = obj.value("shadowOffset").toInt(style.shadowOffset);
    style.shadowOpacity = obj.value("shadowOpacity").toInt(style.shadowOpacity);
    style.plateCornerRadius = obj.value("plateCornerRadius").toInt(style.plateCornerRadius);
    style.borderWidth = obj.value("borderWidth").toInt(style.borderWidth);
    style.horizontalPadding = obj.value("horizontalPadding").toInt(style.horizontalPadding);
    return style;
}

std::string ScoreboardStyle::toJson() const {
    QJsonObject obj;
    obj["design"] = designToInt(design);
    obj["plateTop"] = QString::fromStdString(plateTop);
    obj["plateBottom"] = QString::fromStdString(plateBottom);
    obj["accent"] = QString::fromStdString(accent);
    obj["activeColor"] = QString::fromStdString(activeColor);
    obj["nameText"] = QString::fromStdString(nameText);
    obj["scoreText"] = QString::fromStdString(scoreText);
    obj["metaText"] = QString::fromStdString(metaText);
    obj["badgeBackground"] = QString::fromStdString(badgeBackground);
    obj["borderColor"] = QString::fromStdString(borderColor);
    obj["previewBackground"] = QString::fromStdString(previewBackground);
    obj["fontFamily"] = QString::fromStdString(fontFamily);
    obj["vsLabel"] = QString::fromStdString(vsLabel);
    obj["nameFontSize"] = nameFontSize;
    obj["scoreFontSize"] = scoreFontSize;
    obj["metaFontSize"] = metaFontSize;
    obj["vsFontSize"] = vsFontSize;
    obj["showGameType"] = showGameType;
    obj["showVs"] = showVs;
    obj["showRaceTo"] = showRaceTo;
    obj["showAccentStripe"] = showAccentStripe;
    obj["showShadow"] = showShadow;
    obj["showPlayerNames"] = showPlayerNames;
    obj["showActiveBar"] = showActiveBar;
    obj["nameBold"] = nameBold;
    obj["scoreBold"] = scoreBold;
    obj["uppercaseNames"] = uppercaseNames;
    obj["useGradientPlate"] = useGradientPlate;
    obj["barHeight"] = barHeight;
    obj["barWidth"] = barWidth;
    obj["badgeCornerRadius"] = badgeCornerRadius;
    obj["badgeWidth"] = badgeWidth;
    obj["badgeHeight"] = badgeHeight;
    obj["accentStripeHeight"] = accentStripeHeight;
    obj["shadowOffset"] = shadowOffset;
    obj["shadowOpacity"] = shadowOpacity;
    obj["plateCornerRadius"] = plateCornerRadius;
    obj["borderWidth"] = borderWidth;
    obj["horizontalPadding"] = horizontalPadding;
    return QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString();
}

void ScoreboardStyle::applyPreset(ScoreboardDesign preset) {
    design = preset;
    fontFamily.clear();
    nameFontSize = 0.0;
    scoreFontSize = 0.0;
    metaFontSize = 0.0;
    vsFontSize = 0.0;
    vsLabel = "VS";
    badgeCornerRadius = 0;
    badgeWidth = 0;
    badgeHeight = 0;
    accentStripeHeight = 0;
    shadowOffset = 0;
    shadowOpacity = 70;
    plateCornerRadius = 0;
    borderWidth = 0;
    horizontalPadding = 0;
    useGradientPlate = true;
    uppercaseNames = false;
    showActiveBar = true;
    nameBold = true;
    scoreBold = true;

    switch (preset) {
    case ScoreboardDesign::Broadcast:
        plateTop = "#262a36F2";
        plateBottom = "#12141cFA";
        accent = "#dc2626";
        activeColor = "#3b82f6";
        nameText = "#f0f2f7";
        scoreText = "#ffffff";
        metaText = "#9ca3af";
        badgeBackground = "#202430E6";
        borderColor = "#ffffff33";
        showGameType = true;
        showVs = true;
        showRaceTo = true;
        showAccentStripe = true;
        showShadow = true;
        showPlayerNames = true;
        barHeight = 110;
        barWidth = 1920;
        break;
    case ScoreboardDesign::Minimal:
        plateTop = "#14161cDD";
        plateBottom = "#14161cEE";
        accent = "#6366f1";
        activeColor = "#818cf8";
        nameText = "#e5e7eb";
        scoreText = "#ffffff";
        metaText = "#6b7280";
        badgeBackground = "#1f2937CC";
        borderColor = "#ffffff22";
        showGameType = false;
        showVs = true;
        showRaceTo = true;
        showAccentStripe = false;
        showShadow = false;
        showPlayerNames = true;
        useGradientPlate = false;
        barHeight = 88;
        barWidth = 1920;
        break;
    case ScoreboardDesign::Billiards:
        plateTop = "#0f2e22F0";
        plateBottom = "#071912FA";
        accent = "#d4af37";
        activeColor = "#fbbf24";
        nameText = "#ecfdf5";
        scoreText = "#fef3c7";
        metaText = "#a7f3d0";
        badgeBackground = "#134e32E6";
        borderColor = "#d4af3766";
        showGameType = true;
        showVs = true;
        showRaceTo = true;
        showAccentStripe = true;
        showShadow = true;
        showPlayerNames = true;
        barHeight = 120;
        barWidth = 1920;
        break;
    case ScoreboardDesign::Light:
        plateTop = "#f3f4f6F5";
        plateBottom = "#e5e7ebFA";
        accent = "#2563eb";
        activeColor = "#1d4ed8";
        nameText = "#111827";
        scoreText = "#111827";
        metaText = "#4b5563";
        badgeBackground = "#ffffffDD";
        borderColor = "#11182733";
        showGameType = true;
        showVs = true;
        showRaceTo = true;
        showAccentStripe = true;
        showShadow = true;
        showPlayerNames = true;
        barHeight = 110;
        barWidth = 1920;
        break;
    case ScoreboardDesign::HighContrast:
        plateTop = "#000000F5";
        plateBottom = "#000000FA";
        accent = "#ffff00";
        activeColor = "#00ff00";
        nameText = "#ffffff";
        scoreText = "#ffff00";
        metaText = "#cccccc";
        badgeBackground = "#222222FF";
        borderColor = "#ffff00AA";
        showGameType = true;
        showVs = true;
        showRaceTo = true;
        showAccentStripe = true;
        showShadow = false;
        showPlayerNames = true;
        barHeight = 120;
        barWidth = 1920;
        borderWidth = 2;
        break;
    case ScoreboardDesign::Neon:
        plateTop = "#0a0014F0";
        plateBottom = "#12001eFA";
        accent = "#ff00ff";
        activeColor = "#00ffff";
        nameText = "#f5f3ff";
        scoreText = "#ffffff";
        metaText = "#c4b5fd";
        badgeBackground = "#1e1033E6";
        borderColor = "#ff00ffAA";
        previewBackground = "#0f0518";
        showGameType = true;
        showVs = true;
        showRaceTo = true;
        showAccentStripe = true;
        showShadow = true;
        showPlayerNames = true;
        shadowOpacity = 90;
        barHeight = 118;
        barWidth = 1920;
        borderWidth = 2;
        break;
    case ScoreboardDesign::Retro:
        plateTop = "#2d1810F2";
        plateBottom = "#1a0f0aFA";
        accent = "#f97316";
        activeColor = "#fb923c";
        nameText = "#ffedd5";
        scoreText = "#fff7ed";
        metaText = "#fdba74";
        badgeBackground = "#431407E6";
        borderColor = "#f9731666";
        previewBackground = "#1c0f0a";
        showGameType = true;
        showVs = true;
        showRaceTo = true;
        showAccentStripe = true;
        showShadow = true;
        showPlayerNames = true;
        uppercaseNames = true;
        vsLabel = "VS";
        barHeight = 112;
        barWidth = 1920;
        plateCornerRadius = 0;
        break;
    }
}

} // namespace railshot
