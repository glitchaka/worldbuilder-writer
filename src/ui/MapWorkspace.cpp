#include "ui/MapWorkspace.h"

#include "core/ArchiveDocument.h"
#include "ui/PilinReyEditor.h"
#include "ui/ThemeManager.h"

#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

namespace wbw {
namespace {

QString uid(const QString& prefix) {
    return prefix + QLatin1Char('-') + QUuid::createUuid().toString(QUuid::Id128);
}

QToolButton* chromeButton(const QString& text, const QString& tooltip, QWidget* parent) {
    auto* button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("mapWorkspaceChromeButton"));
    button->setText(text);
    button->setToolTip(tooltip);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

QJsonObject newMapObject(const QString& name) {
    return QJsonObject{
        {QStringLiteral("id"), uid(QStringLiteral("map"))},
        {QStringLiteral("name"), name},
        {QStringLiteral("description"), QString()},
        {QStringLiteral("seed"), static_cast<int>(QDateTime::currentSecsSinceEpoch() & 0x7fffffff)},
        {QStringLiteral("style"), QStringLiteral("Atlas")},
        {QStringLiteral("continents"), 3},
        {QStringLiteral("islands"), 10},
        {QStringLiteral("roughness"), 58},
        {QStringLiteral("markers"), QJsonArray()},
        {QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}
    };
}

bool isBlankNativeTerrain(const QJsonObject& map) {
    const QJsonObject terrain = map.value(QStringLiteral("pilinRey")).toObject()
                                    .value(QStringLiteral("terrainModel")).toObject();
    return terrain.isEmpty()
        || terrain.value(QStringLiteral("generator")).toString() == QStringLiteral("blank-heightmap");
}

} // namespace

MapWorkspace::MapWorkspace(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("mapWorkspace"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    chrome_ = new QFrame(this);
    chrome_->setObjectName(QStringLiteral("mapWorkspaceChrome"));
    chrome_->setFixedHeight(48);
    auto* chromeLayout = new QHBoxLayout(chrome_);
    chromeLayout->setContentsMargins(14, 6, 12, 6);
    chromeLayout->setSpacing(6);

    auto* kicker = new QLabel(tr("MAPA"), chrome_);
    kicker->setObjectName(QStringLiteral("mapWorkspaceKicker"));
    chromeLayout->addWidget(kicker);

    picker_ = new QComboBox(chrome_);
    picker_->setObjectName(QStringLiteral("mapWorkspacePicker"));
    picker_->setMinimumWidth(170);
    picker_->setMaximumWidth(260);
    chromeLayout->addWidget(picker_);

    auto* add = chromeButton(QStringLiteral("+"), tr("Nuevo mapa"), chrome_);
    auto* duplicate = chromeButton(QStringLiteral("⧉"), tr("Duplicar mapa"), chrome_);
    auto* remove = chromeButton(QStringLiteral("−"), tr("Eliminar mapa"), chrome_);
    chromeLayout->addWidget(add);
    chromeLayout->addWidget(duplicate);
    chromeLayout->addWidget(remove);
    chromeLayout->addSpacing(6);

    name_ = new QLineEdit(chrome_);
    name_->setObjectName(QStringLiteral("mapWorkspaceName"));
    name_->setPlaceholderText(tr("Nombre del mapa"));
    name_->setMinimumWidth(180);
    name_->setMaximumWidth(360);
    chromeLayout->addWidget(name_, 1);

    auto* metadata = chromeButton(tr("Detalles"), tr("Nombre, descripción y estilo del mapa"), chrome_);
    metadata->setObjectName(QStringLiteral("mapWorkspaceTextButton"));
    chromeLayout->addWidget(metadata);
    chromeLayout->addStretch(1);

    auto* hint = new QLabel(tr("Terreno editable · clima e hidrología derivada · capas · rutas · asentamientos · assets"), chrome_);
    hint->setObjectName(QStringLiteral("mapWorkspaceHint"));
    hint->setMaximumWidth(520);
    chromeLayout->addWidget(hint);
    root->addWidget(chrome_);

    auto* canvasHost = new QFrame(this);
    canvasHost->setObjectName(QStringLiteral("mapWorkspaceCanvasHost"));
    auto* canvasLayout = new QVBoxLayout(canvasHost);
    canvasLayout->setContentsMargins(0, 0, 0, 0);
    canvasLayout->setSpacing(0);

    editor_ = new PilinReyEditor(canvasHost);
    editor_->setObjectName(QStringLiteral("pilinReyEditor"));
    canvasLayout->addWidget(editor_, 1);

    // Build the real native cartography studio at construction time. This is
    // not tied to a theme change and cannot fall back to the legacy floating UI.
    editor_->activateAzgaarGenerationPipeline();
    editor_->configureProductWorkspace();
    editor_->installCartographicFurniture();
    editor_->setProperty("wbwMapInfrastructureReady", true);

    auto* empty = new QFrame(canvasHost);
    empty->setObjectName(QStringLiteral("mapWorkspaceEmpty"));
    auto* emptyLayout = new QVBoxLayout(empty);
    emptyLayout->setContentsMargins(40, 40, 40, 40);
    emptyLayout->setSpacing(9);
    emptyLayout->addStretch(1);
    auto* mark = new QLabel(QStringLiteral("◇"), empty);
    mark->setObjectName(QStringLiteral("mapWorkspaceEmptyMark"));
    mark->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(mark);
    emptyTitle_ = new QLabel(tr("Crea tu primer mapa"), empty);
    emptyTitle_->setObjectName(QStringLiteral("mapWorkspaceEmptyTitle"));
    emptyTitle_->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyTitle_);
    emptyCopy_ = new QLabel(tr("Crea el documento y abre directamente el generador nativo basado en Azgaar. El resultado queda editable: relieve, biomas, ríos, regiones, rutas, asentamientos y etiquetas."), empty);
    emptyCopy_->setObjectName(QStringLiteral("mapWorkspaceEmptyCopy"));
    emptyCopy_->setAlignment(Qt::AlignCenter);
    emptyCopy_->setWordWrap(true);
    emptyCopy_->setMaximumWidth(680);
    emptyLayout->addWidget(emptyCopy_, 0, Qt::AlignHCenter);
    emptyCreate_ = new QPushButton(tr("Crear mapa"), empty);
    emptyCreate_->setObjectName(QStringLiteral("mapWorkspacePrimary"));
    emptyCreate_->setCursor(Qt::PointingHandCursor);
    emptyLayout->addWidget(emptyCreate_, 0, Qt::AlignHCenter);
    emptyLayout->addStretch(1);
    canvasLayout->addWidget(empty, 1);
    root->addWidget(canvasHost, 1);

    connect(picker_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MapWorkspace::selectMap);
    connect(add, &QToolButton::clicked, this, &MapWorkspace::createMap);
    connect(duplicate, &QToolButton::clicked, this, &MapWorkspace::duplicateMap);
    connect(remove, &QToolButton::clicked, this, &MapWorkspace::removeMap);
    connect(metadata, &QToolButton::clicked, this, &MapWorkspace::editMetadata);
    connect(name_, &QLineEdit::editingFinished, this, &MapWorkspace::renameMap);
    connect(emptyCreate_, &QPushButton::clicked, this, &MapWorkspace::createMap);
    connect(editor_, &PilinReyEditor::mapEdited, this, &MapWorkspace::applyEditorMap);

    setStyleSheet(QStringLiteral(R"QSS(
#mapWorkspace{background:#0a0d11;}
#mapWorkspaceChrome{background:#0d1218;border-bottom:1px solid #242c35;}
#mapWorkspaceKicker{color:#ba925f;font-size:7.5pt;font-weight:700;letter-spacing:1.2px;padding:0 4px;}
#mapWorkspacePicker{background:#111820;color:#e6e0d7;border:1px solid #303a44;border-radius:7px;padding:5px 8px;}
#mapWorkspaceName{background:transparent;color:#f0ebe3;border:0;border-left:1px solid #2a333c;border-radius:0;padding:5px 10px;font-family:'Georgia';font-size:10.5pt;}
#mapWorkspaceChromeButton,#mapWorkspaceTextButton{background:transparent;color:#a7b1bb;border:0;border-radius:7px;padding:6px 8px;}
#mapWorkspaceChromeButton:hover,#mapWorkspaceTextButton:hover{background:#18212a;color:#ffffff;}
#mapWorkspaceHint{color:#697581;font-size:8pt;}
#mapWorkspaceCanvasHost{background:#090d12;}
#mapWorkspaceEmpty{background:#0b1016;border:0;}
#mapWorkspaceEmptyMark{color:#6f98b8;font-size:42pt;}
#mapWorkspaceEmptyTitle{color:#f0ebe3;font-family:'Georgia';font-size:22pt;font-weight:600;}
#mapWorkspaceEmptyCopy{color:#7e8994;font-size:9.5pt;}
#mapWorkspacePrimary{background:#2e78b3;color:#ffffff;border:0;border-radius:8px;padding:9px 18px;font-weight:700;}
#mapWorkspacePrimary:hover{background:#3788c8;}
)QSS"));

    editor_->hide();
    empty->show();
}

void MapWorkspace::setDocument(ArchiveDocument* document) {
    if (document_ == document) {
        refresh();
        return;
    }
    document_ = document;
    currentRow_ = -1;
    editor_->setArchiveDocument(document_);
    refresh();
}

void MapWorkspace::refresh() {
    rebuildPicker(currentRow_);
}

void MapWorkspace::rebuildPicker(int preferredRow) {
    refreshing_ = true;
    picker_->clear();
    if (!document_) {
        currentRow_ = -1;
        refreshing_ = false;
        updateEmptyState();
        return;
    }

    const QJsonArray maps = document_->array(QStringLiteral("maps"));
    for (const QJsonValue& value : maps) {
        const QJsonObject map = value.toObject();
        picker_->addItem(map.value(QStringLiteral("name")).toString(tr("Mapa sin nombre")), map.value(QStringLiteral("id")).toString());
    }

    if (maps.isEmpty()) currentRow_ = -1;
    else currentRow_ = qBound(0, preferredRow < 0 ? 0 : preferredRow, maps.size() - 1);
    picker_->setCurrentIndex(currentRow_);
    refreshing_ = false;
    selectMap(currentRow_);
}

void MapWorkspace::selectMap(int row) {
    if (refreshing_) return;
    currentRow_ = row;
    updateEmptyState();
    if (!document_ || row < 0) return;
    const QJsonArray maps = document_->array(QStringLiteral("maps"));
    if (row >= maps.size()) return;
    const QJsonObject map = maps.at(row).toObject();
    refreshing_ = true;
    name_->setText(map.value(QStringLiteral("name")).toString());
    refreshing_ = false;

    editor_->setMap(map);
    editor_->migrateLegacyMarkers();
    editor_->applyPersistentCanvasPreferences();
    applyTheme();

    // A new/blank map should never dump the user into the old empty blue
    // canvas. It opens the native Azgaar generation controls immediately.
    if (isBlankNativeTerrain(editor_->map())) {
        if (auto* generator = editor_->findChild<QFrame*>(QStringLiteral("pilinGeneratorPopover"))) {
            generator->show();
            generator->raise();
        }
    }
}

void MapWorkspace::createMap() {
    if (!document_) return;
    bool ok = false;
    QString name = QInputDialog::getText(this, tr("Nuevo mapa"), tr("Nombre:"), QLineEdit::Normal, tr("Mapa sin nombre"), &ok).trimmed();
    if (!ok) return;
    if (name.isEmpty()) name = tr("Mapa sin nombre");

    QJsonArray maps = document_->array(QStringLiteral("maps"));
    QJsonObject map = newMapObject(name);
    maps.append(map);
    document_->setArray(QStringLiteral("maps"), maps);
    currentRow_ = maps.size() - 1;
    rebuildPicker(currentRow_);

    // Prime the generator with the map document defaults. The user still
    // decides when to generate; nothing is flattened into a bitmap.
    editor_->setProperty("wbwGeneratorSeed", map.value(QStringLiteral("seed")).toInt());
    editor_->setProperty("wbwGeneratorContinents", map.value(QStringLiteral("continents")).toInt(3));
    editor_->setProperty("wbwGeneratorIslands", map.value(QStringLiteral("islands")).toInt(10));
    editor_->setProperty("wbwGeneratorRoughness", map.value(QStringLiteral("roughness")).toInt(58));
    editor_->setProperty("wbwGeneratorSeaLevel", 42);
    if (auto* generator = editor_->findChild<QFrame*>(QStringLiteral("pilinGeneratorPopover"))) {
        generator->show();
        generator->raise();
    }
    emit changed();
}

void MapWorkspace::duplicateMap() {
    if (!document_ || currentRow_ < 0) return;
    QJsonArray maps = document_->array(QStringLiteral("maps"));
    if (currentRow_ >= maps.size()) return;
    QJsonObject copy = maps.at(currentRow_).toObject();
    copy.insert(QStringLiteral("id"), uid(QStringLiteral("map")));
    copy.insert(QStringLiteral("name"), tr("%1 — copia").arg(copy.value(QStringLiteral("name")).toString(tr("Mapa"))));
    copy.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    maps.insert(currentRow_ + 1, copy);
    document_->setArray(QStringLiteral("maps"), maps);
    currentRow_ += 1;
    rebuildPicker(currentRow_);
    emit changed();
}

void MapWorkspace::removeMap() {
    if (!document_ || currentRow_ < 0) return;
    const QString currentName = picker_->currentText();
    if (QMessageBox::question(this, tr("Eliminar mapa"), tr("¿Eliminar “%1”? Esta acción no se puede deshacer desde la biblioteca.").arg(currentName), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    QJsonArray maps = document_->array(QStringLiteral("maps"));
    if (currentRow_ >= maps.size()) return;
    maps.removeAt(currentRow_);
    document_->setArray(QStringLiteral("maps"), maps);
    currentRow_ = maps.isEmpty() ? -1 : qMin(currentRow_, maps.size() - 1);
    rebuildPicker(currentRow_);
    emit changed();
}

void MapWorkspace::renameMap() {
    if (refreshing_ || !document_ || currentRow_ < 0) return;
    QJsonArray maps = document_->array(QStringLiteral("maps"));
    if (currentRow_ >= maps.size()) return;
    QJsonObject map = maps.at(currentRow_).toObject();
    const QString value = name_->text().trimmed().isEmpty() ? tr("Mapa sin nombre") : name_->text().trimmed();
    if (map.value(QStringLiteral("name")).toString() == value) return;
    map.insert(QStringLiteral("name"), value);
    map.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    maps.replace(currentRow_, map);
    document_->setArray(QStringLiteral("maps"), maps);
    picker_->setItemText(currentRow_, value);
    emit changed();
}

void MapWorkspace::editMetadata() {
    if (!document_ || currentRow_ < 0) return;
    QJsonArray maps = document_->array(QStringLiteral("maps"));
    if (currentRow_ >= maps.size()) return;
    QJsonObject map = maps.at(currentRow_).toObject();

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Detalles del mapa"));
    dialog.resize(520, 360);
    auto* root = new QVBoxLayout(&dialog);
    root->setContentsMargins(18, 18, 18, 18);
    root->setSpacing(12);
    auto* title = new QLabel(tr("Información cartográfica"), &dialog);
    title->setObjectName(QStringLiteral("mapMetadataTitle"));
    root->addWidget(title);

    auto* form = new QFormLayout;
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(10);
    auto* name = new QLineEdit(map.value(QStringLiteral("name")).toString(), &dialog);
    auto* description = new QTextEdit(&dialog);
    description->setPlainText(map.value(QStringLiteral("description")).toString());
    description->setMinimumHeight(130);
    auto* style = new QComboBox(&dialog);
    style->addItems({tr("Atlas"), tr("Pergamino"), tr("Nocturno")});
    const int styleIndex = style->findText(map.value(QStringLiteral("style")).toString());
    if (styleIndex >= 0) style->setCurrentIndex(styleIndex);
    form->addRow(tr("Nombre"), name);
    form->addRow(tr("Estilo"), style);
    form->addRow(tr("Descripción"), description);
    root->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.setStyleSheet(QStringLiteral("QDialog{background:#0d1218;color:#dfe6ed;}#mapMetadataTitle{font-family:'Georgia';font-size:18pt;color:#f0ebe3;}QLineEdit,QTextEdit,QComboBox{background:#101820;color:#e3e8ed;border:1px solid #303b46;border-radius:7px;padding:7px;}QPushButton{background:#17212b;color:#dce4eb;border:1px solid #34414d;border-radius:7px;padding:8px 12px;}"));
    if (dialog.exec() != QDialog::Accepted) return;

    map.insert(QStringLiteral("name"), name->text().trimmed().isEmpty() ? tr("Mapa sin nombre") : name->text().trimmed());
    map.insert(QStringLiteral("description"), description->toPlainText());
    map.insert(QStringLiteral("style"), style->currentText());
    map.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    maps.replace(currentRow_, map);
    document_->setArray(QStringLiteral("maps"), maps);
    rebuildPicker(currentRow_);
    emit changed();
}

void MapWorkspace::applyEditorMap(const QJsonObject& editedMap) {
    if (refreshing_ || !document_ || currentRow_ < 0) return;
    QJsonArray maps = document_->array(QStringLiteral("maps"));
    if (currentRow_ >= maps.size()) return;
    QJsonObject current = maps.at(currentRow_).toObject();
    const QString stableId = current.value(QStringLiteral("id")).toString();
    const QString stableName = current.value(QStringLiteral("name")).toString();
    const QString description = current.value(QStringLiteral("description")).toString();
    const QString style = current.value(QStringLiteral("style")).toString();
    current = editedMap;
    current.insert(QStringLiteral("id"), stableId);
    current.insert(QStringLiteral("name"), stableName);
    current.insert(QStringLiteral("description"), description);
    current.insert(QStringLiteral("style"), style);
    current.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    maps.replace(currentRow_, current);
    document_->setArray(QStringLiteral("maps"), maps);
    emit changed();
}

void MapWorkspace::updateEmptyState() {
    const bool hasMap = document_ && currentRow_ >= 0 && currentRow_ < document_->array(QStringLiteral("maps")).size();
    editor_->setVisible(hasMap);
    QWidget* empty = findChild<QWidget*>(QStringLiteral("mapWorkspaceEmpty"));
    if (empty) empty->setVisible(!hasMap);
    chrome_->setVisible(hasMap);
}

} // namespace wbw
