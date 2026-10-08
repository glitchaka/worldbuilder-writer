#include "ui/WritingPage.h"

#include "core/ArchiveDocument.h"
#include "import/ManuscriptImporter.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QFileDialog>
#include <QFont>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSplitter>
#include <QTabWidget>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
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

void normalizeOrders(QJsonArray& chapters) {
    for (int c = 0; c < chapters.size(); ++c) {
        QJsonObject chapter = chapters.at(c).toObject();
        chapter.insert(QStringLiteral("order"), c);
        QJsonArray scenes = chapter.value(QStringLiteral("scenes")).toArray();
        for (int s = 0; s < scenes.size(); ++s) {
            QJsonObject scene = scenes.at(s).toObject();
            scene.insert(QStringLiteral("order"), s);
            scene.insert(QStringLiteral("chapterId"), chapter.value(QStringLiteral("id")));
            scenes.replace(s, scene);
        }
        chapter.insert(QStringLiteral("scenes"), scenes);
        chapters.replace(c, chapter);
    }
}

QString contentPlainText(const QString& content) {
    QTextDocument doc;
    doc.setHtml(content);
    return doc.toPlainText();
}

} // namespace

WritingPage::WritingPage(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    tabs_ = new QTabWidget;
    tabs_->addTab(buildEditorTab(), tr("Manuscrito"));
    tabs_->addTab(buildSceneBoardTab(), tr("Tablero de escenas"));
    root->addWidget(tabs_);
}

void WritingPage::setDocument(ArchiveDocument* document) {
    document_ = document;
    refresh();
}

QWidget* WritingPage::buildEditorTab() {
    auto* tab = new QWidget;
    auto* outer = new QVBoxLayout(tab);
    auto* actions = new QHBoxLayout;
    auto* addChapterButton = makeButton(tr("+ Capítulo"));
    auto* addSceneButton = makeButton(tr("+ Escena"));
    auto* renameButton = makeButton(tr("Renombrar capítulo"));
    auto* removeButton = makeButton(tr("Eliminar"));
    auto* upButton = makeButton(tr("Subir"));
    auto* downButton = makeButton(tr("Bajar"));
    auto* importButton = makeButton(tr("Importar manuscrito…"));
    auto* focusButton = makeButton(tr("Modo enfoque"));
    actions->addWidget(addChapterButton);
    actions->addWidget(addSceneButton);
    actions->addWidget(renameButton);
    actions->addWidget(removeButton);
    actions->addWidget(upButton);
    actions->addWidget(downButton);
    actions->addStretch();
    actions->addWidget(importButton);
    actions->addWidget(focusButton);
    outer->addLayout(actions);

    auto* split = new QSplitter;
    tree_ = new QTreeWidget;
    tree_->setHeaderHidden(true);
    tree_->setMinimumWidth(230);
    tree_->setMaximumWidth(380);
    split->addWidget(tree_);

    auto* center = new QWidget;
    auto* centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(12, 0, 8, 0);

    auto* metadata = new QHBoxLayout;
    sceneTitle_ = new QLineEdit;
    sceneTitle_->setPlaceholderText(tr("Título de escena"));
    scenePov_ = new QLineEdit;
    scenePov_->setPlaceholderText(tr("POV"));
    sceneLocation_ = new QLineEdit;
    sceneLocation_->setPlaceholderText(tr("Localización"));
    sceneLayer_ = new QLineEdit;
    sceneLayer_->setPlaceholderText(tr("Capa narrativa"));
    sceneStatus_ = new QComboBox;
    sceneStatus_->addItems({tr("Borrador"), tr("Revisión"), tr("Final")});
    metadata->addWidget(sceneTitle_, 3);
    metadata->addWidget(scenePov_, 1);
    metadata->addWidget(sceneLocation_, 1);
    metadata->addWidget(sceneLayer_, 1);
    metadata->addWidget(sceneStatus_, 1);
    centerLayout->addLayout(metadata);

    auto* format = new QHBoxLayout;
    bold_ = new QToolButton;
    bold_->setText(tr("Negrita"));
    bold_->setCheckable(true);
    bold_->setShortcut(QKeySequence::Bold);
    italic_ = new QToolButton;
    italic_->setText(tr("Cursiva"));
    italic_->setCheckable(true);
    italic_->setShortcut(QKeySequence::Italic);
    underline_ = new QToolButton;
    underline_->setText(tr("Subrayado"));
    underline_->setCheckable(true);
    underline_->setShortcut(QKeySequence::Underline);
    auto* undo = new QToolButton;
    undo->setText(tr("Deshacer"));
    undo->setShortcut(QKeySequence::Undo);
    auto* redo = new QToolButton;
    redo->setText(tr("Rehacer"));
    redo->setShortcut(QKeySequence::Redo);
    format->addWidget(bold_);
    format->addWidget(italic_);
    format->addWidget(underline_);
    format->addSpacing(8);
    format->addWidget(undo);
    format->addWidget(redo);
    format->addStretch();
    wordCount_ = new QLabel;
    format->addWidget(wordCount_);
    centerLayout->addLayout(format);

    editor_ = new QTextEdit;
    editor_->setAcceptRichText(true);
    editor_->setUndoRedoEnabled(true);
    editor_->setLineWrapMode(QTextEdit::WidgetWidth);
    editor_->setPlaceholderText(tr("Escribe aquí…"));
    centerLayout->addWidget(editor_, 1);
    split->addWidget(center);

    auto* referencePanel = new QWidget;
    auto* referenceLayout = new QVBoxLayout(referencePanel);
    referenceLayout->setContentsMargins(8, 0, 0, 0);
    auto* referenceTitle = new QLabel(tr("Referencias detectadas"));
    referenceTitle->setStyleSheet(QStringLiteral("font-weight:700;"));
    references_ = new QListWidget;
    references_->setMinimumWidth(210);
    references_->setMaximumWidth(320);
    auto* referenceHint = new QLabel(tr("Doble clic para abrir el registro."));
    referenceHint->setWordWrap(true);
    referenceHint->setStyleSheet(QStringLiteral("color:#8f8e86;"));
    referenceLayout->addWidget(referenceTitle);
    referenceLayout->addWidget(references_, 1);
    referenceLayout->addWidget(referenceHint);
    split->addWidget(referencePanel);
    split->setStretchFactor(1, 1);
    outer->addWidget(split, 1);

    connect(tree_, &QTreeWidget::itemSelectionChanged, this, &WritingPage::selectItem);
    connect(addChapterButton, &QPushButton::clicked, this, &WritingPage::addChapter);
    connect(addSceneButton, &QPushButton::clicked, this, &WritingPage::addScene);
    connect(renameButton, &QPushButton::clicked, this, &WritingPage::renameChapter);
    connect(removeButton, &QPushButton::clicked, this, &WritingPage::removeItem);
    connect(upButton, &QPushButton::clicked, this, [this]() { moveItem(-1); });
    connect(downButton, &QPushButton::clicked, this, [this]() { moveItem(1); });
    connect(importButton, &QPushButton::clicked, this, &WritingPage::importManuscript);
    connect(focusButton, &QPushButton::clicked, this, &WritingPage::openFocusMode);
    connect(sceneTitle_, &QLineEdit::editingFinished, this, &WritingPage::applyScene);
    connect(scenePov_, &QLineEdit::editingFinished, this, &WritingPage::applyScene);
    connect(sceneLocation_, &QLineEdit::editingFinished, this, &WritingPage::applyScene);
    connect(sceneLayer_, &QLineEdit::editingFinished, this, &WritingPage::applyScene);
    connect(sceneStatus_, &QComboBox::currentTextChanged, this, &WritingPage::applyScene);
    connect(editor_, &QTextEdit::textChanged, this, &WritingPage::applyScene);
    connect(editor_, &QTextEdit::cursorPositionChanged, this, &WritingPage::updateFormattingState);
    connect(editor_, &QTextEdit::currentCharFormatChanged, this, &WritingPage::updateFormattingState);
    connect(bold_, &QToolButton::toggled, this, [this](bool checked) { applyCharacterFormat(0, checked); });
    connect(italic_, &QToolButton::toggled, this, [this](bool checked) { applyCharacterFormat(1, checked); });
    connect(underline_, &QToolButton::toggled, this, [this](bool checked) { applyCharacterFormat(2, checked); });
    connect(undo, &QToolButton::clicked, editor_, &QTextEdit::undo);
    connect(redo, &QToolButton::clicked, editor_, &QTextEdit::redo);
    connect(references_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        emit referenceActivated(item->data(Qt::UserRole).toString(), item->data(Qt::UserRole + 1).toString());
    });
    return tab;
}

QWidget* WritingPage::buildSceneBoardTab() {
    auto* tab = new QWidget;
    auto* layout = new QVBoxLayout(tab);
    auto* top = new QHBoxLayout;
    sceneBoardCompact_ = new QCheckBox(tr("Vista compacta"));
    auto* reset = makeButton(tr("Recentrar"));
    top->addWidget(sceneBoardCompact_);
    top->addStretch();
    top->addWidget(reset);
    layout->addLayout(top);
    sceneBoard_ = new QGraphicsView;
    sceneBoard_->setScene(new QGraphicsScene(sceneBoard_));
    sceneBoard_->setDragMode(QGraphicsView::ScrollHandDrag);
    sceneBoard_->setRenderHint(QPainter::Antialiasing, true);
    sceneBoard_->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    layout->addWidget(sceneBoard_, 1);
    connect(sceneBoardCompact_, &QCheckBox::toggled, this, [this](bool checked) {
        if (!document_) return;
        QJsonObject profile = document_->object(QStringLiteral("profile"));
        profile.insert(QStringLiteral("sceneBoardCompact"), checked);
        document_->setObject(QStringLiteral("profile"), profile);
        refreshSceneBoard();
        emit changed();
    });
    connect(reset, &QPushButton::clicked, this, [this]() {
        sceneBoard_->resetTransform();
        if (sceneBoard_->scene()) sceneBoard_->fitInView(sceneBoard_->scene()->itemsBoundingRect().adjusted(-30, -30, 30, 30), Qt::KeepAspectRatio);
    });
    return tab;
}

void WritingPage::refresh() {
    if (!document_) return;
    refreshing_ = true;
    refreshTree();
    const QJsonObject profile = document_->object(QStringLiteral("profile"));
    sceneBoardCompact_->setChecked(profile.value(QStringLiteral("sceneBoardCompact")).toBool(false));
    refreshSceneBoard();
    refreshing_ = false;
    if (tree_->topLevelItemCount() > 0) {
        auto* chapter = tree_->topLevelItem(0);
        tree_->setCurrentItem(chapter->childCount() ? chapter->child(0) : chapter);
    } else {
        selectItem();
    }
}

void WritingPage::refreshTree() {
    tree_->clear();
    const QJsonArray chapters = document_->array(QStringLiteral("writingChapters"));
    for (int c = 0; c < chapters.size(); ++c) {
        const QJsonObject chapter = chapters.at(c).toObject();
        const QString label = chapter.value(QStringLiteral("label")).toString(tr("Capítulo"));
        const QString title = chapter.value(QStringLiteral("title")).toString();
        auto* chapterItem = new QTreeWidgetItem({title.isEmpty() ? label : label + QStringLiteral(" — ") + title});
        chapterItem->setData(0, Qt::UserRole, QStringLiteral("chapter"));
        chapterItem->setData(0, Qt::UserRole + 1, c);
        const QJsonArray scenes = chapter.value(QStringLiteral("scenes")).toArray();
        for (int s = 0; s < scenes.size(); ++s) {
            const QJsonObject scene = scenes.at(s).toObject();
            auto* sceneItem = new QTreeWidgetItem({scene.value(QStringLiteral("title")).toString(tr("Escena"))});
            sceneItem->setData(0, Qt::UserRole, QStringLiteral("scene"));
            sceneItem->setData(0, Qt::UserRole + 1, c);
            sceneItem->setData(0, Qt::UserRole + 2, s);
            chapterItem->addChild(sceneItem);
        }
        tree_->addTopLevelItem(chapterItem);
        chapterItem->setExpanded(true);
    }
}

void WritingPage::selectItem() {
    if (!document_) return;
    refreshing_ = true;
    auto* item = tree_->currentItem();
    const bool sceneSelected = item && item->data(0, Qt::UserRole).toString() == QStringLiteral("scene");
    for (QWidget* widget : {static_cast<QWidget*>(sceneTitle_), static_cast<QWidget*>(scenePov_), static_cast<QWidget*>(sceneLocation_), static_cast<QWidget*>(sceneLayer_), static_cast<QWidget*>(sceneStatus_), static_cast<QWidget*>(editor_)}) widget->setEnabled(sceneSelected);
    if (!sceneSelected) {
        sceneTitle_->clear();
        scenePov_->clear();
        sceneLocation_->clear();
        sceneLayer_->clear();
        sceneStatus_->setCurrentIndex(0);
        editor_->clear();
        references_->clear();
        wordCount_->clear();
        refreshing_ = false;
        return;
    }
    const int c = item->data(0, Qt::UserRole + 1).toInt();
    const int s = item->data(0, Qt::UserRole + 2).toInt();
    const QJsonArray chapters = document_->array(QStringLiteral("writingChapters"));
    if (c < 0 || c >= chapters.size()) { refreshing_ = false; return; }
    const QJsonArray scenes = chapters.at(c).toObject().value(QStringLiteral("scenes")).toArray();
    if (s < 0 || s >= scenes.size()) { refreshing_ = false; return; }
    const QJsonObject scene = scenes.at(s).toObject();
    sceneTitle_->setText(scene.value(QStringLiteral("title")).toString());
    scenePov_->setText(scene.value(QStringLiteral("pov")).toString());
    sceneLocation_->setText(scene.value(QStringLiteral("location")).toString());
    sceneLayer_->setText(scene.value(QStringLiteral("narrativeLayer")).toString());
    const int status = sceneStatus_->findText(scene.value(QStringLiteral("status")).toString());
    sceneStatus_->setCurrentIndex(status >= 0 ? status : 0);
    const QString content = scene.value(QStringLiteral("content")).toString();
    editor_->setHtml(content);
    refreshing_ = false;
    updateFormattingState();
    refreshReferences();
    updateWordCount();
}

void WritingPage::applyScene() {
    if (refreshing_ || !document_) return;
    auto* item = tree_->currentItem();
    if (!item || item->data(0, Qt::UserRole).toString() != QStringLiteral("scene")) return;
    const int c = item->data(0, Qt::UserRole + 1).toInt();
    const int s = item->data(0, Qt::UserRole + 2).toInt();
    QJsonArray chapters = document_->array(QStringLiteral("writingChapters"));
    if (c < 0 || c >= chapters.size()) return;
    QJsonObject chapter = chapters.at(c).toObject();
    QJsonArray scenes = chapter.value(QStringLiteral("scenes")).toArray();
    if (s < 0 || s >= scenes.size()) return;
    QJsonObject scene = scenes.at(s).toObject();
    scene.insert(QStringLiteral("title"), sceneTitle_->text());
    scene.insert(QStringLiteral("pov"), scenePov_->text());
    scene.insert(QStringLiteral("location"), sceneLocation_->text());
    scene.insert(QStringLiteral("narrativeLayer"), sceneLayer_->text());
    scene.insert(QStringLiteral("status"), sceneStatus_->currentText());
    scene.insert(QStringLiteral("content"), editor_->toHtml());
    scene.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    scenes.replace(s, scene);
    chapter.insert(QStringLiteral("scenes"), scenes);
    chapters.replace(c, chapter);
    document_->setArray(QStringLiteral("writingChapters"), chapters);
    item->setText(0, sceneTitle_->text().isEmpty() ? tr("Escena") : sceneTitle_->text());
    updateWordCount();
    refreshReferences();
    refreshSceneBoard();
    emit changed();
}

void WritingPage::addChapter() {
    if (!document_) return;
    QJsonArray chapters = document_->array(QStringLiteral("writingChapters"));
    const int number = chapters.size() + 1;
    chapters.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("chapter"))}, {QStringLiteral("label"), tr("Capítulo %1").arg(number)}, {QStringLiteral("title"), tr("Sin título")}, {QStringLiteral("order"), chapters.size()}, {QStringLiteral("scenes"), QJsonArray()}});
    document_->setArray(QStringLiteral("writingChapters"), chapters);
    refreshTree();
    tree_->setCurrentItem(tree_->topLevelItem(chapters.size() - 1));
    refreshSceneBoard();
    emit changed();
}

void WritingPage::addScene() {
    if (!document_) return;
    QJsonArray chapters = document_->array(QStringLiteral("writingChapters"));
    if (chapters.isEmpty()) {
        addChapter();
        chapters = document_->array(QStringLiteral("writingChapters"));
    }
    int c = 0;
    if (auto* item = tree_->currentItem()) c = item->data(0, Qt::UserRole + 1).toInt();
    c = qBound(0, c, chapters.size() - 1);
    QJsonObject chapter = chapters.at(c).toObject();
    QJsonArray scenes = chapter.value(QStringLiteral("scenes")).toArray();
    scenes.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("scene"))}, {QStringLiteral("chapterId"), chapter.value(QStringLiteral("id"))}, {QStringLiteral("order"), scenes.size()}, {QStringLiteral("title"), tr("Escena %1").arg(scenes.size() + 1)}, {QStringLiteral("content"), QString()}, {QStringLiteral("pov"), QString()}, {QStringLiteral("location"), QString()}, {QStringLiteral("narrativeLayer"), QString()}, {QStringLiteral("status"), tr("Borrador")}, {QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}});
    chapter.insert(QStringLiteral("scenes"), scenes);
    chapters.replace(c, chapter);
    document_->setArray(QStringLiteral("writingChapters"), chapters);
    refreshTree();
    auto* chapterItem = tree_->topLevelItem(c);
    if (chapterItem && chapterItem->childCount()) tree_->setCurrentItem(chapterItem->child(chapterItem->childCount() - 1));
    refreshSceneBoard();
    emit changed();
}

void WritingPage::removeItem() {
    if (!document_) return;
    auto* item = tree_->currentItem();
    if (!item) return;
    QJsonArray chapters = document_->array(QStringLiteral("writingChapters"));
    const QString kind = item->data(0, Qt::UserRole).toString();
    const int c = item->data(0, Qt::UserRole + 1).toInt();
    if (c < 0 || c >= chapters.size()) return;
    if (kind == QStringLiteral("chapter")) {
        chapters.removeAt(c);
    } else if (kind == QStringLiteral("scene")) {
        QJsonObject chapter = chapters.at(c).toObject();
        QJsonArray scenes = chapter.value(QStringLiteral("scenes")).toArray();
        const int s = item->data(0, Qt::UserRole + 2).toInt();
        if (s >= 0 && s < scenes.size()) scenes.removeAt(s);
        chapter.insert(QStringLiteral("scenes"), scenes);
        chapters.replace(c, chapter);
    }
    normalizeOrders(chapters);
    document_->setArray(QStringLiteral("writingChapters"), chapters);
    refreshTree();
    refreshSceneBoard();
    emit changed();
}

void WritingPage::moveItem(int delta) {
    if (!document_ || delta == 0) return;
    auto* item = tree_->currentItem();
    if (!item) return;
    QJsonArray chapters = document_->array(QStringLiteral("writingChapters"));
    const QString kind = item->data(0, Qt::UserRole).toString();
    const int c = item->data(0, Qt::UserRole + 1).toInt();
    if (kind == QStringLiteral("chapter")) {
        const int next = c + delta;
        if (next < 0 || next >= chapters.size()) return;
        const QJsonValue value = chapters.takeAt(c);
        chapters.insert(next, value);
        normalizeOrders(chapters);
        document_->setArray(QStringLiteral("writingChapters"), chapters);
        refreshTree();
        tree_->setCurrentItem(tree_->topLevelItem(next));
    } else if (kind == QStringLiteral("scene") && c >= 0 && c < chapters.size()) {
        QJsonObject chapter = chapters.at(c).toObject();
        QJsonArray scenes = chapter.value(QStringLiteral("scenes")).toArray();
        const int s = item->data(0, Qt::UserRole + 2).toInt();
        const int next = s + delta;
        if (next < 0 || next >= scenes.size()) return;
        const QJsonValue value = scenes.takeAt(s);
        scenes.insert(next, value);
        chapter.insert(QStringLiteral("scenes"), scenes);
        chapters.replace(c, chapter);
        normalizeOrders(chapters);
        document_->setArray(QStringLiteral("writingChapters"), chapters);
        refreshTree();
        tree_->setCurrentItem(tree_->topLevelItem(c)->child(next));
    }
    refreshSceneBoard();
    emit changed();
}

void WritingPage::renameChapter() {
    if (!document_) return;
    auto* item = tree_->currentItem();
    if (!item) return;
    int c = item->data(0, Qt::UserRole + 1).toInt();
    QJsonArray chapters = document_->array(QStringLiteral("writingChapters"));
    if (c < 0 || c >= chapters.size()) return;
    QJsonObject chapter = chapters.at(c).toObject();
    bool ok = false;
    const QString title = QInputDialog::getText(this, tr("Título del capítulo"), tr("Título:"), QLineEdit::Normal, chapter.value(QStringLiteral("title")).toString(), &ok);
    if (!ok) return;
    const QString label = QInputDialog::getText(this, tr("Etiqueta del capítulo"), tr("Etiqueta:"), QLineEdit::Normal, chapter.value(QStringLiteral("label")).toString(), &ok);
    if (!ok) return;
    chapter.insert(QStringLiteral("title"), title.trimmed());
    chapter.insert(QStringLiteral("label"), label.trimmed());
    chapters.replace(c, chapter);
    document_->setArray(QStringLiteral("writingChapters"), chapters);
    refreshTree();
    tree_->setCurrentItem(tree_->topLevelItem(c));
    refreshSceneBoard();
    emit changed();
}

void WritingPage::importManuscript() {
    if (!document_) return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Importar manuscrito"), QString(), tr("Manuscritos (*.txt *.md *.markdown *.docx)"));
    if (path.isEmpty()) return;
    QJsonArray imported;
    QString error;
    if (!ManuscriptImporter::importFile(path, imported, &error)) {
        QMessageBox::critical(this, tr("No se pudo importar"), error);
        return;
    }
    QJsonArray chapters = document_->array(QStringLiteral("writingChapters"));
    if (!chapters.isEmpty()) {
        const auto answer = QMessageBox::question(this, tr("Importar manuscrito"), tr("¿Quieres reemplazar el manuscrito actual? Elige No para añadir los capítulos al final."), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::No);
        if (answer == QMessageBox::Cancel) return;
        if (answer == QMessageBox::Yes) chapters = QJsonArray();
    }
    for (const QJsonValue value : imported) chapters.append(value);
    normalizeOrders(chapters);
    document_->setArray(QStringLiteral("writingChapters"), chapters);
    QJsonObject manuscript = document_->object(QStringLiteral("manuscript"));
    manuscript.insert(QStringLiteral("fileName"), QFileInfo(path).fileName());
    manuscript.insert(QStringLiteral("chapters"), chapters.size());
    manuscript.insert(QStringLiteral("updatedLabel"), QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")));
    document_->setObject(QStringLiteral("manuscript"), manuscript);
    refresh();
    emit changed();
}

void WritingPage::applyCharacterFormat(int property, bool enabled) {
    if (refreshing_ || !editor_->isEnabled()) return;
    QTextCharFormat format;
    if (property == 0) format.setFontWeight(enabled ? QFont::Bold : QFont::Normal);
    else if (property == 1) format.setFontItalic(enabled);
    else if (property == 2) format.setFontUnderline(enabled);
    editor_->mergeCurrentCharFormat(format);
    editor_->setFocus();
}

void WritingPage::updateFormattingState() {
    if (!editor_) return;
    const QTextCharFormat format = editor_->currentCharFormat();
    refreshing_ = true;
    bold_->setChecked(format.fontWeight() >= QFont::DemiBold);
    italic_->setChecked(format.fontItalic());
    underline_->setChecked(format.fontUnderline());
    refreshing_ = false;
}

void WritingPage::refreshReferences() {
    references_->clear();
    if (!document_ || !editor_->isEnabled()) return;
    const QString content = editor_->toPlainText();
    auto scan = [&](const QString& key, const QString& kind, const QString& labelKey) {
        for (const QJsonValue value : document_->array(key)) {
            const QJsonObject object = value.toObject();
            const QString label = object.value(labelKey).toString();
            if (label.size() < 2 || !content.contains(label, Qt::CaseInsensitive)) continue;
            auto* item = new QListWidgetItem(QStringLiteral("%1 · %2").arg(kind, label));
            item->setData(Qt::UserRole, kind);
            item->setData(Qt::UserRole + 1, object.value(QStringLiteral("id")).toString());
            references_->addItem(item);
        }
    };
    scan(QStringLiteral("characters"), tr("Personaje"), QStringLiteral("name"));
    scan(QStringLiteral("world"), tr("Mundo"), QStringLiteral("name"));
    scan(QStringLiteral("magicSystems"), tr("Magia"), QStringLiteral("name"));
    scan(QStringLiteral("worldTexts"), tr("Texto mundo"), QStringLiteral("title"));
    scan(QStringLiteral("magicTexts"), tr("Texto magia"), QStringLiteral("title"));
}

void WritingPage::updateWordCount() {
    const int words = editor_->toPlainText().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts).size();
    wordCount_->setText(tr("%1 palabras").arg(words));
}

void WritingPage::refreshSceneBoard() {
    if (!sceneBoard_ || !sceneBoard_->scene() || !document_) return;
    auto* scene = sceneBoard_->scene();
    scene->clear();
    const bool compact = sceneBoardCompact_->isChecked();
    const qreal cardWidth = compact ? 150.0 : 220.0;
    const qreal cardHeight = compact ? 58.0 : 92.0;
    const qreal gapX = compact ? 175.0 : 250.0;
    const qreal gapY = compact ? 78.0 : 112.0;
    const QJsonArray chapters = document_->array(QStringLiteral("writingChapters"));
    for (int c = 0; c < chapters.size(); ++c) {
        const QJsonObject chapter = chapters.at(c).toObject();
        auto* heading = scene->addSimpleText(chapter.value(QStringLiteral("label")).toString(tr("Capítulo")));
        heading->setBrush(QColor(QStringLiteral("#d1b16e")));
        QFont headingFont = heading->font();
        headingFont.setBold(true);
        heading->setFont(headingFont);
        heading->setPos(c * gapX, 0.0);
        const QJsonArray scenes = chapter.value(QStringLiteral("scenes")).toArray();
        for (int s = 0; s < scenes.size(); ++s) {
            const QJsonObject sceneObject = scenes.at(s).toObject();
            auto* card = scene->addRect(QRectF(0, 0, cardWidth, cardHeight), QPen(QColor(QStringLiteral("#484940"))), QBrush(QColor(QStringLiteral("#24251f"))));
            card->setPos(c * gapX, 38.0 + s * gapY);
            auto* title = new QGraphicsSimpleTextItem(sceneObject.value(QStringLiteral("title")).toString(tr("Escena")), card);
            title->setBrush(QColor(QStringLiteral("#efe9dc")));
            title->setPos(10.0, 8.0);
            if (!compact) {
                const QString detail = QStringLiteral("%1 · %2").arg(sceneObject.value(QStringLiteral("pov")).toString(), sceneObject.value(QStringLiteral("location")).toString());
                auto* meta = new QGraphicsSimpleTextItem(detail, card);
                meta->setBrush(QColor(QStringLiteral("#98968d")));
                meta->setPos(10.0, 34.0);
                const int words = contentPlainText(sceneObject.value(QStringLiteral("content")).toString()).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts).size();
                auto* stats = new QGraphicsSimpleTextItem(tr("%1 palabras · %2").arg(words).arg(sceneObject.value(QStringLiteral("status")).toString()), card);
                stats->setBrush(QColor(QStringLiteral("#8d8b83")));
                stats->setPos(10.0, 58.0);
            }
        }
    }
    scene->setSceneRect(scene->itemsBoundingRect().adjusted(-60, -60, 60, 60));
}

void WritingPage::openFocusMode() {
    if (!editor_->isEnabled()) return;
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Modo enfoque"));
    dialog.setWindowState(Qt::WindowFullScreen);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(80, 36, 80, 36);
    auto* title = new QLabel(sceneTitle_->text());
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral("font-size:18px;font-weight:700;"));
    auto* focusEditor = new QTextEdit;
    focusEditor->setAcceptRichText(true);
    focusEditor->setUndoRedoEnabled(true);
    focusEditor->setHtml(editor_->toHtml());
    focusEditor->setStyleSheet(QStringLiteral("QTextEdit{font-size:18px;background:#181914;border:0;padding:34px;}"));
    auto* close = makeButton(tr("Cerrar enfoque"));
    layout->addWidget(title);
    layout->addWidget(focusEditor, 1);
    layout->addWidget(close, 0, Qt::AlignRight);
    connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    if (dialog.exec() == QDialog::Accepted) {
        editor_->setHtml(focusEditor->toHtml());
        applyScene();
    }
}

} // namespace wbw
