#pragma once

#include <QJsonArray>
#include <QObject>
#include <QString>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class QTcpServer;

namespace wbw {

class GoogleDriveService final : public QObject {
    Q_OBJECT
public:
    explicit GoogleDriveService(QObject* parent = nullptr);
    ~GoogleDriveService() override;

    void setClientCredentials(const QString& clientId, const QString& clientSecret = {});
    QString clientId() const;
    bool isConfigured() const;
    bool isConnected() const;

    void connectAccount();
    void disconnectAccount();

    void listBackups();
    void uploadBackup(const QString& localWbwPath, const QString& projectId);
    void downloadBackup(const QString& fileId, const QString& destinationPath);

signals:
    void connectionChanged(bool connected);
    void authorizationStarted();
    void backupsListed(const QJsonArray& backups);
    void uploadFinished(const QString& fileId, const QString& name);
    void downloadFinished(const QString& destinationPath);
    void statusMessage(const QString& message);
    void errorOccurred(const QString& message);

private:
    void exchangeAuthorizationCode(const QString& code);
    void refreshAccessToken(const std::function<void(bool)>& continuation);
    void withAccessToken(const std::function<void(const QString&)>& continuation);
    void handleTokenReply(QNetworkReply* reply, bool expectRefreshToken, const std::function<void(bool)>& continuation = {});
    void performUpload(const QString& token, const QByteArray& bytes, const QString& name,
                       const QString& projectId, const QString& existingFileId = {});
    void stopLoopbackServer();
    void saveRefreshToken(const QString& token);
    QString savedRefreshToken() const;
    bool accessTokenValid() const;

    QNetworkAccessManager* network_ = nullptr;
    QTcpServer* loopbackServer_ = nullptr;
    QString clientId_;
    QString clientSecret_;
    QString accessToken_;
    QString codeVerifier_;
    QString oauthState_;
    QString redirectUri_;
    qint64 accessTokenExpiryEpoch_ = 0;
};

} // namespace wbw
