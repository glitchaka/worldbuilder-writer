#include "ui/SettingsDialog.h"

#include "storage/GoogleDriveService.h"
#include "ui/ThemeManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace wbw {
namespace {

QFrame* settingsCard(const QString& title, const QString& description, QLayout* body) {
    auto* card = new QFrame;
    card->setObjectName(QStringLiteral("settingsCard"));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 16);
    layout->setSpacing(9);
    auto* heading = new QLabel(title);
    heading->setObjectName(QStringLiteral("settingsCardTitle"));
    layout->addWidget(heading);
    if (!description.isEmpty()) {
        auto* copy = new QLabel(description);
        copy->setObjectName(QStringLiteral("settingsCardCopy"));
        copy->setWordWrap(true);
        layout->addWidget(copy);
    }
    layout->addLayout(body);
    return card;
}

QWidget* settingsPage(const QString& title, const QString& description) {
    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("settingsPage"));
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(18, 16, 18, 18);
    layout->setSpacing(12);
    auto* heading = new QLabel(title);
    heading->setObjectName(QStringLiteral("settingsPageTitle"));
    auto* copy = new QLabel(description);
    copy->setObjectName(QStringLiteral("settingsPageCopy"));
    copy->setWordWrap(true);
    layout->addWidget(heading);
    layout->addWidget(copy);
    return page;
}

QVBoxLayout* pageLayout(QWidget* page) {
    return qobject_cast<QVBoxLayout*>(page->layout());
}

} // namespace

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent), drive_(new GoogleDriveService(this)) {
    setWindowTitle(tr("Configuración"));
    setModal(true);
    resize(980, 690);
    setMinimumSize(820, 590);

    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 16);
    root->setSpacing(12);

    auto* header = new QHBoxLayout;
    auto* headerText = new QVBoxLayout;
    headerText->setSpacing(1);
    auto* title = new QLabel(tr("Configuración"));
    title->setObjectName(QStringLiteral("dialogTitle"));
    auto* description = new QLabel(tr("Preferencias del espacio de escritura, corrección, copias y sincronización."));
    description->setObjectName(QStringLiteral("dialogDescription"));
    headerText->addWidget(title);
    headerText->addWidget(description);
    header->addLayout(headerText);
    header->addStretch();
    auto* close = new QPushButton(tr("Cerrar"));
    close->setObjectName(QStringLiteral("secondaryAction"));
    header->addWidget(close);
    root->addLayout(header);

    auto* workspace = new QHBoxLayout;
    workspace->setSpacing(10);
    sectionList_ = new QListWidget;
    sectionList_->setObjectName(QStringLiteral("settingsNavigation"));
    sectionList_->setFixedWidth(220);
    sectionList_->addItems({tr("General"), tr("Editor"), tr("Corrector"), tr("Copias de seguridad"), tr("Nube"), tr("Exportación"), tr("Pilín Rey")});
    for (int i = 0; i < sectionList_->count(); ++i) sectionList_->item(i)->setSizeHint(QSize(200, 42));
    workspace->addWidget(sectionList_);

    sectionStack_ = new QStackedWidget;
    sectionStack_->setObjectName(QStringLiteral("settingsStack"));
    workspace->addWidget(sectionStack_, 1);
    root->addLayout(workspace, 1);

    // General / apariencia
    QWidget* general = settingsPage(tr("Apariencia"), tr("El tema y el contraste se aplican inmediatamente y se conservan entre sesiones."));
    auto* appearanceForm = new QFormLayout;
    appearanceForm->setHorizontalSpacing(18);
    appearanceForm->setVerticalSpacing(10);
    theme_ = new QComboBox;
    theme_->addItem(tr("Oscuro"), QStringLiteral("dark"));
    theme_->addItem(tr("Claro"), QStringLiteral("light"));
    theme_->setCurrentIndex(ThemeManager::savedMode() == ThemeManager::Mode::Dark ? 0 : 1);
    appearanceForm->addRow(tr("Tema de la aplicación"), theme_);
    pageLayout(general)->addWidget(settingsCard(tr("Tema"), tr("El modo oscuro es la presentación principal; el claro se mantiene como opción persistente."), appearanceForm));
    pageLayout(general)->addStretch();
    sectionStack_->addWidget(general);

    // Editor
    QWidget* editor = settingsPage(tr("Editor"), tr("Tipografía y comportamiento del manuscrito."));
    auto* editorForm = new QFormLayout;
    editorForm->setHorizontalSpacing(18);
    editorForm->setVerticalSpacing(10);
    editorFont_ = new QComboBox;
    editorFont_->addItems({QStringLiteral("Georgia"), QStringLiteral("Garamond"), QStringLiteral("Palatino Linotype"), QStringLiteral("Times New Roman"), QStringLiteral("Segoe UI")});
    const QString savedFont = settings.value(QStringLiteral("editor/fontFamily"), QStringLiteral("Georgia")).toString();
    int fontIndex = editorFont_->findText(savedFont);
    editorFont_->setCurrentIndex(fontIndex >= 0 ? fontIndex : 0);
    editorFontSize_ = new QSpinBox;
    editorFontSize_->setRange(10, 24);
    editorFontSize_->setSuffix(tr(" pt"));
    editorFontSize_->setValue(settings.value(QStringLiteral("editor/fontSize"), 12).toInt());
    autosaveSeconds_ = new QSpinBox;
    autosaveSeconds_->setRange(5, 300);
    autosaveSeconds_->setSuffix(tr(" s"));
    autosaveSeconds_->setValue(settings.value(QStringLiteral("editor/autosaveSeconds"), 15).toInt());
    editorForm->addRow(tr("Fuente del manuscrito"), editorFont_);
    editorForm->addRow(tr("Tamaño"), editorFontSize_);
    editorForm->addRow(tr("Autoguardado"), autosaveSeconds_);
    pageLayout(editor)->addWidget(settingsCard(tr("Escritura"), tr("Los cambios tipográficos se aplican al editor; el intervalo se usa en el siguiente arranque y queda guardado."), editorForm));
    pageLayout(editor)->addStretch();
    sectionStack_->addWidget(editor);

    // Corrector
    QWidget* corrector = settingsPage(tr("Corrector"), tr("LanguageTool revisa el texto seleccionado o la escena actual sin modificar el manuscrito automáticamente."));
    auto* proofForm = new QFormLayout;
    proofLanguage_ = new QComboBox;
    proofLanguage_->addItem(tr("Español"), QStringLiteral("es"));
    proofLanguage_->addItem(tr("Español (Chile)"), QStringLiteral("es-CL"));
    proofLanguage_->addItem(tr("Español (España)"), QStringLiteral("es-ES"));
    const QString proofLanguage = settings.value(QStringLiteral("proof/language"), QStringLiteral("es")).toString();
    int proofIndex = proofLanguage_->findData(proofLanguage);
    proofLanguage_->setCurrentIndex(proofIndex >= 0 ? proofIndex : 0);
    proofForm->addRow(tr("Idioma principal"), proofLanguage_);
    pageLayout(corrector)->addWidget(settingsCard(tr("Ortografía y gramática"), tr("Los reemplazos siguen siendo explícitos desde el menú contextual; nunca se reescribe el texto sin tu acción."), proofForm));
    pageLayout(corrector)->addStretch();
    sectionStack_->addWidget(corrector);

    // Copias locales
    QWidget* backup = settingsPage(tr("Copias de seguridad"), tr("Define una carpeta local para las copias WBW manuales y de seguridad."));
    auto* backupForm = new QFormLayout;
    auto* backupRow = new QWidget;
    auto* backupRowLayout = new QHBoxLayout(backupRow);
    backupRowLayout->setContentsMargins(0, 0, 0, 0);
    backupDirectory_ = new QLineEdit;
    backupDirectory_->setReadOnly(true);
    backupDirectory_->setText(settings.value(QStringLiteral("backupDirectory")).toString());
    auto* chooseBackup = new QPushButton(tr("Elegir…"));
    chooseBackup->setObjectName(QStringLiteral("secondaryAction"));
    backupRowLayout->addWidget(backupDirectory_, 1);
    backupRowLayout->addWidget(chooseBackup);
    backupForm->addRow(tr("Carpeta"), backupRow);
    pageLayout(backup)->addWidget(settingsCard(tr("Respaldo local"), tr("Las copias conservan el proyecto completo en formato .wbw."), backupForm));
    pageLayout(backup)->addStretch();
    sectionStack_->addWidget(backup);

    // Nube / Drive
    QWidget* cloud = settingsPage(tr("Nube"), tr("Google Drive es opcional; las credenciales y el estado de conexión permanecen locales."));
    auto* driveForm = new QFormLayout;
    driveForm->setHorizontalSpacing(16);
    driveForm->setVerticalSpacing(9);
    clientId_ = new QLineEdit;
    clientId_->setText(drive_->clientId());
    clientId_->setPlaceholderText(tr("ID de cliente OAuth para aplicación de escritorio"));
    clientSecret_ = new QLineEdit;
    clientSecret_->setEchoMode(QLineEdit::Password);
    clientSecret_->setPlaceholderText(tr("Secreto de cliente, si corresponde"));
    driveForm->addRow(tr("ID de cliente"), clientId_);
    driveForm->addRow(tr("Secreto"), clientSecret_);
    status_ = new QLabel;
    status_->setObjectName(QStringLiteral("driveStatus"));
    status_->setWordWrap(true);
    driveForm->addRow(tr("Estado"), status_);
    auto* driveActions = new QHBoxLayout;
    auto* saveCredentials = new QPushButton(tr("Guardar credenciales"));
    connectButton_ = new QPushButton(tr("Conectar Google Drive"));
    connectButton_->setObjectName(QStringLiteral("primaryAction"));
    disconnectButton_ = new QPushButton(tr("Desconectar"));
    refreshButton_ = new QPushButton(tr("Actualizar respaldos"));
    driveActions->addWidget(saveCredentials);
    driveActions->addWidget(connectButton_);
    driveActions->addWidget(disconnectButton_);
    driveActions->addStretch();
    driveActions->addWidget(refreshButton_);
    driveForm->addRow(driveActions);
    pageLayout(cloud)->addWidget(settingsCard(tr("Google Drive"), tr("Autoriza la cuenta desde el navegador cuando quieras habilitar sincronización de respaldos."), driveForm));
    backups_ = new QListWidget;
    backups_->setObjectName(QStringLiteral("driveBackups"));
    backups_->setMinimumHeight(160);
    auto* backupsLayout = new QVBoxLayout;
    backupsLayout->addWidget(backups_);
    pageLayout(cloud)->addWidget(settingsCard(tr("Respaldos en Drive"), QString(), backupsLayout), 1);
    sectionStack_->addWidget(cloud);

    // Exportación: muestra únicamente capacidades realmente implementadas.
    QWidget* output = settingsPage(tr("Exportación"), tr("Los formatos se configuran desde Revisión, donde se ve el resultado editorial antes de exportar."));
    auto* outputBody = new QVBoxLayout;
    auto* outputText = new QLabel(tr("Disponible actualmente: PDF de manuscrito, paquete nativo WBW y exportación cartográfica PNG / SVG / PDF desde Pilín Rey."));
    outputText->setWordWrap(true);
    outputBody->addWidget(outputText);
    pageLayout(output)->addWidget(settingsCard(tr("Salida"), tr("No se muestran controles ficticios: esta pantalla refleja solo las rutas de exportación que existen en la aplicación."), outputBody));
    pageLayout(output)->addStretch();
    sectionStack_->addWidget(output);

    // Pilín Rey defaults, persisted and consumed by the map editor.
    QWidget* map = settingsPage(tr("Pilín Rey"), tr("Valores iniciales para el lienzo cartográfico."));
    auto* mapForm = new QFormLayout;
    auto* defaultSnap = new QCheckBox(tr("Ajuste a guía al abrir el editor"));
    defaultSnap->setChecked(settings.value(QStringLiteral("map/defaultSnap"), true).toBool());
    auto* defaultGrid = new QCheckBox(tr("Mostrar cuadrícula al abrir"));
    defaultGrid->setChecked(settings.value(QStringLiteral("map/defaultGrid"), false).toBool());
    mapForm->addRow(defaultSnap);
    mapForm->addRow(defaultGrid);
    pageLayout(map)->addWidget(settingsCard(tr("Lienzo"), tr("Los paneles de capas, assets y plantilla siguen siendo flotantes y aparecen solo cuando los abres."), mapForm));
    pageLayout(map)->addStretch();
    sectionStack_->addWidget(map);

    sectionList_->setCurrentRow(0);

    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(sectionList_, &QListWidget::currentRowChanged, sectionStack_, &QStackedWidget::setCurrentIndex);
    connect(theme_, &QComboBox::currentIndexChanged, this, [this](int index) {
        const QString mode = theme_->itemData(index).toString();
        ThemeManager::saveAndApply(mode == QStringLiteral("dark") ? ThemeManager::Mode::Dark : ThemeManager::Mode::Light);
    });
    connect(editorFont_, &QComboBox::currentTextChanged, this, &SettingsDialog::persistEditorPreferences);
    connect(editorFontSize_, &QSpinBox::valueChanged, this, &SettingsDialog::persistEditorPreferences);
    connect(autosaveSeconds_, &QSpinBox::valueChanged, this, &SettingsDialog::persistEditorPreferences);
    connect(proofLanguage_, &QComboBox::currentIndexChanged, this, &SettingsDialog::persistEditorPreferences);
    connect(chooseBackup, &QPushButton::clicked, this, [this]() {
        const QString current = backupDirectory_->text();
        const QString directory = QFileDialog::getExistingDirectory(this, tr("Carpeta de copias de seguridad"), current);
        if (directory.isEmpty()) return;
        backupDirectory_->setText(directory);
        QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
        settings.setValue(QStringLiteral("backupDirectory"), directory);
    });
    connect(defaultSnap, &QCheckBox::toggled, this, [](bool enabled) {
        QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
        settings.setValue(QStringLiteral("map/defaultSnap"), enabled);
    });
    connect(defaultGrid, &QCheckBox::toggled, this, [](bool enabled) {
        QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
        settings.setValue(QStringLiteral("map/defaultGrid"), enabled);
    });

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

void SettingsDialog::persistEditorPreferences() {
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
    settings.setValue(QStringLiteral("editor/fontFamily"), editorFont_->currentText());
    settings.setValue(QStringLiteral("editor/fontSize"), editorFontSize_->value());
    settings.setValue(QStringLiteral("editor/autosaveSeconds"), autosaveSeconds_->value());
    settings.setValue(QStringLiteral("proof/language"), proofLanguage_->currentData().toString());
    settings.sync();
    ThemeManager::applySaved();
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
