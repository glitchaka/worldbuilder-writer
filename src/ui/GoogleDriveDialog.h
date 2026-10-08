#pragma once

#include <QDialog>
#include <QJsonArray>
#include <QStringList>
#include <functional>

class QLabel;
class QLineEdit;
class QListWidget;
class QNetworkAccessManager;
class QOAuth2AuthorizationCodeFlow;
class QOAuthHttpServerReplyHandler;
class QPushButton;

namespace wbw {

class GoogleDriveDialog final : public QDialog {
    Q_OBJECT
public:
    explicit GoogleDriveDialog(QWidget* parent = nullptr);

signals:
    void restorePackageReady(const QByteArray& packageBytes, const QString& projectId);

private:
    enum class PendingAction { None, Refresh, Backup, Restore };

    void connectGoogle(PendingAction after = PendingAction::Refresh);
    void setBusy(bool busy, const QString& message = {});
    void refreshBackups(std::function<void(bool)> finished = {});
    void backupAllProjects();
    void backupNext();
    void uploadPackage(const QString& projectPath, const QByteArray& bytes, const QString& projectId, const QString& title);
    void restoreSelected();
    void continuePendingAction();
    void updateButtons();
    QString accessToken() const;
    QString googleError(const QByteArray& payload, const QString& fallback) const;

    QLineEdit* clientId_ = nullptr;
    QLabel* status_ = nullptr;
    QListWidget* backupsList_ = nullptr;
    QPushButton* connectButton_ = nullptr;
    QPushButton* backupButton_ = nullptr;
    QPushButton* restoreButton_ = nullptr;
    QPushButton* refreshButton_ = nullptr;

    QNetworkAccessManager* network_ = nullptr;
    QOAuth2AuthorizationCodeFlow* oauth_ = nullptr;
    QOAuthHttpServerReplyHandler* replyHandler_ = nullptr;
    QJsonArray backups_;
    QStringList backupQueue_;
    int backupTotal_ = 0;
    int backupDone_ = 0;
    PendingAction pendingAction_ = PendingAction::None;
    QString pendingRestoreId_;
    QString pendingRestoreProjectId_;
    bool busy_ = false;
};

} // namespace wbw
