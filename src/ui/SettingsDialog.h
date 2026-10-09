#pragma once

#include <QDialog>
#include <QJsonArray>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace wbw {

class GoogleDriveService;

class SettingsDialog final : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private:
    void refreshState();
    void populateBackups(const QJsonArray& backups);

    GoogleDriveService* drive_ = nullptr;
    QComboBox* theme_ = nullptr;
    QLineEdit* clientId_ = nullptr;
    QLineEdit* clientSecret_ = nullptr;
    QLabel* status_ = nullptr;
    QListWidget* backups_ = nullptr;
    QPushButton* connectButton_ = nullptr;
    QPushButton* disconnectButton_ = nullptr;
    QPushButton* refreshButton_ = nullptr;
};

} // namespace wbw
