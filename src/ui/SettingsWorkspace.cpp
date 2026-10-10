#include "ui/SettingsWorkspace.h"

#include "ui/ThemeManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace wbw {

SettingsWorkspace::SettingsWorkspace(QWidget* parent) : QWidget(parent) {
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
    auto* copy = new QLabel(tr("Apariencia, escritura, corrección, copias, exportación y valores iniciales de Pilín Rey."));
    copy->setObjectName(QStringLiteral("settingsWorkspaceCopy"));
    copy->setWordWrap(true);
    heading->addWidget(kicker);
    heading->addWidget(title);
    heading->addWidget(copy);
    root->addLayout(heading);

    auto* body = new QHBoxLayout;
    body->setSpacing(12);

    navigation_ = new QListWidget;
    navigation_->setObjectName(QStringLiteral("settingsWorkspaceNavigation"));
    navigation_->setFixedWidth(210);
    navigation_->addItems({
        tr("Apariencia"), tr("Tipografía"), tr("Corrector"), tr("Copias"), tr("Exportación"), tr("Pilín Rey")
    });
    for (int i = 0; i < navigation_->count(); ++i) navigation_->item(i)->setSizeHint(QSize(190, 40));
    body->addWidget(navigation_);

    pages_ = new QStackedWidget;
    pages_->setObjectName(QStringLiteral("settingsWorkspacePages"));
    body->addWidget(pages_, 1);
    root->addLayout(body, 1);

    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));

    // Appearance
    QWidget* appearance = makePage(tr("Apariencia"), tr("El modo oscuro es la presentación principal y el claro se conserva como alternativa persistente."));
    auto* appearanceForm = new QFormLayout;
    appearanceForm->setVerticalSpacing(10);
    theme_ = new QComboBox;
    theme_->addItem(tr("Oscuro"), QStringLiteral("dark"));
    theme_->addItem(tr("Claro"), QStringLiteral("light"));
    theme_->setCurrentIndex(ThemeManager::savedMode() == ThemeManager::Mode::Dark ? 0 : 1);
    appearanceForm->addRow(tr("Tema"), theme_);
    qobject_cast<QVBoxLayout*>(appearance->layout())->addWidget(makeCard(tr("Interfaz"), tr("Se aplica inmediatamente a toda la aplicación y queda guardado entre sesiones."), appearanceForm));
    qobject_cast<QVBoxLayout*>(appearance->layout())->addStretch();
    pages_->addWidget(appearance);

    // Typography/editor
    QWidget* typography = makePage(tr("Tipografía y escritura"), tr("Valores del manuscrito y frecuencia de autoguardado."));
    auto* editorForm = new QFormLayout;
    editorForm->setVerticalSpacing(10);
    editorFont_ = new QComboBox;
    editorFont_->addItems({QStringLiteral("Georgia"), QStringLiteral("Garamond"), QStringLiteral("Palatino Linotype"), QStringLiteral("Times New Roman"), QStringLiteral("Segoe UI")});
    const QString savedFont = settings.value(QStringLiteral("editor/fontFamily"), QStringLiteral("Georgia")).toString();
    const int fontIndex = editorFont_->findText(savedFont);
    editorFont_->setCurrentIndex(fontIndex >= 0 ? fontIndex : 0);
    editorFontSize_ = new QSpinBox;
    editorFontSize_->setRange(10, 24);
    editorFontSize_->setSuffix(tr(" pt"));
    editorFontSize_->setValue(settings.value(QStringLiteral("editor/fontSize"), 12).toInt());
    autosaveSeconds_ = new QSpinBox;
    autosaveSeconds_->setRange(5, 300);
    autosaveSeconds_->setSuffix(tr(" s"));
    autosaveSeconds_->setValue(settings.value(QStringLiteral("editor/autosaveSeconds"), 15).toInt());
    editorForm->addRow(tr("Fuente"), editorFont_);
    editorForm->addRow(tr("Tamaño"), editorFontSize_);
    editorForm->addRow(tr("Autoguardado"), autosaveSeconds_);
    qobject_cast<QVBoxLayout*>(typography->layout())->addWidget(makeCard(tr("Manuscrito"), tr("El editor usa estos valores como preferencias persistentes."), editorForm));
    qobject_cast<QVBoxLayout*>(typography->layout())->addStretch();
    pages_->addWidget(typography);

    // Proofreader
    QWidget* proof = makePage(tr("Corrector"), tr("El corrector no reescribe el manuscrito por sí solo; las sustituciones siguen siendo explícitas."));
    auto* proofForm = new QFormLayout;
    proofLanguage_ = new QComboBox;
    proofLanguage_->addItem(tr("Español"), QStringLiteral("es"));
    proofLanguage_->addItem(tr("Español (Chile)"), QStringLiteral("es-CL"));
    proofLanguage_->addItem(tr("Español (España)"), QStringLiteral("es-ES"));
    const QString savedProof = settings.value(QStringLiteral("proof/language"), QStringLiteral("es")).toString();
    const int proofIndex = proofLanguage_->findData(savedProof);
    proofLanguage_->setCurrentIndex(proofIndex >= 0 ? proofIndex : 0);
    proofForm->addRow(tr("Idioma principal"), proofLanguage_);
    qobject_cast<QVBoxLayout*>(proof->layout())->addWidget(makeCard(tr("Ortografía y gramática"), tr("La revisión permanece integrada en el manuscrito y conserva el texto original hasta que confirmas un cambio."), proofForm));
    qobject_cast<QVBoxLayout*>(proof->layout())->addStretch();
    pages_->addWidget(proof);

    // Backups
    QWidget* backups = makePage(tr("Copias de seguridad"), tr("Ubicación local para respaldos completos del proyecto."));
    auto* backupForm = new QFormLayout;
    auto* row = new QWidget;
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    backupDirectory_ = new QLineEdit;
    backupDirectory_->setReadOnly(true);
    backupDirectory_->setText(settings.value(QStringLiteral("backupDirectory")).toString());
    auto* choose = new QPushButton(tr("Elegir…"));
    choose->setObjectName(QStringLiteral("settingsSecondary"));
    rowLayout->addWidget(backupDirectory_, 1);
    rowLayout->addWidget(choose);
    backupForm->addRow(tr("Carpeta"), row);
    qobject_cast<QVBoxLayout*>(backups->layout())->addWidget(makeCard(tr("Respaldo local"), tr("Las copias conservan el proyecto completo en formato .wbw."), backupForm));
    qobject_cast<QVBoxLayout*>(backups->layout())->addStretch();
    pages_->addWidget(backups);

    // Export
    QWidget* output = makePage(tr("Exportación"), tr("Salidas que ya existen en la aplicación; no se muestran opciones ficticias."));
    auto* outputLayout = new QVBoxLayout;
    auto* outputText = new QLabel(tr("Manuscrito: PDF y paquete nativo WBW.\nMapas: PNG, SVG y PDF desde Pilín Rey."));
    outputText->setWordWrap(true);
    outputText->setObjectName(QStringLiteral("settingsOutputText"));
    outputLayout->addWidget(outputText);
    qobject_cast<QVBoxLayout*>(output->layout())->addWidget(makeCard(tr("Formatos disponibles"), tr("La exportación editorial se inicia desde Revisión; la cartográfica desde Mapas."), outputLayout));
    qobject_cast<QVBoxLayout*>(output->layout())->addStretch();
    pages_->addWidget(output);

    // Map defaults
    QWidget* map = makePage(tr("Pilín Rey"), tr("Preferencias iniciales del lienzo cartográfico."));
    auto* mapForm = new QFormLayout;
    auto* snap = new QCheckBox(tr("Ajuste a guía al abrir"));
    snap->setChecked(settings.value(QStringLiteral("map/defaultSnap"), true).toBool());
    auto* grid = new QCheckBox(tr("Mostrar cuadrícula al abrir"));
    grid->setChecked(settings.value(QStringLiteral("map/defaultGrid"), false).toBool());
    mapForm->addRow(snap);
    mapForm->addRow(grid);
    qobject_cast<QVBoxLayout*>(map->layout())->addWidget(makeCard(tr("Lienzo"), tr("Capas, assets, plantilla y propiedades siguen siendo paneles contextuales sobre el mapa."), mapForm));
    qobject_cast<QVBoxLayout*>(map->layout())->addStretch();
    pages_->addWidget(map);

    navigation_->setCurrentRow(0);
    connect(navigation_, &QListWidget::currentRowChanged, pages_, &QStackedWidget::setCurrentIndex);
    connect(theme_, &QComboBox::currentIndexChanged, this, [this](int index) {
        const bool dark = theme_->itemData(index).toString() == QStringLiteral("dark");
        ThemeManager::saveAndApply(dark ? ThemeManager::Mode::Dark : ThemeManager::Mode::Light);
        emit preferencesChanged();
    });
    connect(editorFont_, &QComboBox::currentTextChanged, this, &SettingsWorkspace::persistEditor);
    connect(editorFontSize_, &QSpinBox::valueChanged, this, [this](int) { persistEditor(); });
    connect(autosaveSeconds_, &QSpinBox::valueChanged, this, [this](int) { persistEditor(); });
    connect(proofLanguage_, &QComboBox::currentIndexChanged, this, [this](int) { persistEditor(); });
    connect(choose, &QPushButton::clicked, this, &SettingsWorkspace::chooseBackupDirectory);
    connect(snap, &QCheckBox::toggled, this, [this](bool value) {
        QSettings s(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
        s.setValue(QStringLiteral("map/defaultSnap"), value);
        emit preferencesChanged();
    });
    connect(grid, &QCheckBox::toggled, this, [this](bool value) {
        QSettings s(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
        s.setValue(QStringLiteral("map/defaultGrid"), value);
        emit preferencesChanged();
    });

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
#settingsCardCopy,#settingsOutputText{color:#8d96a1;}
#settingsSecondary{background:#1b2026;color:#c7cdd4;border:1px solid #343b44;border-radius:7px;padding:7px 11px;}
#settingsWorkspace QLineEdit,#settingsWorkspace QComboBox,#settingsWorkspace QSpinBox{background:#101317;color:#dde2e7;border:1px solid #343a42;border-radius:7px;padding:7px 9px;}
#settingsWorkspace QCheckBox{color:#c6ccd3;spacing:8px;}
)QSS"));
}

QWidget* SettingsWorkspace::makePage(const QString& title, const QString& description) {
    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("settingsWorkspacePage"));
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(4, 2, 4, 4);
    layout->setSpacing(12);
    auto* heading = new QLabel(title, page);
    heading->setObjectName(QStringLiteral("settingsPageTitle"));
    auto* copy = new QLabel(description, page);
    copy->setObjectName(QStringLiteral("settingsPageCopy"));
    copy->setWordWrap(true);
    layout->addWidget(heading);
    layout->addWidget(copy);
    return page;
}

QWidget* SettingsWorkspace::makeCard(const QString& title, const QString& description, QLayout* body) {
    auto* card = new QFrame;
    card->setObjectName(QStringLiteral("settingsCard"));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 16);
    layout->setSpacing(9);
    auto* heading = new QLabel(title, card);
    heading->setObjectName(QStringLiteral("settingsCardTitle"));
    layout->addWidget(heading);
    if (!description.isEmpty()) {
        auto* copy = new QLabel(description, card);
        copy->setObjectName(QStringLiteral("settingsCardCopy"));
        copy->setWordWrap(true);
        layout->addWidget(copy);
    }
    layout->addLayout(body);
    return card;
}

void SettingsWorkspace::persistEditor() {
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
    settings.setValue(QStringLiteral("editor/fontFamily"), editorFont_->currentText());
    settings.setValue(QStringLiteral("editor/fontSize"), editorFontSize_->value());
    settings.setValue(QStringLiteral("editor/autosaveSeconds"), autosaveSeconds_->value());
    settings.setValue(QStringLiteral("proof/language"), proofLanguage_->currentData().toString());
    emit preferencesChanged();
}

void SettingsWorkspace::chooseBackupDirectory() {
    const QString path = QFileDialog::getExistingDirectory(this, tr("Carpeta de copias de seguridad"), backupDirectory_->text());
    if (path.isEmpty()) return;
    backupDirectory_->setText(path);
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
    settings.setValue(QStringLiteral("backupDirectory"), path);
    emit preferencesChanged();
}

} // namespace wbw
