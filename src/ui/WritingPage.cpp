#include "ui/WritingPage.h"

#include "core/ArchiveDocument.h"
#include "import/ManuscriptImporter.h"
#include "ui/SemanticTextEdit.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFont>
#include <QFontMetrics>
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
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabBar>
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

QString sceneCardMeta(const QJsonObject& scene) {
    QStringList parts;
    const QString pov = scene.value(QStringLiteral("pov")).toString().trimmed();
    const QString location = scene.value(QStringLiteral("location")).toString().trimmed();
    if (!pov.isEmpty()) parts.append(QStringLiteral("POV: %1").arg(pov));
    if (!location.isEmpty()) parts.append(location);
    return parts.join(QStringLiteral(" · "));
}

QColor sceneAccent(const QString& status, bool dark) {
    if (status.compare(QObject::tr("Final"), Qt::CaseInsensitive) == 0) return dark ? QColor(QStringLiteral("#54b47a")) : QColor(QStringLiteral("#2f855a"));
    if (status.compare(QObject::tr("Revisión"), Qt::CaseInsensitive) == 0) return dark ? QColor(QStringLiteral("#72aef0")) : QColor(QStringLiteral("#2f6fb3"));
    return dark ? QColor(QStringLiteral("#d2aa69")) : QColor(QStringLiteral("#b37a32"));
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
        "#focusBar{background:#ffffff;border-bottom:1px solid #d8dee7;}"
        "#focusSceneName{color:#667085;font-size:9pt;}"
        "#focusExit{background:transparent;border:0;color:#667085;padding:7px 10px;}"
        "#focusExit:hover{background:#f3f6fa;color:#175cd3;}"
        "#sceneBoardToolbar,#sceneBoardOutlinePanel{background:#ffffff;border:1px solid #d5dce5;}"
        "#sceneBoardHeading{font-size:15pt;font-weight:700;color:#344054;}"
        "#sceneBoardSubheading,#sceneBoardSelection{color:#667085;font-size:8.5pt;}"
        "#sceneBoardOutline{border:0;background:#ffffff;padding:4px;}"
        "#sceneBoardOutline::item{padding:5px 6px;}"
        "#sceneBoardOutline::item:selected{background:#e8f1ff;color:#175cd3;}"
        "#sceneBoardCanvas{background:#eef2f6;border:1px solid #d5dce5;}"
    ));
}

void WritingPage::setDocument(ArchiveDocument* document) {
    document_ = document;
    if (editor_) editor_->setArchiveDocument(document_);
    refresh();
}

QWidget* WritingPage::buildEditorTab() {
    editorTab_ = new QWidget;
    editorTab_->setObjectName(QStringLiteral("writingEditorTab"));
    editorOuterLayout_ = new QVBoxLayout(editorTab_);
    editorOuterLayout_->setContentsMargins(30, 22, 30, 30);
    editorOuterLayout_->setSpacing(14);

    auto* hero = new QFrame;
    heroPanel_ = hero;
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
    description->setWordWrap(true);
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
    editorOuterLayout_->addWidget(hero);

    commandBarPanel_ = new QWidget(editorTab_);
    auto* commandBar = new QHBoxLayout(commandBarPanel_);
    commandBar->setContentsMargins(0, 0, 0, 0);
    commandBar->setSpacing(6);
    toggleIndex_ = new QToolButton;
    toggleIndex_->setText(tr("Índice"));
    toggleIndex_->setCheckable(true);
    toggleIndex_->setChecked(true);
    toggleDetails_ = new QToolButton;
    toggleDetails_->setText(tr("Detalles"));
    toggleDetails_->setCheckable(true);
    toggleDetails_->setChecked(false);
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

    commandBar->addWidget(toggleIndex_);
    commandBar->addWidget(toggleDetails_);
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
    editorOuterLayout_->addWidget(commandBarPanel_);

    focusBar_ = new QWidget(editorTab_);
    focusBar_->setObjectName(QStringLiteral("focusBar"));
    auto* focusLayout = new QHBoxLayout(focusBar_);
    focusLayout->setContentsMargins(18, 7, 18, 7);
    focusSceneName_ = new QLabel;
    focusSceneName_->setObjectName(QStringLiteral("focusSceneName"));
    auto* focusExit = makeButton(tr("Salir de enfoque  Esc"));
    focusExit->setObjectName(QStringLiteral("focusExit"));
    focusLayout->addWidget(focusSceneName_);
    focusLayout->addStretch();
    focusLayout->addWidget(focusExit);
    focusBar_->hide();
    editorOuterLayout_->addWidget(focusBar_);

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
    editorOuterLayout_->addWidget(editorSplit_, 1);

    focusEscape_ = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    focusEscape_->setEnabled(false);

    connect(toggleIndex_, &QToolButton::toggled, indexPanel_, &QWidget::setVisible);
    connect(toggleDetails_, &QToolButton::toggled, metadataPanel_, &QWidget::setVisible);
    connect(focusExit, &QPushButton::clicked, this, &WritingPage::closeFocusMode);
    connect(focusEscape_, &QShortcut::activated, this, &WritingPage::closeFocusMode);
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
    return editorTab_;
}

QWidget* WritingPage::buildSceneBoardTab() {
    auto* tab = new QWidget;
    tab->setObjectName(QStringLiteral("sceneBoardTab"));
    auto* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(18, 16, 18, 18);
    layout->setSpacing(10);

    auto* toolbar = new QFrame;
    toolbar->setObjectName(QStringLiteral("sceneBoardToolbar"));
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(14, 10, 14, 10);
    toolbarLayout->setSpacing(8);

    auto* headingColumn = new QVBoxLayout;
    headingColumn->setSpacing(0);
    auto* heading = new QLabel(tr("Tablero de escenas"));
    heading->setObjectName(QStringLiteral("sceneBoardHeading"));
    auto* subheading = new QLabel(tr("Selecciona una tarjeta para trabajar con la escena; doble clic en el índice para abrirla en el manuscrito."));
    subheading->setObjectName(QStringLiteral("sceneBoardSubheading"));
    headingColumn->addWidget(heading);
    headingColumn->addWidget(subheading);
    toolbarLayout->addLayout(headingColumn);
    toolbarLayout->addStretch();

    auto* search = new QLineEdit;
    search->setObjectName(QStringLiteral("sceneBoardSearch"));
    search->setPlaceholderText(tr("Buscar escenas…"));
    search->setClearButtonEnabled(true);
    search->setFixedWidth(220);
    auto* statusFilter = new QComboBox;
    statusFilter->setObjectName(QStringLiteral("sceneBoardStatus"));
    statusFilter->addItems({tr("Todos los estados"), tr("Borrador"), tr("Revisión"), tr("Final")});
    statusFilter->setFixedWidth(145);
    sceneBoardCompact_ = new QCheckBox(tr("Compacta"));
    auto* reset = makeButton(tr("Encajar"));
    auto* open = makeButton(tr("Abrir escena"));
    open->setObjectName(QStringLiteral("writingSubtle"));
    open->setEnabled(false);
    auto* add = makeButton(tr("+ Escena"));
    add->setObjectName(QStringLiteral("writingPrimary"));
    toolbarLayout->addWidget(search);
    toolbarLayout->addWidget(statusFilter);
    toolbarLayout->addWidget(sceneBoardCompact_);
    toolbarLayout->addWidget(reset);
    toolbarLayout->addWidget(open);
    toolbarLayout->addWidget(add);
    layout->addWidget(toolbar);

    auto* workspace = new QSplitter(Qt::Horizontal);
    workspace->setChildrenCollapsible(false);
    workspace->setHandleWidth(6);

    auto* outlinePanel = new QFrame;
    outlinePanel->setObjectName(QStringLiteral("sceneBoardOutlinePanel"));
    outlinePanel->setMinimumWidth(230);
    outlinePanel->setMaximumWidth(320);
    auto* outlineLayout = new QVBoxLayout(outlinePanel);
    outlineLayout->setContentsMargins(8, 8, 8, 8);
    outlineLayout->setSpacing(6);
    auto* outlineTitle = new QLabel(tr("CAPÍTULOS Y ESCENAS"));
    outlineTitle->setObjectName(QStringLiteral("sectionLabel"));
    auto* outline = new QTreeWidget;
    outline->setObjectName(QStringLiteral("sceneBoardOutline"));
    outline->setHeaderHidden(true);
    outline->setIndentation(14);
    auto* selection = new QLabel(tr("Ninguna escena seleccionada"));
    selection->setObjectName(QStringLiteral("sceneBoardSelection"));
    selection->setWordWrap(true);
    outlineLayout->addWidget(outlineTitle);
    outlineLayout->addWidget(outline, 1);
    outlineLayout->addWidget(selection);
    workspace->addWidget(outlinePanel);

    sceneBoard_ = new QGraphicsView;
    sceneBoard_->setObjectName(QStringLiteral("sceneBoardCanvas"));
    sceneBoard_->setScene(new QGraphicsScene(sceneBoard_));
    sceneBoard_->setDragMode(QGraphicsView::ScrollHandDrag);
    sceneBoard_->setRenderHint(QPainter::Antialiasing, true);
    sceneBoard_->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    sceneBoard_->setResizeAnchor(QGraphicsView::AnchorViewCenter);
    sceneBoard_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    workspace->addWidget(sceneBoard_);
    workspace->setStretchFactor(0, 0);
    workspace->setStretchFactor(1, 1);
    workspace->setSizes({270, 1100});
    layout->addWidget(workspace, 1);

    const auto openScene = [this](int chapterIndex, int sceneIndex) {
        if (!tree_ || chapterIndex < 0 || chapterIndex >= tree_->topLevelItemCount()) return;
        QTreeWidgetItem* chapter = tree_->topLevelItem(chapterIndex);
        if (!chapter || sceneIndex < 0 || sceneIndex >= chapter->childCount()) return;
        tabs_->setCurrentIndex(0);
        tree_->setCurrentItem(chapter->child(sceneIndex));
        tree_->scrollToItem(chapter->child(sceneIndex));
        editor_->setFocus();
    };

    connect(outline, &QTreeWidget::itemDoubleClicked, this, [openScene](QTreeWidgetItem* item, int) {
        if (!item || item->data(0, Qt::UserRole).toString() != QStringLiteral("scene")) return;
        openScene(item->data(0, Qt::UserRole + 1).toInt(), item->data(0, Qt::UserRole + 2).toInt());
    });
    connect(outline, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item) {
        if (!item || item->data(0, Qt::UserRole).toString() != QStringLiteral("scene") || !sceneBoard_->scene()) return;
        const int c = item->data(0, Qt::UserRole + 1).toInt();
        const int s = item->data(0, Qt::UserRole + 2).toInt();
        for (QGraphicsItem* graphic : sceneBoard_->scene()->items()) {
            if (graphic->data(0).toString() != QStringLiteral("scene-card")) continue;
            graphic->setSelected(graphic->data(1).toInt() == c && graphic->data(2).toInt() == s);
        }
    });
    connect(sceneBoard_->scene(), &QGraphicsScene::selectionChanged, this, [this, outline, selection, open]() {
        int c = -1;
        int s = -1;
        for (QGraphicsItem* item : sceneBoard_->scene()->selectedItems()) {
            if (item->data(0).toString() != QStringLiteral("scene-card")) continue;
            c = item->data(1).toInt();
            s = item->data(2).toInt();
            break;
        }
        sceneBoard_->setProperty("selectedChapter", c);
        sceneBoard_->setProperty("selectedScene", s);
        open->setEnabled(c >= 0 && s >= 0);
        if (c < 0 || s < 0) {
            selection->setText(tr("Ninguna escena seleccionada"));
            return;
        }
        const QJsonArray chapters = document_ ? document_->array(QStringLiteral("writingChapters")) : QJsonArray();
        if (c < chapters.size()) {
            const QJsonArray scenes = chapters.at(c).toObject().value(QStringLiteral("scenes")).toArray();
            if (s < scenes.size()) selection->setText(scenes.at(s).toObject().value(QStringLiteral("title")).toString(tr("Escena")));
        }
        if (c < outline->topLevelItemCount()) {
            QTreeWidgetItem* chapter = outline->topLevelItem(c);
            if (chapter && s < chapter->childCount()) {
                QSignalBlocker blocker(outline);
                outline->setCurrentItem(chapter->child(s));
            }
        }
    });
    connect(open, &QPushButton::clicked, this, [this, openScene]() {
        openScene(sceneBoard_->property("selectedChapter").toInt(), sceneBoard_->property("selectedScene").toInt());
    });
    connect(add, &QPushButton::clicked, this, [this]() {
        const int c = sceneBoard_->property("selectedChapter").toInt();
        if (tree_ && c >= 0 && c < tree_->topLevelItemCount()) tree_->setCurrentItem(tree_->topLevelItem(c));
        addScene();
        tabs_->setCurrentIndex(1);
    });
    connect(sceneBoardCompact_, &QCheckBox::toggled, this, [this](bool checked) {
        if (!document_) return;
        QJsonObject profile = document_->object(QStringLiteral("profile"));
        profile.insert(QStringLiteral("sceneBoardCompact"), checked);
        document_->setObject(QStringLiteral("profile"), profile);
        refreshSceneBoard();
        emit changed();
    });
    connect(search, &QLineEdit::textChanged, this, [this]() { refreshSceneBoard(); });
    connect(statusFilter, &QComboBox::currentTextChanged, this, [this]() { refreshSceneBoard(); });
    connect(reset, &QPushButton::clicked, this, [this]() {
        sceneBoard_->resetTransform();
        const QRectF bounds = sceneBoard_->scene() ? sceneBoard_->scene()->itemsBoundingRect().adjusted(-20, -20, 20, 20) : QRectF();
        if (bounds.isValid() && bounds.width() > sceneBoard_->viewport()->width()) sceneBoard_->fitInView(bounds, Qt::KeepAspectRatio);
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

    auto* outline = findChild<QTreeWidget*>(QStringLiteral("sceneBoardOutline"));
    auto* search = findChild<QLineEdit*>(QStringLiteral("sceneBoardSearch"));
    auto* statusFilter = findChild<QComboBox*>(QStringLiteral("sceneBoardStatus"));
    if (outline) outline->clear();

    QGraphicsScene* scene = sceneBoard_->scene();
    scene->clear();
    const bool dark = qApp && qApp->property("wbwDarkMode").toBool();
    const QColor canvas = dark ? QColor(QStringLiteral("#12161d")) : QColor(QStringLiteral("#eef2f6"));
    const QColor laneFill = dark ? QColor(QStringLiteral("#171c24")) : QColor(QStringLiteral("#f7f9fc"));
    const QColor laneBorder = dark ? QColor(QStringLiteral("#303a48")) : QColor(QStringLiteral("#d7dee8"));
    const QColor cardFill = dark ? QColor(QStringLiteral("#1d2430")) : QColor(QStringLiteral("#ffffff"));
    const QColor cardBorder = dark ? QColor(QStringLiteral("#394454")) : QColor(QStringLiteral("#cfd7e2"));
    const QColor titleColor = dark ? QColor(QStringLiteral("#f2f4f7")) : QColor(QStringLiteral("#1f2937"));
    const QColor muted = dark ? QColor(QStringLiteral("#9aa7b8")) : QColor(QStringLiteral("#667085"));
    const QColor quiet = dark ? QColor(QStringLiteral("#778397")) : QColor(QStringLiteral("#8a94a3"));
    scene->setBackgroundBrush(canvas);

    const bool compact = sceneBoardCompact_->isChecked();
    const qreal laneWidth = compact ? 230.0 : 290.0;
    const qreal cardWidth = laneWidth - 24.0;
    const qreal cardHeight = compact ? 72.0 : 112.0;
    const qreal cardGap = compact ? 10.0 : 12.0;
    const qreal laneGap = 16.0;
    const qreal laneTop = 48.0;
    const QString query = search ? search->text().trimmed() : QString();
    const QString wantedStatus = statusFilter && statusFilter->currentIndex() > 0 ? statusFilter->currentText() : QString();
    const int selectedChapter = sceneBoard_->property("selectedChapter").toInt();
    const int selectedScene = sceneBoard_->property("selectedScene").toInt();

    const QJsonArray chapters = document_->array(QStringLiteral("writingChapters"));
    qreal x = 0.0;
    qreal maxHeight = 520.0;
    int visibleColumns = 0;

    for (int c = 0; c < chapters.size(); ++c) {
        const QJsonObject chapter = chapters.at(c).toObject();
        const QJsonArray scenes = chapter.value(QStringLiteral("scenes")).toArray();
        QVector<int> visibleScenes;
        for (int s = 0; s < scenes.size(); ++s) {
            const QJsonObject sceneObject = scenes.at(s).toObject();
            const QString title = sceneObject.value(QStringLiteral("title")).toString();
            const QString status = sceneObject.value(QStringLiteral("status")).toString();
            const QString haystack = QStringLiteral("%1 %2 %3 %4").arg(title, sceneObject.value(QStringLiteral("pov")).toString(), sceneObject.value(QStringLiteral("location")).toString(), status);
            if (!query.isEmpty() && !haystack.contains(query, Qt::CaseInsensitive)) continue;
            if (!wantedStatus.isEmpty() && status.compare(wantedStatus, Qt::CaseInsensitive) != 0) continue;
            visibleScenes.append(s);
        }

        if (outline) {
            const QString chapterLabel = chapter.value(QStringLiteral("label")).toString(tr("Capítulo %1").arg(c + 1));
            const QString chapterTitle = chapter.value(QStringLiteral("title")).toString();
            auto* chapterItem = new QTreeWidgetItem({chapterTitle.isEmpty() ? chapterLabel : chapterLabel + QStringLiteral(" — ") + chapterTitle});
            chapterItem->setData(0, Qt::UserRole, QStringLiteral("chapter"));
            QFont chapterFont = chapterItem->font(0);
            chapterFont.setBold(true);
            chapterItem->setFont(0, chapterFont);
            for (const int s : visibleScenes) {
                const QJsonObject sceneObject = scenes.at(s).toObject();
                const int words = scenePlainText(sceneObject).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts).size();
                auto* item = new QTreeWidgetItem({QStringLiteral("%1   ·   %2").arg(sceneObject.value(QStringLiteral("title")).toString(tr("Escena"))).arg(tr("%1 palabras").arg(words))});
                item->setData(0, Qt::UserRole, QStringLiteral("scene"));
                item->setData(0, Qt::UserRole + 1, c);
                item->setData(0, Qt::UserRole + 2, s);
                chapterItem->addChild(item);
            }
            if (!visibleScenes.isEmpty() || (query.isEmpty() && wantedStatus.isEmpty())) {
                outline->addTopLevelItem(chapterItem);
                chapterItem->setExpanded(true);
            } else delete chapterItem;
        }

        if (visibleScenes.isEmpty()) continue;

        const qreal laneHeight = qMax<qreal>(500.0, laneTop + 18.0 + visibleScenes.size() * (cardHeight + cardGap));
        maxHeight = qMax(maxHeight, laneHeight);
        auto* lane = scene->addRect(QRectF(x, 0.0, laneWidth, laneHeight), QPen(laneBorder), QBrush(laneFill));
        lane->setZValue(-10.0);

        const QString chapterLabel = chapter.value(QStringLiteral("label")).toString(tr("Capítulo %1").arg(c + 1));
        const QString chapterTitle = chapter.value(QStringLiteral("title")).toString();
        auto* heading = scene->addSimpleText(chapterTitle.isEmpty() ? chapterLabel : chapterLabel + QStringLiteral(" — ") + chapterTitle);
        heading->setBrush(titleColor);
        QFont headingFont = heading->font();
        headingFont.setBold(true);
        headingFont.setPointSizeF(10.0);
        heading->setFont(headingFont);
        heading->setPos(x + 12.0, 12.0);
        auto* count = scene->addSimpleText(tr("%1 escenas").arg(visibleScenes.size()));
        count->setBrush(quiet);
        QFont countFont = count->font();
        countFont.setPointSizeF(8.0);
        count->setFont(countFont);
        count->setPos(x + 12.0, 30.0);

        qreal y = laneTop;
        for (const int s : visibleScenes) {
            const QJsonObject sceneObject = scenes.at(s).toObject();
            const QString title = sceneObject.value(QStringLiteral("title")).toString(tr("Escena"));
            const QString status = sceneObject.value(QStringLiteral("status")).toString(tr("Borrador"));
            const int words = scenePlainText(sceneObject).split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts).size();

            auto* card = scene->addRect(QRectF(0, 0, cardWidth, cardHeight), QPen(cardBorder), QBrush(cardFill));
            card->setPos(x + 12.0, y);
            card->setFlag(QGraphicsItem::ItemIsSelectable, true);
            card->setData(0, QStringLiteral("scene-card"));
            card->setData(1, c);
            card->setData(2, s);
            card->setData(3, title);
            card->setZValue(2.0);
            if (c == selectedChapter && s == selectedScene) card->setSelected(true);

            auto* accent = new QGraphicsRectItem(QRectF(0, 0, 4.0, cardHeight), card);
            accent->setPen(Qt::NoPen);
            accent->setBrush(sceneAccent(status, dark));

            const QString code = QStringLiteral("%1-%2").arg(c + 1, 2, 10, QLatin1Char('0')).arg(s + 1, 2, 10, QLatin1Char('0'));
            auto* codeText = new QGraphicsSimpleTextItem(code, card);
            codeText->setBrush(quiet);
            QFont codeFont = codeText->font();
            codeFont.setPointSizeF(7.5);
            codeFont.setBold(true);
            codeText->setFont(codeFont);
            codeText->setPos(12.0, 8.0);

            QFont titleFont;
            titleFont.setFamily(QStringLiteral("Segoe UI"));
            titleFont.setPointSizeF(compact ? 9.5 : 10.5);
            titleFont.setBold(true);
            const QString elidedTitle = QFontMetrics(titleFont).elidedText(title, Qt::ElideRight, static_cast<int>(cardWidth - 26.0));
            auto* cardTitle = new QGraphicsSimpleTextItem(elidedTitle, card);
            cardTitle->setBrush(titleColor);
            cardTitle->setFont(titleFont);
            cardTitle->setPos(12.0, compact ? 28.0 : 30.0);

            if (!compact) {
                const QString metaText = sceneCardMeta(sceneObject);
                if (!metaText.isEmpty()) {
                    QFont metaFont;
                    metaFont.setFamily(QStringLiteral("Segoe UI"));
                    metaFont.setPointSizeF(8.0);
                    auto* meta = new QGraphicsSimpleTextItem(QFontMetrics(metaFont).elidedText(metaText, Qt::ElideRight, static_cast<int>(cardWidth - 26.0)), card);
                    meta->setBrush(muted);
                    meta->setFont(metaFont);
                    meta->setPos(12.0, 56.0);
                }
            }

            auto* stats = new QGraphicsSimpleTextItem(QStringLiteral("%1  ·  %2").arg(tr("%1 palabras").arg(words), status), card);
            stats->setBrush(quiet);
            QFont statsFont = stats->font();
            statsFont.setPointSizeF(7.5);
            stats->setFont(statsFont);
            stats->setPos(12.0, compact ? 50.0 : 84.0);
            y += cardHeight + cardGap;
        }
        x += laneWidth + laneGap;
        ++visibleColumns;
    }

    if (visibleColumns == 0) {
        auto* empty = scene->addSimpleText(tr("No hay escenas que coincidan con los filtros."));
        empty->setBrush(muted);
        QFont emptyFont = empty->font();
        emptyFont.setPointSizeF(11.0);
        empty->setFont(emptyFont);
        empty->setPos(28.0, 28.0);
        scene->setSceneRect(QRectF(0, 0, 700, 420));
    } else {
        scene->setSceneRect(QRectF(0, 0, qMax<qreal>(x - laneGap, 700.0), maxHeight));
    }
}

void WritingPage::openFocusMode() {
    if (focusActive_ || !editor_ || !editor_->isEnabled()) return;

    applyScene();
    focusActive_ = true;
    indexWasVisible_ = indexPanel_->isVisible();
    detailsWereVisible_ = metadataPanel_->isVisible();
    tabBarWasVisible_ = tabs_->tabBar()->isVisible();
    previousWindowState_ = window()->windowState();

    QWidget* topShell = window()->findChild<QWidget*>(QStringLiteral("topShell"));
    topShellWasVisible_ = topShell && topShell->isVisible();
    if (topShell) topShell->hide();
    if (QWidget* titleBar = window()->findChild<QWidget*>(QStringLiteral("appTitleBar"))) titleBar->hide();

    tabs_->tabBar()->hide();
    heroPanel_->hide();
    commandBarPanel_->hide();
    indexPanel_->hide();
    metadataPanel_->hide();
    focusSceneName_->setText(sceneTitle_->text());
    focusBar_->show();
    editorOuterLayout_->setContentsMargins(0, 0, 0, 0);
    editorOuterLayout_->setSpacing(0);
    editorSplit_->setHandleWidth(0);
    focusEscape_->setEnabled(true);

    window()->showFullScreen();
    editor_->setFocus();
}

void WritingPage::closeFocusMode() {
    if (!focusActive_) return;

    applyScene();
    focusActive_ = false;
    focusEscape_->setEnabled(false);
    focusBar_->hide();
    editorOuterLayout_->setContentsMargins(30, 22, 30, 30);
    editorOuterLayout_->setSpacing(14);
    editorSplit_->setHandleWidth(6);
    heroPanel_->show();
    commandBarPanel_->show();
    tabs_->tabBar()->setVisible(tabBarWasVisible_);
    indexPanel_->setVisible(indexWasVisible_);
    metadataPanel_->setVisible(detailsWereVisible_);

    if (QWidget* topShell = window()->findChild<QWidget*>(QStringLiteral("topShell")); topShell && topShellWasVisible_) topShell->show();
    if (QWidget* titleBar = window()->findChild<QWidget*>(QStringLiteral("appTitleBar"))) titleBar->show();

    if (previousWindowState_.testFlag(Qt::WindowMaximized)) window()->showMaximized();
    else window()->showNormal();

    editor_->setFocus();
}

} // namespace wbw
