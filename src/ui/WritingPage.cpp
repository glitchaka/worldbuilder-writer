#include "ui/WritingPage.h"

#include "core/ArchiveDocument.h"
#include "import/ManuscriptImporter.h"
#include "ui/SemanticTextEdit.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QFileDialog>
#include <QFont>
#include <QFrame>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QShortcut>
#include <QSplitter>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
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

QString scenePlainText(const QJsonObject& scene) {
    if (scene.contains(QStringLiteral("text"))) return scene.value(QStringLiteral("text")).toString();
    QTextDocument doc;
    doc.setHtml(scene.value(QStringLiteral("content")).toString());
    return doc.toPlainText();
}

QLabel* sectionLabel(const QString& text) {
    auto* label = new QLabel(text);
    label->setObjectName(QStringLiteral("sectionLabel"));
    return label;
}

} // namespace

WritingPage::WritingPage(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("writingPage"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    tabs_ = new QTabWidget;
    tabs_->setObjectName(QStringLiteral("writingTabs"));
    tabs_->addTab(buildEditorTab(), tr("Manuscrito"));
    tabs_->addTab(buildSceneBoardTab(), tr("Tablero de escenas"));
    root->addWidget(tabs_);

    setStyleSheet(QStringLiteral(
        "#writingPage{background:#edf1f5;}"
        "#writingTabs::pane{border:0;background:#edf1f5;}"
        "#writingTabs QTabBar::tab{padding:11px 16px;}"
        "#writingEditorTab,#sceneBoardTab{background:#edf1f5;}"
        "#writingHero{background:#f5f8fc;border:1px solid #d7dee8;}"
        "#writingKicker,#sectionLabel{color:#8a6a52;font-size:8pt;font-weight:700;letter-spacing:1px;}"
        "#writingTitle{color:#d2aa69;font-family:'Georgia';font-size:28pt;}"
        "#writingDescription{color:#667085;font-family:'Georgia';font-size:10pt;}"
        "#indexPanel,#editorPanel,#metadataPanel{background:#ffffff;border:1px solid #d5dce5;}"
        "#indexTitle,#editorPanelTitle{color:#344054;font-size:8pt;font-weight:700;letter-spacing:1px;}"
        "#sceneEditor{background:#ffffff;border:0;padding:30px 42px;font-family:'Georgia';font-size:12pt;selection-background-color:#dceafe;}"
        "#writingPrimary{background:#1668d4;color:#ffffff;border:1px solid #1668d4;font-weight:600;}"
        "#writingPrimary:hover{background:#0f5fc8;}"
        "#writingSubtle{background:#ffffff;color:#344054;border:1px solid #cbd3dd;}"
        "#proofState{color:#667085;font-size:8.5pt;}"
    ));
}

void WritingPage::setDocument(ArchiveDocument* document) {
    document_ = document;
    if (editor_) editor_->setArchiveDocument(document_);
    refresh();
}

QWidget* WritingPage::buildEditorTab() {
    auto* tab = new QWidget;
    tab->setObjectName(QStringLiteral("writingEditorTab"));
    auto* outer = new QVBoxLayout(tab);
    outer->setContentsMargins(30, 22, 30, 30);
    outer->setSpacing(14);

    auto* hero = new QFrame;
    hero->setObjectName(QStringLiteral("writingHero"));
    auto* heroLayout = new QHBoxLayout(hero);
    heroLayout->setContentsMargins(20, 14, 18, 14);
    heroLayout->setSpacing(18);
    auto* heroCopy = new QVBoxLayout;
    heroCopy->setSpacing(2);
    auto* kicker = new QLabel(tr("CAPÍTULOS / ESCENAS / CAPAS NARRATIVAS"));
    kicker->setObjectName(QStringLiteral("writingKicker"));
    auto* title = new QLabel(tr("Manuscrito"));
    title->setObjectName(QStringLiteral("writingTitle"));
    auto* description = new QLabel(tr("El texto es la superficie principal. Índice y detalles se abren solo cuando los necesitas."));
    description->setObjectName(QStringLiteral("writingDescription"));
    heroCopy->addWidget(kicker);
    heroCopy->addWidget(title);
    heroCopy->addWidget(description);
    heroLayout->addLayout(heroCopy, 1);

    auto* importButton = makeButton(tr("Importar manuscrito"));
    importButton->setObjectName(QStringLiteral("writingSubtle"));
    auto* focusButton = makeButton(tr("Sin distracciones"));
    focusButton->setObjectName(QStringLiteral("writingSubtle"));
    auto* addSceneButton = makeButton(tr("+ Escena"));
    addSceneButton->setObjectName(QStringLiteral("writingSubtle"));
    auto* addChapterButton = makeButton(tr("+ Capítulo"));
    addChapterButton->setObjectName(QStringLiteral("writingPrimary"));
    heroLayout->addWidget(importButton);
    heroLayout->addWidget(focusButton);
    heroLayout->addWidget(addSceneButton);
    heroLayout->addWidget(addChapterButton);
    outer->addWidget(hero);

    auto* commandBar = new QHBoxLayout;
    commandBar->setSpacing(6);
    auto* toggleIndex = new QToolButton;
    toggleIndex->setText(tr("Índice"));
    toggleIndex->setCheckable(true);
    toggleIndex->setChecked(true);
    auto* toggleDetails = new QToolButton;
    toggleDetails->setText(tr("Detalles"));
    toggleDetails->setCheckable(true);
    toggleDetails->setChecked(false);
    bold_ = new QToolButton;
    bold_->setText(tr("B"));
    bold_->setToolTip(tr("Negrita"));
    bold_->setCheckable(true);
    bold_->setShortcut(QKeySequence::Bold);
    italic_ = new QToolButton;
    italic_->setText(tr("I"));
    italic_->setToolTip(tr("Cursiva"));
    italic_->setCheckable(true);
    italic_->setShortcut(QKeySequence::Italic);
    underline_ = new QToolButton;
    underline_->setText(tr("U"));
    underline_->setToolTip(tr("Subrayado"));
    underline_->setCheckable(true);
    underline_->setShortcut(QKeySequence::Underline);
    auto* undo = new QToolButton;
    undo->setText(tr("↶"));
    undo->setToolTip(tr("Deshacer"));
    undo->setShortcut(QKeySequence::Undo);
    auto* redo = new QToolButton;
    redo->setText(tr("↷"));
    redo->setToolTip(tr("Rehacer"));
    redo->setShortcut(QKeySequence::Redo);
    auto* proof = makeButton(tr("Revisar texto"));
    proof->setObjectName(QStringLiteral("writingSubtle"));
    proofState_ = new QLabel;
    proofState_->setObjectName(QStringLiteral("proofState"));
    wordCount_ = new QLabel;
    wordCount_->setStyleSheet(QStringLiteral("color:#667085;"));

    commandBar->addWidget(toggleIndex);
    commandBar->addWidget(toggleDetails);
    commandBar->addSpacing(10);
    commandBar->addWidget(bold_);
    commandBar->addWidget(italic_);
    commandBar->addWidget(underline_);
    commandBar->addWidget(undo);
    commandBar->addWidget(redo);
    commandBar->addSpacing(10);
    commandBar->addWidget(proof);
    commandBar->addWidget(proofState_);
    commandBar->addStretch();
    commandBar->addWidget(wordCount_);
    outer->addLayout(commandBar);

    editorSplit_ = new QSplitter;
    editorSplit_->setChildrenCollapsible(false);
    editorSplit_->setHandleWidth(6);

    indexPanel_ = new QWidget;
    indexPanel_->setObjectName(QStringLiteral("indexPanel"));
    auto* indexLayout = new QVBoxLayout(indexPanel_);
    indexLayout->setContentsMargins(10, 10, 10, 10);
    auto* indexHeader = new QHBoxLayout;
    auto* indexTitle = new QLabel(tr("ÍNDICE"));
    indexTitle->setObjectName(QStringLiteral("indexTitle"));
    auto* removeButton = makeButton(tr("Eliminar"));
    indexHeader->addWidget(indexTitle);
    indexHeader->addStretch();
    indexHeader->addWidget(removeButton);
    indexLayout->addLayout(indexHeader);
    tree_ = new QTreeWidget;
    tree_->setHeaderHidden(true);
    tree_->setMinimumWidth(220);
    tree_->setMaximumWidth(330);
    tree_->setIndentation(16);
    indexLayout->addWidget(tree_, 1);
    auto* indexActions = new QHBoxLayout;
    auto* renameButton = makeButton(tr("Renombrar"));
    auto* upButton = makeButton(tr("↑"));
    auto* downButton = makeButton(tr("↓"));
    indexActions->addWidget(renameButton, 1);
    indexActions->addWidget(upButton);
    indexActions->addWidget(downButton);
    indexLayout->addLayout(indexActions);
    editorSplit_->addWidget(indexPanel_);

    auto* center = new QWidget;
    center->setObjectName(QStringLiteral("editorPanel"));
    auto* centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);

    metadataPanel_ = new QWidget;
    metadataPanel_->setObjectName(QStringLiteral("metadataPanel"));
    auto* metadata = new QGridLayout(metadataPanel_);
    metadata->setContentsMargins(12, 10, 12, 10);
    metadata->setHorizontalSpacing(8);
    metadata->setVerticalSpacing(6);
    sceneTitle_ = new QLineEdit;
    sceneTitle_->setPlaceholderText(tr("Título de escena"));
    scenePov_ = new QLineEdit;
    scenePov_->setPlaceholderText(tr("Personaje focal"));
    sceneLocation_ = new QLineEdit;
    sceneLocation_->setPlaceholderText(tr("Lugar de la escena"));
    sceneLayer_ = new QLineEdit;
    sceneLayer_->setPlaceholderText(tr("Capa narrativa"));
    sceneStatus_ = new QComboBox;
    sceneStatus_->addItems({tr("Borrador"), tr("Revisión"), tr("Final")});
    metadata->addWidget(sectionLabel(tr("Título")), 0, 0);
    metadata->addWidget(sectionLabel(tr("Estado")), 0, 2);
    metadata->addWidget(sceneTitle_, 1, 0, 1, 2);
    metadata->addWidget(sceneStatus_, 1, 2);
    metadata->addWidget(sectionLabel(tr("POV")), 2, 0);
    metadata->addWidget(sectionLabel(tr("Ubicación")), 2, 1);
    metadata->addWidget(sectionLabel(tr("Capa narrativa")), 2, 2);
    metadata->addWidget(scenePov_, 3, 0);
    metadata->addWidget(sceneLocation_, 3, 1);
    metadata->addWidget(sceneLayer_, 3, 2);
    metadataPanel_->hide();
    centerLayout->addWidget(metadataPanel_);

    editor_ = new SemanticTextEdit;
    editor_->setObjectName(QStringLiteral("sceneEditor"));
    editor_->setAcceptRichText(false);
    editor_->setUndoRedoEnabled(true);
    editor_->setLineWrapMode(QTextEdit::WidgetWidth);
    editor_->setPlaceholderText(tr("Escribe aquí…"));
    centerLayout->addWidget(editor_, 1);
    editorSplit_->addWidget(center);
    editorSplit_->setStretchFactor(0, 0);
    editorSplit_->setStretchFactor(1, 1);
    editorSplit_->setSizes({260, 1100});
    outer->addWidget(editorSplit_, 1);

    connect(toggleIndex, &QToolButton::toggled, indexPanel_, &QWidget::setVisible);
    connect(toggleDetails, &QToolButton::toggled, metadataPanel_, &QWidget::setVisible);
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
    connect(editor_, &SemanticTextEdit::referenceActivated, this, &WritingPage::referenceActivated);
    connect(editor_, &SemanticTextEdit::proofreadStarted, this, [this]() { proofState_->setText(tr("Revisando…")); });
    connect(editor_, &SemanticTextEdit::proofreadFinished, this, [this](int count) { proofState_->setText(tr("%1 observaciones").arg(count)); });
    connect(editor_, &SemanticTextEdit::proofreadError, this, [this](const QString& error) { proofState_->setText(error); });
    connect(proof, &QPushButton::clicked, editor_, &SemanticTextEdit::runProofread);
    connect(bold_, &QToolButton::toggled, this, [this](bool checked) { applyCharacterFormat(0, checked); });
    connect(italic_, &QToolButton::toggled, this, [this](bool checked) { applyCharacterFormat(1, checked); });
    connect(underline_, &QToolButton::toggled, this, [this](bool checked) { applyCharacterFormat(2, checked); });
    connect(undo, &QToolButton::clicked, editor_, &QTextEdit::undo);
    connect(redo, &QToolButton::clicked, editor_, &QTextEdit::redo);
    return tab;
}

QWidget* WritingPage::buildSceneBoardTab() {
    auto* tab = new QWidget;
    tab->setObjectName(QStringLiteral("sceneBoardTab"));
    auto* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(30, 22, 30, 30);
    auto* top = new QHBoxLayout;
    auto* heading = new QLabel(tr("Tablero de escenas"));
    QFont headingFont = heading->font();
    headingFont.setFamily(QStringLiteral("Georgia"));
    headingFont.setPointSize(20);
    heading->setFont(headingFont);
    sceneBoardCompact_ = new QCheckBox(tr("Vista compacta"));
    auto* reset = makeButton(tr("Recentrar"));
    top->addWidget(heading);
    top->addStretch();
    top->addWidget(sceneBoardCompact_);
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
    editor_->setArchiveDocument(document_);
    refreshTree();
    const QJsonObject profile = document_->object(QStringLiteral("profile"));
    sceneBoardCompact_->setChecked(profile.value(QStringLiteral("sceneBoardCompact")).toBool(false));
    refreshSceneBoard();
    refreshing_ = false;
    if (tree_->topLevelItemCount() > 0) {
        auto* chapter = tree_->topLevelItem(0);
        tree_->setCurrentItem(chapter->childCount() ? chapter->child(0) : chapter);
    } else selectItem();
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
        QFont chapterFont = chapterItem->font(0);
        chapterFont.setBold(true);
        chapterItem->setFont(0, chapterFont);
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
        sceneTitle_->clear(); scenePov_->clear(); sceneLocation_->clear(); sceneLayer_->clear();
        sceneStatus_->setCurrentIndex(0);
        editor_->clear();
        wordCount_->clear();
        proofState_->clear();
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
    loadSceneContent(scene);
    refreshing_ = false;
    updateFormattingState();
    editor_->refreshSemanticReferences();
    editor_->clearProofread();
    updateWordCount();
}

void WritingPage::loadSceneContent(const QJsonObject& scene) {
    editor_->clear();
    if (scene.contains(QStringLiteral("text"))) {
        editor_->setPlainText(scene.value(QStringLiteral("text")).toString());
        applyFormatting(scene.value(QStringLiteral("formatting")).toArray());
        return;
    }

    QTextDocument legacy;
    legacy.setHtml(scene.value(QStringLiteral("content")).toString());
    editor_->setPlainText(legacy.toPlainText());
    for (QTextBlock block = legacy.begin(); block.isValid(); block = block.next()) {
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid()) continue;
            const QTextCharFormat source = fragment.charFormat();
            if (source.fontWeight() < QFont::DemiBold && !source.fontItalic() && !source.fontUnderline()) continue;
            QTextCursor cursor(editor_->document());
            cursor.setPosition(qMin(fragment.position(), editor_->document()->characterCount() - 1));
            cursor.setPosition(qMin(fragment.position() + fragment.length(), editor_->document()->characterCount() - 1), QTextCursor::KeepAnchor);
            QTextCharFormat format;
            format.setFontWeight(source.fontWeight());
            format.setFontItalic(source.fontItalic());
            format.setFontUnderline(source.fontUnderline());
            cursor.mergeCharFormat(format);
        }
    }
}

QJsonArray WritingPage::serializeFormatting() const {
    QJsonArray result;
    if (!editor_) return result;
    for (QTextBlock block = editor_->document()->begin(); block.isValid(); block = block.next()) {
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid() || fragment.length() <= 0) continue;
            const QTextCharFormat format = fragment.charFormat();
            const bool bold = format.fontWeight() >= QFont::DemiBold;
            const bool italic = format.fontItalic();
            const bool underline = format.fontUnderline();
            if (!bold && !italic && !underline) continue;
            result.append(QJsonObject{
                {QStringLiteral("start"), fragment.position()},
                {QStringLiteral("length"), fragment.length()},
                {QStringLiteral("bold"), bold},
                {QStringLiteral("italic"), italic},
                {QStringLiteral("underline"), underline}
            });
        }
    }
    return result;
}

void WritingPage::applyFormatting(const QJsonArray& formatting) {
    for (const QJsonValue value : formatting) {
        const QJsonObject run = value.toObject();
        const int start = run.value(QStringLiteral("start")).toInt();
        const int length = run.value(QStringLiteral("length")).toInt();
        if (length <= 0 || start < 0 || start >= editor_->document()->characterCount()) continue;
        QTextCursor cursor(editor_->document());
        cursor.setPosition(start);
        cursor.setPosition(qMin(start + length, editor_->document()->characterCount() - 1), QTextCursor::KeepAnchor);
        QTextCharFormat format;
        format.setFontWeight(run.value(QStringLiteral("bold")).toBool() ? QFont::Bold : QFont::Normal);
        format.setFontItalic(run.value(QStringLiteral("italic")).toBool());
        format.setFontUnderline(run.value(QStringLiteral("underline")).toBool());
        cursor.mergeCharFormat(format);
    }
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
    scene.insert(QStringLiteral("text"), editor_->toPlainText());
    scene.insert(QStringLiteral("formatting"), serializeFormatting());
    scene.remove(QStringLiteral("content"));
    scene.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    scenes.replace(s, scene);
    chapter.insert(QStringLiteral("scenes"), scenes);
    chapters.replace(c, chapter);
    document_->setArray(QStringLiteral("writingChapters"), chapters);
    item->setText(0, sceneTitle_->text().isEmpty() ? tr("Escena") : sceneTitle_->text());
    updateWordCount();
    editor_->refreshSemanticReferences();
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
    if (chapters.isEmpty()) { addChapter(); chapters = document_->array(QStringLiteral("writingChapters")); }
    int c = 0;
    if (auto* item = tree_->currentItem()) c = item->data(0, Qt::UserRole + 1).toInt();
    c = qBound(0, c, chapters.size() - 1);
    QJsonObject chapter = chapters.at(c).toObject();
    QJsonArray scenes = chapter.value(QStringLiteral("scenes")).toArray();
    scenes.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("scene"))}, {QStringLiteral("chapterId"), chapter.value(QStringLiteral("id"))}, {QStringLiteral("order"), scenes.size()}, {QStringLiteral("title"), tr("Escena %1").arg(scenes.size() + 1)}, {QStringLiteral("text"), QString()}, {QStringLiteral("formatting"), QJsonArray()}, {QStringLiteral("pov"), QString()}, {QStringLiteral("location"), QString()}, {QStringLiteral("narrativeLayer"), QString()}, {QStringLiteral("status"), tr("Borrador")}, {QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}});
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
    if (kind == QStringLiteral("chapter")) chapters.removeAt(c);
    else if (kind == QStringLiteral("scene")) {
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
    const int c = item->data(0, Qt::UserRole + 1).toInt();
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
    applyScene();
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

void WritingPage::updateWordCount() {
    const int words = editor_->toPlainText().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts).size();
    wordCount_->setText(tr("%1 palabras").arg(words));
}

void WritingPage::refreshSceneBoard() {
    if (!sceneBoard_ || !sceneBoard_->scene() || !document_) return;
    auto* scene = sceneBoard_->scene();
    scene->clear();
    scene->setBackgroundBrush(QColor(QStringLiteral("#f7f9fc")));
    const bool compact = sceneBoardCompact_->isChecked();
    const qreal cardWidth = compact ? 150.0 : 220.0;
    const qreal cardHeight = compact ? 58.0 : 92.0;
    const qreal gapX = compact ? 175.0 : 250.0;
    const qreal gapY = compact ? 78.0 : 112.0;
    const QJsonArray chapters = document_->array(QStringLiteral("writingChapters"));
    for (int c = 0; c < chapters.size(); ++c) {
        const QJsonObject chapter = chapters.at(c).toObject();
        auto* heading = scene->addSimpleText(chapter.value(QStringLiteral("label")).toString(tr("Capítulo")));
        heading->setBrush(QColor(QStringLiteral("#8a6a52")));
        QFont headingFont = heading->font();
        headingFont.setBold(true);
        heading->setFont(headingFont);
        heading->setPos(c * gapX, 0.0);
        const QJsonArray scenes = chapter.value(QStringLiteral("scenes")).toArray();
        for (int s = 0; s < scenes.size(); ++s) {
            const QJsonObject sceneObject = scenes.at(s).toObject();
            auto* card = scene->addRect(QRectF(0, 0, cardWidth, cardHeight), QPen(QColor(QStringLiteral("#cfd7e2"))), QBrush(QColor(QStringLiteral("#ffffff"))));
            card->setPos(c * gapX, 38.0 + s * gapY);
            auto* cardTitle = new QGraphicsSimpleTextItem(sceneObject.value(QStringLiteral("title")).toString(tr("Escena")), card);
            cardTitle->setBrush(QColor(QStringLiteral("#1f2937")));
            cardTitle->setPos(10.0, 8.0);
            if (!compact) {
                const QString detail = QStringLiteral("%1 · %2").arg(sceneObject.value(QStringLiteral("pov")).toString(), sceneObject.value(QStringLiteral("location")).toString());
                auto* meta = new QGraphicsSimpleTextItem(detail, card);
                meta->setBrush(QColor(QStringLiteral("#667085")));
                meta->setPos(10.0, 34.0);
                const int words = scenePlainText(sceneObject).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts).size();
                auto* stats = new QGraphicsSimpleTextItem(tr("%1 palabras · %2").arg(words).arg(sceneObject.value(QStringLiteral("status")).toString()), card);
                stats->setBrush(QColor(QStringLiteral("#8a94a3")));
                stats->setPos(10.0, 58.0);
            }
        }
    }
    scene->setSceneRect(scene->itemsBoundingRect().adjusted(-60, -60, 60, 60));
}

void WritingPage::openFocusMode() {
    if (!editor_ || !editor_->isEnabled()) return;

    QWidget* originalParent = editor_->parentWidget();
    QLayout* originalLayout = originalParent ? originalParent->layout() : nullptr;
    if (!originalLayout) return;
    originalLayout->removeWidget(editor_);

    QDialog focus(window());
    focus.setObjectName(QStringLiteral("realFocusMode"));
    focus.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    focus.setWindowState(Qt::WindowFullScreen);
    focus.setStyleSheet(QStringLiteral(
        "#realFocusMode{background:#eef1f4;}"
        "#focusPage{background:#ffffff;border:1px solid #d9dee5;}"
        "#focusExit{background:transparent;border:0;color:#667085;padding:8px 12px;}"
        "#focusExit:hover{color:#175cd3;background:#f5f7fa;}"
    ));
    auto* root = new QVBoxLayout(&focus);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* top = new QHBoxLayout;
    top->setContentsMargins(22, 8, 22, 8);
    auto* sceneName = new QLabel(sceneTitle_->text());
    sceneName->setStyleSheet(QStringLiteral("color:#667085;font-size:9pt;"));
    auto* exit = makeButton(tr("Salir de enfoque  Esc"));
    exit->setObjectName(QStringLiteral("focusExit"));
    top->addWidget(sceneName);
    top->addStretch();
    top->addWidget(exit);
    root->addLayout(top);

    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("focusPage"));
    page->setMaximumWidth(900);
    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(80, 56, 80, 80);
    pageLayout->addWidget(editor_);
    auto* center = new QHBoxLayout;
    center->setContentsMargins(0, 0, 0, 0);
    center->addStretch();
    center->addWidget(page, 1);
    center->addStretch();
    root->addLayout(center, 1);

    auto closeFocus = [&focus]() { focus.accept(); };
    connect(exit, &QPushButton::clicked, &focus, closeFocus);
    QShortcut escape(QKeySequence(Qt::Key_Escape), &focus);
    connect(&escape, &QShortcut::activated, &focus, closeFocus);

    editor_->setFocus();
    focus.exec();

    pageLayout->removeWidget(editor_);
    editor_->setParent(originalParent);
    originalLayout->addWidget(editor_);
    editor_->show();
    editor_->setFocus();
    applyScene();
}

} // namespace wbw
