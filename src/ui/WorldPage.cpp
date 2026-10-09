#include "ui/WorldPage.h"

#include "core/ArchiveDocument.h"
#include "ui/PilinReyEditor.h"

#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <QUuid>
#include <QVBoxLayout>

namespace wbw {
namespace {

QString uid(const QString& prefix) {
    return prefix + QLatin1Char('-') + QUuid::createUuid().toString(QUuid::Id128);
}

QPushButton* makeButton(const QString& text) {
    auto* button = new QPushButton(text);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

QTextEdit* shortText() {
    auto* edit = new QTextEdit;
    edit->setMaximumHeight(88);
    return edit;
}

QWidget* fieldBlock(const QString& title, QWidget* field) {
    auto* box = new QWidget;
    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);
    auto* label = new QLabel(title);
    label->setObjectName(QStringLiteral("fieldTitle"));
    layout->addWidget(label);
    layout->addWidget(field);
    return box;
}

QString jsonStrings(const QJsonArray& array) {
    QStringList result;
    for (const QJsonValue value : array) if (!value.toString().trimmed().isEmpty()) result.append(value.toString().trimmed());
    return result.join(QStringLiteral(", "));
}

QJsonArray stringsJson(const QString& value) {
    QJsonArray result;
    for (QString item : value.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        item = item.trimmed();
        if (!item.isEmpty()) result.append(item);
    }
    return result;
}

QString evidenceText(const QJsonArray& array) {
    QStringList lines;
    for (const QJsonValue value : array) {
        const QJsonObject item = value.toObject();
        const QString chapter = item.value(QStringLiteral("chapter")).toString();
        const QString body = item.value(QStringLiteral("text")).toString();
        lines.append(chapter.isEmpty() ? body : chapter + QStringLiteral(" | ") + body);
    }
    return lines.join(QLatin1Char('\n'));
}

QJsonArray textEvidence(const QString& value) {
    QJsonArray result;
    for (QString line : value.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        line = line.trimmed();
        if (line.isEmpty()) continue;
        const qsizetype separator = line.indexOf(QLatin1Char('|'));
        result.append(QJsonObject{
            {QStringLiteral("chapter"), separator < 0 ? QString() : line.left(separator).trimmed()},
            {QStringLiteral("text"), separator < 0 ? line : line.mid(separator + 1).trimmed()}
        });
    }
    return result;
}

void setCombo(QComboBox* combo, const QString& value) {
    const int index = combo->findText(value);
    if (index >= 0) combo->setCurrentIndex(index);
    else if (!value.isEmpty()) {
        combo->addItem(value);
        combo->setCurrentText(value);
    }
}

QString fileDataUrl(const QString& path, QByteArray* bytesOut = nullptr, QString* mimeOut = nullptr) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const QByteArray bytes = file.readAll();
    const QString mime = QMimeDatabase().mimeTypeForFile(path, QMimeDatabase::MatchContent).name();
    if (bytesOut) *bytesOut = bytes;
    if (mimeOut) *mimeOut = mime;
    return QStringLiteral("data:%1;base64,%2").arg(mime.isEmpty() ? QStringLiteral("application/octet-stream") : mime, QString::fromLatin1(bytes.toBase64()));
}

QByteArray dataUrlBytes(const QString& value) {
    if (!value.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive)) return {};
    const qsizetype comma = value.indexOf(QLatin1Char(','));
    if (comma < 0) return {};
    const QString meta = value.mid(5, comma - 5);
    const QByteArray payload = value.mid(comma + 1).toLatin1();
    return meta.contains(QStringLiteral(";base64"), Qt::CaseInsensitive) ? QByteArray::fromBase64(payload) : QByteArray::fromPercentEncoding(payload);
}

QString attachmentKind(const QString& mime, const QString& name) {
    if (mime.startsWith(QStringLiteral("image/"))) return QStringLiteral("image");
    if (mime.startsWith(QStringLiteral("text/")) || name.endsWith(QStringLiteral(".txt"), Qt::CaseInsensitive) || name.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive)) return QStringLiteral("text");
    return QStringLiteral("document");
}

} // namespace

WorldPage::WorldPage(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("worldPage"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    tabs_ = new QTabWidget;
    tabs_->setObjectName(QStringLiteral("worldTabs"));
    tabs_->addTab(buildAtlasTab(), tr("Atlas"));
    tabs_->addTab(buildMagicTab(), tr("Magia"));
    tabs_->addTab(buildMapsTab(), tr("Mapas"));
    tabs_->addTab(buildTextsTab(), tr("Textos de referencia"));
    root->addWidget(tabs_);

    setStyleSheet(QStringLiteral(
        "#worldPage{background:#edf1f5;}"
        "#worldTabs::pane{border:0;background:#edf1f5;}"
        "#atlasTab,#magicTab,#mapsTab{background:#edf1f5;}"
        "#worldIndexPanel,#worldEditorCard,#magicIndexPanel,#magicEditorCard,#mapIndexPanel,#mapEditorCard,#legacyMarkers{background:#ffffff;border:1px solid #d5dce5;}"
        "#pageKicker,#fieldTitle,#panelTitle{color:#667085;font-size:8pt;font-weight:700;letter-spacing:.6px;}"
        "#pageTitle{font-family:'Georgia';font-size:24pt;color:#344054;}"
        "#pageDescription{font-family:'Georgia';color:#667085;}"
        "#generatorBar{background:#f7f9fc;border:1px solid #d8dee7;}"
        "QTextEdit{min-height:62px;}"
    ));
}

void WorldPage::setDocument(ArchiveDocument* document) {
    document_ = document;
    refresh();
}

QWidget* WorldPage::buildAtlasTab() {
    auto* tab = new QWidget;
    tab->setObjectName(QStringLiteral("atlasTab"));
    auto* root = new QVBoxLayout(tab);
    root->setContentsMargins(30, 22, 30, 30);
    root->setSpacing(16);

    auto* heading = new QVBoxLayout;
    auto* kicker = new QLabel(tr("PAÍSES / REGIONES / CULTURAS / LUGARES"));
    kicker->setObjectName(QStringLiteral("pageKicker"));
    auto* title = new QLabel(tr("Atlas"));
    title->setObjectName(QStringLiteral("pageTitle"));
    auto* description = new QLabel(tr("Organiza el mundo por entradas relacionadas, sin convertir cada ficha en un formulario interminable."));
    description->setObjectName(QStringLiteral("pageDescription"));
    description->setWordWrap(true);
    heading->addWidget(kicker);
    heading->addWidget(title);
    heading->addWidget(description);
    root->addLayout(heading);

    auto* split = new QSplitter;
    split->setChildrenCollapsible(false);
    split->setHandleWidth(8);

    auto* left = new QWidget;
    left->setObjectName(QStringLiteral("worldIndexPanel"));
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(12, 12, 12, 12);
    auto* leftTitle = new QLabel(tr("ENTRADAS"));
    leftTitle->setObjectName(QStringLiteral("panelTitle"));
    leftLayout->addWidget(leftTitle);
    worldList_ = new QListWidget;
    worldList_->setMinimumWidth(235);
    worldList_->setMaximumWidth(340);
    leftLayout->addWidget(worldList_, 1);
    auto* listActions = new QHBoxLayout;
    auto* add = makeButton(tr("+ Entrada"));
    auto* remove = makeButton(tr("Eliminar"));
    listActions->addWidget(add, 1);
    listActions->addWidget(remove);
    leftLayout->addLayout(listActions);
    split->addWidget(left);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* editor = new QWidget;
    editor->setObjectName(QStringLiteral("worldEditorCard"));
    auto* editorLayout = new QVBoxLayout(editor);
    editorLayout->setContentsMargins(18, 16, 18, 20);
    editorLayout->setSpacing(12);
    auto* editorTitle = new QLabel(tr("FICHA DE MUNDO"));
    editorTitle->setObjectName(QStringLiteral("panelTitle"));
    editorLayout->addWidget(editorTitle);

    worldKind_ = new QComboBox;
    worldKind_->addItems({tr("País/Reino"), tr("Región"), tr("Ciudad/Lugar"), tr("Pueblo/Cultura"), tr("Moneda"), tr("Idioma"), tr("Religión"), tr("Organización"), tr("Objeto/Artefacto"), tr("Cosmología"), tr("Concepto"), tr("Otro")});
    worldName_ = new QLineEdit;
    worldAliases_ = new QLineEdit;
    worldAliases_->setPlaceholderText(tr("Separados por comas"));
    worldSummary_ = shortText();
    worldGeography_ = shortText();
    worldGovernment_ = shortText();
    worldPeoples_ = shortText();
    worldCulture_ = shortText();
    worldEconomy_ = shortText();
    worldCurrency_ = shortText();
    worldLanguages_ = shortText();
    worldReligions_ = shortText();
    worldMilitary_ = shortText();
    worldHistory_ = shortText();
    worldRelations_ = shortText();
    worldLocations_ = shortText();
    worldConflicts_ = shortText();
    worldNotes_ = shortText();
    worldTags_ = new QLineEdit;
    worldTags_->setPlaceholderText(tr("Separadas por comas"));
    worldAttachments_ = new QListWidget;
    worldAttachments_->setMaximumHeight(110);

    auto* identity = new QGridLayout;
    identity->setHorizontalSpacing(10);
    identity->setVerticalSpacing(10);
    identity->addWidget(fieldBlock(tr("Tipo"), worldKind_), 0, 0);
    identity->addWidget(fieldBlock(tr("Nombre"), worldName_), 0, 1);
    identity->addWidget(fieldBlock(tr("Alias"), worldAliases_), 0, 2);
    identity->addWidget(fieldBlock(tr("Resumen"), worldSummary_), 1, 0, 1, 3);
    editorLayout->addLayout(identity);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);
    grid->addWidget(fieldBlock(tr("Geografía"), worldGeography_), 0, 0);
    grid->addWidget(fieldBlock(tr("Gobierno"), worldGovernment_), 0, 1);
    grid->addWidget(fieldBlock(tr("Pueblos"), worldPeoples_), 1, 0);
    grid->addWidget(fieldBlock(tr("Cultura"), worldCulture_), 1, 1);
    grid->addWidget(fieldBlock(tr("Economía"), worldEconomy_), 2, 0);
    grid->addWidget(fieldBlock(tr("Moneda"), worldCurrency_), 2, 1);
    grid->addWidget(fieldBlock(tr("Idiomas"), worldLanguages_), 3, 0);
    grid->addWidget(fieldBlock(tr("Religiones"), worldReligions_), 3, 1);
    grid->addWidget(fieldBlock(tr("Fuerza militar"), worldMilitary_), 4, 0);
    grid->addWidget(fieldBlock(tr("Historia"), worldHistory_), 4, 1);
    grid->addWidget(fieldBlock(tr("Relaciones"), worldRelations_), 5, 0);
    grid->addWidget(fieldBlock(tr("Localizaciones"), worldLocations_), 5, 1);
    grid->addWidget(fieldBlock(tr("Conflictos"), worldConflicts_), 6, 0);
    grid->addWidget(fieldBlock(tr("Notas"), worldNotes_), 6, 1);
    editorLayout->addLayout(grid);
    editorLayout->addWidget(fieldBlock(tr("Etiquetas"), worldTags_));
    editorLayout->addWidget(fieldBlock(tr("Adjuntos"), worldAttachments_));

    auto* attachmentActions = new QHBoxLayout;
    auto* attach = makeButton(tr("Añadir adjunto…"));
    auto* detach = makeButton(tr("Quitar"));
    auto* exportAttachment = makeButton(tr("Exportar…"));
    attachmentActions->addWidget(attach);
    attachmentActions->addWidget(detach);
    attachmentActions->addWidget(exportAttachment);
    attachmentActions->addStretch();
    editorLayout->addLayout(attachmentActions);
    editorLayout->addStretch();
    scroll->setWidget(editor);
    split->addWidget(scroll);
    split->setStretchFactor(1, 1);
    split->setSizes({270, 1050});
    root->addWidget(split, 1);

    connect(worldList_, &QListWidget::currentRowChanged, this, &WorldPage::selectWorld);
    connect(add, &QPushButton::clicked, this, &WorldPage::addWorld);
    connect(remove, &QPushButton::clicked, this, &WorldPage::removeWorld);
    connect(attach, &QPushButton::clicked, this, &WorldPage::addWorldAttachment);
    connect(detach, &QPushButton::clicked, this, &WorldPage::removeWorldAttachment);
    connect(exportAttachment, &QPushButton::clicked, this, &WorldPage::exportWorldAttachment);
    connect(worldKind_, &QComboBox::currentTextChanged, this, &WorldPage::applyWorld);
    connect(worldName_, &QLineEdit::editingFinished, this, &WorldPage::applyWorld);
    connect(worldAliases_, &QLineEdit::editingFinished, this, &WorldPage::applyWorld);
    connect(worldTags_, &QLineEdit::editingFinished, this, &WorldPage::applyWorld);
    for (QTextEdit* field : {worldSummary_, worldGeography_, worldGovernment_, worldPeoples_, worldCulture_, worldEconomy_, worldCurrency_, worldLanguages_, worldReligions_, worldMilitary_, worldHistory_, worldRelations_, worldLocations_, worldConflicts_, worldNotes_})
        connect(field, &QTextEdit::textChanged, this, &WorldPage::applyWorld);
    return tab;
}

QWidget* WorldPage::buildMagicTab() {
    auto* tab = new QWidget;
    tab->setObjectName(QStringLiteral("magicTab"));
    auto* root = new QVBoxLayout(tab);
    root->setContentsMargins(30, 22, 30, 30);
    root->setSpacing(16);

    auto* heading = new QVBoxLayout;
    auto* kicker = new QLabel(tr("FUENTE / ACCESO / COSTE / LÍMITES"));
    kicker->setObjectName(QStringLiteral("pageKicker"));
    auto* title = new QLabel(tr("Sistemas de magia"));
    title->setObjectName(QStringLiteral("pageTitle"));
    heading->addWidget(kicker);
    heading->addWidget(title);
    root->addLayout(heading);

    auto* split = new QSplitter;
    split->setChildrenCollapsible(false);
    split->setHandleWidth(8);
    auto* left = new QWidget;
    left->setObjectName(QStringLiteral("magicIndexPanel"));
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(12, 12, 12, 12);
    auto* leftTitle = new QLabel(tr("SISTEMAS"));
    leftTitle->setObjectName(QStringLiteral("panelTitle"));
    leftLayout->addWidget(leftTitle);
    magicList_ = new QListWidget;
    magicList_->setMinimumWidth(235);
    magicList_->setMaximumWidth(340);
    leftLayout->addWidget(magicList_, 1);
    auto* listActions = new QHBoxLayout;
    auto* add = makeButton(tr("+ Sistema"));
    auto* remove = makeButton(tr("Eliminar"));
    listActions->addWidget(add, 1);
    listActions->addWidget(remove);
    leftLayout->addLayout(listActions);
    split->addWidget(left);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* editor = new QWidget;
    editor->setObjectName(QStringLiteral("magicEditorCard"));
    auto* editorLayout = new QVBoxLayout(editor);
    editorLayout->setContentsMargins(18, 16, 18, 20);
    editorLayout->setSpacing(12);
    auto* editorTitle = new QLabel(tr("FICHA DEL SISTEMA"));
    editorTitle->setObjectName(QStringLiteral("panelTitle"));
    editorLayout->addWidget(editorTitle);

    magicName_ = new QLineEdit;
    magicCategory_ = new QLineEdit;
    magicStatus_ = new QComboBox;
    magicStatus_->addItems({tr("Canónico"), tr("En desarrollo"), tr("Secreto"), tr("Descartado")});
    magicSource_ = shortText();
    magicPrinciple_ = shortText();
    magicAccess_ = shortText();
    magicCost_ = shortText();
    magicLimits_ = shortText();
    magicManifestations_ = shortText();
    magicMaterials_ = shortText();
    magicInstitutions_ = shortText();
    magicUsers_ = shortText();
    magicRisks_ = shortText();
    magicHistory_ = shortText();
    magicNotes_ = shortText();
    magicEvidence_ = shortText();
    magicEvidence_->setPlaceholderText(tr("Capítulo | evidencia, una por línea"));
    magicTags_ = new QLineEdit;
    magicTags_->setPlaceholderText(tr("Separadas por comas"));
    magicAttachments_ = new QListWidget;
    magicAttachments_->setMaximumHeight(110);

    auto* identity = new QGridLayout;
    identity->setHorizontalSpacing(10);
    identity->addWidget(fieldBlock(tr("Nombre"), magicName_), 0, 0);
    identity->addWidget(fieldBlock(tr("Categoría"), magicCategory_), 0, 1);
    identity->addWidget(fieldBlock(tr("Estado"), magicStatus_), 0, 2);
    editorLayout->addLayout(identity);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);
    grid->addWidget(fieldBlock(tr("Fuente"), magicSource_), 0, 0);
    grid->addWidget(fieldBlock(tr("Principio"), magicPrinciple_), 0, 1);
    grid->addWidget(fieldBlock(tr("Acceso"), magicAccess_), 1, 0);
    grid->addWidget(fieldBlock(tr("Coste"), magicCost_), 1, 1);
    grid->addWidget(fieldBlock(tr("Límites"), magicLimits_), 2, 0);
    grid->addWidget(fieldBlock(tr("Manifestaciones"), magicManifestations_), 2, 1);
    grid->addWidget(fieldBlock(tr("Materiales"), magicMaterials_), 3, 0);
    grid->addWidget(fieldBlock(tr("Instituciones"), magicInstitutions_), 3, 1);
    grid->addWidget(fieldBlock(tr("Usuarios"), magicUsers_), 4, 0);
    grid->addWidget(fieldBlock(tr("Riesgos"), magicRisks_), 4, 1);
    grid->addWidget(fieldBlock(tr("Historia"), magicHistory_), 5, 0);
    grid->addWidget(fieldBlock(tr("Notas"), magicNotes_), 5, 1);
    grid->addWidget(fieldBlock(tr("Evidencias"), magicEvidence_), 6, 0, 1, 2);
    editorLayout->addLayout(grid);
    editorLayout->addWidget(fieldBlock(tr("Etiquetas"), magicTags_));
    editorLayout->addWidget(fieldBlock(tr("Adjuntos"), magicAttachments_));
    auto* attachmentActions = new QHBoxLayout;
    auto* attach = makeButton(tr("Añadir adjunto…"));
    auto* detach = makeButton(tr("Quitar"));
    auto* exportAttachment = makeButton(tr("Exportar…"));
    attachmentActions->addWidget(attach);
    attachmentActions->addWidget(detach);
    attachmentActions->addWidget(exportAttachment);
    attachmentActions->addStretch();
    editorLayout->addLayout(attachmentActions);
    editorLayout->addStretch();
    scroll->setWidget(editor);
    split->addWidget(scroll);
    split->setStretchFactor(1, 1);
    split->setSizes({270, 1050});
    root->addWidget(split, 1);

    connect(magicList_, &QListWidget::currentRowChanged, this, &WorldPage::selectMagic);
    connect(add, &QPushButton::clicked, this, &WorldPage::addMagic);
    connect(remove, &QPushButton::clicked, this, &WorldPage::removeMagic);
    connect(attach, &QPushButton::clicked, this, &WorldPage::addMagicAttachment);
    connect(detach, &QPushButton::clicked, this, &WorldPage::removeMagicAttachment);
    connect(exportAttachment, &QPushButton::clicked, this, &WorldPage::exportMagicAttachment);
    connect(magicName_, &QLineEdit::editingFinished, this, &WorldPage::applyMagic);
    connect(magicCategory_, &QLineEdit::editingFinished, this, &WorldPage::applyMagic);
    connect(magicTags_, &QLineEdit::editingFinished, this, &WorldPage::applyMagic);
    connect(magicStatus_, &QComboBox::currentTextChanged, this, &WorldPage::applyMagic);
    for (QTextEdit* field : {magicSource_, magicPrinciple_, magicAccess_, magicCost_, magicLimits_, magicManifestations_, magicMaterials_, magicInstitutions_, magicUsers_, magicRisks_, magicHistory_, magicNotes_, magicEvidence_})
        connect(field, &QTextEdit::textChanged, this, &WorldPage::applyMagic);
    return tab;
}

QWidget* WorldPage::buildMapsTab() {
    auto* tab = new QWidget;
    tab->setObjectName(QStringLiteral("mapsTab"));
    auto* root = new QVBoxLayout(tab);
    root->setContentsMargins(30, 22, 30, 30);
    root->setSpacing(12);

    auto* split = new QSplitter;
    split->setChildrenCollapsible(false);
    split->setHandleWidth(8);
    auto* left = new QWidget;
    left->setObjectName(QStringLiteral("mapIndexPanel"));
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(12, 12, 12, 12);
    auto* leftTitle = new QLabel(tr("MAPAS"));
    leftTitle->setObjectName(QStringLiteral("panelTitle"));
    leftLayout->addWidget(leftTitle);
    mapList_ = new QListWidget;
    mapList_->setMinimumWidth(210);
    mapList_->setMaximumWidth(300);
    leftLayout->addWidget(mapList_, 1);
    auto* mapActions = new QHBoxLayout;
    auto* add = makeButton(tr("+ Mapa"));
    auto* remove = makeButton(tr("Eliminar"));
    mapActions->addWidget(add, 1);
    mapActions->addWidget(remove);
    leftLayout->addLayout(mapActions);
    split->addWidget(left);

    auto* center = new QWidget;
    center->setObjectName(QStringLiteral("mapEditorCard"));
    auto* centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(14, 12, 14, 14);
    centerLayout->setSpacing(9);
    auto* titleRow = new QHBoxLayout;
    mapName_ = new QLineEdit;
    mapName_->setPlaceholderText(tr("Nombre del mapa"));
    auto* chooseBackground = makeButton(tr("Cargar plantilla…"));
    auto* clearBackground = makeButton(tr("Quitar plantilla"));
    titleRow->addWidget(mapName_, 1);
    titleRow->addWidget(chooseBackground);
    titleRow->addWidget(clearBackground);
    centerLayout->addLayout(titleRow);
    mapDescription_ = new QTextEdit;
    mapDescription_->setMaximumHeight(58);
    mapDescription_->setPlaceholderText(tr("Descripción del mapa"));
    centerLayout->addWidget(mapDescription_);

    auto* generator = new QFrame;
    generator->setObjectName(QStringLiteral("generatorBar"));
    auto* generatorLayout = new QHBoxLayout(generator);
    generatorLayout->setContentsMargins(9, 7, 9, 7);
    generatorLayout->addWidget(new QLabel(tr("Generador auxiliar")));
    mapStyle_ = new QComboBox;
    mapStyle_->addItems({tr("Pergamino"), tr("Atlas"), tr("Nocturno")});
    mapSeed_ = new QSpinBox;
    mapSeed_->setRange(0, 2147483647);
    mapContinents_ = new QSpinBox;
    mapContinents_->setRange(1, 12);
    mapIslands_ = new QSpinBox;
    mapIslands_->setRange(0, 80);
    mapRoughness_ = new QSpinBox;
    mapRoughness_->setRange(1, 10);
    generatorLayout->addWidget(mapStyle_);
    generatorLayout->addWidget(new QLabel(tr("Semilla")));
    generatorLayout->addWidget(mapSeed_);
    generatorLayout->addWidget(new QLabel(tr("Continentes")));
    generatorLayout->addWidget(mapContinents_);
    generatorLayout->addWidget(new QLabel(tr("Islas")));
    generatorLayout->addWidget(mapIslands_);
    generatorLayout->addWidget(new QLabel(tr("Rugosidad")));
    generatorLayout->addWidget(mapRoughness_);
    generatorLayout->addStretch();
    centerLayout->addWidget(generator);

    mapCanvas_ = new PilinReyEditor;
    centerLayout->addWidget(mapCanvas_, 1);
    split->addWidget(center);

    auto* right = new QWidget;
    right->setObjectName(QStringLiteral("legacyMarkers"));
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(10, 10, 10, 10);
    auto* markerTitle = new QLabel(tr("MARCADORES HEREDADOS"));
    markerTitle->setObjectName(QStringLiteral("panelTitle"));
    rightLayout->addWidget(markerTitle);
    mapMarkers_ = new QListWidget;
    mapMarkers_->setMinimumWidth(180);
    mapMarkers_->setMaximumWidth(250);
    rightLayout->addWidget(mapMarkers_, 1);
    auto* removeMarker = makeButton(tr("Eliminar marcador"));
    rightLayout->addWidget(removeMarker);
    auto* hint = new QLabel(tr("Se conservan para migración. Los asentamientos nuevos se crean directamente en Pilín Rey."));
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#667085;"));
    rightLayout->addWidget(hint);
    split->addWidget(right);
    split->setStretchFactor(1, 1);
    split->setSizes({230, 1000, 210});
    root->addWidget(split, 1);

    connect(mapList_, &QListWidget::currentRowChanged, this, &WorldPage::selectMap);
    connect(add, &QPushButton::clicked, this, &WorldPage::addMap);
    connect(remove, &QPushButton::clicked, this, &WorldPage::removeMap);
    connect(removeMarker, &QPushButton::clicked, this, &WorldPage::removeMapMarker);
    connect(chooseBackground, &QPushButton::clicked, this, &WorldPage::chooseMapBackground);
    connect(clearBackground, &QPushButton::clicked, this, &WorldPage::clearMapBackground);
    connect(mapName_, &QLineEdit::editingFinished, this, &WorldPage::applyMap);
    connect(mapDescription_, &QTextEdit::textChanged, this, &WorldPage::applyMap);
    connect(mapSeed_, &QSpinBox::valueChanged, this, &WorldPage::applyMap);
    connect(mapStyle_, &QComboBox::currentTextChanged, this, &WorldPage::applyMap);
    connect(mapContinents_, &QSpinBox::valueChanged, this, &WorldPage::applyMap);
    connect(mapIslands_, &QSpinBox::valueChanged, this, &WorldPage::applyMap);
    connect(mapRoughness_, &QSpinBox::valueChanged, this, &WorldPage::applyMap);
    connect(mapCanvas_, &PilinReyEditor::addMarkerRequested, this, &WorldPage::addMapMarker);
    connect(mapCanvas_, &PilinReyEditor::markerMoved, this, &WorldPage::moveMapMarker);
    return tab;
}

QWidget* WorldPage::buildTextsTab() {
    auto* outer = new QTabWidget;
    auto build = [this](bool magic) {
        auto* tab = new QWidget;
        auto* layout = new QVBoxLayout(tab);
        layout->setContentsMargins(30, 22, 30, 30);
        auto* split = new QSplitter;
        auto* list = new QListWidget;
        split->addWidget(list);
        auto* editor = new QWidget;
        auto* form = new QFormLayout(editor);
        auto* title = new QLineEdit;
        auto* source = new QLineEdit;
        auto* content = new QTextEdit;
        form->addRow(tr("Título"), title);
        form->addRow(tr("Origen"), source);
        form->addRow(tr("Contenido"), content);
        auto* actions = new QHBoxLayout;
        auto* import = makeButton(tr("Importar TXT/MD…"));
        auto* add = makeButton(tr("+ Texto"));
        auto* remove = makeButton(tr("Eliminar"));
        actions->addWidget(import);
        actions->addWidget(add);
        actions->addWidget(remove);
        actions->addStretch();
        form->addRow(actions);
        split->addWidget(editor);
        split->setStretchFactor(1, 1);
        layout->addWidget(split);
        if (magic) {
            magicTextList_ = list;
            magicTextTitle_ = title;
            magicTextSource_ = source;
            magicTextContent_ = content;
        } else {
            worldTextList_ = list;
            worldTextTitle_ = title;
            worldTextSource_ = source;
            worldTextContent_ = content;
        }
        connect(list, &QListWidget::currentRowChanged, this, [this, magic](int row) { selectText(magic, row); });
        connect(title, &QLineEdit::editingFinished, this, [this, magic]() { applyText(magic); });
        connect(source, &QLineEdit::editingFinished, this, [this, magic]() { applyText(magic); });
        connect(content, &QTextEdit::textChanged, this, [this, magic]() { applyText(magic); });
        connect(import, &QPushButton::clicked, this, [this, magic]() { importText(magic); });
        connect(add, &QPushButton::clicked, this, [this, magic]() { addText(magic); });
        connect(remove, &QPushButton::clicked, this, [this, magic]() { removeText(magic); });
        return tab;
    };
    outer->addTab(build(false), tr("Mundo"));
    outer->addTab(build(true), tr("Magia"));
    return outer;
}

void WorldPage::refresh() {
    if (!document_) return;
    refreshing_ = true;
    refreshAtlas();
    refreshMagic();
    refreshMaps();
    refreshTexts();
    refreshing_ = false;
    if (worldList_->count()) worldList_->setCurrentRow(0); else selectWorld(-1);
    if (magicList_->count()) magicList_->setCurrentRow(0); else selectMagic(-1);
    if (mapList_->count()) mapList_->setCurrentRow(0); else selectMap(-1);
    if (worldTextList_->count()) worldTextList_->setCurrentRow(0); else selectText(false, -1);
    if (magicTextList_->count()) magicTextList_->setCurrentRow(0); else selectText(true, -1);
}

void WorldPage::refreshAtlas() {
    worldList_->clear();
    for (const QJsonValue value : document_->array(QStringLiteral("world"))) {
        const QJsonObject object = value.toObject();
        worldList_->addItem(QStringLiteral("%1 · %2").arg(object.value(QStringLiteral("kind")).toString(), object.value(QStringLiteral("name")).toString(tr("Sin nombre"))));
    }
}

void WorldPage::selectWorld(int row) {
    if (!document_) return;
    refreshing_ = true;
    const QJsonArray array = document_->array(QStringLiteral("world"));
    const QJsonObject object = row >= 0 && row < array.size() ? array.at(row).toObject() : QJsonObject();
    setCombo(worldKind_, object.value(QStringLiteral("kind")).toString());
    worldName_->setText(object.value(QStringLiteral("name")).toString());
    worldAliases_->setText(jsonStrings(object.value(QStringLiteral("aliases")).toArray()));
    worldSummary_->setPlainText(object.value(QStringLiteral("summary")).toString());
    worldGeography_->setPlainText(object.value(QStringLiteral("geography")).toString());
    worldGovernment_->setPlainText(object.value(QStringLiteral("government")).toString());
    worldPeoples_->setPlainText(object.value(QStringLiteral("peoples")).toString());
    worldCulture_->setPlainText(object.value(QStringLiteral("culture")).toString());
    worldEconomy_->setPlainText(object.value(QStringLiteral("economy")).toString());
    worldCurrency_->setPlainText(object.value(QStringLiteral("currency")).toString());
    worldLanguages_->setPlainText(object.value(QStringLiteral("languages")).toString());
    worldReligions_->setPlainText(object.value(QStringLiteral("religions")).toString());
    worldMilitary_->setPlainText(object.value(QStringLiteral("military")).toString());
    worldHistory_->setPlainText(object.value(QStringLiteral("history")).toString());
    worldRelations_->setPlainText(object.value(QStringLiteral("relations")).toString());
    worldLocations_->setPlainText(object.value(QStringLiteral("locations")).toString());
    worldConflicts_->setPlainText(object.value(QStringLiteral("conflicts")).toString());
    worldNotes_->setPlainText(object.value(QStringLiteral("notes")).toString());
    worldTags_->setText(jsonStrings(object.value(QStringLiteral("tags")).toArray()));
    worldAttachments_->clear();
    for (const QJsonValue value : object.value(QStringLiteral("attachments")).toArray()) {
        auto* item = new QListWidgetItem(value.toObject().value(QStringLiteral("name")).toString());
        item->setData(Qt::UserRole, value.toObject().value(QStringLiteral("id")).toString());
        worldAttachments_->addItem(item);
    }
    refreshing_ = false;
}

void WorldPage::applyWorld() {
    if (refreshing_ || !document_) return;
    const int row = worldList_->currentRow();
    QJsonArray array = document_->array(QStringLiteral("world"));
    if (row < 0 || row >= array.size()) return;
    QJsonObject object = array.at(row).toObject();
    object.insert(QStringLiteral("kind"), worldKind_->currentText());
    object.insert(QStringLiteral("name"), worldName_->text());
    object.insert(QStringLiteral("aliases"), stringsJson(worldAliases_->text()));
    object.insert(QStringLiteral("summary"), worldSummary_->toPlainText());
    object.insert(QStringLiteral("geography"), worldGeography_->toPlainText());
    object.insert(QStringLiteral("government"), worldGovernment_->toPlainText());
    object.insert(QStringLiteral("peoples"), worldPeoples_->toPlainText());
    object.insert(QStringLiteral("culture"), worldCulture_->toPlainText());
    object.insert(QStringLiteral("economy"), worldEconomy_->toPlainText());
    object.insert(QStringLiteral("currency"), worldCurrency_->toPlainText());
    object.insert(QStringLiteral("languages"), worldLanguages_->toPlainText());
    object.insert(QStringLiteral("religions"), worldReligions_->toPlainText());
    object.insert(QStringLiteral("military"), worldMilitary_->toPlainText());
    object.insert(QStringLiteral("history"), worldHistory_->toPlainText());
    object.insert(QStringLiteral("relations"), worldRelations_->toPlainText());
    object.insert(QStringLiteral("locations"), worldLocations_->toPlainText());
    object.insert(QStringLiteral("conflicts"), worldConflicts_->toPlainText());
    object.insert(QStringLiteral("notes"), worldNotes_->toPlainText());
    object.insert(QStringLiteral("tags"), stringsJson(worldTags_->text()));
    array.replace(row, object);
    document_->setArray(QStringLiteral("world"), array);
    worldList_->item(row)->setText(QStringLiteral("%1 · %2").arg(worldKind_->currentText(), worldName_->text().isEmpty() ? tr("Sin nombre") : worldName_->text()));
    emit changed();
}

void WorldPage::addWorld() {
    if (!document_) return;
    QJsonArray array = document_->array(QStringLiteral("world"));
    array.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("world"))}, {QStringLiteral("kind"), tr("Otro")}, {QStringLiteral("name"), tr("Entrada sin nombre")}, {QStringLiteral("aliases"), QJsonArray()}, {QStringLiteral("summary"), QString()}, {QStringLiteral("geography"), QString()}, {QStringLiteral("government"), QString()}, {QStringLiteral("peoples"), QString()}, {QStringLiteral("culture"), QString()}, {QStringLiteral("economy"), QString()}, {QStringLiteral("currency"), QString()}, {QStringLiteral("languages"), QString()}, {QStringLiteral("religions"), QString()}, {QStringLiteral("military"), QString()}, {QStringLiteral("history"), QString()}, {QStringLiteral("relations"), QString()}, {QStringLiteral("locations"), QString()}, {QStringLiteral("conflicts"), QString()}, {QStringLiteral("notes"), QString()}, {QStringLiteral("tags"), QJsonArray()}, {QStringLiteral("attachments"), QJsonArray()}});
    document_->setArray(QStringLiteral("world"), array);
    refreshAtlas();
    worldList_->setCurrentRow(array.size() - 1);
    emit changed();
}

void WorldPage::removeWorld() {
    if (!document_) return;
    const int row = worldList_->currentRow();
    QJsonArray array = document_->array(QStringLiteral("world"));
    if (row < 0 || row >= array.size()) return;
    array.removeAt(row);
    document_->setArray(QStringLiteral("world"), array);
    refreshAtlas();
    if (worldList_->count()) worldList_->setCurrentRow(qMin(row, worldList_->count() - 1));
    emit changed();
}

void WorldPage::addWorldAttachment() {
    if (!document_) return;
    const int row = worldList_->currentRow();
    if (row < 0) return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Añadir adjunto"));
    if (path.isEmpty()) return;
    QFileInfo info(path);
    if (info.size() > 50LL * 1024LL * 1024LL) {
        QMessageBox::warning(this, tr("Adjunto demasiado grande"), tr("El límite por adjunto es 50 MB."));
        return;
    }
    QByteArray bytes;
    QString mime;
    const QString dataUrl = fileDataUrl(path, &bytes, &mime);
    if (dataUrl.isEmpty()) return;
    QJsonArray array = document_->array(QStringLiteral("world"));
    QJsonObject object = array.at(row).toObject();
    QJsonArray attachments = object.value(QStringLiteral("attachments")).toArray();
    attachments.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("attachment"))}, {QStringLiteral("name"), info.fileName()}, {QStringLiteral("mimeType"), mime}, {QStringLiteral("size"), static_cast<double>(bytes.size())}, {QStringLiteral("kind"), attachmentKind(mime, info.fileName())}, {QStringLiteral("dataUrl"), dataUrl}});
    object.insert(QStringLiteral("attachments"), attachments);
    array.replace(row, object);
    document_->setArray(QStringLiteral("world"), array);
    selectWorld(row);
    emit changed();
}

void WorldPage::removeWorldAttachment() {
    if (!document_) return;
    const int row = worldList_->currentRow();
    const int attachmentRow = worldAttachments_->currentRow();
    if (row < 0 || attachmentRow < 0) return;
    QJsonArray array = document_->array(QStringLiteral("world"));
    QJsonObject object = array.at(row).toObject();
    QJsonArray attachments = object.value(QStringLiteral("attachments")).toArray();
    if (attachmentRow >= attachments.size()) return;
    attachments.removeAt(attachmentRow);
    object.insert(QStringLiteral("attachments"), attachments);
    array.replace(row, object);
    document_->setArray(QStringLiteral("world"), array);
    selectWorld(row);
    emit changed();
}

void WorldPage::exportWorldAttachment() {
    if (!document_) return;
    const int row = worldList_->currentRow();
    const int attachmentRow = worldAttachments_->currentRow();
    if (row < 0 || attachmentRow < 0) return;
    const QJsonArray attachments = document_->array(QStringLiteral("world")).at(row).toObject().value(QStringLiteral("attachments")).toArray();
    if (attachmentRow >= attachments.size()) return;
    const QJsonObject attachment = attachments.at(attachmentRow).toObject();
    const QByteArray bytes = dataUrlBytes(attachment.value(QStringLiteral("dataUrl")).toString());
    if (bytes.isEmpty()) return;
    const QString path = QFileDialog::getSaveFileName(this, tr("Exportar adjunto"), attachment.value(QStringLiteral("name")).toString());
    if (path.isEmpty()) return;
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly)) { file.write(bytes); file.commit(); }
}

void WorldPage::refreshMagic() {
    magicList_->clear();
    for (const QJsonValue value : document_->array(QStringLiteral("magicSystems"))) magicList_->addItem(value.toObject().value(QStringLiteral("name")).toString(tr("Sistema sin nombre")));
}

void WorldPage::selectMagic(int row) {
    if (!document_) return;
    refreshing_ = true;
    const QJsonArray array = document_->array(QStringLiteral("magicSystems"));
    const QJsonObject object = row >= 0 && row < array.size() ? array.at(row).toObject() : QJsonObject();
    magicName_->setText(object.value(QStringLiteral("name")).toString());
    magicCategory_->setText(object.value(QStringLiteral("category")).toString());
    setCombo(magicStatus_, object.value(QStringLiteral("status")).toString());
    magicSource_->setPlainText(object.value(QStringLiteral("source")).toString());
    magicPrinciple_->setPlainText(object.value(QStringLiteral("principle")).toString());
    magicAccess_->setPlainText(object.value(QStringLiteral("access")).toString());
    magicCost_->setPlainText(object.value(QStringLiteral("cost")).toString());
    magicLimits_->setPlainText(object.value(QStringLiteral("limits")).toString());
    magicManifestations_->setPlainText(object.value(QStringLiteral("manifestations")).toString());
    magicMaterials_->setPlainText(object.value(QStringLiteral("materials")).toString());
    magicInstitutions_->setPlainText(object.value(QStringLiteral("institutions")).toString());
    magicUsers_->setPlainText(object.value(QStringLiteral("users")).toString());
    magicRisks_->setPlainText(object.value(QStringLiteral("risks")).toString());
    magicHistory_->setPlainText(object.value(QStringLiteral("history")).toString());
    magicNotes_->setPlainText(object.value(QStringLiteral("notes")).toString());
    magicEvidence_->setPlainText(evidenceText(object.value(QStringLiteral("evidence")).toArray()));
    magicTags_->setText(jsonStrings(object.value(QStringLiteral("tags")).toArray()));
    magicAttachments_->clear();
    for (const QJsonValue value : object.value(QStringLiteral("attachments")).toArray()) {
        auto* item = new QListWidgetItem(value.toObject().value(QStringLiteral("name")).toString());
        item->setData(Qt::UserRole, value.toObject().value(QStringLiteral("id")).toString());
        magicAttachments_->addItem(item);
    }
    refreshing_ = false;
}

void WorldPage::applyMagic() {
    if (refreshing_ || !document_) return;
    const int row = magicList_->currentRow();
    QJsonArray array = document_->array(QStringLiteral("magicSystems"));
    if (row < 0 || row >= array.size()) return;
    QJsonObject object = array.at(row).toObject();
    object.insert(QStringLiteral("name"), magicName_->text());
    object.insert(QStringLiteral("category"), magicCategory_->text());
    object.insert(QStringLiteral("status"), magicStatus_->currentText());
    object.insert(QStringLiteral("source"), magicSource_->toPlainText());
    object.insert(QStringLiteral("principle"), magicPrinciple_->toPlainText());
    object.insert(QStringLiteral("access"), magicAccess_->toPlainText());
    object.insert(QStringLiteral("cost"), magicCost_->toPlainText());
    object.insert(QStringLiteral("limits"), magicLimits_->toPlainText());
    object.insert(QStringLiteral("manifestations"), magicManifestations_->toPlainText());
    object.insert(QStringLiteral("materials"), magicMaterials_->toPlainText());
    object.insert(QStringLiteral("institutions"), magicInstitutions_->toPlainText());
    object.insert(QStringLiteral("users"), magicUsers_->toPlainText());
    object.insert(QStringLiteral("risks"), magicRisks_->toPlainText());
    object.insert(QStringLiteral("history"), magicHistory_->toPlainText());
    object.insert(QStringLiteral("notes"), magicNotes_->toPlainText());
    object.insert(QStringLiteral("evidence"), textEvidence(magicEvidence_->toPlainText()));
    object.insert(QStringLiteral("tags"), stringsJson(magicTags_->text()));
    array.replace(row, object);
    document_->setArray(QStringLiteral("magicSystems"), array);
    magicList_->item(row)->setText(magicName_->text().isEmpty() ? tr("Sistema sin nombre") : magicName_->text());
    emit changed();
}

void WorldPage::addMagic() {
    if (!document_) return;
    QJsonArray array = document_->array(QStringLiteral("magicSystems"));
    array.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("magic"))}, {QStringLiteral("name"), tr("Sistema sin nombre")}, {QStringLiteral("category"), QString()}, {QStringLiteral("status"), tr("En desarrollo")}, {QStringLiteral("source"), QString()}, {QStringLiteral("principle"), QString()}, {QStringLiteral("access"), QString()}, {QStringLiteral("cost"), QString()}, {QStringLiteral("limits"), QString()}, {QStringLiteral("manifestations"), QString()}, {QStringLiteral("materials"), QString()}, {QStringLiteral("institutions"), QString()}, {QStringLiteral("users"), QString()}, {QStringLiteral("risks"), QString()}, {QStringLiteral("history"), QString()}, {QStringLiteral("notes"), QString()}, {QStringLiteral("evidence"), QJsonArray()}, {QStringLiteral("tags"), QJsonArray()}, {QStringLiteral("attachments"), QJsonArray()}});
    document_->setArray(QStringLiteral("magicSystems"), array);
    refreshMagic();
    magicList_->setCurrentRow(array.size() - 1);
    emit changed();
}

void WorldPage::removeMagic() {
    if (!document_) return;
    const int row = magicList_->currentRow();
    QJsonArray array = document_->array(QStringLiteral("magicSystems"));
    if (row < 0 || row >= array.size()) return;
    array.removeAt(row);
    document_->setArray(QStringLiteral("magicSystems"), array);
    refreshMagic();
    if (magicList_->count()) magicList_->setCurrentRow(qMin(row, magicList_->count() - 1));
    emit changed();
}

void WorldPage::addMagicAttachment() {
    if (!document_) return;
    const int row = magicList_->currentRow();
    if (row < 0) return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Añadir adjunto"));
    if (path.isEmpty()) return;
    QFileInfo info(path);
    if (info.size() > 50LL * 1024LL * 1024LL) { QMessageBox::warning(this, tr("Adjunto demasiado grande"), tr("El límite por adjunto es 50 MB.")); return; }
    QByteArray bytes;
    QString mime;
    const QString dataUrl = fileDataUrl(path, &bytes, &mime);
    if (dataUrl.isEmpty()) return;
    QJsonArray array = document_->array(QStringLiteral("magicSystems"));
    QJsonObject object = array.at(row).toObject();
    QJsonArray attachments = object.value(QStringLiteral("attachments")).toArray();
    attachments.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("attachment"))}, {QStringLiteral("name"), info.fileName()}, {QStringLiteral("mimeType"), mime}, {QStringLiteral("size"), static_cast<double>(bytes.size())}, {QStringLiteral("kind"), attachmentKind(mime, info.fileName())}, {QStringLiteral("dataUrl"), dataUrl}});
    object.insert(QStringLiteral("attachments"), attachments);
    array.replace(row, object);
    document_->setArray(QStringLiteral("magicSystems"), array);
    selectMagic(row);
    emit changed();
}

void WorldPage::removeMagicAttachment() {
    if (!document_) return;
    const int row = magicList_->currentRow();
    const int attachmentRow = magicAttachments_->currentRow();
    if (row < 0 || attachmentRow < 0) return;
    QJsonArray array = document_->array(QStringLiteral("magicSystems"));
    QJsonObject object = array.at(row).toObject();
    QJsonArray attachments = object.value(QStringLiteral("attachments")).toArray();
    if (attachmentRow >= attachments.size()) return;
    attachments.removeAt(attachmentRow);
    object.insert(QStringLiteral("attachments"), attachments);
    array.replace(row, object);
    document_->setArray(QStringLiteral("magicSystems"), array);
    selectMagic(row);
    emit changed();
}

void WorldPage::exportMagicAttachment() {
    if (!document_) return;
    const int row = magicList_->currentRow();
    const int attachmentRow = magicAttachments_->currentRow();
    if (row < 0 || attachmentRow < 0) return;
    const QJsonArray attachments = document_->array(QStringLiteral("magicSystems")).at(row).toObject().value(QStringLiteral("attachments")).toArray();
    if (attachmentRow >= attachments.size()) return;
    const QJsonObject attachment = attachments.at(attachmentRow).toObject();
    const QByteArray bytes = dataUrlBytes(attachment.value(QStringLiteral("dataUrl")).toString());
    if (bytes.isEmpty()) return;
    const QString path = QFileDialog::getSaveFileName(this, tr("Exportar adjunto"), attachment.value(QStringLiteral("name")).toString());
    if (path.isEmpty()) return;
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly)) { file.write(bytes); file.commit(); }
}

void WorldPage::refreshMaps() {
    mapList_->clear();
    for (const QJsonValue value : document_->array(QStringLiteral("maps"))) mapList_->addItem(value.toObject().value(QStringLiteral("name")).toString(tr("Mapa sin nombre")));
}

void WorldPage::selectMap(int row) {
    if (!document_) return;
    refreshing_ = true;
    const QJsonArray maps = document_->array(QStringLiteral("maps"));
    const QJsonObject map = row >= 0 && row < maps.size() ? maps.at(row).toObject() : QJsonObject();
    mapName_->setText(map.value(QStringLiteral("name")).toString());
    mapDescription_->setPlainText(map.value(QStringLiteral("description")).toString());
    mapSeed_->setValue(map.value(QStringLiteral("seed")).toInt(1));
    setCombo(mapStyle_, map.value(QStringLiteral("style")).toString(QStringLiteral("Pergamino")));
    mapContinents_->setValue(map.value(QStringLiteral("continents")).toInt(3));
    mapIslands_->setValue(map.value(QStringLiteral("islands")).toInt(8));
    mapRoughness_->setValue(map.value(QStringLiteral("roughness")).toInt(5));
    mapMarkers_->clear();
    for (const QJsonValue value : map.value(QStringLiteral("markers")).toArray()) {
        const QJsonObject marker = value.toObject();
        auto* item = new QListWidgetItem(QStringLiteral("%1 · %2").arg(marker.value(QStringLiteral("kind")).toString(), marker.value(QStringLiteral("label")).toString()));
        item->setData(Qt::UserRole, marker.value(QStringLiteral("id")).toString());
        mapMarkers_->addItem(item);
    }
    mapCanvas_->setMap(map);
    refreshing_ = false;
}

void WorldPage::applyMap() {
    if (refreshing_ || !document_) return;
    const int row = mapList_->currentRow();
    QJsonArray maps = document_->array(QStringLiteral("maps"));
    if (row < 0 || row >= maps.size()) return;
    QJsonObject map = maps.at(row).toObject();
    map.insert(QStringLiteral("name"), mapName_->text());
    map.insert(QStringLiteral("description"), mapDescription_->toPlainText());
    map.insert(QStringLiteral("seed"), mapSeed_->value());
    map.insert(QStringLiteral("style"), mapStyle_->currentText());
    map.insert(QStringLiteral("continents"), mapContinents_->value());
    map.insert(QStringLiteral("islands"), mapIslands_->value());
    map.insert(QStringLiteral("roughness"), mapRoughness_->value());
    map.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    maps.replace(row, map);
    document_->setArray(QStringLiteral("maps"), maps);
    mapList_->item(row)->setText(mapName_->text().isEmpty() ? tr("Mapa sin nombre") : mapName_->text());
    mapCanvas_->setMap(map);
    emit changed();
}

void WorldPage::addMap() {
    if (!document_) return;
    QJsonArray maps = document_->array(QStringLiteral("maps"));
    maps.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("map"))}, {QStringLiteral("name"), tr("Nuevo mapa")}, {QStringLiteral("description"), QString()}, {QStringLiteral("seed"), static_cast<int>(QDateTime::currentSecsSinceEpoch() & 0x7fffffff)}, {QStringLiteral("style"), tr("Pergamino")}, {QStringLiteral("continents"), 3}, {QStringLiteral("islands"), 8}, {QStringLiteral("roughness"), 5}, {QStringLiteral("markers"), QJsonArray()}, {QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}});
    document_->setArray(QStringLiteral("maps"), maps);
    refreshMaps();
    mapList_->setCurrentRow(maps.size() - 1);
    emit changed();
}

void WorldPage::removeMap() {
    if (!document_) return;
    const int row = mapList_->currentRow();
    QJsonArray maps = document_->array(QStringLiteral("maps"));
    if (row < 0 || row >= maps.size()) return;
    maps.removeAt(row);
    document_->setArray(QStringLiteral("maps"), maps);
    refreshMaps();
    if (mapList_->count()) mapList_->setCurrentRow(qMin(row, mapList_->count() - 1)); else selectMap(-1);
    emit changed();
}

void WorldPage::chooseMapBackground() {
    if (!document_) return;
    const int row = mapList_->currentRow();
    if (row < 0) return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Imagen de fondo"), QString(), tr("Imágenes (*.png *.jpg *.jpeg *.webp)"));
    if (path.isEmpty()) return;
    const QString dataUrl = fileDataUrl(path);
    if (dataUrl.isEmpty()) return;
    QJsonArray maps = document_->array(QStringLiteral("maps"));
    QJsonObject map = maps.at(row).toObject();
    map.insert(QStringLiteral("backgroundImageDataUrl"), dataUrl);
    maps.replace(row, map);
    document_->setArray(QStringLiteral("maps"), maps);
    selectMap(row);
    emit changed();
}

void WorldPage::clearMapBackground() {
    if (!document_) return;
    const int row = mapList_->currentRow();
    QJsonArray maps = document_->array(QStringLiteral("maps"));
    if (row < 0 || row >= maps.size()) return;
    QJsonObject map = maps.at(row).toObject();
    map.remove(QStringLiteral("backgroundImageDataUrl"));
    maps.replace(row, map);
    document_->setArray(QStringLiteral("maps"), maps);
    selectMap(row);
    emit changed();
}

void WorldPage::addMapMarker(double x, double y) {
    if (!document_) return;
    const int row = mapList_->currentRow();
    if (row < 0) return;
    bool ok = false;
    const QString label = QInputDialog::getText(this, tr("Nuevo marcador"), tr("Nombre:"), QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || label.isEmpty()) return;
    const QStringList kinds{tr("Capital"), tr("Ciudad"), tr("Ruina"), tr("Puerto"), tr("Fortaleza"), tr("Lugar")};
    const QString kind = QInputDialog::getItem(this, tr("Tipo de marcador"), tr("Tipo:"), kinds, 5, false, &ok);
    if (!ok) return;
    QJsonArray maps = document_->array(QStringLiteral("maps"));
    QJsonObject map = maps.at(row).toObject();
    QJsonArray markers = map.value(QStringLiteral("markers")).toArray();
    markers.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("marker"))}, {QStringLiteral("label"), label}, {QStringLiteral("x"), x}, {QStringLiteral("y"), y}, {QStringLiteral("kind"), kind}});
    map.insert(QStringLiteral("markers"), markers);
    maps.replace(row, map);
    document_->setArray(QStringLiteral("maps"), maps);
    selectMap(row);
    emit changed();
}

void WorldPage::removeMapMarker() {
    if (!document_) return;
    const int mapRow = mapList_->currentRow();
    const int markerRow = mapMarkers_->currentRow();
    if (mapRow < 0 || markerRow < 0) return;
    QJsonArray maps = document_->array(QStringLiteral("maps"));
    QJsonObject map = maps.at(mapRow).toObject();
    QJsonArray markers = map.value(QStringLiteral("markers")).toArray();
    if (markerRow >= markers.size()) return;
    markers.removeAt(markerRow);
    map.insert(QStringLiteral("markers"), markers);
    maps.replace(mapRow, map);
    document_->setArray(QStringLiteral("maps"), maps);
    selectMap(mapRow);
    emit changed();
}

void WorldPage::moveMapMarker(const QString& id, double x, double y) {
    if (refreshing_ || !document_) return;
    const int mapRow = mapList_->currentRow();
    QJsonArray maps = document_->array(QStringLiteral("maps"));
    if (mapRow < 0 || mapRow >= maps.size()) return;
    QJsonObject map = maps.at(mapRow).toObject();
    QJsonArray markers = map.value(QStringLiteral("markers")).toArray();
    bool didChange = false;
    for (int i = 0; i < markers.size(); ++i) {
        QJsonObject marker = markers.at(i).toObject();
        if (marker.value(QStringLiteral("id")).toString() != id) continue;
        if (qAbs(marker.value(QStringLiteral("x")).toDouble() - x) < 0.01 && qAbs(marker.value(QStringLiteral("y")).toDouble() - y) < 0.01) return;
        marker.insert(QStringLiteral("x"), x);
        marker.insert(QStringLiteral("y"), y);
        markers.replace(i, marker);
        didChange = true;
        break;
    }
    if (didChange) {
        map.insert(QStringLiteral("markers"), markers);
        maps.replace(mapRow, map);
        document_->setArray(QStringLiteral("maps"), maps);
        emit changed();
    }
}

void WorldPage::refreshTexts() {
    worldTextList_->clear();
    magicTextList_->clear();
    for (const QJsonValue value : document_->array(QStringLiteral("worldTexts"))) worldTextList_->addItem(value.toObject().value(QStringLiteral("title")).toString(tr("Texto sin título")));
    for (const QJsonValue value : document_->array(QStringLiteral("magicTexts"))) magicTextList_->addItem(value.toObject().value(QStringLiteral("title")).toString(tr("Texto sin título")));
}

void WorldPage::selectText(bool magic, int row) {
    if (!document_) return;
    refreshing_ = true;
    const QJsonArray array = document_->array(magic ? QStringLiteral("magicTexts") : QStringLiteral("worldTexts"));
    const QJsonObject object = row >= 0 && row < array.size() ? array.at(row).toObject() : QJsonObject();
    QLineEdit* title = magic ? magicTextTitle_ : worldTextTitle_;
    QLineEdit* source = magic ? magicTextSource_ : worldTextSource_;
    QTextEdit* content = magic ? magicTextContent_ : worldTextContent_;
    title->setText(object.value(QStringLiteral("title")).toString());
    source->setText(object.value(QStringLiteral("sourceName")).toString());
    content->setPlainText(object.value(QStringLiteral("content")).toString());
    refreshing_ = false;
}

void WorldPage::applyText(bool magic) {
    if (refreshing_ || !document_) return;
    QListWidget* list = magic ? magicTextList_ : worldTextList_;
    const int row = list->currentRow();
    const QString key = magic ? QStringLiteral("magicTexts") : QStringLiteral("worldTexts");
    QJsonArray array = document_->array(key);
    if (row < 0 || row >= array.size()) return;
    QJsonObject object = array.at(row).toObject();
    QLineEdit* title = magic ? magicTextTitle_ : worldTextTitle_;
    QLineEdit* source = magic ? magicTextSource_ : worldTextSource_;
    QTextEdit* content = magic ? magicTextContent_ : worldTextContent_;
    object.insert(QStringLiteral("title"), title->text());
    object.insert(QStringLiteral("sourceName"), source->text());
    object.insert(QStringLiteral("content"), content->toPlainText());
    array.replace(row, object);
    document_->setArray(key, array);
    list->item(row)->setText(title->text().isEmpty() ? tr("Texto sin título") : title->text());
    emit changed();
}

void WorldPage::importText(bool magic) {
    if (!document_) return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Importar texto de referencia"), QString(), tr("Texto (*.txt *.md *.markdown)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;
    const QString key = magic ? QStringLiteral("magicTexts") : QStringLiteral("worldTexts");
    QJsonArray array = document_->array(key);
    array.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("text"))}, {QStringLiteral("title"), QFileInfo(path).completeBaseName()}, {QStringLiteral("content"), QString::fromUtf8(file.readAll())}, {QStringLiteral("sourceName"), QFileInfo(path).fileName()}, {QStringLiteral("importedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}});
    document_->setArray(key, array);
    refreshTexts();
    (magic ? magicTextList_ : worldTextList_)->setCurrentRow(array.size() - 1);
    emit changed();
}

void WorldPage::addText(bool magic) {
    if (!document_) return;
    const QString key = magic ? QStringLiteral("magicTexts") : QStringLiteral("worldTexts");
    QJsonArray array = document_->array(key);
    array.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("text"))}, {QStringLiteral("title"), tr("Nuevo texto")}, {QStringLiteral("content"), QString()}, {QStringLiteral("sourceName"), QString()}, {QStringLiteral("importedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}});
    document_->setArray(key, array);
    refreshTexts();
    (magic ? magicTextList_ : worldTextList_)->setCurrentRow(array.size() - 1);
    emit changed();
}

void WorldPage::removeText(bool magic) {
    if (!document_) return;
    QListWidget* list = magic ? magicTextList_ : worldTextList_;
    const int row = list->currentRow();
    const QString key = magic ? QStringLiteral("magicTexts") : QStringLiteral("worldTexts");
    QJsonArray array = document_->array(key);
    if (row < 0 || row >= array.size()) return;
    array.removeAt(row);
    document_->setArray(key, array);
    refreshTexts();
    if (list->count()) list->setCurrentRow(qMin(row, list->count() - 1));
    emit changed();
}

bool WorldPage::openReference(const QString& kind, const QString& id) {
    if (!document_ || id.isEmpty()) return false;
    auto selectById = [&](const QString& key, QListWidget* list, int tabIndex) {
        const QJsonArray array = document_->array(key);
        for (int i = 0; i < array.size(); ++i) {
            if (array.at(i).toObject().value(QStringLiteral("id")).toString() == id) {
                tabs_->setCurrentIndex(tabIndex);
                list->setCurrentRow(i);
                return true;
            }
        }
        return false;
    };
    if (kind == QStringLiteral("world") || kind == tr("Mundo")) return selectById(QStringLiteral("world"), worldList_, 0);
    if (kind == QStringLiteral("magic") || kind == tr("Magia")) return selectById(QStringLiteral("magicSystems"), magicList_, 1);
    if (kind == QStringLiteral("worldText") || kind == tr("Texto mundo")) {
        tabs_->setCurrentIndex(3);
        return selectById(QStringLiteral("worldTexts"), worldTextList_, 3);
    }
    if (kind == QStringLiteral("magicText") || kind == tr("Texto magia")) {
        tabs_->setCurrentIndex(3);
        return selectById(QStringLiteral("magicTexts"), magicTextList_, 3);
    }
    return false;
}

} // namespace wbw
