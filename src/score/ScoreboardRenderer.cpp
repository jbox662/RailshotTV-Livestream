#include "score/ScoreboardRenderer.h"

#include <QFont>
#include <QFontDatabase>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace railshot {

namespace {

constexpr int kReferenceWidth = 1920;

const char* gameTypeLabel(GameType type) {
    switch (type) {
    case GameType::EightBall: return "8-BALL";
    case GameType::NineBall: return "9-BALL";
    case GameType::Generic: return "";
    }
    return "";
}

QColor colorFromHex(const std::string& hex, const QColor& fallback = QColor(Qt::white)) {
    QString value = QString::fromStdString(hex);
    if (value.startsWith('#')) {
        value = value.mid(1);
    }
    if (value.length() == 6) {
        value += "FF";
    }
    bool ok = false;
    const QRgb rgb = value.toUInt(&ok, 16);
    return ok ? QColor::fromRgba(rgb) : fallback;
}

QFont makeFont(const ScoreboardStyle& style, double autoSize, double scale, bool bold) {
    QFont font;
    if (!style.fontFamily.empty()) {
        font.setFamily(QString::fromStdString(style.fontFamily));
    } else {
        font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    }
    font.setPointSizeF(autoSize * scale);
    font.setBold(bold);
    return font;
}

double autoNameSize(const ScoreboardStyle& style) {
    if (style.nameFontSize > 0.0) {
        return style.nameFontSize;
    }
    if (style.design == ScoreboardDesign::Minimal) {
        return 17.0;
    }
    if (style.design == ScoreboardDesign::Retro) {
        return 18.0;
    }
    return 20.0;
}

double autoScoreSize(const ScoreboardStyle& style) {
    if (style.scoreFontSize > 0.0) {
        return style.scoreFontSize;
    }
    switch (style.design) {
    case ScoreboardDesign::Minimal: return 28.0;
    case ScoreboardDesign::Billiards: return 36.0;
    case ScoreboardDesign::Neon: return 38.0;
    case ScoreboardDesign::Retro: return 32.0;
    default: return 34.0;
    }
}

double autoMetaSize(const ScoreboardStyle& style) {
    return style.metaFontSize > 0.0 ? style.metaFontSize : 11.0;
}

double autoVsSize(const ScoreboardStyle& style) {
    return style.vsFontSize > 0.0 ? style.vsFontSize : 16.0;
}

int autoBadgeWidth(const ScoreboardStyle& style, double scale) {
    if (style.badgeWidth > 0) {
        return static_cast<int>(style.badgeWidth * scale);
    }
    const int base = style.design == ScoreboardDesign::Minimal ? 60 : 72;
    return static_cast<int>(base * scale);
}

int autoBadgeHeight(const ScoreboardStyle& style, double scale) {
    if (style.badgeHeight > 0) {
        return static_cast<int>(style.badgeHeight * scale);
    }
    const int base = style.design == ScoreboardDesign::Minimal ? 52 : 64;
    return static_cast<int>(base * scale);
}

int autoBadgeRadius(const ScoreboardStyle& style, double scale) {
    if (style.badgeCornerRadius > 0) {
        return static_cast<int>(style.badgeCornerRadius * scale);
    }
    if (style.design == ScoreboardDesign::Retro) {
        return static_cast<int>(2 * scale);
    }
    return static_cast<int>((style.design == ScoreboardDesign::Minimal ? 4 : 8) * scale);
}

void drawScoreBadge(QPainter& painter, const QRect& rect, int score, bool active, const ScoreboardStyle& style,
                    double scale) {
    QPainterPath path;
    const int radius = autoBadgeRadius(style, scale);
    path.addRoundedRect(rect, radius, radius);

    const QColor activeColor = colorFromHex(style.activeColor, QColor(59, 130, 246));
    const QColor badgeBg = colorFromHex(style.badgeBackground, QColor(32, 36, 48, 230));
    const QColor scoreColor = colorFromHex(style.scoreText, QColor(255, 255, 255));

    if (active) {
        if (style.design == ScoreboardDesign::Neon) {
            painter.fillPath(path, activeColor);
            painter.setPen(QPen(activeColor.lighter(160), std::max(2, static_cast<int>(3 * scale))));
        } else {
            QLinearGradient fill(rect.topLeft(), rect.bottomLeft());
            fill.setColorAt(0, activeColor.lighter(110));
            fill.setColorAt(1, activeColor);
            painter.fillPath(path, fill);
            painter.setPen(QPen(activeColor.lighter(150), std::max(1, static_cast<int>(2 * scale))));
        }
    } else {
        painter.fillPath(path, badgeBg);
        painter.setPen(QPen(badgeBg.darker(130), std::max(1, static_cast<int>(2 * scale))));
    }
    painter.drawPath(path);

    if (style.design == ScoreboardDesign::HighContrast || style.design == ScoreboardDesign::Neon) {
        painter.setPen(QPen(colorFromHex(style.accent, Qt::yellow), std::max(1, static_cast<int>(2 * scale))));
        painter.drawPath(path);
    }

    QFont scoreFont = makeFont(style, autoScoreSize(style), scale, style.scoreBold);
    scoreFont.setLetterSpacing(QFont::AbsoluteSpacing, -1);
    painter.setFont(scoreFont);
    painter.setPen(scoreColor);
    painter.drawText(rect, Qt::AlignCenter, QString::number(score));
}

void fillPlateBackground(QPainter& painter, const QRect& plate, const ScoreboardStyle& style) {
    const QColor plateTop = colorFromHex(style.plateTop, QColor(38, 42, 54, 242));
    const QColor plateBottom = colorFromHex(style.plateBottom, QColor(18, 20, 28, 250));

    if (style.plateCornerRadius > 0) {
        QPainterPath path;
        path.addRoundedRect(plate, style.plateCornerRadius, style.plateCornerRadius);
        if (!style.useGradientPlate || style.design == ScoreboardDesign::Minimal) {
            painter.fillPath(path, plateTop);
        } else {
            QLinearGradient plateGrad(plate.topLeft(), plate.bottomLeft());
            plateGrad.setColorAt(0.0, plateTop);
            plateGrad.setColorAt(1.0, plateBottom);
            painter.fillPath(path, plateGrad);
        }
        if (style.borderWidth > 0) {
            painter.setPen(QPen(colorFromHex(style.borderColor, QColor(255, 255, 255, 51)), style.borderWidth));
            painter.drawPath(path);
        }
        return;
    }

    if (!style.useGradientPlate || style.design == ScoreboardDesign::Minimal) {
        painter.fillRect(plate, plateTop);
    } else {
        QLinearGradient plateGrad(plate.topLeft(), plate.bottomLeft());
        plateGrad.setColorAt(0.0, plateTop);
        plateGrad.setColorAt(1.0, plateBottom);
        painter.fillRect(plate, plateGrad);
    }

    if (style.borderWidth > 0) {
        painter.setPen(QPen(colorFromHex(style.borderColor, QColor(255, 255, 255, 51)), style.borderWidth));
        painter.drawRect(plate.adjusted(0, 0, -1, -1));
    }
}

} // namespace

MatchState sampleMatchStateForPreview() {
    MatchState state;
    state.player1Name = "Player One";
    state.player2Name = "Player Two";
    state.player1Score = 4;
    state.player2Score = 3;
    state.activePlayer = 1;
    state.raceTo = 7;
    state.gameType = GameType::NineBall;
    return state;
}

void paintScoreboard(QPainter& painter, const MatchState& state, const ScoreboardStyle& style, int w, int h) {
    const double scale = static_cast<double>(w) / static_cast<double>(kReferenceWidth);
    const QRect plate(0, 0, w, h);

    const QColor accent = colorFromHex(style.accent, QColor(220, 38, 38));
    const QColor nameColor = colorFromHex(style.nameText, QColor(240, 242, 247));
    const QColor activeNameColor = colorFromHex(style.activeColor, QColor(147, 197, 253)).lighter(130);
    const QColor metaColor = colorFromHex(style.metaText, QColor(156, 163, 175));

    if (style.showShadow) {
        const int shadowOffset =
            style.shadowOffset > 0 ? style.shadowOffset : std::max(2, static_cast<int>(4 * scale));
        const int alpha = std::clamp(style.shadowOpacity, 0, 255);
        painter.fillRect(plate.adjusted(0, shadowOffset, 0, shadowOffset), QColor(0, 0, 0, alpha));
    }

    fillPlateBackground(painter, plate, style);

    if (style.showAccentStripe) {
        const int stripeH = style.accentStripeHeight > 0
                                ? std::max(1, static_cast<int>(style.accentStripeHeight * scale))
                                : std::max(2, static_cast<int>(3 * scale));
        painter.fillRect(0, h - stripeH, w, stripeH, accent);
    }

    if (style.design == ScoreboardDesign::Billiards || style.design == ScoreboardDesign::Retro) {
        painter.setPen(QPen(accent, std::max(1, static_cast<int>(2 * scale))));
        painter.drawLine(0, static_cast<int>(2 * scale), w, static_cast<int>(2 * scale));
    }

    if (style.design == ScoreboardDesign::Neon) {
        painter.setPen(QPen(accent, std::max(1, static_cast<int>(2 * scale))));
        painter.drawRect(plate.adjusted(1, 1, -2, -2));
    }

    if (style.design != ScoreboardDesign::HighContrast) {
        painter.setPen(QPen(QColor(255, 255, 255, 28), 1));
        painter.drawLine(0, 0, w, 0);
    }

    const int pad = style.horizontalPadding > 0
                        ? static_cast<int>(style.horizontalPadding * scale)
                        : static_cast<int>((style.design == ScoreboardDesign::Minimal ? 28 : 40) * scale);
    const int badgeW = autoBadgeWidth(style, scale);
    const int badgeH = autoBadgeHeight(style, scale);
    const int midX = w / 2;

    QFont nameFont = makeFont(style, autoNameSize(style), scale, style.nameBold);
    QFont metaFont = makeFont(style, autoMetaSize(style), scale, false);
    QFont vsFont = makeFont(style, autoVsSize(style), scale, true);

    auto displayName = [&](const QString& name) {
        return style.uppercaseNames ? name.toUpper() : name;
    };

    auto drawPlayerBlock = [&](bool leftSide, const QString& name, int score, bool active) {
        const int badgeX = leftSide ? pad : w - pad - badgeW;
        const int badgeY = (h - badgeH) / 2;
        const QRect badge(badgeX, badgeY, badgeW, badgeH);
        drawScoreBadge(painter, badge, score, active, style, scale);

        if (style.showPlayerNames) {
            QRect nameRect;
            const QString label = displayName(name);
            if (leftSide) {
                nameRect =
                    QRect(badge.right() + static_cast<int>(16 * scale), badgeY - static_cast<int>(4 * scale),
                          midX - badge.right() - static_cast<int>(32 * scale), badgeH + static_cast<int>(8 * scale));
                painter.setPen(active ? activeNameColor : nameColor);
                painter.setFont(nameFont);
                painter.drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, label);
            } else {
                nameRect =
                    QRect(midX + static_cast<int>(16 * scale), badgeY - static_cast<int>(4 * scale),
                          badgeX - midX - static_cast<int>(32 * scale), badgeH + static_cast<int>(8 * scale));
                painter.setPen(active ? activeNameColor : nameColor);
                painter.setFont(nameFont);
                painter.drawText(nameRect, Qt::AlignRight | Qt::AlignVCenter, label);
            }
        }

        if (active && style.showActiveBar) {
            const int barW = std::max(3, static_cast<int>(4 * scale));
            const int barH = static_cast<int>(36 * scale);
            const int barY = (h - barH) / 2;
            const int barX = leftSide ? 0 : w - barW;
            painter.fillRect(barX, barY, barW, barH, colorFromHex(style.activeColor, QColor(59, 130, 246)));
        }
    };

    drawPlayerBlock(true, QString::fromStdString(state.player1Name), state.player1Score, state.activePlayer == 1);
    drawPlayerBlock(false, QString::fromStdString(state.player2Name), state.player2Score, state.activePlayer == 2);

    if (style.showGameType) {
        QString centerTitle = "MATCH";
        const char* gt = gameTypeLabel(state.gameType);
        if (gt[0] != '\0') {
            centerTitle = QString::fromUtf8(gt);
        }

        painter.setPen(metaColor);
        painter.setFont(metaFont);
        painter.drawText(QRect(midX - static_cast<int>(120 * scale), static_cast<int>(18 * scale),
                               static_cast<int>(240 * scale), static_cast<int>(20 * scale)),
                         Qt::AlignCenter, centerTitle);
    }

    if (style.showVs) {
        painter.setPen(nameColor);
        painter.setFont(vsFont);
        const QString vsText = QString::fromStdString(style.vsLabel);
        painter.drawText(QRect(midX - static_cast<int>(60 * scale), (h - static_cast<int>(30 * scale)) / 2,
                               static_cast<int>(120 * scale), static_cast<int>(30 * scale)),
                         Qt::AlignCenter, vsText);
    }

    if (style.showRaceTo && state.raceTo > 0 && state.gameType != GameType::EightBall) {
        painter.setPen(metaColor);
        painter.setFont(metaFont);
        painter.drawText(QRect(midX - static_cast<int>(120 * scale), h - static_cast<int>(28 * scale),
                               static_cast<int>(240 * scale), static_cast<int>(20 * scale)),
                         Qt::AlignCenter, QString("Race to %1").arg(state.raceTo));
    }
}

QImage renderScoreboardImage(const MatchState& state, const ScoreboardStyle& style, int width, int height) {
    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    if (!painter.isActive()) {
        return image;
    }
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    paintScoreboard(painter, state, style, width, height);
    painter.end();
    return image;
}

} // namespace railshot
