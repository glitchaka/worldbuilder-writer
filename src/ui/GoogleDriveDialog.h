#pragma once

#include <QDialog>
#include <QJsonArray>
#include <QString>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace wbw {

class GoogleDriveService;

class GoogleDriveDialog final : public QDialog {
    Q_OBJECT
public:
    explicit GoogleDriveDialog(QWidget* parent = nullptr);

signals:
    void libraryChanged();

private:
    void refreshConnectionState();
    void refreshBackups(const QJsonArray& backups);
    void saveCredentials();
    void connectDrive();
    void backupLibrary();
    void backupNext();
    void restoreSelected();
    void handleDownloaded(const QString& path);
    void setBusy(bool busy, const QString& message = {});

    GoogleDriveService* drive_ = nullptr;
    QLineEdit* clientId_ = nullptr;
    QLineEdit* clientSecret_ = nullptr;
    QLabel* connectionState_ = nullptr;
    QLabel* message_ = nullptr;
    QListWidget* backups_ = nullptr;
    QPushButton* connectButton_ = nullptr;
    QPushButton* disconnectButton_ = nullptr;
    QPushButton* backupButton_ = nullptr;
    QPushButton* restoreButton_ = nullptr;
    QPushButton* refreshButton_ = nullptr;

    QStringList backupQueue_;
    QString pendingRestorePath_;
    int backupTotal_ = 0;
    int backupDone_ = 0;
    bool busy_ = false;
};

} // namespace wbw
