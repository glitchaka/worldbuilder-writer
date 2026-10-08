#include "ui/PlanningPage.h"

#include "core/ArchiveDocument.h"
#include "ui/RelationshipBoard.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeDatabase>
#include <QPixmap>
#include <QPushButton>
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

QString jsonStrings(const QJsonArray& array) {
    QStringList values;
    for (const QJsonValue value : array) if (!value.toString().trimmed().isEmpty()) values.append(value.toString().trimmed());
    return values.join(QStringLiteral(", "));
}

QJsonArray stringsJson(const QString& text) {
    QJsonArray result;
    for (QString value : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        value = value.trimmed();
        if (!value.isEmpty()) result.append(value);
    }
    return result;
}

QString evidenceText(const QJsonArray& array) {
    QStringList lines;
    for (const QJsonValue value : array) {
        const QJsonObject item = value.toObject();
        const QString chapter = item.value(QStringLiteral("chapter")).toString();
        const QString text = item.value(QStringLiteral("text")).toString();
        lines.append(chapter.isEmpty() ? text : chapter + QStringLiteral(" | ") + text);
    }
    return lines.join(QLatin1Char('\n'));
}

QJsonArray textEvidence(const QString& text) {
    QJsonArray result;
    for (QString line : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        line = line.trimmed();
        if (line.isEmpty()) continue;
        const qsizetype separator = line.indexOf(QLatin1Char('|'));
        if (separator < 0) result.append(QJsonObject{{QStringLiteral("chapter"), QString()}, {QStringLiteral("text"), line}});
        else result.append(QJsonObject{{QStringLiteral("chapter"), line.left(separator).trimmed()}, {QStringLiteral("text"), line.mid(separator + 1).trimmed()}});
    }
    return result;
}

QString intArrayText(const QJsonArray& array) {
    QStringList values;
    for (const QJsonValue value : array) values.append(QString::number(value.toInt()));
    return values.join(QStringLiteral(", "));
}

QJsonArray textIntArray(const QString& text) {
    QJsonArray result;
    for (const QString& part : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        bool ok = false;
        const int value = part.trimmed().toInt(&ok);
        if (ok) result.append(value);
    }
    return result;
}

void setComboText(QComboBox* combo, const QString& value) {
    const int index = combo->findText(value);
    if (index >= 0) combo->setCurrentIndex(index);
    else if (!value.isEmpty()) {
        combo->addItem(value);
        combo->setCurrentText(value);
    }
}

void setComboData(QComboBox* combo, const QString& value) {
    const int index = combo->findData(value);
    combo->setCurrentIndex(index >= 0 ? index : 0);
}

QByteArray dataUrlBytes(const QString& dataUrl) {
    const qsizetype comma = dataUrl.indexOf(QLatin1Char(','));
    if (!dataUrl.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive) || comma < 0) return {};
    const QString meta = dataUrl.mid(5, comma - 5);
    const QByteArray bytes = dataUrl.mid(comma + 1).toLatin1();
    return meta.contains(QStringLiteral(";base64"), Qt::CaseInsensitive) ? QByteArray::fromBase64(bytes) : QByteArray::fromPercentEncoding(bytes);
}

QWidget* scrollForm(QFormLayout*& form) {
    auto* content = new QWidget;
    form = new QFormLayout(content);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setLabelAlignment(Qt::AlignTop | Qt::AlignLeft);
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    return scroll;
}

} // namespace

PlanningPage::PlanningPage(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    tabs_ = new QTabWidget;
    tabs_->addTab(buildCharactersTab(), tr("Personajes"));
    tabs_->addTab(buildBoardTab(), tr("Tablero y relaciones"));
    tabs_->addTab(buildTheoriesTab(), tr("Hilos / teorías"));
    tabs_->addTab(buildTimelineTab(), tr("Cronología"));
    root->addWidget(tabs_);
}

void PlanningPage::setDocument(ArchiveDocument* document) {
    document_ = document;
    refresh();
}

QWidget* PlanningPage::buildCharactersTab() {
    auto* tab = new QWidget;
    auto* layout = new QVBoxLayout(tab);
    auto* split = new QSplitter;
    characterList_ = new QListWidget;
    characterList_->setMinimumWidth(220);
    split->addWidget(characterList_);

    QFormLayout* form = nullptr;
    QWidget* formScroll = scrollForm(form);
    characterImage_ = new QLabel(tr("Sin imagen"));
    characterImage_->setMinimumSize(180, 150);
    characterImage_->setMaximumHeight(220);
    characterImage_->setAlignment(Qt::AlignCenter);
    characterImage_->setStyleSheet(QStringLiteral("border:1px solid #393a32;border-radius:4px;"));
    auto* imageButton = makeButton(tr("Elegir imagen…"));
    auto* imageRow = new QHBoxLayout;
    imageRow->addWidget(characterImage_, 1);
    imageRow->addWidget(imageButton, 0, Qt::AlignTop);
    form->addRow(tr("Imagen"), imageRow);

    characterName_ = new QLineEdit;
    characterAliases_ = new QLineEdit;
    characterCategory_ = new QComboBox;
    characterCategory_->addItems({tr("Principal"), tr("Secundario"), tr("Incidental"), tr("Histórico"), tr("Entidad")});
    characterStatus_ = new QComboBox;
    characterStatus_->addItems({tr("Activo"), tr("Muerto"), tr("Desconocido"), tr("Histórico"), tr("Entidad")});
    characterRole_ = new QLineEdit;
    characterOccupation_ = new QLineEdit;
    characterOrigin_ = new QLineEdit;
    characterAffiliation_ = new QLineEdit;
    characterSummary_ = new QTextEdit;
    characterBackground_ = new QTextEdit;
    characterPhysical_ = new QTextEdit;
    characterTraits_ = new QLineEdit;
    characterEvidence_ = new QTextEdit;
    characterPresence_ = new QLineEdit;
    characterColor_ = new QLineEdit;
    characterBoardVisible_ = new QCheckBox(tr("Mostrar en el tablero"));
    characterSummary_->setMaximumHeight(110);
    characterBackground_->setMaximumHeight(150);
    characterPhysical_->setMaximumHeight(120);
    characterEvidence_->setMaximumHeight(130);
    characterAliases_->setPlaceholderText(tr("Alias separados por comas"));
    characterTraits_->setPlaceholderText(tr("Rasgos separados por comas"));
    characterEvidence_->setPlaceholderText(tr("Capítulo | evidencia, una por línea"));
    characterPresence_->setPlaceholderText(tr("Presencia por capítulo: 0, 4, 12…"));

    form->addRow(tr("Nombre"), characterName_);
    form->addRow(tr("Alias"), characterAliases_);
    form->addRow(tr("Categoría"), characterCategory_);
    form->addRow(tr("Estado"), characterStatus_);
    form->addRow(tr("Rol"), characterRole_);
    form->addRow(tr("Ocupación"), characterOccupation_);
    form->addRow(tr("Origen"), characterOrigin_);
    form->addRow(tr("Afiliación"), characterAffiliation_);
    form->addRow(tr("Resumen"), characterSummary_);
    form->addRow(tr("Trasfondo"), characterBackground_);
    form->addRow(tr("Descripción física"), characterPhysical_);
    form->addRow(tr("Rasgos"), characterTraits_);
    form->addRow(tr("Evidencias"), characterEvidence_);
    form->addRow(tr("Presencia"), characterPresence_);
    form->addRow(tr("Color"), characterColor_);
    form->addRow(QString(), characterBoardVisible_);

    auto* buttons = new QHBoxLayout;
    auto* add = makeButton(tr("+ Personaje"));
    auto* remove = makeButton(tr("Eliminar"));
    buttons->addWidget(add);
    buttons->addWidget(remove);
    buttons->addStretch();
    form->addRow(buttons);
    split->addWidget(formScroll);
    split->setStretchFactor(1, 1);
    layout->addWidget(split);

    connect(characterList_, &QListWidget::currentRowChanged, this, &PlanningPage::selectCharacter);
    connect(imageButton, &QPushButton::clicked, this, &PlanningPage::chooseCharacterImage);
    connect(add, &QPushButton::clicked, this, &PlanningPage::addCharacter);
    connect(remove, &QPushButton::clicked, this, &PlanningPage::removeCharacter);
    for (QLineEdit* field : {characterName_, characterAliases_, characterRole_, characterOccupation_, characterOrigin_, characterAffiliation_, characterTraits_, characterPresence_, characterColor_})
        connect(field, &QLineEdit::editingFinished, this, &PlanningPage::applyCharacter);
    for (QTextEdit* field : {characterSummary_, characterBackground_, characterPhysical_, characterEvidence_})
        connect(field, &QTextEdit::textChanged, this, &PlanningPage::applyCharacter);
    connect(characterCategory_, &QComboBox::currentTextChanged, this, &PlanningPage::applyCharacter);
    connect(characterStatus_, &QComboBox::currentTextChanged, this, &PlanningPage::applyCharacter);
    connect(characterBoardVisible_, &QCheckBox::toggled, this, &PlanningPage::applyCharacter);
    return tab;
}

QWidget* PlanningPage::buildBoardTab() {
    auto* tab = new QWidget;
    auto* layout = new QVBoxLayout(tab);
    auto* topBar = new QHBoxLayout;
    auto* all = makeButton(tr("Ver red completa"));
    auto* focus = makeButton(tr("Enfocar origen de relación"));
    topBar->addWidget(all);
    topBar->addWidget(focus);
    topBar->addStretch();
    layout->addLayout(topBar);

    auto* vertical = new QSplitter(Qt::Vertical);
    board_ = new RelationshipBoard;
    vertical->addWidget(board_);

    auto* relationPanel = new QWidget;
    auto* relationLayout = new QHBoxLayout(relationPanel);
    relationshipList_ = new QListWidget;
    relationshipList_->setMinimumWidth(270);
    relationLayout->addWidget(relationshipList_);
    auto* editor = new QWidget;
    auto* form = new QFormLayout(editor);
    relationshipSource_ = new QComboBox;
    relationshipTarget_ = new QComboBox;
    relationshipType_ = new QComboBox;
    relationshipType_->addItems({tr("Familia"), tr("Enemigo"), tr("Conocido"), tr("Solo interactuaron"), tr("Amistad"), tr("Alianza"), tr("Conflicto"), tr("Investigación"), tr("Tutela"), tr("Romance"), tr("Sospecha"), tr("Trabajo"), tr("Culto")});
    relationshipLabel_ = new QLineEdit;
    relationshipCertainty_ = new QComboBox;
    relationshipCertainty_->addItems({tr("Hecho"), tr("Hipótesis")});
    relationshipStrength_ = new QSpinBox;
    relationshipStrength_->setRange(1, 3);
    relationshipDetails_ = new QTextEdit;
    relationshipDetails_->setMaximumHeight(100);
    form->addRow(tr("Origen"), relationshipSource_);
    form->addRow(tr("Destino"), relationshipTarget_);
    form->addRow(tr("Tipo"), relationshipType_);
    form->addRow(tr("Etiqueta"), relationshipLabel_);
    form->addRow(tr("Certeza"), relationshipCertainty_);
    form->addRow(tr("Fortaleza"), relationshipStrength_);
    form->addRow(tr("Detalle"), relationshipDetails_);
    auto* actions = new QHBoxLayout;
    auto* add = makeButton(tr("+ Relación"));
    auto* remove = makeButton(tr("Eliminar"));
    actions->addWidget(add);
    actions->addWidget(remove);
    actions->addStretch();
    form->addRow(actions);
    relationLayout->addWidget(editor, 1);
    vertical->addWidget(relationPanel);
    vertical->setStretchFactor(0, 3);
    vertical->setStretchFactor(1, 2);
    layout->addWidget(vertical, 1);

    connect(all, &QPushButton::clicked, board_, &RelationshipBoard::showAllCharacters);
    connect(focus, &QPushButton::clicked, this, [this]() {
        const QString id = relationshipSource_->currentData().toString();
        if (!id.isEmpty()) board_->focusCharacter(id);
    });
    connect(board_, &RelationshipBoard::characterMoved, this, &PlanningPage::moveCharacter);
    connect(board_, &RelationshipBoard::characterActivated, this, [this](const QString& id) {
        if (!document_) return;
        const QJsonArray characters = document_->array(QStringLiteral("characters"));
        for (int i = 0; i < characters.size(); ++i) if (characters.at(i).toObject().value(QStringLiteral("id")).toString() == id) {
            tabs_->setCurrentIndex(0);
            characterList_->setCurrentRow(i);
            break;
        }
    });
    connect(relationshipList_, &QListWidget::currentRowChanged, this, &PlanningPage::selectRelationship);
    connect(add, &QPushButton::clicked, this, &PlanningPage::addRelationship);
    connect(remove, &QPushButton::clicked, this, &PlanningPage::removeRelationship);
    connect(relationshipSource_, &QComboBox::currentIndexChanged, this, &PlanningPage::applyRelationship);
    connect(relationshipTarget_, &QComboBox::currentIndexChanged, this, &PlanningPage::applyRelationship);
    connect(relationshipType_, &QComboBox::currentTextChanged, this, &PlanningPage::applyRelationship);
    connect(relationshipLabel_, &QLineEdit::editingFinished, this, &PlanningPage::applyRelationship);
    connect(relationshipCertainty_, &QComboBox::currentTextChanged, this, &PlanningPage::applyRelationship);
    connect(relationshipStrength_, &QSpinBox::valueChanged, this, &PlanningPage::applyRelationship);
    connect(relationshipDetails_, &QTextEdit::textChanged, this, &PlanningPage::applyRelationship);
    return tab;
}

QWidget* PlanningPage::buildTheoriesTab() {
    auto* tab = new QWidget;
    auto* layout = new QVBoxLayout(tab);
    auto* split = new QSplitter;
    theoryList_ = new QListWidget;
    split->addWidget(theoryList_);
    QFormLayout* form = nullptr;
    QWidget* editor = scrollForm(form);
    theoryTitle_ = new QLineEdit;
    theoryStatus_ = new QComboBox;
    theoryStatus_->addItems({tr("Confirmada"), tr("Muy probable"), tr("Abierta"), tr("Descartada")});
    theoryConfidence_ = new QSpinBox;
    theoryConfidence_->setRange(0, 100);
    theoryConfidence_->setSuffix(QStringLiteral(" %"));
    theoryThesis_ = new QTextEdit;
    theoryEvidence_ = new QTextEdit;
    theoryCounterpoint_ = new QTextEdit;
    theoryCharacters_ = new QLineEdit;
    theoryTags_ = new QLineEdit;
    theoryEvidence_->setPlaceholderText(tr("Capítulo | evidencia, una por línea"));
    theoryCharacters_->setPlaceholderText(tr("IDs de personajes separados por comas"));
    theoryTags_->setPlaceholderText(tr("Etiquetas separadas por comas"));
    form->addRow(tr("Título"), theoryTitle_);
    form->addRow(tr("Estado"), theoryStatus_);
    form->addRow(tr("Confianza"), theoryConfidence_);
    form->addRow(tr("Tesis"), theoryThesis_);
    form->addRow(tr("Evidencias"), theoryEvidence_);
    form->addRow(tr("Contraargumento"), theoryCounterpoint_);
    form->addRow(tr("Personajes"), theoryCharacters_);
    form->addRow(tr("Etiquetas"), theoryTags_);
    auto* actions = new QHBoxLayout;
    auto* add = makeButton(tr("+ Teoría"));
    auto* remove = makeButton(tr("Eliminar"));
    actions->addWidget(add);
    actions->addWidget(remove);
    actions->addStretch();
    form->addRow(actions);
    split->addWidget(editor);
    split->setStretchFactor(1, 1);
    layout->addWidget(split);

    connect(theoryList_, &QListWidget::currentRowChanged, this, &PlanningPage::selectTheory);
    connect(add, &QPushButton::clicked, this, &PlanningPage::addTheory);
    connect(remove, &QPushButton::clicked, this, &PlanningPage::removeTheory);
    connect(theoryTitle_, &QLineEdit::editingFinished, this, &PlanningPage::applyTheory);
    connect(theoryStatus_, &QComboBox::currentTextChanged, this, &PlanningPage::applyTheory);
    connect(theoryConfidence_, &QSpinBox::valueChanged, this, &PlanningPage::applyTheory);
    connect(theoryThesis_, &QTextEdit::textChanged, this, &PlanningPage::applyTheory);
    connect(theoryEvidence_, &QTextEdit::textChanged, this, &PlanningPage::applyTheory);
    connect(theoryCounterpoint_, &QTextEdit::textChanged, this, &PlanningPage::applyTheory);
    connect(theoryCharacters_, &QLineEdit::editingFinished, this, &PlanningPage::applyTheory);
    connect(theoryTags_, &QLineEdit::editingFinished, this, &PlanningPage::applyTheory);
    return tab;
}

QWidget* PlanningPage::buildTimelineTab() {
    auto* tab = new QWidget;
    auto* layout = new QVBoxLayout(tab);
    auto* split = new QSplitter;
    timelineList_ = new QListWidget;
    split->addWidget(timelineList_);
    auto* editor = new QWidget;
    auto* form = new QFormLayout(editor);
    timelineChapter_ = new QLineEdit;
    timelineWhen_ = new QLineEdit;
    timelineTitle_ = new QLineEdit;
    timelineSummary_ = new QTextEdit;
    timelineCharacters_ = new QLineEdit;
    timelineIntensity_ = new QSpinBox;
    timelineIntensity_->setRange(0, 10);
    timelineCharacters_->setPlaceholderText(tr("IDs de personajes separados por comas"));
    form->addRow(tr("Capítulo"), timelineChapter_);
    form->addRow(tr("Cuándo"), timelineWhen_);
    form->addRow(tr("Título"), timelineTitle_);
    form->addRow(tr("Resumen"), timelineSummary_);
    form->addRow(tr("Personajes"), timelineCharacters_);
    form->addRow(tr("Intensidad"), timelineIntensity_);
    auto* actions = new QHBoxLayout;
    auto* add = makeButton(tr("+ Evento"));
    auto* remove = makeButton(tr("Eliminar"));
    auto* up = makeButton(tr("Subir"));
    auto* down = makeButton(tr("Bajar"));
    actions->addWidget(add);
    actions->addWidget(remove);
    actions->addWidget(up);
    actions->addWidget(down);
    actions->addStretch();
    form->addRow(actions);
    split->addWidget(editor);
    split->setStretchFactor(1, 1);
    layout->addWidget(split);

    connect(timelineList_, &QListWidget::currentRowChanged, this, &PlanningPage::selectTimeline);
    connect(add, &QPushButton::clicked, this, &PlanningPage::addTimeline);
    connect(remove, &QPushButton::clicked, this, &PlanningPage::removeTimeline);
    connect(up, &QPushButton::clicked, this, [this]() {
        if (!document_) return;
        const int row = timelineList_->currentRow();
        if (row <= 0) return;
        QJsonArray array = document_->array(QStringLiteral("timeline"));
        const QJsonValue value = array.takeAt(row);
        array.insert(row - 1, value);
        document_->setArray(QStringLiteral("timeline"), array);
        refreshTimeline();
        timelineList_->setCurrentRow(row - 1);
        emit changed();
    });
    connect(down, &QPushButton::clicked, this, [this]() {
        if (!document_) return;
        const int row = timelineList_->currentRow();
        QJsonArray array = document_->array(QStringLiteral("timeline"));
        if (row < 0 || row >= array.size() - 1) return;
        const QJsonValue value = array.takeAt(row);
        array.insert(row + 1, value);
        document_->setArray(QStringLiteral("timeline"), array);
        refreshTimeline();
        timelineList_->setCurrentRow(row + 1);
        emit changed();
    });
    connect(timelineChapter_, &QLineEdit::editingFinished, this, &PlanningPage::applyTimeline);
    connect(timelineWhen_, &QLineEdit::editingFinished, this, &PlanningPage::applyTimeline);
    connect(timelineTitle_, &QLineEdit::editingFinished, this, &PlanningPage::applyTimeline);
    connect(timelineSummary_, &QTextEdit::textChanged, this, &PlanningPage::applyTimeline);
    connect(timelineCharacters_, &QLineEdit::editingFinished, this, &PlanningPage::applyTimeline);
    connect(timelineIntensity_, &QSpinBox::valueChanged, this, &PlanningPage::applyTimeline);
    return tab;
}

void PlanningPage::refresh() {
    if (!document_) return;
    refreshing_ = true;
    refreshCharacters();
    refreshRelationships();
    refreshTheories();
    refreshTimeline();
    refreshing_ = false;
    if (characterList_->count() > 0) characterList_->setCurrentRow(0);
    if (relationshipList_->count() > 0) relationshipList_->setCurrentRow(0);
    if (theoryList_->count() > 0) theoryList_->setCurrentRow(0);
    if (timelineList_->count() > 0) timelineList_->setCurrentRow(0);
}

void PlanningPage::refreshCharacters() {
    characterList_->clear();
    relationshipSource_->clear();
    relationshipTarget_->clear();
    relationshipSource_->addItem(tr("—"), QString());
    relationshipTarget_->addItem(tr("—"), QString());
    const QJsonArray characters = document_->array(QStringLiteral("characters"));
    for (const QJsonValue value : characters) {
        const QJsonObject character = value.toObject();
        const QString name = character.value(QStringLiteral("name")).toString(tr("Personaje sin nombre"));
        const QString id = character.value(QStringLiteral("id")).toString();
        characterList_->addItem(name);
        relationshipSource_->addItem(name, id);
        relationshipTarget_->addItem(name, id);
    }
    board_->setData(characters, document_->array(QStringLiteral("relationships")));
}

void PlanningPage::selectCharacter(int row) {
    if (!document_) return;
    refreshing_ = true;
    const QJsonArray characters = document_->array(QStringLiteral("characters"));
    const QJsonObject object = row >= 0 && row < characters.size() ? characters.at(row).toObject() : QJsonObject();
    characterName_->setText(object.value(QStringLiteral("name")).toString());
    characterAliases_->setText(jsonStrings(object.value(QStringLiteral("aliases")).toArray()));
    setComboText(characterCategory_, object.value(QStringLiteral("category")).toString());
    setComboText(characterStatus_, object.value(QStringLiteral("status")).toString());
    characterRole_->setText(object.value(QStringLiteral("role")).toString());
    characterOccupation_->setText(object.value(QStringLiteral("occupation")).toString());
    characterOrigin_->setText(object.value(QStringLiteral("origin")).toString());
    characterAffiliation_->setText(object.value(QStringLiteral("affiliation")).toString());
    characterSummary_->setPlainText(object.value(QStringLiteral("summary")).toString());
    characterBackground_->setPlainText(object.value(QStringLiteral("background")).toString());
    characterPhysical_->setPlainText(object.value(QStringLiteral("physical")).toString());
    characterTraits_->setText(jsonStrings(object.value(QStringLiteral("traits")).toArray()));
    characterEvidence_->setPlainText(evidenceText(object.value(QStringLiteral("evidence")).toArray()));
    characterPresence_->setText(intArrayText(object.value(QStringLiteral("presence")).toArray()));
    characterColor_->setText(object.value(QStringLiteral("color")).toString());
    const QJsonObject board = object.value(QStringLiteral("board")).toObject();
    characterBoardVisible_->setChecked(!board.contains(QStringLiteral("visible")) || board.value(QStringLiteral("visible")).toBool(true));
    const QByteArray image = dataUrlBytes(object.value(QStringLiteral("imageUrl")).toString());
    QPixmap pixmap;
    if (!image.isEmpty()) pixmap.loadFromData(image);
    if (pixmap.isNull()) {
        characterImage_->setPixmap(QPixmap());
        characterImage_->setText(tr("Sin imagen"));
    } else {
        characterImage_->setText(QString());
        characterImage_->setPixmap(pixmap.scaled(characterImage_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    refreshing_ = false;
}

void PlanningPage::applyCharacter() {
    if (refreshing_ || !document_) return;
    const int row = characterList_->currentRow();
    QJsonArray characters = document_->array(QStringLiteral("characters"));
    if (row < 0 || row >= characters.size()) return;
    QJsonObject object = characters.at(row).toObject();
    object.insert(QStringLiteral("name"), characterName_->text());
    object.insert(QStringLiteral("aliases"), stringsJson(characterAliases_->text()));
    object.insert(QStringLiteral("category"), characterCategory_->currentText());
    object.insert(QStringLiteral("status"), characterStatus_->currentText());
    object.insert(QStringLiteral("role"), characterRole_->text());
    object.insert(QStringLiteral("occupation"), characterOccupation_->text());
    object.insert(QStringLiteral("origin"), characterOrigin_->text());
    object.insert(QStringLiteral("affiliation"), characterAffiliation_->text());
    object.insert(QStringLiteral("summary"), characterSummary_->toPlainText());
    object.insert(QStringLiteral("background"), characterBackground_->toPlainText());
    object.insert(QStringLiteral("physical"), characterPhysical_->toPlainText());
    object.insert(QStringLiteral("traits"), stringsJson(characterTraits_->text()));
    object.insert(QStringLiteral("evidence"), textEvidence(characterEvidence_->toPlainText()));
    object.insert(QStringLiteral("presence"), textIntArray(characterPresence_->text()));
    object.insert(QStringLiteral("color"), characterColor_->text());
    QJsonObject board = object.value(QStringLiteral("board")).toObject();
    if (!board.contains(QStringLiteral("x"))) board.insert(QStringLiteral("x"), 100.0 + row * 40.0);
    if (!board.contains(QStringLiteral("y"))) board.insert(QStringLiteral("y"), 100.0 + row * 35.0);
    board.insert(QStringLiteral("visible"), characterBoardVisible_->isChecked());
    object.insert(QStringLiteral("board"), board);
    characters.replace(row, object);
    document_->setArray(QStringLiteral("characters"), characters);
    characterList_->item(row)->setText(characterName_->text().isEmpty() ? tr("Personaje sin nombre") : characterName_->text());
    board_->setData(characters, document_->array(QStringLiteral("relationships")));
    emit changed();
}

void PlanningPage::chooseCharacterImage() {
    if (!document_) return;
    const int row = characterList_->currentRow();
    if (row < 0) return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Imagen del personaje"), QString(), tr("Imágenes (*.png *.jpg *.jpeg *.webp *.gif)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;
    const QByteArray bytes = file.readAll();
    const QString mime = QMimeDatabase().mimeTypeForFile(path, QMimeDatabase::MatchContent).name();
    QJsonArray characters = document_->array(QStringLiteral("characters"));
    QJsonObject object = characters.at(row).toObject();
    object.insert(QStringLiteral("imageUrl"), QStringLiteral("data:%1;base64,%2").arg(mime, QString::fromLatin1(bytes.toBase64())));
    characters.replace(row, object);
    document_->setArray(QStringLiteral("characters"), characters);
    selectCharacter(row);
    emit changed();
}

void PlanningPage::addCharacter() {
    if (!document_) return;
    QJsonArray characters = document_->array(QStringLiteral("characters"));
    characters.append(QJsonObject{
        {QStringLiteral("id"), uid(QStringLiteral("character"))},
        {QStringLiteral("name"), tr("Personaje sin nombre")},
        {QStringLiteral("aliases"), QJsonArray()},
        {QStringLiteral("category"), tr("Secundario")},
        {QStringLiteral("status"), tr("Activo")},
        {QStringLiteral("role"), QString()},
        {QStringLiteral("occupation"), QString()},
        {QStringLiteral("origin"), QString()},
        {QStringLiteral("affiliation"), QString()},
        {QStringLiteral("summary"), QString()},
        {QStringLiteral("background"), QString()},
        {QStringLiteral("physical"), QString()},
        {QStringLiteral("traits"), QJsonArray()},
        {QStringLiteral("evidence"), QJsonArray()},
        {QStringLiteral("presence"), QJsonArray()},
        {QStringLiteral("color"), QStringLiteral("amber")},
        {QStringLiteral("board"), QJsonObject{{QStringLiteral("x"), 120.0 + characters.size() * 30.0}, {QStringLiteral("y"), 120.0 + characters.size() * 25.0}, {QStringLiteral("visible"), true}}}
    });
    document_->setArray(QStringLiteral("characters"), characters);
    refreshCharacters();
    characterList_->setCurrentRow(characters.size() - 1);
    refreshRelationships();
    emit changed();
}

void PlanningPage::removeCharacter() {
    if (!document_) return;
    const int row = characterList_->currentRow();
    QJsonArray characters = document_->array(QStringLiteral("characters"));
    if (row < 0 || row >= characters.size()) return;
    const QString id = characters.at(row).toObject().value(QStringLiteral("id")).toString();
    characters.removeAt(row);
    QJsonArray relationships = document_->array(QStringLiteral("relationships"));
    for (int i = relationships.size() - 1; i >= 0; --i) {
        const QJsonObject rel = relationships.at(i).toObject();
        if (rel.value(QStringLiteral("source")).toString() == id || rel.value(QStringLiteral("target")).toString() == id) relationships.removeAt(i);
    }
    document_->setArray(QStringLiteral("characters"), characters);
    document_->setArray(QStringLiteral("relationships"), relationships);
    refreshCharacters();
    refreshRelationships();
    if (characterList_->count()) characterList_->setCurrentRow(qMin(row, characterList_->count() - 1));
    emit changed();
}

void PlanningPage::refreshRelationships() {
    relationshipList_->clear();
    const QJsonArray relationships = document_->array(QStringLiteral("relationships"));
    QHash<QString, QString> names;
    for (const QJsonValue value : document_->array(QStringLiteral("characters"))) names.insert(value.toObject().value(QStringLiteral("id")).toString(), value.toObject().value(QStringLiteral("name")).toString());
    for (const QJsonValue value : relationships) {
        const QJsonObject rel = value.toObject();
        relationshipList_->addItem(QStringLiteral("%1 → %2 · %3").arg(names.value(rel.value(QStringLiteral("source")).toString(), tr("?")), names.value(rel.value(QStringLiteral("target")).toString(), tr("?")), rel.value(QStringLiteral("type")).toString()));
    }
    board_->setData(document_->array(QStringLiteral("characters")), relationships);
}

void PlanningPage::selectRelationship(int row) {
    if (!document_) return;
    refreshing_ = true;
    const QJsonArray relationships = document_->array(QStringLiteral("relationships"));
    const QJsonObject rel = row >= 0 && row < relationships.size() ? relationships.at(row).toObject() : QJsonObject();
    setComboData(relationshipSource_, rel.value(QStringLiteral("source")).toString());
    setComboData(relationshipTarget_, rel.value(QStringLiteral("target")).toString());
    setComboText(relationshipType_, rel.value(QStringLiteral("type")).toString());
    relationshipLabel_->setText(rel.value(QStringLiteral("label")).toString());
    setComboText(relationshipCertainty_, rel.value(QStringLiteral("certainty")).toString());
    relationshipStrength_->setValue(rel.value(QStringLiteral("strength")).toInt(1));
    relationshipDetails_->setPlainText(rel.value(QStringLiteral("details")).toString());
    refreshing_ = false;
}

void PlanningPage::applyRelationship() {
    if (refreshing_ || !document_) return;
    const int row = relationshipList_->currentRow();
    QJsonArray relationships = document_->array(QStringLiteral("relationships"));
    if (row < 0 || row >= relationships.size()) return;
    QJsonObject rel = relationships.at(row).toObject();
    rel.insert(QStringLiteral("source"), relationshipSource_->currentData().toString());
    rel.insert(QStringLiteral("target"), relationshipTarget_->currentData().toString());
    rel.insert(QStringLiteral("type"), relationshipType_->currentText());
    rel.insert(QStringLiteral("label"), relationshipLabel_->text());
    rel.insert(QStringLiteral("certainty"), relationshipCertainty_->currentText());
    rel.insert(QStringLiteral("strength"), relationshipStrength_->value());
    rel.insert(QStringLiteral("details"), relationshipDetails_->toPlainText());
    relationships.replace(row, rel);
    document_->setArray(QStringLiteral("relationships"), relationships);
    refreshRelationships();
    relationshipList_->setCurrentRow(row);
    emit changed();
}

void PlanningPage::addRelationship() {
    if (!document_) return;
    const QJsonArray characters = document_->array(QStringLiteral("characters"));
    if (characters.size() < 2) return;
    QJsonArray relationships = document_->array(QStringLiteral("relationships"));
    relationships.append(QJsonObject{
        {QStringLiteral("id"), uid(QStringLiteral("relationship"))},
        {QStringLiteral("source"), characters.at(0).toObject().value(QStringLiteral("id"))},
        {QStringLiteral("target"), characters.at(1).toObject().value(QStringLiteral("id"))},
        {QStringLiteral("type"), tr("Conocido")},
        {QStringLiteral("label"), QString()},
        {QStringLiteral("certainty"), tr("Hecho")},
        {QStringLiteral("strength"), 1},
        {QStringLiteral("details"), QString()}
    });
    document_->setArray(QStringLiteral("relationships"), relationships);
    refreshRelationships();
    relationshipList_->setCurrentRow(relationships.size() - 1);
    emit changed();
}

void PlanningPage::removeRelationship() {
    if (!document_) return;
    const int row = relationshipList_->currentRow();
    QJsonArray relationships = document_->array(QStringLiteral("relationships"));
    if (row < 0 || row >= relationships.size()) return;
    relationships.removeAt(row);
    document_->setArray(QStringLiteral("relationships"), relationships);
    refreshRelationships();
    if (relationshipList_->count()) relationshipList_->setCurrentRow(qMin(row, relationshipList_->count() - 1));
    emit changed();
}

void PlanningPage::moveCharacter(const QString& id, double x, double y) {
    if (refreshing_ || !document_) return;
    QJsonArray characters = document_->array(QStringLiteral("characters"));
    bool changedValue = false;
    for (int i = 0; i < characters.size(); ++i) {
        QJsonObject character = characters.at(i).toObject();
        if (character.value(QStringLiteral("id")).toString() != id) continue;
        QJsonObject board = character.value(QStringLiteral("board")).toObject();
        if (qAbs(board.value(QStringLiteral("x")).toDouble() - x) < 0.01 && qAbs(board.value(QStringLiteral("y")).toDouble() - y) < 0.01) return;
        board.insert(QStringLiteral("x"), x);
        board.insert(QStringLiteral("y"), y);
        character.insert(QStringLiteral("board"), board);
        characters.replace(i, character);
        changedValue = true;
        break;
    }
    if (changedValue) {
        document_->setArray(QStringLiteral("characters"), characters);
        emit changed();
    }
}

void PlanningPage::refreshTheories() {
    theoryList_->clear();
    for (const QJsonValue value : document_->array(QStringLiteral("theories"))) theoryList_->addItem(value.toObject().value(QStringLiteral("title")).toString(tr("Teoría sin título")));
}

void PlanningPage::selectTheory(int row) {
    if (!document_) return;
    refreshing_ = true;
    const QJsonArray array = document_->array(QStringLiteral("theories"));
    const QJsonObject object = row >= 0 && row < array.size() ? array.at(row).toObject() : QJsonObject();
    theoryTitle_->setText(object.value(QStringLiteral("title")).toString());
    setComboText(theoryStatus_, object.value(QStringLiteral("status")).toString());
    theoryConfidence_->setValue(object.value(QStringLiteral("confidence")).toInt());
    theoryThesis_->setPlainText(object.value(QStringLiteral("thesis")).toString());
    theoryEvidence_->setPlainText(evidenceText(object.value(QStringLiteral("evidence")).toArray()));
    theoryCounterpoint_->setPlainText(object.value(QStringLiteral("counterpoint")).toString());
    theoryCharacters_->setText(jsonStrings(object.value(QStringLiteral("characterIds")).toArray()));
    theoryTags_->setText(jsonStrings(object.value(QStringLiteral("tags")).toArray()));
    refreshing_ = false;
}

void PlanningPage::applyTheory() {
    if (refreshing_ || !document_) return;
    const int row = theoryList_->currentRow();
    QJsonArray array = document_->array(QStringLiteral("theories"));
    if (row < 0 || row >= array.size()) return;
    QJsonObject object = array.at(row).toObject();
    object.insert(QStringLiteral("title"), theoryTitle_->text());
    object.insert(QStringLiteral("status"), theoryStatus_->currentText());
    object.insert(QStringLiteral("confidence"), theoryConfidence_->value());
    object.insert(QStringLiteral("thesis"), theoryThesis_->toPlainText());
    object.insert(QStringLiteral("evidence"), textEvidence(theoryEvidence_->toPlainText()));
    object.insert(QStringLiteral("counterpoint"), theoryCounterpoint_->toPlainText());
    object.insert(QStringLiteral("characterIds"), stringsJson(theoryCharacters_->text()));
    object.insert(QStringLiteral("tags"), stringsJson(theoryTags_->text()));
    array.replace(row, object);
    document_->setArray(QStringLiteral("theories"), array);
    theoryList_->item(row)->setText(theoryTitle_->text().isEmpty() ? tr("Teoría sin título") : theoryTitle_->text());
    emit changed();
}

void PlanningPage::addTheory() {
    if (!document_) return;
    QJsonArray array = document_->array(QStringLiteral("theories"));
    array.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("theory"))}, {QStringLiteral("title"), tr("Nueva teoría")}, {QStringLiteral("status"), tr("Abierta")}, {QStringLiteral("confidence"), 50}, {QStringLiteral("thesis"), QString()}, {QStringLiteral("evidence"), QJsonArray()}, {QStringLiteral("counterpoint"), QString()}, {QStringLiteral("characterIds"), QJsonArray()}, {QStringLiteral("tags"), QJsonArray()}});
    document_->setArray(QStringLiteral("theories"), array);
    refreshTheories();
    theoryList_->setCurrentRow(array.size() - 1);
    emit changed();
}

void PlanningPage::removeTheory() {
    if (!document_) return;
    const int row = theoryList_->currentRow();
    QJsonArray array = document_->array(QStringLiteral("theories"));
    if (row < 0 || row >= array.size()) return;
    array.removeAt(row);
    document_->setArray(QStringLiteral("theories"), array);
    refreshTheories();
    if (theoryList_->count()) theoryList_->setCurrentRow(qMin(row, theoryList_->count() - 1));
    emit changed();
}

void PlanningPage::refreshTimeline() {
    timelineList_->clear();
    for (const QJsonValue value : document_->array(QStringLiteral("timeline"))) {
        const QJsonObject event = value.toObject();
        timelineList_->addItem(QStringLiteral("%1 · %2 — %3").arg(event.value(QStringLiteral("chapter")).toString(), event.value(QStringLiteral("when")).toString(), event.value(QStringLiteral("title")).toString()));
    }
}

void PlanningPage::selectTimeline(int row) {
    if (!document_) return;
    refreshing_ = true;
    const QJsonArray array = document_->array(QStringLiteral("timeline"));
    const QJsonObject event = row >= 0 && row < array.size() ? array.at(row).toObject() : QJsonObject();
    timelineChapter_->setText(event.value(QStringLiteral("chapter")).toString());
    timelineWhen_->setText(event.value(QStringLiteral("when")).toString());
    timelineTitle_->setText(event.value(QStringLiteral("title")).toString());
    timelineSummary_->setPlainText(event.value(QStringLiteral("summary")).toString());
    timelineCharacters_->setText(jsonStrings(event.value(QStringLiteral("characterIds")).toArray()));
    timelineIntensity_->setValue(event.value(QStringLiteral("intensity")).toInt());
    refreshing_ = false;
}

void PlanningPage::applyTimeline() {
    if (refreshing_ || !document_) return;
    const int row = timelineList_->currentRow();
    QJsonArray array = document_->array(QStringLiteral("timeline"));
    if (row < 0 || row >= array.size()) return;
    QJsonObject event = array.at(row).toObject();
    event.insert(QStringLiteral("chapter"), timelineChapter_->text());
    event.insert(QStringLiteral("when"), timelineWhen_->text());
    event.insert(QStringLiteral("title"), timelineTitle_->text());
    event.insert(QStringLiteral("summary"), timelineSummary_->toPlainText());
    event.insert(QStringLiteral("characterIds"), stringsJson(timelineCharacters_->text()));
    event.insert(QStringLiteral("intensity"), timelineIntensity_->value());
    array.replace(row, event);
    document_->setArray(QStringLiteral("timeline"), array);
    refreshTimeline();
    timelineList_->setCurrentRow(row);
    emit changed();
}

void PlanningPage::addTimeline() {
    if (!document_) return;
    QJsonArray array = document_->array(QStringLiteral("timeline"));
    array.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("timeline"))}, {QStringLiteral("chapter"), QString()}, {QStringLiteral("when"), QString()}, {QStringLiteral("title"), tr("Nuevo evento")}, {QStringLiteral("summary"), QString()}, {QStringLiteral("characterIds"), QJsonArray()}, {QStringLiteral("intensity"), 1}});
    document_->setArray(QStringLiteral("timeline"), array);
    refreshTimeline();
    timelineList_->setCurrentRow(array.size() - 1);
    emit changed();
}

void PlanningPage::removeTimeline() {
    if (!document_) return;
    const int row = timelineList_->currentRow();
    QJsonArray array = document_->array(QStringLiteral("timeline"));
    if (row < 0 || row >= array.size()) return;
    array.removeAt(row);
    document_->setArray(QStringLiteral("timeline"), array);
    refreshTimeline();
    if (timelineList_->count()) timelineList_->setCurrentRow(qMin(row, timelineList_->count() - 1));
    emit changed();
}

} // namespace wbw
