#include "ui/WritingMockupController.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMainWindow>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace wbw {
namespace {

QPushButton* buttonByText(QWidget* root, const QString& text) {
    if (!root) return nullptr;
    for (QPushButton* button : root->findChildren<QPushButton*>())
        if (button->text() == text) return button;
    return nullptr;
}

QLineEdit* fieldByPlaceholder(QWidget* root, const QString& text) {
    if (!root) return nullptr;
    for (QLineEdit* edit : root->findChildren<QLineEdit*>())
        if (edit->placeholderText() == text) return edit;
    return nullptr;
}

QToolButton* toolByText(QWidget* root, const QString& text) {
    if (!root) return nullptr;
    for (QToolButton* button : root->findChildren<QToolButton*>())
        if (button->text() == text) return button;
    return nullptr;
}

void ensureShortcut(QWidget* root, const QString& name, const QKeySequence& key, QPushButton* target) {
    if (!root || !target) return;
    auto* shortcut = root->findChild<QShortcut*>(name);
    if (!shortcut) {
        shortcut = new QShortcut(root);
        shortcut->setObjectName(name);
        shortcut->setContext(Qt::WindowShortcut);
        QObject::connect(shortcut, &QShortcut::activated, target, &QPushButton::click);
    }
    shortcut->setKey(key);
}

void addMetaCard(QVBoxLayout* layout, QWidget* owner, const QString& title, QWidget* field) {
    if (!layout || !owner || !field) return;
    auto* card = new QFrame(owner);
    card->setObjectName(QStringLiteral("writingMetaCard"));
    auto* box = new QVBoxLayout(card);
    box->setContentsMargins(12, 10, 12, 11);
    box->setSpacing(6);
    auto* label = new QLabel(title, card);
    label->setObjectName(QStringLiteral("writingMetaLabel"));
    box->addWidget(label);
    field->setParent(card);
    field->setMinimumHeight(34);
    box->addWidget(field);
    layout->addWidget(card);
}

void buildMetadata(QWidget* metadata) {
    if (!metadata || metadata->property("wbwEditorialMetadata").toBool()) return;
    auto* title = fieldByPlaceholder(metadata, QObject::tr("Título de escena"));
    auto* pov = fieldByPlaceholder(metadata, QObject::tr("POV"));
    auto* location = fieldByPlaceholder(metadata, QObject::tr("Ubicación"));
    auto* layer = fieldByPlaceholder(metadata, QObject::tr("Capa narrativa"));
    auto* status = metadata->findChild<QComboBox*>();
    if (!title || !pov || !location || !layer || !status) return;

    if (QLayout* old = metadata->layout()) {
        while (QLayoutItem* item = old->takeAt(0)) delete item;
        delete old;
    }

    auto* layout = new QVBoxLayout(metadata);
    layout->setContentsMargins(14, 18, 14, 18);
    layout->setSpacing(10);

    auto* eyebrow = new QLabel(QObject::tr("ESCENA ACTUAL"), metadata);
    eyebrow->setObjectName(QStringLiteral("writingContextEyebrow"));
    auto* heading = new QLabel(QObject::tr("Contexto"), metadata);
    heading->setObjectName(QStringLiteral("writingContextTitle"));
    auto* description = new QLabel(QObject::tr("Información que acompaña al manuscrito sin competir con la escritura."), metadata);
    description->setObjectName(QStringLiteral("writingContextDescription"));
    description->setWordWrap(true);
    layout->addWidget(eyebrow);
    layout->addWidget(heading);
    layout->addWidget(description);
    layout->addSpacing(4);

    addMetaCard(layout, metadata, QObject::tr("TÍTULO DE ESCENA"), title);
    addMetaCard(layout, metadata, QObject::tr("ESTADO"), status);
    addMetaCard(layout, metadata, QObject::tr("PUNTO DE VISTA"), pov);
    addMetaCard(layout, metadata, QObject::tr("UBICACIÓN"), location);
    addMetaCard(layout, metadata, QObject::tr("CAPA NARRATIVA"), layer);
    layout->addStretch(1);

    metadata->setProperty("wbwEditorialMetadata", true);
}

void buildIndexHeader(QWidget* index) {
    if (!index || index->property("wbwEditorialIndex").toBool()) return;
    auto* layout = qobject_cast<QVBoxLayout*>(index->layout());
    if (!layout) return;
    auto* eyebrow = new QLabel(QObject::tr("MANUSCRITO"), index);
    eyebrow->setObjectName(QStringLiteral("writingIndexEyebrow"));
    auto* title = new QLabel(QObject::tr("Capítulos y escenas"), index);
    title->setObjectName(QStringLiteral("writingIndexHeading"));
    auto* copy = new QLabel(QObject::tr("Orden narrativo, escenas y acceso rápido."), index);
    copy->setObjectName(QStringLiteral("writingIndexCopy"));
    copy->setWordWrap(true);
    layout->insertWidget(0, copy);
    layout->insertWidget(0, title);
    layout->insertWidget(0, eyebrow);
    if (QLabel* legacy = index->findChild<QLabel*>(QStringLiteral("indexTitle"))) legacy->hide();
    index->setProperty("wbwEditorialIndex", true);
}

void buildDocumentStage(QWidget* editorPanel, QTextEdit* editor) {
    if (!editorPanel || !editor || editorPanel->property("wbwEditorialStage").toBool()) return;
    auto* parentLayout = qobject_cast<QVBoxLayout*>(editorPanel->layout());
    if (!parentLayout) return;
    const int editorIndex = parentLayout->indexOf(editor);
    if (editorIndex < 0) return;
    parentLayout->removeWidget(editor);

    auto* stage = new QFrame(editorPanel);
    stage->setObjectName(QStringLiteral("writingDocumentStage"));
    auto* stageLayout = new QVBoxLayout(stage);
    stageLayout->setContentsMargins(0, 0, 0, 0);
    stageLayout->setSpacing(0);

    auto* header = new QFrame(stage);
    header->setObjectName(QStringLiteral("writingDocumentHeader"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(30, 18, 30, 16);
    auto* titles = new QVBoxLayout;
    titles->setSpacing(2);
    auto* eyebrow = new QLabel(QObject::tr("MANUSCRITO"), header);
    eyebrow->setObjectName(QStringLiteral("writingDocumentEyebrow"));
    auto* sceneTitle = new QLabel(QObject::tr("Escena sin título"), header);
    sceneTitle->setObjectName(QStringLiteral("writingDocumentTitle"));
    auto* hint = new QLabel(QObject::tr("Las referencias del atlas viven dentro del texto y abren su ficha contextual."), header);
    hint->setObjectName(QStringLiteral("writingDocumentHint"));
    hint->setWordWrap(true);
    titles->addWidget(eyebrow);
    titles->addWidget(sceneTitle);
    titles->addWidget(hint);
    headerLayout->addLayout(titles, 1);
    stageLayout->addWidget(header);

    editor->setParent(stage);
    stageLayout->addWidget(editor, 1);
    parentLayout->insertWidget(editorIndex, stage, 1);

    if (QLineEdit* sourceTitle = fieldByPlaceholder(editorPanel->window(), QObject::tr("Título de escena"))) {
        QObject::connect(sourceTitle, &QLineEdit::textChanged, sceneTitle, [sceneTitle](const QString& text) {
            const QString clean = text.trimmed();
            sceneTitle->setText(clean.isEmpty() ? QObject::tr("Escena sin título") : clean);
        });
        const QString clean = sourceTitle->text().trimmed();
        if (!clean.isEmpty()) sceneTitle->setText(clean);
    }

    editorPanel->setProperty("wbwEditorialStage", true);
}

class WritingWorkspaceFilter final : public QObject {
public:
    explicit WritingWorkspaceFilter(QMainWindow* window) : QObject(window), window_(window) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == window_ && (event->type() == QEvent::Show || event->type() == QEvent::Resize))
            QTimer::singleShot(0, this, [this]() { apply(); });
        return QObject::eventFilter(watched, event);
    }

private:
    void apply() {
        if (!window_) return;
        QWidget* page = window_->findChild<QWidget*>(QStringLiteral("writingPage"));
        if (!page) return;

        if (auto* tabs = page->findChild<QTabWidget*>(QStringLiteral("writingTabs")); tabs && tabs->tabBar()) tabs->tabBar()->hide();
        if (QWidget* hero = page->findChild<QWidget*>(QStringLiteral("writingHero"))) hero->hide();

        QWidget* command = page->findChild<QWidget*>(QStringLiteral("writingCommandBar"));
        QWidget* index = page->findChild<QWidget*>(QStringLiteral("indexPanel"));
        QWidget* editorPanel = page->findChild<QWidget*>(QStringLiteral("editorPanel"));
        QWidget* metadata = page->findChild<QWidget*>(QStringLiteral("metadataPanel"));
        auto* editor = page->findChild<QTextEdit*>(QStringLiteral("sceneEditor"));
        auto* split = index ? qobject_cast<QSplitter*>(index->parentWidget()) : nullptr;

        if (split && metadata && metadata->parentWidget() != split) {
            metadata->setParent(split);
            split->addWidget(metadata);
        }

        buildIndexHeader(index);
        buildMetadata(metadata);
        buildDocumentStage(editorPanel, editor);

        const bool wide = window_->width() >= 1180;
        if (index) { index->setMinimumWidth(wide ? 236 : 196); index->setMaximumWidth(wide ? 286 : 224); }
        if (metadata) { metadata->setVisible(wide); metadata->setMinimumWidth(wide ? 260 : 0); metadata->setMaximumWidth(wide ? 320 : 0); }
        if (split) {
            split->setHandleWidth(1);
            split->setChildrenCollapsible(true);
            split->setStretchFactor(0, 0);
            split->setStretchFactor(1, 1);
            split->setStretchFactor(2, 0);
            split->setSizes(wide ? QList<int>{252, 930, 292} : QList<int>{205, 850, 0});
        }
        if (QToolButton* details = toolByText(command, QObject::tr("Detalles"))) {
            QSignalBlocker blocker(details);
            details->setChecked(wide);
        }

        QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
        if (editor) {
            QFont font(settings.value(QStringLiteral("editor/fontFamily"), QStringLiteral("Georgia")).toString());
            font.setPointSize(qBound(11, settings.value(QStringLiteral("editor/fontSize"), 14).toInt(), 24));
            editor->setFont(font);
            editor->setFrameShape(QFrame::NoFrame);
            editor->setLineWrapMode(QTextEdit::WidgetWidth);
            editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        }

        if (command) {
            command->setMinimumHeight(46);
            command->setMaximumHeight(46);
            for (QAbstractButton* button : command->findChildren<QAbstractButton*>()) {
                button->setCursor(Qt::PointingHandCursor);
                button->setMinimumHeight(30);
            }
        }

        if (QWidget* board = page->findChild<QWidget*>(QStringLiteral("sceneBoardTab"))) {
            if (QWidget* toolbar = board->findChild<QWidget*>(QStringLiteral("sceneBoardToolbar"))) { toolbar->setMinimumHeight(66); toolbar->setMaximumHeight(72); }
            if (QWidget* outline = board->findChild<QWidget*>(QStringLiteral("sceneBoardOutlinePanel"))) { outline->setMinimumWidth(210); outline->setMaximumWidth(270); }
            if (auto* view = board->findChild<QGraphicsView*>(QStringLiteral("sceneBoardCanvas"))) {
                view->setFrameShape(QFrame::NoFrame);
                view->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
                view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            }
        }

        ensureShortcut(page, QStringLiteral("wbwFocusShortcut"), QKeySequence(settings.value(QStringLiteral("shortcut/focus"), QStringLiteral("Ctrl+Shift+F")).toString()), buttonByText(page, QObject::tr("Enfoque")));
        ensureShortcut(page, QStringLiteral("wbwProofShortcut"), QKeySequence(settings.value(QStringLiteral("shortcut/proofread"), QStringLiteral("F7")).toString()), buttonByText(page, QObject::tr("Revisar")));

        page->setStyleSheet(QStringLiteral(R"QSS(
#writingPage,#writingEditorTab,#sceneBoardTab{background:#0a0c0f;color:#d8d3c8;}
#writingTabs::pane{border:0;background:#0a0c0f;}
#writingCommandBar{background:#0d1014;border:0;border-bottom:1px solid #292821;padding:4px 18px;}
#writingCommandBar QToolButton,#writingCommandBar QPushButton{background:transparent;color:#9f9b92;border:0;border-radius:6px;padding:5px 9px;}
#writingCommandBar QToolButton:hover,#writingCommandBar QPushButton:hover{background:#1b1c1d;color:#f4efe4;}
#writingCommandBar QToolButton:checked{background:#31291f;color:#e0b677;}
#indexPanel{background:#0d1013;border:0;border-right:1px solid #292821;}
#writingIndexEyebrow,#writingDocumentEyebrow,#writingContextEyebrow{color:#b88a52;font-size:7.5pt;font-weight:700;letter-spacing:1.5px;}
#writingIndexHeading{color:#f1ede4;font-family:'Georgia';font-size:16pt;font-weight:600;}
#writingIndexCopy{color:#767870;font-family:'Georgia';font-size:8.5pt;}
#manuscriptTree{background:transparent;color:#c8c5bc;border:0;outline:0;padding:6px 0;}
#manuscriptTree::item{padding:8px 8px;border-radius:6px;margin:1px 0;}
#manuscriptTree::item:hover{background:#171a1d;color:#f0ece3;}
#manuscriptTree::item:selected{background:#28251f;color:#e7c28a;border-left:2px solid #b88a52;}
#editorPanel{background:#0a0c0f;border:0;}
#writingDocumentStage{background:#0a0c0f;border:0;}
#writingDocumentHeader{background:#0a0c0f;border:0;border-bottom:1px solid #1f211f;}
#writingDocumentTitle{color:#f2eee5;font-family:'Georgia';font-size:22pt;font-weight:500;}
#writingDocumentHint{color:#70736e;font-family:'Georgia';font-size:8.5pt;}
#sceneEditor{background:#11120f;color:#ded8ca;border:0;padding:58px 13%;selection-background-color:#5a4630;selection-color:#fff7e8;font-family:'Georgia';}
#sceneEditor QScrollBar:vertical{background:#11120f;width:8px;}
#sceneEditor QScrollBar::handle:vertical{background:#34342e;border-radius:4px;min-height:36px;}
#metadataPanel{background:#0d1013;border:0;border-left:1px solid #292821;}
#writingContextTitle{color:#f1ede4;font-family:'Georgia';font-size:18pt;font-weight:600;}
#writingContextDescription{color:#767870;font-family:'Georgia';font-size:8.5pt;}
#writingMetaCard{background:#121518;border:1px solid #2a2b27;border-radius:9px;}
#writingMetaLabel{color:#8f918c;font-size:7.5pt;font-weight:700;letter-spacing:.9px;}
#writingMetaCard QLineEdit,#writingMetaCard QComboBox{background:#0c0f12;color:#dedad2;border:1px solid #33342f;border-radius:6px;padding:7px 9px;}
#writingMetaCard QLineEdit:focus,#writingMetaCard QComboBox:focus{border-color:#8c673f;}
#proofState,#wordCount,#focusSceneName,#sceneBoardSubheading,#sceneBoardSelection{color:#747872;font-size:8.5pt;}
#focusBar{background:#090b0d;border:0;border-bottom:1px solid #262720;}
#sceneBoardToolbar{background:#0d1013;border:0;border-bottom:1px solid #292821;padding:5px 12px;}
#sceneBoardOutlinePanel{background:#0d1013;border:0;border-right:1px solid #292821;}
#sceneBoardHeading{color:#f1ede4;font-family:'Georgia';font-size:18pt;font-weight:600;}
#sceneBoardOutline{background:transparent;color:#c7c3ba;border:0;outline:0;}
#sceneBoardOutline::item{padding:8px;border-radius:6px;}
#sceneBoardOutline::item:selected{background:#28251f;color:#e7c28a;}
#sceneBoardSearch,#sceneBoardStatus{background:#101317;color:#ddd9d0;border:1px solid #33342f;border-radius:7px;padding:7px 9px;}
#sceneBoardCanvas{background:#090b0d;border:0;}
QSplitter::handle{background:#292821;}
)QSS"));
    }

    QPointer<QMainWindow> window_;
};

} // namespace

void installWritingMockupController(QMainWindow* window) {
    if (!window || window->property("wbwWritingMockupControllerV3").toBool()) return;
    window->setProperty("wbwWritingMockupControllerV3", true);
    auto* filter = new WritingWorkspaceFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
