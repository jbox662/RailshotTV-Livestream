#pragma once

#include "core/models/SourceTypes.h"
#include "score/ScoreboardStyle.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFontComboBox;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace railshot {

class ScoreboardPreviewWidget;

class ScoreboardConfigDialog : public QDialog {
    Q_OBJECT

public:
    explicit ScoreboardConfigDialog(const Source& source, QWidget* parent = nullptr);

    void applyToSource(Source& source) const;

signals:
    void liveStyleChanged(const ScoreboardStyle& style);

private slots:
    void onPresetChanged(int index);
    void onStyleEdited();
    void updatePreview();

private:
    void pickColor(QPushButton* button, std::string ScoreboardStyle::* field);
    void loadFromStyle(const ScoreboardStyle& style);
    [[nodiscard]] ScoreboardStyle currentStyle() const;

    ScoreboardStyle style_;
    uint64_t lastScoreVersion_ = 0;

    ScoreboardPreviewWidget* previewWidget_ = nullptr;

    QComboBox* presetCombo_ = nullptr;
    QFontComboBox* fontCombo_ = nullptr;
    QLineEdit* vsLabelEdit_ = nullptr;
    QSpinBox* barHeightSpin_ = nullptr;
    QSpinBox* barWidthSpin_ = nullptr;
    QDoubleSpinBox* nameFontSpin_ = nullptr;
    QDoubleSpinBox* scoreFontSpin_ = nullptr;
    QDoubleSpinBox* metaFontSpin_ = nullptr;
    QDoubleSpinBox* vsFontSpin_ = nullptr;
    QSpinBox* badgeWidthSpin_ = nullptr;
    QSpinBox* badgeHeightSpin_ = nullptr;
    QSpinBox* badgeRadiusSpin_ = nullptr;
    QSpinBox* accentStripeSpin_ = nullptr;
    QSpinBox* shadowOffsetSpin_ = nullptr;
    QSpinBox* shadowOpacitySpin_ = nullptr;
    QSpinBox* plateRadiusSpin_ = nullptr;
    QSpinBox* borderWidthSpin_ = nullptr;
    QSpinBox* paddingSpin_ = nullptr;

    QCheckBox* showGameType_ = nullptr;
    QCheckBox* showVs_ = nullptr;
    QCheckBox* showRaceTo_ = nullptr;
    QCheckBox* showAccentStripe_ = nullptr;
    QCheckBox* showShadow_ = nullptr;
    QCheckBox* showPlayerNames_ = nullptr;
    QCheckBox* showActiveBar_ = nullptr;
    QCheckBox* nameBold_ = nullptr;
    QCheckBox* scoreBold_ = nullptr;
    QCheckBox* uppercaseNames_ = nullptr;
    QCheckBox* useGradientPlate_ = nullptr;

    QPushButton* plateTopBtn_ = nullptr;
    QPushButton* plateBottomBtn_ = nullptr;
    QPushButton* accentBtn_ = nullptr;
    QPushButton* activeBtn_ = nullptr;
    QPushButton* nameTextBtn_ = nullptr;
    QPushButton* scoreTextBtn_ = nullptr;
    QPushButton* metaTextBtn_ = nullptr;
    QPushButton* badgeBgBtn_ = nullptr;
    QPushButton* borderColorBtn_ = nullptr;
    QPushButton* previewBgBtn_ = nullptr;
};

} // namespace railshot
