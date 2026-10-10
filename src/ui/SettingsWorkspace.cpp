#include "ui/SettingsWorkspace.h"

#include "storage/GoogleDriveService.h"
#include "ui/ThemeManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QKeySequenceEdit>
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

SettingsWorkspace::SettingsWorkspace(QWidget* parent)
    : QWidget(parent), drive_(new GoogleDriveService(this)) {
    setObjectName(QStringLiteral("settingsWorkspace"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 18, 22, 22);
    root->setSpacing(14);

    auto* heading = new QVBoxLayout;
    heading->setSpacing(2);
    auto* kicker = new QLabel(tr("CONFIGURACIÓN / SALIDA"));
    kicker->setObjectName(QStringLiteral("settingsKicker"));
    auto* title = new QLabel(tr("Preferencias"));
    title->setObjectName(QStringLiteral("settingsWorkspaceTitle"));
    auto* copy = new QLabel(tr("Apariencia, escritura, atajos, corrección, copias, nube, exportación y valores iniciales de Pilín Rey."));
    copy->setObjectName(QStringLiteral("settingsWorkspaceCopy"));
    copy->setWordWrap(true);
    heading->addWidget(kicker); heading->addWidget(title); heading->addWidget(copy);
    root->addLayout(heading);

    auto* body = new QHBoxLayout;
    body->setSpacing(12);
    navigation_ = new QListWidget;
    navigation_->setObjectName(QStringLiteral("settingsWorkspaceNavigation"));
    navigation_->setFixedWidth(210);
    navigation_->addItems({tr("Apariencia"), tr("Tipografía"), tr("Atajos"), tr("Corrector"), tr("Copias"), tr("Nube"), tr("Exportación"), tr("Pilín Rey")});
    for (int i = 0; i < navigation_->count(); ++i) navigation_->item(i)->setSizeHint(QSize(190, 40));
    body->addWidget(navigation_);

    pages_ = new QStackedWidget;
    pages_->setObjectName(QStringLiteral("settingsWorkspacePages"));
    body->addWidget(pages_, 1);
    root->addLayout(body, 1);

    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));

    QWidget* appearance = makePage(tr("Apariencia"), tr("El modo oscuro es la presentación principal y el claro se conserva como alternativa persistente."));
    auto* appearanceForm = new QFormLayout;
    theme_ = new QComboBox;
    theme_->addItem(tr("Oscuro"), QStringLiteral("dark"));
    theme_->addItem(tr("Claro"), QStringLiteral("light"));
    theme_->setCurrentIndex(ThemeManager::savedMode() == ThemeManager::Mode::Dark ? 0 : 1);
    appearanceForm->addRow(tr("Tema"), theme_);
    qobject_cast<QVBoxLayout*>(appearance->layout())->addWidget(makeCard(tr("Interfaz"), tr("Se aplica inmediatamente a toda la aplicación y queda guardado entre sesiones."), appearanceForm));
    qobject_cast<QVBoxLayout*>(appearance->layout())->addStretch();
    pages_->addWidget(appearance);

    QWidget* typography = makePage(tr("Tipografía y escritura"), tr("Valores del manuscrito y frecuencia de autoguardado."));
    auto* editorForm = new QFormLayout;
    editorFont_ = new QComboBox;
    editorFont_->addItems({QStringLiteral("Georgia"), QStringLiteral("Garamond"), QStringLiteral("Palatino Linotype"), QStringLiteral("Times New Roman"), QStringLiteral("Segoe UI")});
    const QString savedFont = settings.value(QStringLiteral("editor/fontFamily"), QStringLiteral("Georgia")).toString();
    editorFont_->setCurrentIndex(qMax(0, editorFont_->findText(savedFont)));
    editorFontSize_ = new QSpinBox;
    editorFontSize_->setRange(10, 24); editorFontSize_->setSuffix(tr(" pt")); editorFontSize_->setValue(settings.value(QStringLiteral("editor/fontSize"), 12).toInt());
    autosaveSeconds_ = new QSpinBox;
    autosaveSeconds_->setRange(5, 300); autosaveSeconds_->setSuffix(tr(" s")); autosaveSeconds_->setValue(settings.value(QStringLiteral("editor/autosaveSeconds"), 15).toInt());
    editorForm->addRow(tr("Fuente"), editorFont_); editorForm->addRow(tr("Tamaño"), editorFontSize_); editorForm->addRow(tr("Autoguardado"), autosaveSeconds_);
    qobject_cast<QVBoxLayout*>(typography->layout())->addWidget(makeCard(tr("Manuscrito"), tr("El editor usa estos valores como preferencias persistentes."), editorForm));
    qobject_cast<QVBoxLayout*>(typography->layout())->addStretch();
    pages_->addWidget(typography);

    QWidget* shortcuts = makePage(tr("Atajos de teclado"), tr("Los atajos de escritura son editables y se aplican al volver al manuscrito."));
    auto* shortcutForm = new QFormLayout;
    shortcutForm->setVerticalSpacing(10);
    focusShortcut_ = new QKeySequenceEdit(QKeySequence(settings.value(QStringLiteral("shortcut/focus"), QStringLiteral("Ctrl+Shift+F")).toString()));
    proofShortcut_ = new QKeySequenceEdit(QKeySequence(settings.value(QStringLiteral("shortcut/proofread"), QStringLiteral("F7")).toString()));
    focusShortcut_->setClearButtonEnabled(true);
    proofShortcut_->setClearButtonEnabled(true);
    shortcutForm->addRow(tr("Modo enfoque"), focusShortcut_);
    shortcutForm->addRow(tr("Revisar texto"), proofShortcut_);
    auto* fixed = new QLabel(tr("Siempre disponibles: Ctrl+B negrita · Ctrl+I cursiva · Ctrl+U subrayado · Ctrl+Z deshacer · Ctrl+Y rehacer · Esc salir de enfoque."));
    fixed->setWordWrap(true);
    fixed->setObjectName(QStringLiteral("settingsOutputText"));
    auto* shortcutBody = new QVBoxLayout;
    shortcutBody->addLayout(shortcutForm);
    shortcutBody->addWidget(fixed);
    qobject_cast<QVBoxLayout*>(shortcuts->layout())->addWidget(makeCard(tr("Escritura"), tr("No se crean combinaciones decorativas: estos atajos disparan acciones reales del manuscrito."), shortcutBody));
    qobject_cast<QVBoxLayout*>(shortcuts->layout())->addStretch();
    pages_->addWidget(shortcuts);

    QWidget* proof = makePage(tr("Corrector"), tr("El corrector no reescribe el manuscrito por sí solo; las sustituciones siguen siendo explícitas."));
    auto* proofForm = new QFormLayout;
    proofLanguage_ = new QComboBox;
    proofLanguage_->addItem(tr("Español"), QStringLiteral("es")); proofLanguage_->addItem(tr("Español (Chile)"), QStringLiteral("es-CL")); proofLanguage_->addItem(tr("Español (España)"), QStringLiteral("es-ES"));
    const int proofIndex = proofLanguage_->findData(settings.value(QStringLiteral("proof/language"), QStringLiteral("es")).toString());
    proofLanguage_->setCurrentIndex(proofIndex >= 0 ? proofIndex : 0);
    proofForm->addRow(tr("Idioma principal"), proofLanguage_);
    qobject_cast<QVBoxLayout*>(proof->layout())->addWidget(makeCard(tr("Ortografía y gramática"), tr("La revisión permanece integrada en el manuscrito y conserva el texto original hasta que confirmas un cambio."), proofForm));
    qobject_cast<QVBoxLayout*>(proof->layout())->addStretch();
    pages_->addWidget(proof);

    QWidget* backups = makePage(tr("Copias de seguridad"), tr("Ubicación local para respaldos completos del proyecto."));
    auto* backupForm = new QFormLayout;
    auto* backupRow = new QWidget;
    auto* backupRowLayout = new QHBoxLayout(backupRow); backupRowLayout->setContentsMargins(0, 0, 0, 0);
    backupDirectory_ = new QLineEdit; backupDirectory_->setReadOnly(true); backupDirectory_->setText(settings.value(QStringLiteral("backupDirectory")).toString());
    auto* choose = new QPushButton(tr("Elegir…")); choose->setObjectName(QStringLiteral("settingsSecondary"));
    backupRowLayout->addWidget(backupDirectory_, 1); backupRowLayout->addWidget(choose); backupForm->addRow(tr("Carpeta"), backupRow);
    qobject_cast<QVBoxLayout*>(backups->layout())->addWidget(makeCard(tr("Respaldo local"), tr("Las copias conservan el proyecto completo en formato .wbw."), backupForm));
    qobject_cast<QVBoxLayout*>(backups->layout())->addStretch();
    pages_->addWidget(backups);

    QWidget* cloud = makePage(tr("Nube"), tr("Google Drive es opcional. Las credenciales y el estado de conexión permanecen locales."));
    auto* cloudForm = new QFormLayout; cloudForm->setHorizontalSpacing(16); cloudForm->setVerticalSpacing(9);
    clientId_ = new QLineEdit; clientId_->setText(drive_->clientId()); clientId_->setPlaceholderText(tr("ID de cliente OAuth para aplicación de escritorio"));
    clientSecret_ = new QLineEdit; clientSecret_->setEchoMode(QLineEdit::Password); clientSecret_->setPlaceholderText(tr("Secreto de cliente, si corresponde"));
    cloudStatus_ = new QLabel; cloudStatus_->setObjectName(QStringLiteral("cloudStatus")); cloudStatus_->setWordWrap(true);
    cloudForm->addRow(tr("ID de cliente"), clientId_); cloudForm->addRow(tr("Secreto"), clientSecret_); cloudForm->addRow(tr("Estado"), cloudStatus_);
    auto* cloudActions = new QHBoxLayout;
    auto* saveCredentials = new QPushButton(tr("Guardar credenciales")); saveCredentials->setObjectName(QStringLiteral("settingsSecondary"));
    connectDrive_ = new QPushButton(tr("Conectar Google Drive")); connectDrive_->setObjectName(QStringLiteral("settingsPrimary"));
    disconnectDrive_ = new QPushButton(tr("Desconectar")); disconnectDrive_->setObjectName(QStringLiteral("settingsSecondary"));
    auto* refresh = new QPushButton(tr("Actualizar respaldos")); refresh->setObjectName(QStringLiteral("settingsSecondary"));
    cloudActions->addWidget(saveCredentials); cloudActions->addWidget(connectDrive_); cloudActions->addWidget(disconnectDrive_); cloudActions->addStretch(); cloudActions->addWidget(refresh);
    cloudForm->addRow(cloudActions);
    qobject_cast<QVBoxLayout*>(cloud->layout())->addWidget(makeCard(tr("Google Drive"), tr("Autoriza la cuenta desde el navegador cuando quieras habilitar sincronización de respaldos."), cloudForm));
    cloudBackups_ = new QListWidget; cloudBackups_->setObjectName(QStringLiteral("cloudBackups")); cloudBackups_->setMinimumHeight(180);
    auto* backupListLayout = new QVBoxLayout; backupListLayout->addWidget(cloudBackups_);
    qobject_cast<QVBoxLayout*>(cloud->layout())->addWidget(makeCard(tr("Respaldos en Drive"), tr("Lista de copias WBW vinculadas a Worldbuilder Writer."), backupListLayout), 1);
    pages_->addWidget(cloud);

    QWidget* output = makePage(tr("Exportación"), tr("Salidas que ya existen en la aplicación; no se muestran opciones ficticias."));
    auto* outputLayout = new QVBoxLayout;
    auto* outputText = new QLabel(tr("Manuscrito: PDF y paquete nativo WBW.\nMapas: PNG, SVG y PDF desde Pilín Rey.")); outputText->setWordWrap(true); outputText->setObjectName(QStringLiteral("settingsOutputText"));
    outputLayout->addWidget(outputText);
    qobject_cast<QVBoxLayout*>(output->layout())->addWidget(makeCard(tr("Formatos disponibles"), tr("La exportación editorial se inicia desde Revisión; la cartográfica desde Mapas."), outputLayout));
    qobject_cast<QVBoxLayout*>(output->layout())->addStretch();
    pages_->addWidget(output);

    QWidget* map = makePage(tr("Pilín Rey"), tr("Preferencias iniciales del lienzo cartográfico."));
    auto* mapForm = new QFormLayout;
    auto* snap = new QCheckBox(tr("Ajuste a guía al abrir")); snap->setChecked(settings.value(QStringLiteral("map/defaultSnap"), true).toBool());
    auto* grid = new QCheckBox(tr("Mostrar cuadrícula al abrir")); grid->setChecked(settings.value(QStringLiteral("map/defaultGrid"), false).toBool());
    mapForm->addRow(snap); mapForm->addRow(grid);
    qobject_cast<QVBoxLayout*>(map->layout())->addWidget(makeCard(tr("Lienzo"), tr("Capas, assets, plantilla y propiedades siguen siendo paneles contextuales sobre el mapa."), mapForm));
    qobject_cast<QVBoxLayout*>(map->layout())->addStretch();
    pages_->addWidget(map);

    navigation_->setCurrentRow(0);
    connect(navigation_, &QListWidget::currentRowChanged, pages_, &QStackedWidget::setCurrentIndex);
    connect(theme_, &QComboBox::currentIndexChanged, this, [this](int index) { ThemeManager::saveAndApply(theme_->itemData(index).toString() == QStringLiteral("dark") ? ThemeManager::Mode::Dark : ThemeManager::Mode::Light); emit preferencesChanged(); });
    connect(editorFont_, &QComboBox::currentTextChanged, this, &SettingsWorkspace::persistEditor);
    connect(editorFontSize_, &QSpinBox::valueChanged, this, [this](int) { persistEditor(); });
    connect(autosaveSeconds_, &QSpinBox::valueChanged, this, [this](int) { persistEditor(); });
    connect(focusShortcut_, &QKeySequenceEdit::keySequenceChanged, this, &SettingsWorkspace::persistShortcuts);
    connect(proofShortcut_, &QKeySequenceEdit::keySequenceChanged, this, &SettingsWorkspace::persistShortcuts);
    connect(proofLanguage_, &QComboBox::currentIndexChanged, this, [this](int) { persistEditor(); });
    connect(choose, &QPushButton::clicked, this, &SettingsWorkspace::chooseBackupDirectory);
    connect(snap, &QCheckBox::toggled, this, [this](bool value) { QSettings s(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter")); s.setValue(QStringLiteral("map/defaultSnap"), value); emit preferencesChanged(); });
    connect(grid, &QCheckBox::toggled, this, [this](bool value) { QSettings s(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter")); s.setValue(QStringLiteral("map/defaultGrid"), value); emit preferencesChanged(); });

    connect(saveCredentials, &QPushButton::clicked, this, [this]() { drive_->setClientCredentials(clientId_->text().trimmed(), clientSecret_->text()); refreshCloudState(); cloudStatus_->setText(tr("Credenciales guardadas localmente.")); });
    connect(connectDrive_, &QPushButton::clicked, this, [this]() { drive_->setClientCredentials(clientId_->text().trimmed(), clientSecret_->text()); drive_->connectAccount(); });
    connect(disconnectDrive_, &QPushButton::clicked, drive_, &GoogleDriveService::disconnectAccount);
    connect(refresh, &QPushButton::clicked, drive_, &GoogleDriveService::listBackups);
    connect(drive_, &GoogleDriveService::connectionChanged, this, [this](bool) { refreshCloudState(); });
    connect(drive_, &GoogleDriveService::authorizationStarted, this, [this]() { cloudStatus_->setText(tr("Se abrió el navegador. Autoriza Worldbuilder Writer y vuelve a esta ventana.")); });
    connect(drive_, &GoogleDriveService::statusMessage, cloudStatus_, &QLabel::setText);
    connect(drive_, &GoogleDriveService::errorOccurred, this, [this](const QString& message) { cloudStatus_->setText(message); QMessageBox::warning(this, tr("Google Drive"), message); });
    connect(drive_, &GoogleDriveService::backupsListed, this, &SettingsWorkspace::populateCloudBackups);
    refreshCloudState();

    setStyleSheet(QStringLiteral(R"QSS(
#settingsWorkspace{background:#101215;color:#d8dde4;}
#settingsKicker{color:#b98a53;font-size:8pt;font-weight:700;letter-spacing:1px;}
#settingsWorkspaceTitle{color:#f1f3f5;font-family:'Georgia';font-size:25pt;font-weight:600;}
#settingsWorkspaceCopy{color:#7f8791;font-size:9.5pt;}
#settingsWorkspaceNavigation{background:#14171b;border:1px solid #292e35;border-radius:10px;padding:7px;color:#9ca4ae;outline:0;}
#settingsWorkspaceNavigation::item{padding:8px 10px;border-radius:7px;}
#settingsWorkspaceNavigation::item:selected{background:#252a31;color:#f2f4f6;border-left:2px solid #bd8e56;}
#settingsWorkspacePages,#settingsWorkspacePage{background:#101215;}
#settingsPageTitle{color:#edf0f3;font-family:'Georgia';font-size:18pt;font-weight:600;}
#settingsPageCopy{color:#7d8590;}
#settingsCard{background:#15191e;border:1px solid #2b3138;border-radius:10px;}
#settingsCardTitle{color:#eef1f4;font-size:11pt;font-weight:700;}
#settingsCardCopy,#settingsOutputText,#cloudStatus{color:#8d96a1;}
#settingsSecondary{background:#1b2026;color:#c7cdd4;border:1px solid #343b44;border-radius:7px;padding:7px 11px;}
#settingsPrimary{background:#c59a5d;color:#111315;border:0;border-radius:7px;padding:7px 11px;font-weight:700;}
#settingsWorkspace QLineEdit,#settingsWorkspace QComboBox,#settingsWorkspace QSpinBox,#settingsWorkspace QKeySequenceEdit{background:#101317;color:#dde2e7;border:1px solid #343a42;border-radius:7px;padding:7px 9px;}
#settingsWorkspace QCheckBox{color:#c6ccd3;spacing:8px;}
#cloudBackups{background:#101317;color:#d8dde4;border:1px solid #2f363f;border-radius:8px;outline:0;}
#cloudBackups::item{padding:8px;border-bottom:1px solid #20252b;}
)QSS"));
}

QWidget* SettingsWorkspace::makePage(const QString& title, const QString& description) {
    auto* page = new QWidget; page->setObjectName(QStringLiteral("settingsWorkspacePage"));
    auto* layout = new QVBoxLayout(page); layout->setContentsMargins(4, 2, 4, 4); layout->setSpacing(12);
    auto* heading = new QLabel(title, page); heading->setObjectName(QStringLiteral("settingsPageTitle"));
    auto* copy = new QLabel(description, page); copy->setObjectName(QStringLiteral("settingsPageCopy")); copy->setWordWrap(true);
    layout->addWidget(heading); layout->addWidget(copy); return page;
}

QWidget* SettingsWorkspace::makeCard(const QString& title, const QString& description, QLayout* body) {
    auto* card = new QFrame; card->setObjectName(QStringLiteral("settingsCard"));
    auto* layout = new QVBoxLayout(card); layout->setContentsMargins(16, 14, 16, 16); layout->setSpacing(9);
    auto* heading = new QLabel(title, card); heading->setObjectName(QStringLiteral("settingsCardTitle")); layout->addWidget(heading);
    if (!description.isEmpty()) { auto* copy = new QLabel(description, card); copy->setObjectName(QStringLiteral("settingsCardCopy")); copy->setWordWrap(true); layout->addWidget(copy); }
    layout->addLayout(body); return card;
}

void SettingsWorkspace::persistEditor() {
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
    settings.setValue(QStringLiteral("editor/fontFamily"), editorFont_->currentText());
    settings.setValue(QStringLiteral("editor/fontSize"), editorFontSize_->value());
    settings.setValue(QStringLiteral("editor/autosaveSeconds"), autosaveSeconds_->value());
    settings.setValue(QStringLiteral("proof/language"), proofLanguage_->currentData().toString());
    emit preferencesChanged();
}

void SettingsWorkspace::persistShortcuts() {
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
    settings.setValue(QStringLiteral("shortcut/focus"), focusShortcut_->keySequence().toString(QKeySequence::PortableText));
    settings.setValue(QStringLiteral("shortcut/proofread"), proofShortcut_->keySequence().toString(QKeySequence::PortableText));
    emit preferencesChanged();
}

void SettingsWorkspace::chooseBackupDirectory() {
    const QString path = QFileDialog::getExistingDirectory(this, tr("Carpeta de copias de seguridad"), backupDirectory_->text());
    if (path.isEmpty()) return;
    backupDirectory_->setText(path);
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter")); settings.setValue(QStringLiteral("backupDirectory"), path);
    emit preferencesChanged();
}

void SettingsWorkspace::refreshCloudState() {
    if (!drive_ || !cloudStatus_) return;
    const bool configured = drive_->isConfigured(); const bool connected = drive_->isConnected();
    cloudStatus_->setText(connected ? tr("Conectado a Google Drive.") : configured ? tr("Credenciales configuradas. Falta autorizar la cuenta.") : tr("Google Drive no está configurado."));
    if (connectDrive_) connectDrive_->setEnabled(configured && !connected);
    if (disconnectDrive_) disconnectDrive_->setEnabled(connected);
    if (connected) drive_->listBackups();
}

void SettingsWorkspace::populateCloudBackups(const QJsonArray& files) {
    if (!cloudBackups_) return;
    cloudBackups_->clear();
    for (const QJsonValue& value : files) {
        const QJsonObject file = value.toObject();
        const QString name = file.value(QStringLiteral("name")).toString(tr("Copia WBW"));
        const QString modified = file.value(QStringLiteral("modifiedTime")).toString();
        auto* item = new QListWidgetItem(modified.isEmpty() ? name : QStringLiteral("%1  ·  %2").arg(name, modified));
        item->setData(Qt::UserRole, file.value(QStringLiteral("id")).toString());
        cloudBackups_->addItem(item);
    }
    if (files.isEmpty()) cloudBackups_->addItem(tr("No hay respaldos WBW en esta cuenta."));
}

} // namespace wbw
