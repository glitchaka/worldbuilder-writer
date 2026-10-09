#include "storage/GoogleDriveService.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>

namespace wbw {
namespace {

QString base64Url(const QByteArray& bytes) {
    return QString::fromLatin1(bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QString randomToken(int bytes = 48) {
    QByteArray data(bytes, Qt::Uninitialized);
    for (int i = 0; i < bytes; ++i) data[i] = static_cast<char>(QRandomGenerator::global()->generate() & 0xff);
    return base64Url(data);
}

QNetworkRequest authorizedRequest(const QUrl& url, const QString& token) {
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + token.toUtf8());
    request.setRawHeader("Accept", "application/json");
    return request;
}

QString settingsGroup() { return QStringLiteral("googleDrive"); }

QString escapeDriveQuery(QString value) {
    value.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    value.replace(QStringLiteral("'"), QStringLiteral("\\'"));
    return value;
}

} // namespace

GoogleDriveService::GoogleDriveService(QObject* parent)
    : QObject(parent), network_(new QNetworkAccessManager(this)) {
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
    settings.beginGroup(settingsGroup());
    clientId_ = settings.value(QStringLiteral("clientId")).toString();
    clientSecret_ = settings.value(QStringLiteral("clientSecret")).toString();
    settings.endGroup();
}

GoogleDriveService::~GoogleDriveService() { stopLoopbackServer(); }

void GoogleDriveService::setClientCredentials(const QString& clientId, const QString& clientSecret) {
    clientId_ = clientId.trimmed();
    clientSecret_ = clientSecret.trimmed();
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
    settings.beginGroup(settingsGroup());
    settings.setValue(QStringLiteral("clientId"), clientId_);
    settings.setValue(QStringLiteral("clientSecret"), clientSecret_);
    settings.endGroup();
}

QString GoogleDriveService::clientId() const { return clientId_; }
bool GoogleDriveService::isConfigured() const { return !clientId_.isEmpty(); }
bool GoogleDriveService::isConnected() const { return !savedRefreshToken().isEmpty(); }

void GoogleDriveService::saveRefreshToken(const QString& token) {
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
    settings.beginGroup(settingsGroup());
    if (token.isEmpty()) settings.remove(QStringLiteral("refreshToken"));
    else settings.setValue(QStringLiteral("refreshToken"), token);
    settings.endGroup();
}

QString GoogleDriveService::savedRefreshToken() const {
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
    settings.beginGroup(settingsGroup());
    const QString token = settings.value(QStringLiteral("refreshToken")).toString();
    settings.endGroup();
    return token;
}

bool GoogleDriveService::accessTokenValid() const {
    return !accessToken_.isEmpty() && QDateTime::currentSecsSinceEpoch() + 30 < accessTokenExpiryEpoch_;
}

void GoogleDriveService::connectAccount() {
    if (clientId_.isEmpty()) {
        emit errorOccurred(tr("Configura primero el ID de cliente OAuth de Google."));
        return;
    }

    stopLoopbackServer();
    loopbackServer_ = new QTcpServer(this);
    if (!loopbackServer_->listen(QHostAddress::LocalHost, 0)) {
        emit errorOccurred(tr("No se pudo abrir el receptor OAuth local."));
        stopLoopbackServer();
        return;
    }

    codeVerifier_ = randomToken(64);
    oauthState_ = randomToken(32);
    const QString challenge = base64Url(QCryptographicHash::hash(codeVerifier_.toUtf8(), QCryptographicHash::Sha256));
    redirectUri_ = QStringLiteral("http://127.0.0.1:%1").arg(loopbackServer_->serverPort());

    connect(loopbackServer_, &QTcpServer::newConnection, this, [this]() {
        QTcpSocket* socket = loopbackServer_->nextPendingConnection();
        if (!socket) return;
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            const QByteArray requestBytes = socket->readAll();
            const QList<QByteArray> lines = requestBytes.split('\n');
            if (lines.isEmpty()) return;
            const QList<QByteArray> firstLine = lines.first().trimmed().split(' ');
            if (firstLine.size() < 2) return;

            const QUrl callbackUrl(QStringLiteral("http://127.0.0.1") + QString::fromUtf8(firstLine.at(1)));
            const QUrlQuery query(callbackUrl);
            const QString state = query.queryItemValue(QStringLiteral("state"));
            const QString code = query.queryItemValue(QStringLiteral("code"));
            const QString error = query.queryItemValue(QStringLiteral("error"));

            QByteArray body;
            if (!error.isEmpty())
                body = "<html><body><h2>Worldbuilder Writer</h2><p>La autorización fue cancelada. Puedes cerrar esta pestaña.</p></body></html>";
            else if (state != oauthState_ || code.isEmpty())
                body = "<html><body><h2>Worldbuilder Writer</h2><p>La respuesta OAuth no es válida. Puedes cerrar esta pestaña.</p></body></html>";
            else
                body = "<html><body><h2>Worldbuilder Writer</h2><p>Google Drive quedó autorizado. Puedes cerrar esta pestaña y volver a la aplicación.</p></body></html>";

            const QByteArray response = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: "
                + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
            socket->write(response);
            socket->disconnectFromHost();

            if (!error.isEmpty()) {
                emit errorOccurred(tr("Google rechazó la autorización: %1").arg(error));
                stopLoopbackServer();
                return;
            }
            if (state != oauthState_ || code.isEmpty()) {
                emit errorOccurred(tr("La respuesta OAuth recibida no coincide con la solicitud iniciada."));
                stopLoopbackServer();
                return;
            }

            stopLoopbackServer();
            exchangeAuthorizationCode(code);
        });
    });

    QUrl auth(QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"), clientId_);
    query.addQueryItem(QStringLiteral("redirect_uri"), redirectUri_);
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("scope"), QStringLiteral("https://www.googleapis.com/auth/drive.file"));
    query.addQueryItem(QStringLiteral("access_type"), QStringLiteral("offline"));
    query.addQueryItem(QStringLiteral("prompt"), QStringLiteral("consent"));
    query.addQueryItem(QStringLiteral("state"), oauthState_);
    query.addQueryItem(QStringLiteral("code_challenge"), challenge);
    query.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    auth.setQuery(query);

    emit authorizationStarted();
    emit statusMessage(tr("Abriendo Google para autorizar Drive…"));
    if (!QDesktopServices::openUrl(auth)) {
        emit errorOccurred(tr("No se pudo abrir el navegador para autorizar Google Drive."));
        stopLoopbackServer();
    }
}

void GoogleDriveService::exchangeAuthorizationCode(const QString& code) {
    QNetworkRequest request(QUrl(QStringLiteral("https://oauth2.googleapis.com/token")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));

    QUrlQuery form;
    form.addQueryItem(QStringLiteral("client_id"), clientId_);
    if (!clientSecret_.isEmpty()) form.addQueryItem(QStringLiteral("client_secret"), clientSecret_);
    form.addQueryItem(QStringLiteral("code"), code);
    form.addQueryItem(QStringLiteral("code_verifier"), codeVerifier_);
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    form.addQueryItem(QStringLiteral("redirect_uri"), redirectUri_);

    QNetworkReply* reply = network_->post(request, form.query(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleTokenReply(reply, true, [this](bool ok) {
            if (ok) {
                emit connectionChanged(true);
                emit statusMessage(tr("Google Drive conectado."));
            }
        });
    });
}

void GoogleDriveService::refreshAccessToken(const std::function<void(bool)>& continuation) {
    const QString refreshToken = savedRefreshToken();
    if (refreshToken.isEmpty()) {
        continuation(false);
        return;
    }

    QNetworkRequest request(QUrl(QStringLiteral("https://oauth2.googleapis.com/token")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("client_id"), clientId_);
    if (!clientSecret_.isEmpty()) form.addQueryItem(QStringLiteral("client_secret"), clientSecret_);
    form.addQueryItem(QStringLiteral("refresh_token"), refreshToken);
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));

    QNetworkReply* reply = network_->post(request, form.query(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply, continuation]() {
        handleTokenReply(reply, false, continuation);
    });
}

void GoogleDriveService::handleTokenReply(QNetworkReply* reply, bool expectRefreshToken, const std::function<void(bool)>& continuation) {
    const QByteArray payload = reply->readAll();
    const bool networkOk = reply->error() == QNetworkReply::NoError;
    reply->deleteLater();

    const QJsonObject object = QJsonDocument::fromJson(payload).object();
    const QString accessToken = object.value(QStringLiteral("access_token")).toString();
    if (!networkOk || accessToken.isEmpty()) {
        const QString description = object.value(QStringLiteral("error_description")).toString();
        const QString code = object.value(QStringLiteral("error")).toString();
        emit errorOccurred(description.isEmpty() ? tr("No se pudo obtener un token de Google (%1).").arg(code) : description);
        if (continuation) continuation(false);
        return;
    }

    accessToken_ = accessToken;
    accessTokenExpiryEpoch_ = QDateTime::currentSecsSinceEpoch() + object.value(QStringLiteral("expires_in")).toInt(3600);
    const QString refreshToken = object.value(QStringLiteral("refresh_token")).toString();
    if (!refreshToken.isEmpty()) saveRefreshToken(refreshToken);
    else if (expectRefreshToken && savedRefreshToken().isEmpty()) {
        emit errorOccurred(tr("Google no devolvió un token de actualización. Vuelve a autorizar la cuenta."));
        if (continuation) continuation(false);
        return;
    }
    if (continuation) continuation(true);
}

void GoogleDriveService::withAccessToken(const std::function<void(const QString&)>& continuation) {
    if (accessTokenValid()) {
        continuation(accessToken_);
        return;
    }
    refreshAccessToken([this, continuation](bool ok) {
        if (!ok) {
            emit connectionChanged(false);
            return;
        }
        continuation(accessToken_);
    });
}

void GoogleDriveService::disconnectAccount() {
    const QString token = accessToken_.isEmpty() ? savedRefreshToken() : accessToken_;
    saveRefreshToken(QString());
    accessToken_.clear();
    accessTokenExpiryEpoch_ = 0;
    stopLoopbackServer();
    emit connectionChanged(false);
    emit statusMessage(tr("Google Drive desconectado."));

    if (!token.isEmpty()) {
        QNetworkRequest request(QUrl(QStringLiteral("https://oauth2.googleapis.com/revoke")));
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
        QUrlQuery form;
        form.addQueryItem(QStringLiteral("token"), token);
        QNetworkReply* reply = network_->post(request, form.query(QUrl::FullyEncoded).toUtf8());
        connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    }
}

void GoogleDriveService::listBackups() {
    withAccessToken([this](const QString& token) {
        QUrl url(QStringLiteral("https://www.googleapis.com/drive/v3/files"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("q"), QStringLiteral("trashed = false and appProperties has { key='worldbuilderWriterBackup' and value='true' }"));
        query.addQueryItem(QStringLiteral("spaces"), QStringLiteral("drive"));
        query.addQueryItem(QStringLiteral("pageSize"), QStringLiteral("100"));
        query.addQueryItem(QStringLiteral("fields"), QStringLiteral("files(id,name,size,modifiedTime,createdTime,appProperties)"));
        query.addQueryItem(QStringLiteral("orderBy"), QStringLiteral("modifiedTime desc"));
        url.setQuery(query);
        QNetworkReply* reply = network_->get(authorizedRequest(url, token));
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            const QByteArray payload = reply->readAll();
            const auto error = reply->error();
            reply->deleteLater();
            if (error != QNetworkReply::NoError) {
                emit errorOccurred(tr("No se pudieron listar las copias de Google Drive."));
                return;
            }
            emit backupsListed(QJsonDocument::fromJson(payload).object().value(QStringLiteral("files")).toArray());
            emit statusMessage(tr("Copias de Google Drive actualizadas."));
        });
    });
}

void GoogleDriveService::uploadBackup(const QString& localWbwPath, const QString& projectId) {
    QFile file(localWbwPath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred(tr("No se pudo abrir la copia .wbw para subirla."));
        return;
    }
    const QByteArray bytes = file.readAll();
    const QString name = QFileInfo(localWbwPath).fileName();
    const QString cleanProjectId = projectId.trimmed();
    if (cleanProjectId.isEmpty()) {
        emit errorOccurred(tr("El proyecto no tiene un identificador válido para el respaldo."));
        return;
    }

    withAccessToken([this, bytes, name, cleanProjectId](const QString& token) {
        QUrl url(QStringLiteral("https://www.googleapis.com/drive/v3/files"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("q"), QStringLiteral("trashed = false and appProperties has { key='worldbuilderWriterProject' and value='%1' }").arg(escapeDriveQuery(cleanProjectId)));
        query.addQueryItem(QStringLiteral("spaces"), QStringLiteral("drive"));
        query.addQueryItem(QStringLiteral("pageSize"), QStringLiteral("10"));
        query.addQueryItem(QStringLiteral("fields"), QStringLiteral("files(id,name,modifiedTime)"));
        url.setQuery(query);
        QNetworkReply* reply = network_->get(authorizedRequest(url, token));
        connect(reply, &QNetworkReply::finished, this, [this, reply, token, bytes, name, cleanProjectId]() {
            const QByteArray payload = reply->readAll();
            const auto error = reply->error();
            reply->deleteLater();
            if (error != QNetworkReply::NoError) {
                emit errorOccurred(tr("No se pudo comprobar si ya existe un respaldo para %1.").arg(name));
                return;
            }
            const QJsonArray files = QJsonDocument::fromJson(payload).object().value(QStringLiteral("files")).toArray();
            const QString existingId = files.isEmpty() ? QString() : files.first().toObject().value(QStringLiteral("id")).toString();
            performUpload(token, bytes, name, cleanProjectId, existingId);
        });
    });
}

void GoogleDriveService::performUpload(const QString& token, const QByteArray& bytes, const QString& name,
                                       const QString& projectId, const QString& existingFileId) {
    const QByteArray boundary = "wbw_" + randomToken(16).toUtf8();
    const QJsonObject metadata{
        {QStringLiteral("name"), name},
        {QStringLiteral("mimeType"), QStringLiteral("application/vnd.worldbuilder-writer.project+zip")},
        {QStringLiteral("appProperties"), QJsonObject{
            {QStringLiteral("worldbuilderWriterBackup"), QStringLiteral("true")},
            {QStringLiteral("worldbuilderWriterProject"), projectId}
        }}
    };

    QByteArray body;
    body += "--" + boundary + "\r\n";
    body += "Content-Type: application/json; charset=UTF-8\r\n\r\n";
    body += QJsonDocument(metadata).toJson(QJsonDocument::Compact) + "\r\n";
    body += "--" + boundary + "\r\n";
    body += "Content-Type: application/vnd.worldbuilder-writer.project+zip\r\n\r\n";
    body += bytes + "\r\n";
    body += "--" + boundary + "--\r\n";

    QUrl url(existingFileId.isEmpty()
        ? QStringLiteral("https://www.googleapis.com/upload/drive/v3/files")
        : QStringLiteral("https://www.googleapis.com/upload/drive/v3/files/%1").arg(existingFileId));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("uploadType"), QStringLiteral("multipart"));
    query.addQueryItem(QStringLiteral("fields"), QStringLiteral("id,name,modifiedTime,size"));
    url.setQuery(query);

    QNetworkRequest request = authorizedRequest(url, token);
    request.setRawHeader("Content-Type", "multipart/related; boundary=" + boundary);
    QNetworkReply* reply = existingFileId.isEmpty()
        ? network_->post(request, body)
        : network_->sendCustomRequest(request, QByteArrayLiteral("PATCH"), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, name]() {
        const QByteArray payload = reply->readAll();
        const auto error = reply->error();
        reply->deleteLater();
        if (error != QNetworkReply::NoError) {
            const QString message = QJsonDocument::fromJson(payload).object().value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString();
            emit errorOccurred(message.isEmpty() ? tr("No se pudo subir la copia a Google Drive.") : message);
            return;
        }
        const QJsonObject object = QJsonDocument::fromJson(payload).object();
        emit uploadFinished(object.value(QStringLiteral("id")).toString(), object.value(QStringLiteral("name")).toString(name));
        emit statusMessage(tr("Copia subida a Google Drive."));
    });
}

void GoogleDriveService::downloadBackup(const QString& fileId, const QString& destinationPath) {
    if (fileId.isEmpty() || destinationPath.isEmpty()) return;
    withAccessToken([this, fileId, destinationPath](const QString& token) {
        QUrl url(QStringLiteral("https://www.googleapis.com/drive/v3/files/%1").arg(fileId));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("alt"), QStringLiteral("media"));
        url.setQuery(query);
        QNetworkReply* reply = network_->get(authorizedRequest(url, token));
        connect(reply, &QNetworkReply::finished, this, [this, reply, destinationPath]() {
            const QByteArray payload = reply->readAll();
            const auto error = reply->error();
            reply->deleteLater();
            if (error != QNetworkReply::NoError) {
                emit errorOccurred(tr("No se pudo descargar la copia desde Google Drive."));
                return;
            }
            QFile file(destinationPath);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(payload) != payload.size()) {
                emit errorOccurred(tr("No se pudo guardar la copia descargada."));
                return;
            }
            file.close();
            emit downloadFinished(destinationPath);
            emit statusMessage(tr("Copia restaurada desde Google Drive."));
        });
    });
}

void GoogleDriveService::stopLoopbackServer() {
    if (!loopbackServer_) return;
    loopbackServer_->close();
    loopbackServer_->deleteLater();
    loopbackServer_ = nullptr;
}

} // namespace wbw
