#include "ui/GoogleDriveDialog.h"

#include "core/ArchiveDocument.h"
#include "storage/ProjectStore.h"
#include "storage/WbwPackage.h"

#include <QAbstractOAuth>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QOAuth2AuthorizationCodeFlow>
#include <QOAuthHttpServerReplyHandler>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryFile>
#include <QUrlQuery>
#include <QUuid>
#include <QVBoxLayout>

namespace wbw {
namespace {

constexpr auto DriveScope = "https://www.googleapis.com/auth/drive.file";
constexpr auto BackupFlag = "worldbuilderWriterBackup";
constexpr auto ProjectFlag = "worldbuilderWriterProject";
constexpr auto WbwMime = "application/vnd.worldbuilder-writer.project+zip";

QNetworkRequest authorizedRequest(const QUrl& url, const QString& token) {
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + token.toUtf8());
    request.setRawHeader("Accept", "application/json");
    return request;
}

QString safeTitle(QString value) {
    value = value.normalized(QString::NormalizationForm_D);
    value.remove(QRegularExpression(QStringLiteral("[\\x{0300}-\\x{036f}]")));
    value.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")), QStringLiteral("-"));
    value.remove(QRegularExpression(QStringLiteral("^-+|-+$")));
    return value.isEmpty() ? QStringLiteral("proyecto") : value.left(100);
}

QString projectIdForPath(const QString& projectPath) {
    QString id = QFileInfo(projectPath).dir().dirName();
    id.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")), QStringLiteral("-"));
    if (id.isEmpty()) id = QStringLiteral("project-") + QUuid::createUuid().toString(QUuid::Id128);
    return id.left(120);
}

} // namespace

GoogleDriveDialog::GoogleDriveDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Respaldo en Google Drive"));
    resize(760, 620);

    auto* root = new QVBoxLayout(this);
    auto* intro = new QLabel(tr("Worldbuilder Writer funciona sin cuenta. Esta conexión sirve solo para respaldar y restaurar proyectos .wbw creados por la aplicación. Usa un cliente OAuth de tipo «Aplicación de escritorio» de Google Cloud."));
    intro->setWordWrap(true);
    root->addWidget(intro);

    auto* form = new QFormLayout;
    clientId_ = new QLineEdit;
    clientId_->setPlaceholderText(QStringLiteral("000000000000-abc.apps.googleusercontent.com"));
    clientId_->setClearButtonEnabled(true);
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
    clientId_->setText(settings.value(QStringLiteral("googleDriveClientId")).toString());
    form->addRow(tr("Identificador OAuth"), clientId_);
    root->addLayout(form);

    auto* connectRow = new QHBoxLayout;
    connectButton_ = new QPushButton(tr("Conectar con Google Drive"));
    refreshButton_ = new QPushButton(tr("Actualizar copias"));
    connectRow->addWidget(connectButton_);
    connectRow->addWidget(refreshButton_);
    connectRow->addStretch();
    root->addLayout(connectRow);

    status_ = new QLabel(tr("No conectado."));
    status_->setWordWrap(true);
    root->addWidget(status_);

    backupsList_ = new QListWidget;
    backupsList_->setAlternatingRowColors(true);
    root->addWidget(backupsList_, 1);

    auto* actions = new QHBoxLayout;
    backupButton_ = new QPushButton(tr("Respaldar toda la biblioteca"));
    restoreButton_ = new QPushButton(tr("Restaurar seleccionada"));
    auto* close = new QPushButton(tr("Cerrar"));
    actions->addWidget(backupButton_);
    actions->addWidget(restoreButton_);
    actions->addStretch();
    actions->addWidget(close);
    root->addLayout(actions);

    network_ = new QNetworkAccessManager(this);
    oauth_ = new QOAuth2AuthorizationCodeFlow(this);
    oauth_->setNetworkAccessManager(network_);
    oauth_->setAuthorizationUrl(QUrl(QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth")));
    oauth_->setAccessTokenUrl(QUrl(QStringLiteral("https://oauth2.googleapis.com/token")));
    oauth_->setScope(QString::fromLatin1(DriveScope));
    oauth_->setPkceMethod(QOAuth2AuthorizationCodeFlow::PkceMethod::S256, 64);
    oauth_->setModifyParametersFunction([](QAbstractOAuth::Stage stage, QMultiMap<QString, QVariant>* parameters) {
        if (stage == QAbstractOAuth::Stage::RequestingAuthorization) {
            parameters->insert(QStringLiteral("access_type"), QStringLiteral("offline"));
            parameters->insert(QStringLiteral("prompt"), QStringLiteral("consent"));
        }
    });

    replyHandler_ = new QOAuthHttpServerReplyHandler(this);
    replyHandler_->setCallbackPath(QStringLiteral("/oauth2/callback"));
    replyHandler_->setCallbackText(tr("Worldbuilder Writer recibió la autorización. Ya puedes cerrar esta pestaña y volver a la aplicación."));
    oauth_->setReplyHandler(replyHandler_);

    connect(oauth_, &QAbstractOAuth::authorizeWithBrowser, this, [](const QUrl& url) {
        QDesktopServices::openUrl(url);
    });
    connect(oauth_, &QAbstractOAuth::granted, this, [this]() {
        replyHandler_->close();
        QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
        settings.setValue(QStringLiteral("googleDriveClientId"), clientId_->text().trimmed());
        setBusy(false, tr("Google Drive conectado. El acceso se mantiene solo durante esta sesión."));
        continuePendingAction();
    });
    connect(oauth_, &QAbstractOAuth::requestFailed, this, [this](QAbstractOAuth::Error) {
        replyHandler_->close();
        pendingAction_ = PendingAction::None;
        setBusy(false, tr("Google no autorizó la conexión o la solicitud OAuth falló."));
    });

    connect(connectButton_, &QPushButton::clicked, this, [this]() { connectGoogle(PendingAction::Refresh); });
    connect(refreshButton_, &QPushButton::clicked, this, [this]() {
        if (accessToken().isEmpty()) connectGoogle(PendingAction::Refresh);
        else refreshBackups();
    });
    connect(backupButton_, &QPushButton::clicked, this, [this]() {
        if (accessToken().isEmpty()) connectGoogle(PendingAction::Backup);
        else refreshBackups([this](bool success) { if (success) backupAllProjects(); });
    });
    connect(restoreButton_, &QPushButton::clicked, this, &GoogleDriveDialog::restoreSelected);
    connect(backupsList_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) { restoreSelected(); });
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    connect(clientId_, &QLineEdit::textChanged, this, [this]() { updateButtons(); });

    updateButtons();
}

QString GoogleDriveDialog::accessToken() const {
    return oauth_ ? oauth_->token() : QString();
}

void GoogleDriveDialog::setBusy(bool busy, const QString& message) {
    busy_ = busy;
    if (!message.isEmpty()) status_->setText(message);
    updateButtons();
}

void GoogleDriveDialog::updateButtons() {
    const bool validClient = QRegularExpression(QStringLiteral("^[0-9A-Za-z-]+\\.apps\\.googleusercontent\\.com$")).match(clientId_->text().trimmed()).hasMatch();
    const bool connected = !accessToken().isEmpty();
    connectButton_->setEnabled(!busy_ && validClient);
    connectButton_->setText(connected ? tr("Cambiar cuenta") : tr("Conectar con Google Drive"));
    refreshButton_->setEnabled(!busy_ && (connected || validClient));
    backupButton_->setEnabled(!busy_ && (connected || validClient));
    restoreButton_->setEnabled(!busy_ && backupsList_->currentItem() != nullptr && (connected || validClient));
    clientId_->setEnabled(!busy_);
    backupsList_->setEnabled(!busy_);
}

void GoogleDriveDialog::connectGoogle(PendingAction after) {
    const QString client = clientId_->text().trimmed();
    if (!QRegularExpression(QStringLiteral("^[0-9A-Za-z-]+\\.apps\\.googleusercontent\\.com$")).match(client).hasMatch()) {
        status_->setText(tr("Introduce un identificador OAuth válido terminado en .apps.googleusercontent.com."));
        return;
    }
    pendingAction_ = after;
    oauth_->setClientIdentifier(client);
    oauth_->setToken(QString());
    if (replyHandler_->isListening()) replyHandler_->close();
    if (!replyHandler_->listen(QHostAddress::LocalHost, 0)) {
        pendingAction_ = PendingAction::None;
        status_->setText(tr("No se pudo abrir el puerto local temporal para recibir la autorización de Google."));
        return;
    }
    setBusy(true, tr("Abriendo Google en el navegador del sistema…"));
    oauth_->grant();
}

void GoogleDriveDialog::continuePendingAction() {
    const PendingAction action = pendingAction_;
    pendingAction_ = PendingAction::None;
    if (action == PendingAction::Backup) {
        refreshBackups([this](bool success) { if (success) backupAllProjects(); });
    } else if (action == PendingAction::Restore) {
        restoreSelected();
    } else {
        refreshBackups();
    }
}

QString GoogleDriveDialog::googleError(const QByteArray& payload, const QString& fallback) const {
    const QJsonDocument document = QJsonDocument::fromJson(payload);
    const QJsonObject object = document.object();
    const QJsonObject error = object.value(QStringLiteral("error")).toObject();
    const QString message = error.value(QStringLiteral("message")).toString();
    return message.isEmpty() ? fallback : message;
}

void GoogleDriveDialog::refreshBackups(std::function<void(bool)> finished) {
    if (accessToken().isEmpty()) {
        if (finished) finished(false);
        return;
    }
    setBusy(true, tr("Consultando copias disponibles…"));
    QUrl url(QStringLiteral("https://www.googleapis.com/drive/v3/files"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("q"), QStringLiteral("trashed = false and appProperties has { key='%1' and value='true' }").arg(QString::fromLatin1(BackupFlag)));
    query.addQueryItem(QStringLiteral("spaces"), QStringLiteral("drive"));
    query.addQueryItem(QStringLiteral("orderBy"), QStringLiteral("modifiedTime desc"));
    query.addQueryItem(QStringLiteral("fields"), QStringLiteral("files(id,name,modifiedTime,size,appProperties)"));
    query.addQueryItem(QStringLiteral("pageSize"), QStringLiteral("100"));
    url.setQuery(query);

    QNetworkReply* reply = network_->get(authorizedRequest(url, accessToken()));
    connect(reply, &QNetworkReply::finished, this, [this, reply, finished]() {
        const QByteArray payload = reply->readAll();
        const bool success = reply->error() == QNetworkReply::NoError;
        if (!success) {
            setBusy(false, googleError(payload, tr("Google Drive no pudo listar las copias.")));
            reply->deleteLater();
            if (finished) finished(false);
            return;
        }
        backups_ = QJsonDocument::fromJson(payload).object().value(QStringLiteral("files")).toArray();
        backupsList_->clear();
        for (const QJsonValue value : backups_) {
            const QJsonObject file = value.toObject();
            const QDateTime modified = QDateTime::fromString(file.value(QStringLiteral("modifiedTime")).toString(), Qt::ISODate);
            bool sizeOk = false;
            const double bytes = file.value(QStringLiteral("size")).toString().toDouble(&sizeOk);
            QString detail = modified.isValid() ? modified.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")) : tr("fecha desconocida");
            if (sizeOk) detail += QStringLiteral(" · %1 MB").arg(bytes / 1024.0 / 1024.0, 0, 'f', 1);
            auto* item = new QListWidgetItem(QStringLiteral("%1\n%2").arg(file.value(QStringLiteral("name")).toString(), detail));
            item->setData(Qt::UserRole, file.value(QStringLiteral("id")).toString());
            item->setData(Qt::UserRole + 1, file.value(QStringLiteral("appProperties")).toObject().value(QString::fromLatin1(ProjectFlag)).toString());
            backupsList_->addItem(item);
        }
        setBusy(false, backups_.isEmpty() ? tr("No hay copias de Worldbuilder Writer en esta cuenta.") : tr("%1 copias disponibles.").arg(backups_.size()));
        reply->deleteLater();
        if (finished) finished(true);
    });
}

void GoogleDriveDialog::backupAllProjects() {
    backupQueue_ = ProjectStore::projectFiles();
    backupTotal_ = backupQueue_.size();
    backupDone_ = 0;
    if (backupQueue_.isEmpty()) {
        setBusy(false, tr("No hay proyectos locales para respaldar."));
        return;
    }
    setBusy(true, tr("Preparando %1 proyectos…").arg(backupTotal_));
    backupNext();
}

void GoogleDriveDialog::backupNext() {
    if (backupQueue_.isEmpty()) {
        setBusy(false, tr("%1 %2 respaldados en Google Drive.").arg(backupDone_).arg(backupDone_ == 1 ? tr("proyecto") : tr("proyectos")));
        refreshBackups();
        return;
    }

    const QString projectPath = backupQueue_.takeFirst();
    ArchiveDocument document;
    QString error;
    if (!ProjectStore::loadJsonFile(projectPath, document, &error)) {
        setBusy(false, tr("No se pudo leer %1: %2").arg(projectPath, error));
        backupQueue_.clear();
        return;
    }

    QTemporaryFile temporary(QDir::tempPath() + QStringLiteral("/WorldbuilderWriter-XXXXXX.wbw"));
    temporary.setAutoRemove(false);
    if (!temporary.open()) {
        setBusy(false, tr("No se pudo preparar el paquete temporal del respaldo."));
        backupQueue_.clear();
        return;
    }
    const QString tempPath = temporary.fileName();
    temporary.close();
    if (!WbwPackage::exportPackage(tempPath, document, &error)) {
        QFile::remove(tempPath);
        setBusy(false, tr("No se pudo empaquetar %1: %2").arg(document.storyTitle(), error));
        backupQueue_.clear();
        return;
    }
    QFile package(tempPath);
    if (!package.open(QIODevice::ReadOnly)) {
        QFile::remove(tempPath);
        setBusy(false, tr("No se pudo leer el paquete temporal del respaldo."));
        backupQueue_.clear();
        return;
    }
    const QByteArray bytes = package.readAll();
    package.close();
    QFile::remove(tempPath);

    const QString projectId = projectIdForPath(projectPath);
    const QString title = document.storyTitle().isEmpty() ? document.title() : document.storyTitle();
    status_->setText(tr("Respaldando %1 (%2/%3)…").arg(title).arg(backupDone_ + 1).arg(backupTotal_));
    uploadPackage(projectPath, bytes, projectId, title);
}

void GoogleDriveDialog::uploadPackage(const QString&, const QByteArray& bytes, const QString& projectId, const QString& title) {
    QString existingId;
    for (const QJsonValue value : backups_) {
        const QJsonObject file = value.toObject();
        const QString remoteProject = file.value(QStringLiteral("appProperties")).toObject().value(QString::fromLatin1(ProjectFlag)).toString();
        if (remoteProject == projectId) {
            existingId = file.value(QStringLiteral("id")).toString();
            break;
        }
    }

    QJsonObject metadata{
        {QStringLiteral("name"), safeTitle(title) + QStringLiteral(".wbw")},
        {QStringLiteral("mimeType"), QString::fromLatin1(WbwMime)},
        {QStringLiteral("appProperties"), QJsonObject{
            {QString::fromLatin1(BackupFlag), QStringLiteral("true")},
            {QString::fromLatin1(ProjectFlag), projectId}
        }}
    };
    const QByteArray boundary = QByteArrayLiteral("worldbuilder_writer_") + QUuid::createUuid().toString(QUuid::Id128).toLatin1();
    QByteArray body;
    body += "--" + boundary + "\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n";
    body += QJsonDocument(metadata).toJson(QJsonDocument::Compact);
    body += "\r\n--" + boundary + "\r\nContent-Type: " + QByteArrayLiteral(WbwMime) + "\r\n\r\n";
    body += bytes;
    body += "\r\n--" + boundary + "--\r\n";

    QString endpoint;
    QByteArray method;
    if (existingId.isEmpty()) {
        endpoint = QStringLiteral("https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart&fields=id,name,modifiedTime,size,appProperties");
        method = QByteArrayLiteral("POST");
    } else {
        endpoint = QStringLiteral("https://www.googleapis.com/upload/drive/v3/files/%1?uploadType=multipart&fields=id,name,modifiedTime,size,appProperties")
            .arg(QString::fromUtf8(QUrl::toPercentEncoding(existingId)));
        method = QByteArrayLiteral("PATCH");
    }

    QNetworkRequest request = authorizedRequest(QUrl(endpoint), accessToken());
    request.setRawHeader("Content-Type", QByteArrayLiteral("multipart/related; boundary=") + boundary);
    QNetworkReply* reply = network_->sendCustomRequest(request, method, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray payload = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            setBusy(false, googleError(payload, tr("Google Drive no pudo subir el respaldo.")));
            backupQueue_.clear();
            reply->deleteLater();
            return;
        }
        ++backupDone_;
        reply->deleteLater();
        backupNext();
    });
}

void GoogleDriveDialog::restoreSelected() {
    if (!backupsList_->currentItem()) {
        status_->setText(tr("Selecciona una copia para restaurar."));
        return;
    }
    pendingRestoreId_ = backupsList_->currentItem()->data(Qt::UserRole).toString();
    pendingRestoreProjectId_ = backupsList_->currentItem()->data(Qt::UserRole + 1).toString();
    if (accessToken().isEmpty()) {
        connectGoogle(PendingAction::Restore);
        return;
    }
    if (QMessageBox::question(this, tr("Restaurar proyecto"), tr("¿Descargar esta copia y restaurarla en la biblioteca local? Si ya existe el mismo proyecto, se reemplazará su copia local guardada.")) != QMessageBox::Yes) return;

    setBusy(true, tr("Descargando y comprobando el respaldo…"));
    const QString encodedId = QString::fromUtf8(QUrl::toPercentEncoding(pendingRestoreId_));
    const QUrl url(QStringLiteral("https://www.googleapis.com/drive/v3/files/%1?alt=media").arg(encodedId));
    QNetworkReply* reply = network_->get(authorizedRequest(url, accessToken()));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray payload = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            setBusy(false, googleError(payload, tr("Google Drive no pudo descargar el respaldo.")));
            reply->deleteLater();
            return;
        }
        const QString projectId = pendingRestoreProjectId_.isEmpty()
            ? QStringLiteral("project-") + QUuid::createUuid().toString(QUuid::Id128)
            : pendingRestoreProjectId_;
        emit restorePackageReady(payload, projectId);
        setBusy(false, tr("Copia descargada y enviada a la biblioteca local."));
        reply->deleteLater();
    });
}

} // namespace wbw
