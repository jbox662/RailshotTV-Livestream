#pragma once

#include <string>

namespace railshot {

enum class ScoreboardDesign {
    Broadcast = 0,
    Minimal = 1,
    Billiards = 2,
    Light = 3,
    HighContrast = 4,
    Neon = 5,
    Retro = 6
};

struct ScoreboardStyle {
    ScoreboardDesign design = ScoreboardDesign::Broadcast;

    std::string plateTop = "#262a36F2";
    std::string plateBottom = "#12141cFA";
    std::string accent = "#dc2626";
    std::string activeColor = "#3b82f6";
    std::string nameText = "#f0f2f7";
    std::string scoreText = "#ffffff";
    std::string metaText = "#9ca3af";
    std::string badgeBackground = "#202430";
    std::string borderColor = "#ffffff33";
    std::string previewBackground = "#1a1a2e";

    std::string fontFamily;
    std::string vsLabel = "VS";

    double nameFontSize = 0.0;
    double scoreFontSize = 0.0;
    double metaFontSize = 0.0;
    double vsFontSize = 0.0;

    bool showGameType = true;
    bool showVs = true;
    bool showRaceTo = true;
    bool showAccentStripe = true;
    bool showShadow = true;
    bool showPlayerNames = true;
    bool showActiveBar = true;
    bool nameBold = true;
    bool scoreBold = true;
    bool uppercaseNames = false;
    bool useGradientPlate = true;

    int barHeight = 110;
    int barWidth = 1920;
    int badgeCornerRadius = 0;
    int badgeWidth = 0;
    int badgeHeight = 0;
    int accentStripeHeight = 0;
    int shadowOffset = 0;
    int shadowOpacity = 70;
    int plateCornerRadius = 0;
    int borderWidth = 0;
    int horizontalPadding = 0;

    [[nodiscard]] static ScoreboardStyle defaults();
    [[nodiscard]] static ScoreboardStyle fromJson(const std::string& json);
    [[nodiscard]] std::string toJson() const;
    void applyPreset(ScoreboardDesign preset);
};

} // namespace railshot
