#include "ui/SettingsDialog.h"

#include "storage/GoogleDriveService.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace wbw {

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent), drive_(new GoogleDriveService(this)) {
    setWindowTitle(tr("Configuración"));
    setModal(true);
    resize(680, 560);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 22);
    root->setSpacing(14);

    auto* kicker = new QLabel(tr("CONFIGURACIÓN"));
    kicker->setObjectName(QStringLiteral("dialogKicker"));
    auto* title = new QLabel(tr("Google Drive"));
    title->setObjectName(QStringLiteral("dialogTitle"));
    auto* description = new QLabel(tr("Worldbuilder Writer usa Google Drive solo para respaldos. La biblioteca y los proyectos siguen siendo locales."));
    description->setObjectName(QStringLiteral("dialogDescription"));
    description->setWordWrap(true);
    root->addWidget(kicker);
    root->addWidget(title);
    root->addWidget(description);

    auto* form = new QFormLayout;
    form->setVerticalSpacing(10);
    form->setHorizontalSpacing(16);
    clientId_ = new QLineEdit;
    clientId_->setText(drive_->clientId());
    clientId_->setPlaceholderText(tr("ID de cliente OAuth para aplicación de escritorio"));
    clientSecret_ = new QLineEdit;
    clientSecret_->setEchoMode(QLineEdit::Password);
    clientSecret_->setPlaceholderText(tr("Secreto de cliente, si tu credencial lo requiere"));
    form->addRow(tr("ID de cliente"), clientId_);
    form->addRow(tr("Secreto"), clientSecret_);
    root->addLayout(form);

    status_ = new QLabel;
    status_->setObjectName(QStringLiteral("driveStatus"));
    status_->setWordWrap(true);
    root->addWidget(status_);

    auto* actions = new QHBoxLayout;
    auto* saveCredentials = new QPushButton(tr("Guardar credenciales"));
    connectButton_ = new QPushButton(tr("Conectar Google Drive"));
    connectButton_->setObjectName(QStringLiteral("primaryAction"));
    disconnectButton_ = new QPushButton(tr("Desconectar"));
    refreshButton_ = new QPushButton(tr("Actualizar respaldos"));
    actions->addWidget(saveCredentials);
    actions->addWidget(connectButton_);
    actions->addWidget(disconnectButton_);
    actions->addStretch();
    actions->addWidget(refreshButton_);
    root->addLayout(actions);

    auto* backupTitle = new QLabel(tr("Respaldos encontrados"));
    backupTitle->setObjectName(QStringLiteral("fieldLabel"));
    root->addWidget(backupTitle);
    backups_ = new QListWidget;
    backups_->setObjectName(QStringLiteral("driveBackups"));
    root->addWidget(backups_, 1);

    auto* bottom = new QHBoxLayout;
    bottom->addStretch();
    auto* close = new QPushButton(tr("Cerrar"));
    bottom->addWidget(close);
    root->addLayout(bottom);

    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(saveCredentials, &QPushButton::clicked, this, [this]() {
        drive_->setClientCredentials(clientId_->text(), clientSecret_->text());
        refreshState();
        status_->setText(tr("Credenciales guardadas localmente."));
    });
    connect(connectButton_, &QPushButton::clicked, this, [this]() {
        drive_->setClientCredentials(clientId_->text(), clientSecret_->text());
        drive_->connectAccount();
    });
    connect(disconnectButton_, &QPushButton::clicked, drive_, &GoogleDriveService::disconnectAccount);
    connect(refreshButton_, &QPushButton::clicked, drive_, &GoogleDriveService::listBackups);

    connect(drive_, &GoogleDriveService::connectionChanged, this, [this](bool) { refreshState(); });
    connect(drive_, &GoogleDriveService::authorizationStarted, this, [this]() {
        status_->setText(tr("Se abrió el navegador. Autoriza Worldbuilder Writer y vuelve a esta ventana."));
    });
    connect(drive_, &GoogleDriveService::statusMessage, status_, &QLabel::setText);
    connect(drive_, &GoogleDriveService::errorOccurred, this, [this](const QString& message) {
        status_->setText(message);
        QMessageBox::warning(this, tr("Google Drive"), message);
    });
    connect(drive_, &GoogleDriveService::backupsListed, this, &SettingsDialog::populateBackups);

    refreshState();
    if (drive_->isConnected()) drive_->listBackups();
}

void SettingsDialog::refreshState() {
    const bool configured = drive_->isConfigured();
    const bool connected = drive_->isConnected();
    connectButton_->setEnabled(configured && !connected);
    disconnectButton_->setEnabled(connected);
    refreshButton_->setEnabled(connected);
    if (connected) status_->setText(tr("Google Drive conectado."));
    else if (configured) status_->setText(tr("Credenciales configuradas. Falta autorizar la cuenta."));
    else status_->setText(tr("Configura un ID de cliente OAuth para habilitar respaldos en Google Drive."));
}

void SettingsDialog::populateBackups(const QJsonArray& backups) {
    backups_->clear();
    for (const QJsonValue value : backups) {
        const QJsonObject backup = value.toObject();
        const QString name = backup.value(QStringLiteral("name")).toString(tr("Respaldo sin nombre"));
        const QString modified = backup.value(QStringLiteral("modifiedTime")).toString();
        auto* item = new QListWidgetItem(modified.isEmpty() ? name : QStringLiteral("%1\n%2").arg(name, modified));
        item->setData(Qt::UserRole, backup.value(QStringLiteral("id")).toString());
        backups_->addItem(item);
    }
    if (backups_->count() == 0) backups_->addItem(tr("No hay respaldos disponibles en Drive."));
}

} // namespace wbw
