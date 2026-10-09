#include "ui/GoogleDriveDialog.h"

#include "core/ArchiveDocument.h"
#include "storage/GoogleDriveService.h"
#include "storage/ProjectStore.h"
#include "storage/WbwPackage.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryFile>
#include <QVBoxLayout>

namespace wbw {
namespace {

QString projectIdForPath(const QString& projectPath) {
    QString id = QFileInfo(projectPath).dir().dirName();
    id.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")), QStringLiteral("-"));
    return id.left(120);
}

QString safeBackupName(QString value) {
    value = value.normalized(QString::NormalizationForm_D);
    value.remove(QRegularExpression(QStringLiteral("[\\x{0300}-\\x{036f}]")));
    value.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")), QStringLiteral("-"));
    value.remove(QRegularExpression(QStringLiteral("^-+|-+$")));
    return value.isEmpty() ? QStringLiteral("proyecto") : value.left(100);
}

} // namespace

GoogleDriveDialog::GoogleDriveDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Google Drive"));
    resize(760, 620);
    setModal(true);

    drive_ = new GoogleDriveService(this);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 20, 22, 20);
    root->setSpacing(13);

    auto* kicker = new QLabel(tr("RESPALDO OPCIONAL"));
    kicker->setObjectName(QStringLiteral("dialogKicker"));
    auto* title = new QLabel(tr("Google Drive"));
    title->setObjectName(QStringLiteral("dialogTitle"));
    auto* intro = new QLabel(tr("Worldbuilder Writer funciona y guarda sin cuenta. Google Drive se usa únicamente para respaldar o restaurar las obras de la biblioteca como paquetes completos .wbw."));
    intro->setObjectName(QStringLiteral("dialogDescription"));
    intro->setWordWrap(true);
    root->addWidget(kicker);
    root->addWidget(title);
    root->addWidget(intro);

    auto* form = new QFormLayout;
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(9);
    clientId_ = new QLineEdit;
    clientId_->setPlaceholderText(QStringLiteral("000000000000-abc.apps.googleusercontent.com"));
    clientId_->setText(drive_->clientId());
    clientSecret_ = new QLineEdit;
    clientSecret_->setEchoMode(QLineEdit::Password);
    clientSecret_->setPlaceholderText(tr("Opcional para el cliente de escritorio"));
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
    settings.beginGroup(QStringLiteral("googleDrive"));
    clientSecret_->setText(settings.value(QStringLiteral("clientSecret")).toString());
    settings.endGroup();
    form->addRow(tr("ID de cliente OAuth"), clientId_);
    form->addRow(tr("Secreto de cliente"), clientSecret_);
    root->addLayout(form);

    auto* credentialHint = new QLabel(tr("Usa credenciales OAuth de tipo Aplicación de escritorio. La autorización se abre en el navegador del sistema y vuelve a la aplicación por un puerto local temporal."));
    credentialHint->setWordWrap(true);
    credentialHint->setStyleSheet(QStringLiteral("color:#667085;"));
    root->addWidget(credentialHint);

    auto* connectionRow = new QHBoxLayout;
    connectButton_ = new QPushButton(tr("Conectar con Google Drive"));
    disconnectButton_ = new QPushButton(tr("Desconectar"));
    connectionState_ = new QLabel;
    connectionState_->setObjectName(QStringLiteral("driveConnectionState"));
    connectionRow->addWidget(connectButton_);
    connectionRow->addWidget(disconnectButton_);
    connectionRow->addWidget(connectionState_, 1);
    root->addLayout(connectionRow);

    auto* actions = new QHBoxLayout;
    backupButton_ = new QPushButton(tr("Respaldar toda la biblioteca"));
    backupButton_->setObjectName(QStringLiteral("primaryAction"));
    refreshButton_ = new QPushButton(tr("Actualizar copias"));
    restoreButton_ = new QPushButton(tr("Restaurar seleccionada"));
    actions->addWidget(backupButton_);
    actions->addWidget(refreshButton_);
    actions->addWidget(restoreButton_);
    actions->addStretch();
    root->addLayout(actions);

    backups_ = new QListWidget;
    backups_->setObjectName(QStringLiteral("driveBackupList"));
    root->addWidget(backups_, 1);

    message_ = new QLabel;
    message_->setWordWrap(true);
    message_->setObjectName(QStringLiteral("driveMessage"));
    root->addWidget(message_);

    auto* bottom = new QHBoxLayout;
    auto* close = new QPushButton(tr("Cerrar"));
    bottom->addStretch();
    bottom->addWidget(close);
    root->addLayout(bottom);

    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(connectButton_, &QPushButton::clicked, this, &GoogleDriveDialog::connectDrive);
    connect(disconnectButton_, &QPushButton::clicked, drive_, &GoogleDriveService::disconnectAccount);
    connect(backupButton_, &QPushButton::clicked, this, &GoogleDriveDialog::backupLibrary);
    connect(refreshButton_, &QPushButton::clicked, drive_, &GoogleDriveService::listBackups);
    connect(restoreButton_, &QPushButton::clicked, this, &GoogleDriveDialog::restoreSelected);
    connect(backups_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) { restoreSelected(); });
    connect(backups_, &QListWidget::currentRowChanged, this, [this](int) { refreshConnectionState(); });
    connect(clientId_, &QLineEdit::editingFinished, this, &GoogleDriveDialog::saveCredentials);
    connect(clientSecret_, &QLineEdit::editingFinished, this, &GoogleDriveDialog::saveCredentials);

    connect(drive_, &GoogleDriveService::connectionChanged, this, [this](bool) {
        refreshConnectionState();
        setBusy(false);
        if (drive_->isConnected()) drive_->listBackups();
    });
    connect(drive_, &GoogleDriveService::authorizationStarted, this, [this]() {
        setBusy(true, tr("Esperando la autorización en el navegador…"));
    });
    connect(drive_, &GoogleDriveService::statusMessage, this, [this](const QString& text) {
        if (!text.isEmpty()) message_->setText(text);
    });
    connect(drive_, &GoogleDriveService::errorOccurred, this, [this](const QString& text) {
        backupQueue_.clear();
        pendingRestorePath_.clear();
        setBusy(false, text);
    });
    connect(drive_, &GoogleDriveService::backupsListed, this, &GoogleDriveDialog::refreshBackups);
    connect(drive_, &GoogleDriveService::uploadFinished, this, [this](const QString&, const QString&) {
        ++backupDone_;
        backupNext();
    });
    connect(drive_, &GoogleDriveService::downloadFinished, this, &GoogleDriveDialog::handleDownloaded);

    refreshConnectionState();
    if (drive_->isConnected() && drive_->isConfigured()) drive_->listBackups();
}

void GoogleDriveDialog::saveCredentials() {
    drive_->setClientCredentials(clientId_->text(), clientSecret_->text());
    refreshConnectionState();
}

void GoogleDriveDialog::refreshConnectionState() {
    const bool configured = drive_->isConfigured();
    const bool connected = drive_->isConnected();
    connectionState_->setText(connected ? tr("● Cuenta autorizada") : configured ? tr("○ Credenciales guardadas") : tr("○ Sin configurar"));
    connectionState_->setStyleSheet(connected ? QStringLiteral("color:#2f855a;font-weight:600;") : QStringLiteral("color:#667085;"));
    connectButton_->setEnabled(!busy_ && configured);
    disconnectButton_->setEnabled(!busy_ && connected);
    backupButton_->setEnabled(!busy_ && connected);
    refreshButton_->setEnabled(!busy_ && connected);
    restoreButton_->setEnabled(!busy_ && connected && backups_->currentItem());
    clientId_->setEnabled(!busy_);
    clientSecret_->setEnabled(!busy_);
    backups_->setEnabled(!busy_);
}

void GoogleDriveDialog::setBusy(bool busy, const QString& message) {
    busy_ = busy;
    if (!message.isEmpty()) message_->setText(message);
    refreshConnectionState();
}

void GoogleDriveDialog::connectDrive() {
    saveCredentials();
    const QRegularExpression clientPattern(QStringLiteral("^[0-9A-Za-z-]+\\.apps\\.googleusercontent\\.com$"));
    if (!clientPattern.match(clientId_->text().trimmed()).hasMatch()) {
        message_->setText(tr("El ID de cliente OAuth no tiene el formato de Google esperado."));
        return;
    }
    drive_->connectAccount();
}

void GoogleDriveDialog::refreshBackups(const QJsonArray& backups) {
    backups_->clear();
    for (const QJsonValue value : backups) {
        const QJsonObject file = value.toObject();
        const QString name = file.value(QStringLiteral("name")).toString(tr("Copia sin nombre"));
        const QDateTime modified = QDateTime::fromString(file.value(QStringLiteral("modifiedTime")).toString(), Qt::ISODate);
        bool ok = false;
        const double bytes = file.value(QStringLiteral("size")).toString().toDouble(&ok);
        QString detail = modified.isValid() ? modified.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")) : tr("fecha desconocida");
        if (ok) detail += QStringLiteral(" · %1 MB").arg(bytes / 1024.0 / 1024.0, 0, 'f', 1);
        auto* item = new QListWidgetItem(QStringLiteral("%1\n%2").arg(name, detail));
        item->setData(Qt::UserRole, file.value(QStringLiteral("id")).toString());
        item->setData(Qt::UserRole + 1, file.value(QStringLiteral("appProperties")).toObject().value(QStringLiteral("worldbuilderWriterProject")).toString());
        item->setSizeHint(QSize(0, 54));
        backups_->addItem(item);
    }
    setBusy(false, backups.isEmpty() ? tr("No hay respaldos de Worldbuilder Writer en esta cuenta.") : tr("%1 respaldos disponibles.").arg(backups.size()));
}

void GoogleDriveDialog::backupLibrary() {
    if (!drive_->isConnected()) return;
    backupQueue_ = ProjectStore::projectFiles();
    backupTotal_ = backupQueue_.size();
    backupDone_ = 0;
    if (backupQueue_.isEmpty()) {
        message_->setText(tr("No hay obras locales para respaldar."));
        return;
    }
    setBusy(true, tr("Preparando %1 obras para Google Drive…").arg(backupTotal_));
    backupNext();
}

void GoogleDriveDialog::backupNext() {
    if (backupQueue_.isEmpty()) {
        setBusy(false, tr("%1 %2 respaldadas en Google Drive.").arg(backupDone_).arg(backupDone_ == 1 ? tr("obra") : tr("obras")));
        drive_->listBackups();
        return;
    }

    const QString projectPath = backupQueue_.takeFirst();
    ArchiveDocument document;
    QString error;
    if (!ProjectStore::loadJsonFile(projectPath, document, &error)) {
        backupQueue_.clear();
        setBusy(false, tr("No se pudo leer una obra local: %1").arg(error));
        return;
    }

    QString title = document.storyTitle().isEmpty() ? document.title() : document.storyTitle();
    if (title.isEmpty()) title = tr("proyecto");
    QTemporaryFile temporary(QDir::tempPath() + QStringLiteral("/%1-XXXXXX.wbw").arg(safeBackupName(title)));
    temporary.setAutoRemove(false);
    if (!temporary.open()) {
        backupQueue_.clear();
        setBusy(false, tr("No se pudo crear el paquete temporal de respaldo."));
        return;
    }
    const QString tempPath = temporary.fileName();
    temporary.close();
    if (!WbwPackage::exportPackage(tempPath, document, &error)) {
        QFile::remove(tempPath);
        backupQueue_.clear();
        setBusy(false, tr("No se pudo empaquetar “%1”: %2").arg(title, error));
        return;
    }

    const QString projectId = projectIdForPath(projectPath);
    message_->setText(tr("Respaldando “%1” (%2/%3)…").arg(title).arg(backupDone_ + 1).arg(backupTotal_));
    drive_->uploadBackup(tempPath, projectId);
    QFile::remove(tempPath);
}

void GoogleDriveDialog::restoreSelected() {
    auto* item = backups_->currentItem();
    if (!item || busy_) return;
    if (QMessageBox::question(this, tr("Restaurar respaldo"), tr("¿Restaurar “%1” como una obra local nueva?").arg(item->text().section(QLatin1Char('\n'), 0, 0))) != QMessageBox::Yes) return;

    QTemporaryFile temporary(QDir::tempPath() + QStringLiteral("/WorldbuilderWriter-restore-XXXXXX.wbw"));
    temporary.setAutoRemove(false);
    if (!temporary.open()) {
        message_->setText(tr("No se pudo preparar el archivo temporal de restauración."));
        return;
    }
    pendingRestorePath_ = temporary.fileName();
    temporary.close();
    setBusy(true, tr("Descargando y comprobando el respaldo…"));
    drive_->downloadBackup(item->data(Qt::UserRole).toString(), pendingRestorePath_);
}

void GoogleDriveDialog::handleDownloaded(const QString& path) {
    ArchiveDocument restored;
    QString error;
    if (!WbwPackage::importPackage(path, restored, &error)) {
        QFile::remove(path);
        pendingRestorePath_.clear();
        setBusy(false, tr("El respaldo descargado no es un paquete .wbw válido: %1").arg(error));
        return;
    }
    QFile::remove(path);
    pendingRestorePath_.clear();

    const QString directory = ProjectStore::createProjectDirectory();
    if (!ProjectStore::saveIntoProjectDirectory(directory, restored, &error)) {
        setBusy(false, tr("No se pudo guardar la obra restaurada en la biblioteca: %1").arg(error));
        return;
    }

    emit libraryChanged();
    setBusy(false, tr("“%1” quedó restaurada en la biblioteca local.").arg(restored.storyTitle().isEmpty() ? restored.title() : restored.storyTitle()));
}

} // namespace wbw
