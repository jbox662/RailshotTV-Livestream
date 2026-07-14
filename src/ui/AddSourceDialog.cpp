#include "ui/AddSourceDialog.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QVector>
#include <QVBoxLayout>

namespace railshot {
namespace {

struct SourceOption {
    QString id;
    QString title;
    QString description;
    QString category;
    QString defaultName;
};

QVector<SourceOption> allOptions() {
    return {
        {QStringLiteral("video_device"), QStringLiteral("Video Device"),
         QStringLiteral("Webcam, capture card, or other DirectShow camera"),
         QStringLiteral("Capture"), QStringLiteral("Video Capture Device")},
        {QStringLiteral("display_capture"), QStringLiteral("Display Capture"),
         QStringLiteral("Capture an entire monitor"),
         QStringLiteral("Capture"), QStringLiteral("Display Capture")},
        {QStringLiteral("window_capture"), QStringLiteral("Window Capture"),
         QStringLiteral("Capture a single application window (WGC)"),
         QStringLiteral("Capture"), QStringLiteral("Window Capture")},
        {QStringLiteral("game_capture"), QStringLiteral("Game Capture"),
         QStringLiteral("Inject-free game/window capture via Graphics Capture"),
         QStringLiteral("Capture"), QStringLiteral("Game Capture")},
        {QStringLiteral("image"), QStringLiteral("Image"),
         QStringLiteral("Static PNG or JPEG on the canvas"),
         QStringLiteral("Media"), QStringLiteral("Image")},
        {QStringLiteral("media_file"), QStringLiteral("Media File"),
         QStringLiteral("Video or audio file with optional loop"),
         QStringLiteral("Media"), QStringLiteral("Media Source")},
        {QStringLiteral("audio_file"), QStringLiteral("Audio File"),
         QStringLiteral("Audio-only clip routed through the mixer"),
         QStringLiteral("Media"), QStringLiteral("Audio File")},
        {QStringLiteral("desktop_audio"), QStringLiteral("Desktop Audio"),
         QStringLiteral("System playback via WASAPI loopback"),
         QStringLiteral("Audio"), QStringLiteral("Desktop Audio")},
        {QStringLiteral("app_audio"), QStringLiteral("Application Audio Capture"),
         QStringLiteral("Capture audio from a single process (WASAPI process loopback)"),
         QStringLiteral("Audio"), QStringLiteral("Application Audio Capture")},
        {QStringLiteral("audio_input"), QStringLiteral("Audio Input Capture"),
         QStringLiteral("Microphone or line-in via WASAPI"),
         QStringLiteral("Audio"), QStringLiteral("Audio Input Capture")},
        {QStringLiteral("ndi"), QStringLiteral("NDI Source"),
         QStringLiteral("Network video from an NDI sender"),
         QStringLiteral("Network"), QStringLiteral("NDI Source")},
        {QStringLiteral("browser"), QStringLiteral("Browser"),
         QStringLiteral("Web page or overlay URL (WebView2)"),
         QStringLiteral("Overlay"), QStringLiteral("Browser Source")},
        {QStringLiteral("text"), QStringLiteral("Text"),
         QStringLiteral("On-screen title or lower-third style text"),
         QStringLiteral("Overlay"), QStringLiteral("Text")},
        {QStringLiteral("color"), QStringLiteral("Color Source"),
         QStringLiteral("Solid color plate for backgrounds or mats"),
         QStringLiteral("Overlay"), QStringLiteral("Color Source")},
        {QStringLiteral("scoreboard"), QStringLiteral("Scoreboard"),
         QStringLiteral("RailShot live score overlay"),
         QStringLiteral("RailShot"), QStringLiteral("Scoreboard")},
    };
}

QColor accentForCategory(const QString& category) {
    if (category == QLatin1String("Capture")) {
        return QColor(0x2f, 0x9e, 0x62);
    }
    if (category == QLatin1String("Media")) {
        return QColor(0xc9, 0xa2, 0x27);
    }
    if (category == QLatin1String("Audio")) {
        return QColor(0x5c, 0xb8, 0xd6);
    }
    if (category == QLatin1String("Network")) {
        return QColor(0x6b, 0x9f, 0x7a);
    }
    if (category == QLatin1String("Overlay")) {
        return QColor(0xe0, 0x8a, 0x3c);
    }
    return QColor(0x2f, 0x9e, 0x62); // RailShot
}

QIcon makeSourceIcon(const QString& id, const QColor& accent) {
    QPixmap pm(48, 48);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    p.setBrush(QColor(0x1a, 0x21, 0x1c));
    p.setPen(QPen(accent.darker(115), 1.5));
    p.drawRoundedRect(QRectF(1.5, 1.5, 45, 45), 10, 10);

    p.setPen(QPen(accent, 2.2));
    p.setBrush(Qt::NoBrush);

    if (id == QLatin1String("video_device")) {
        p.drawRoundedRect(QRectF(12, 16, 24, 16), 3, 3);
        p.drawEllipse(QPointF(24, 24), 5, 5);
        p.drawLine(QPointF(36, 20), QPointF(42, 17));
        p.drawLine(QPointF(36, 28), QPointF(42, 31));
    } else if (id == QLatin1String("display_capture")) {
        p.drawRoundedRect(QRectF(10, 12, 28, 20), 2, 2);
        p.drawLine(QPointF(18, 36), QPointF(30, 36));
        p.drawLine(QPointF(24, 32), QPointF(24, 36));
    } else if (id == QLatin1String("window_capture") || id == QLatin1String("game_capture")) {
        p.drawRoundedRect(QRectF(11, 12, 26, 24), 3, 3);
        p.drawLine(QPointF(11, 18), QPointF(37, 18));
        p.drawEllipse(QPointF(15, 15), 1.2, 1.2);
        p.drawEllipse(QPointF(19, 15), 1.2, 1.2);
    } else if (id == QLatin1String("image")) {
        p.drawRect(QRectF(12, 14, 24, 20));
        p.drawLine(QPointF(12, 28), QPointF(20, 20));
        p.drawLine(QPointF(20, 20), QPointF(26, 26));
        p.drawLine(QPointF(26, 26), QPointF(36, 18));
        p.setBrush(accent);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(30, 18), 2.5, 2.5);
    } else if (id == QLatin1String("media_file") || id == QLatin1String("audio_file")) {
        if (id == QLatin1String("audio_file")) {
            p.drawEllipse(QPointF(18, 28), 5, 5);
            p.drawLine(QPointF(23, 28), QPointF(23, 14));
            p.drawLine(QPointF(23, 14), QPointF(32, 12));
            p.drawLine(QPointF(32, 12), QPointF(32, 26));
            p.drawEllipse(QPointF(27, 26), 5, 5);
        } else {
            QPolygonF play;
            play << QPointF(18, 14) << QPointF(34, 24) << QPointF(18, 34);
            p.setBrush(accent);
            p.setPen(Qt::NoPen);
            p.drawPolygon(play);
        }
    } else if (id == QLatin1String("desktop_audio") || id == QLatin1String("audio_input")
               || id == QLatin1String("app_audio")) {
        p.drawRoundedRect(QRectF(14, 14, 12, 16), 2, 2);
        p.drawArc(QRectF(26, 16, 10, 14), -60 * 16, 120 * 16);
        p.drawArc(QRectF(30, 13, 10, 20), -50 * 16, 100 * 16);
    } else if (id == QLatin1String("ndi")) {
        p.drawEllipse(QPointF(24, 24), 10, 10);
        p.drawEllipse(QPointF(24, 24), 5, 5);
        p.setBrush(accent);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(24, 24), 2.5, 2.5);
    } else if (id == QLatin1String("browser")) {
        p.drawEllipse(QPointF(24, 24), 12, 12);
        p.drawEllipse(QPointF(24, 24), 12, 7);
        p.drawLine(QPointF(24, 12), QPointF(24, 36));
    } else if (id == QLatin1String("text")) {
        p.setFont(QFont(QStringLiteral("Bahnschrift"), 18, QFont::Bold));
        p.drawText(QRectF(0, 0, 48, 48), Qt::AlignCenter, QStringLiteral("Aa"));
    } else if (id == QLatin1String("color")) {
        p.setBrush(accent);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(12, 12, 24, 24), 4, 4);
        p.setBrush(QColor(0xe8, 0xeb, 0xe6));
        p.drawRoundedRect(QRectF(18, 18, 12, 12), 2, 2);
    } else if (id == QLatin1String("scoreboard")) {
        p.drawRoundedRect(QRectF(8, 18, 32, 12), 2, 2);
        p.setBrush(accent);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(10, 20, 8, 8), 1, 1);
        p.drawRoundedRect(QRectF(30, 20, 8, 8), 1, 1);
    } else {
        p.drawEllipse(QPointF(24, 24), 8, 8);
    }

    p.end();
    return QIcon(pm);
}

} // namespace

AddSourceDialog::AddSourceDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("Add Source"));
    setModal(true);
    setMinimumSize(460, 560);
    resize(480, 620);
    setObjectName(QStringLiteral("rsAddSourceDialog"));

    setStyleSheet(QStringLiteral(
        "QDialog#rsAddSourceDialog {"
        "  background-color: #0f1311;"
        "  color: #e8ebe6;"
        "}"
        "QLabel#rsAddSourceTitle {"
        "  font-size: 18px;"
        "  font-weight: 700;"
        "  color: #f2f5f1;"
        "  letter-spacing: 0.2px;"
        "}"
        "QLabel#rsAddSourceHint {"
        "  color: #8a968c;"
        "  font-size: 12px;"
        "}"
        "QLineEdit#rsAddSourceSearch, QLineEdit#rsAddSourceName {"
        "  background-color: #161b18;"
        "  border: 1px solid #2a342e;"
        "  border-radius: 6px;"
        "  padding: 8px 12px;"
        "  color: #e8ebe6;"
        "  selection-background-color: #2f9e62;"
        "}"
        "QLineEdit#rsAddSourceSearch:focus, QLineEdit#rsAddSourceName:focus {"
        "  border-color: #2f9e62;"
        "}"
        "QListWidget#rsAddSourceList {"
        "  background-color: #121714;"
        "  border: 1px solid #1c2420;"
        "  border-radius: 8px;"
        "  padding: 4px;"
        "  outline: none;"
        "}"
        "QListWidget#rsAddSourceList::item {"
        "  border-radius: 6px;"
        "  padding: 6px 8px;"
        "  margin: 2px 2px;"
        "  color: #e8ebe6;"
        "}"
        "QListWidget#rsAddSourceList::item:hover {"
        "  background-color: #1a211c;"
        "}"
        "QListWidget#rsAddSourceList::item:selected {"
        "  background-color: #1e3228;"
        "  border: 1px solid #2f9e62;"
        "}"
        "QLabel#rsAddSourceNameLabel {"
        "  color: #c5d0c8;"
        "  font-size: 11px;"
        "  font-weight: 700;"
        "  letter-spacing: 1px;"
        "  text-transform: uppercase;"
        "}"
        "QPushButton#rsAddSourceOk {"
        "  background-color: #2f9e62;"
        "  border: none;"
        "  border-radius: 6px;"
        "  color: #0b0d0c;"
        "  font-weight: 700;"
        "  padding: 8px 18px;"
        "  min-width: 88px;"
        "}"
        "QPushButton#rsAddSourceOk:hover { background-color: #38b572; }"
        "QPushButton#rsAddSourceOk:disabled { background-color: #2a342e; color: #6a756c; }"
        "QPushButton#rsAddSourceCancel {"
        "  background-color: #1a211c;"
        "  border: 1px solid #2a342e;"
        "  border-radius: 6px;"
        "  color: #d7e0d9;"
        "  font-weight: 600;"
        "  padding: 8px 18px;"
        "  min-width: 88px;"
        "}"
        "QPushButton#rsAddSourceCancel:hover {"
        "  border-color: #2f9e62;"
        "  color: #ffffff;"
        "}"
        "QFrame#rsAddSourceDivider {"
        "  background-color: #1c2420;"
        "  max-height: 1px;"
        "}"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 16);
    root->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("Create new source"), this);
    title->setObjectName(QStringLiteral("rsAddSourceTitle"));
    root->addWidget(title);

    auto* hint = new QLabel(QStringLiteral("Choose a source type to add to the current scene."), this);
    hint->setObjectName(QStringLiteral("rsAddSourceHint"));
    root->addWidget(hint);

    searchEdit_ = new QLineEdit(this);
    searchEdit_->setObjectName(QStringLiteral("rsAddSourceSearch"));
    searchEdit_->setPlaceholderText(QStringLiteral("Search sources…"));
    searchEdit_->setClearButtonEnabled(true);
    root->addWidget(searchEdit_);

    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("rsAddSourceList"));
    list_->setIconSize(QSize(40, 40));
    list_->setSpacing(1);
    list_->setUniformItemSizes(false);
    list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    root->addWidget(list_, 1);

    auto* divider = new QFrame(this);
    divider->setObjectName(QStringLiteral("rsAddSourceDivider"));
    divider->setFrameShape(QFrame::HLine);
    root->addWidget(divider);

    auto* nameLabel = new QLabel(QStringLiteral("Name"), this);
    nameLabel->setObjectName(QStringLiteral("rsAddSourceNameLabel"));
    root->addWidget(nameLabel);

    nameEdit_ = new QLineEdit(this);
    nameEdit_->setObjectName(QStringLiteral("rsAddSourceName"));
    nameEdit_->setPlaceholderText(QStringLiteral("Source name"));
    root->addWidget(nameEdit_);

    auto* buttons = new QHBoxLayout();
    buttons->setSpacing(10);
    buttons->addStretch();
    auto* cancel = new QPushButton(QStringLiteral("Cancel"), this);
    cancel->setObjectName(QStringLiteral("rsAddSourceCancel"));
    okButton_ = new QPushButton(QStringLiteral("OK"), this);
    okButton_->setObjectName(QStringLiteral("rsAddSourceOk"));
    okButton_->setDefault(true);
    okButton_->setEnabled(false);
    buttons->addWidget(cancel);
    buttons->addWidget(okButton_);
    root->addLayout(buttons);

    connect(searchEdit_, &QLineEdit::textChanged, this, [this](const QString&) {
        populateList(searchEdit_->text());
    });
    connect(list_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem*, QListWidgetItem*) {
        onSelectionChanged();
    });
    connect(list_, &QListWidget::itemActivated, this, &AddSourceDialog::onItemActivated);
    connect(okButton_, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

    populateList(QString());
}

void AddSourceDialog::populateList(const QString& filter) {
    const QString previous = selectedId_;
    list_->clear();
    selectedId_.clear();

    const QString needle = filter.trimmed().toLower();
    QString lastCategory;

    for (const SourceOption& opt : allOptions()) {
        if (!needle.isEmpty()) {
            const QString hay = (opt.title + QLatin1Char(' ') + opt.description + QLatin1Char(' ')
                                 + opt.category)
                                    .toLower();
            if (!hay.contains(needle)) {
                continue;
            }
        }

        if (opt.category != lastCategory) {
            auto* header = new QListWidgetItem(opt.category.toUpper());
            header->setFlags(Qt::NoItemFlags);
            header->setForeground(QBrush(QColor(0x8a, 0x96, 0x8c)));
            QFont headerFont = header->font();
            headerFont.setPointSize(9);
            headerFont.setBold(true);
            headerFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
            header->setFont(headerFont);
            header->setSizeHint(QSize(0, 28));
            list_->addItem(header);
            lastCategory = opt.category;
        }

        auto* item = new QListWidgetItem(makeSourceIcon(opt.id, accentForCategory(opt.category)),
                                         opt.title + QLatin1Char('\n') + opt.description);
        item->setData(Qt::UserRole, opt.id);
        item->setData(Qt::UserRole + 1, opt.defaultName);
        item->setSizeHint(QSize(0, 56));
        list_->addItem(item);

        if (opt.id == previous) {
            list_->setCurrentItem(item);
        }
    }

    if (!list_->currentItem()) {
        for (int i = 0; i < list_->count(); ++i) {
            if (list_->item(i)->flags() & Qt::ItemIsSelectable) {
                list_->setCurrentRow(i);
                break;
            }
        }
    }
    onSelectionChanged();
}

void AddSourceDialog::onSelectionChanged() {
    QListWidgetItem* item = list_->currentItem();
    const bool valid = item && (item->flags() & Qt::ItemIsEnabled) && !item->data(Qt::UserRole).isNull();
    okButton_->setEnabled(valid);
    if (!valid) {
        selectedId_.clear();
        return;
    }
    selectedId_ = item->data(Qt::UserRole).toString();
    updateDefaultName();
}

void AddSourceDialog::updateDefaultName() {
    QListWidgetItem* item = list_->currentItem();
    if (!item) {
        return;
    }
    // Only overwrite if user hasn't customized the field (empty or matches last default).
    const QString suggested = item->data(Qt::UserRole + 1).toString();
    if (nameEdit_->text().isEmpty() || nameEdit_->property("rsDefaultName").toString() == nameEdit_->text()) {
        nameEdit_->setText(suggested);
        nameEdit_->setProperty("rsDefaultName", suggested);
        nameEdit_->selectAll();
    }
}

void AddSourceDialog::onItemActivated(QListWidgetItem* item) {
    if (!item || item->data(Qt::UserRole).isNull()) {
        return;
    }
    selectedId_ = item->data(Qt::UserRole).toString();
    updateDefaultName();
    accept();
}

QString AddSourceDialog::selectedTypeId() const {
    return selectedId_;
}

QString AddSourceDialog::sourceName() const {
    const QString name = nameEdit_ ? nameEdit_->text().trimmed() : QString();
    if (!name.isEmpty()) {
        return name;
    }
    if (list_ && list_->currentItem()) {
        return list_->currentItem()->data(Qt::UserRole + 1).toString();
    }
    return QStringLiteral("New Source");
}

} // namespace railshot
