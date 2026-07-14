#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;

namespace railshot {

class AddSourceDialog : public QDialog {
    Q_OBJECT
public:
    explicit AddSourceDialog(QWidget* parent = nullptr);

    /// Stable id used by MainWindow (e.g. "video_device", "display_capture").
    [[nodiscard]] QString selectedTypeId() const;
    [[nodiscard]] QString sourceName() const;

private:
    void populateList(const QString& filter);
    void onSelectionChanged();
    void onItemActivated(QListWidgetItem* item);
    void updateDefaultName();

    QLineEdit* searchEdit_ = nullptr;
    QListWidget* list_ = nullptr;
    QLineEdit* nameEdit_ = nullptr;
    QPushButton* okButton_ = nullptr;
    QString selectedId_;
};

} // namespace railshot
