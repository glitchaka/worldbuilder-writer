#include "ui/WritingMockupController.h"

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
#include <QSettings>
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

QLineEdit* fieldByPlaceholder(QWidget* root, const QString& placeholder) {
    if (!root) return nullptr;
    for (QLineEdit* field : root->findChildren<QLineEdit*>())
        if (field->placeholderText() == placeholder) return field;
    return nullptr;
}

QLabel* label(const QString& text, const QString& name, QWidget* parent) {
    auto* value = new QLabel(text, parent);
    value->setObjectName(name);
    return value;
}

void addMetaRow(QVBoxLayout* layout, QWidget* owner, const QString& title, QWidget* field) {
    if (!layout || !owner || !field) return;
    auto* row = new QFrame(owner);
    row->setObjectName(QStringLiteral("writingMetaRow"));
    auto* box = new QVBoxLayout(row);
    box->setContentsMargins(0, 0, 0, 12);
    box->setSpacing(5);
    box->addWidget(label(title, QStringLiteral("writingMetaLabel"), row));
    field->setParent(row);
    field->setMinimumHeight(34);
    box->addWidget(field);
    layout->addWidget(row);
}

void buildMetadata(QWidget* metadata) {
    if (!metadata || metadata->property("wbwEditorialMetadataStableV1").toBool()) return;
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
    layout->setContentsMargins(16, 0, 16, 16);
    layout->setSpacing(0);

    auto* tabs = new QWidget(metadata);
    tabs->setObjectName(QStringLiteral("writingContextTabs"));
    auto* tabsLayout = new QHBoxLayout(tabs);
    tabsLayout->setContentsMargins(0, 0, 0, 0);
    tabsLayout->setSpacing(18);
    tabsLayout->addWidget(label(QObject::tr("Escena"), QStringLiteral("writingContextTabActive"), tabs));
    tabsLayout->addWidget(label(QObject::tr("Notas"), QStringLiteral("writingContextTab"), tabs));
    tabsLayout->addWidget(label(QObject::tr("Referencias"), QStringLiteral("writingContextTab"), tabs));
    tabsLayout->addStretch(1);
    layout->addWidget(tabs);

    auto* divider = new QFrame(metadata);
    divider->setObjectName(QStringLiteral("writingContextDivider"));
    divider->setFixedHeight(1);
    layout->addWidget(divider);
    layout->addSpacing(16);

    auto* sceneHead = new QHBoxLayout;
    sceneHead->addWidget(label(QStringLiteral("●"), QStringLiteral("writingSceneDot"), metadata));
    sceneHead->addWidget(label(QObject::tr("Escena actual"), QStringLiteral("writingSceneHeader"), metadata));
    sceneHead->addStretch(1);
    layout->addLayout(sceneHead);
    layout->addSpacing(12);

    addMetaRow(layout, metadata, QObject::tr("TÍTULO"), title);
    addMetaRow(layout, metadata, QObject::tr("ESTADO"), status);
    addMetaRow(layout, metadata, QObject::tr("POV"), pov);
    addMetaRow(layout, metadata, QObject::tr("UBICACIÓN"), location);
    addMetaRow(layout, metadata, QObject::tr("TIEMPO / CAPA NARRATIVA"), layer);

    auto* stats = new QFrame(metadata);
    stats->setObjectName(QStringLiteral("writingContextStats"));
    auto* statsLayout = new QHBoxLayout(stats);
    statsLayout->setContentsMargins(10, 12, 10, 12);
    const QStringList names{QObject::tr("palabras"), QObject::tr("referencias"), QObject::tr("notas")};
    for (const QString& text : names) {
        auto* cell = new QWidget(stats);
        auto* cellLayout = new QVBoxLayout(cell);
        cellLayout->setContentsMargins(0, 0, 0, 0);
        cellLayout->setSpacing(0);
        auto* value = label(QStringLiteral("—"), QStringLiteral("writingContextStatValue"), cell);
        auto* name = label(text, QStringLiteral("writingContextStatName"), cell);
        value->setAlignment(Qt::AlignCenter);
        name->setAlignment(Qt::AlignCenter);
        cellLayout->addWidget(value);
        cellLayout->addWidget(name);
        statsLayout->addWidget(cell, 1);
    }
    layout->addWidget(stats);
    layout->addSpacing(16);

    layout->addWidget(label(QObject::tr("REFERENCIAS EN ESTA ESCENA"), QStringLiteral("writingContextSectionTitle"), metadata));
    auto* hint = label(QObject::tr("Las referencias semánticas viven dentro del manuscrito. Selecciona una mención para abrir su tarjeta contextual."), QStringLiteral("writingContextHint"), metadata);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    layout->addStretch(1);

    metadata->setProperty("wbwEditorialMetadataStableV1", true);
}

void buildIndex(QWidget* index) {
    if (!index || index->property("wbwEditorialIndexStableV1").toBool()) return;
    auto* layout = qobject_cast<QVBoxLayout*>(index->layout());
    if (!layout) return;
    if (QLabel* legacy = index->findChild<QLabel*>(QStringLiteral("indexTitle"))) legacy->hide();

    auto* project = new QWidget(index);
    project->setObjectName(QStringLiteral("writingProjectHeader"));
    auto* projectLayout = new QVBoxLayout(project);
    projectLayout->setContentsMargins(6, 8, 6, 10);
    projectLayout->setSpacing(3);
    projectLayout->addWidget(label(QObject::tr("MANUSCRITO"), QStringLiteral("writingProjectName"), project));
    projectLayout->addWidget(label(QObject::tr("Capítulos y escenas"), QStringLiteral("writingProjectSection"), project));
    layout->insertWidget(0, project);

    auto* search = new QLineEdit(index);
    search->setObjectName(QStringLiteral("writingSceneSearch"));
    search->setPlaceholderText(QObject::tr("Buscar capítulos o escenas…"));
    search->setClearButtonEnabled(true);
    layout->insertWidget(1, search);

    if (auto* tree = index->findChild<QTreeWidget*>(QStringLiteral("manuscriptTree"))) {
        QObject::connect(search, &QLineEdit::textChanged, tree, [tree](const QString& text) {
            const QString needle = text.trimmed();
            for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                QTreeWidgetItem* chapter = tree->topLevelItem(i);
                const bool chapterMatch = needle.isEmpty() || chapter->text(0).contains(needle, Qt::CaseInsensitive);
                bool childMatch = false;
                for (int j = 0; j < chapter->childCount(); ++j) {
                    QTreeWidgetItem* scene = chapter->child(j);
                    const bool match = needle.isEmpty() || scene->text(0).contains(needle, Qt::CaseInsensitive);
                    scene->setHidden(!match && !chapterMatch);
                    childMatch |= match;
                }
                chapter->setHidden(!chapterMatch && !childMatch);
                if (!needle.isEmpty() && (chapterMatch || childMatch)) chapter->setExpanded(true);
            }
        });
    }

    index->setProperty("wbwEditorialIndexStableV1", true);
}

void buildDocumentStage(QWidget* editorPanel, QTextEdit* editor) {
    if (!editorPanel || !editor || editorPanel->property("wbwEditorialStageStableV1").toBool()) return;
    auto* layout = qobject_cast<QVBoxLayout*>(editorPanel->layout());
    if (!layout) return;
    const int editorIndex = layout->indexOf(editor);
    if (editorIndex < 0) return;
    layout->removeWidget(editor);

    auto* stage = new QFrame(editorPanel);
    stage->setObjectName(QStringLiteral("writingDocumentStage"));
    auto* stageLayout = new QVBoxLayout(stage);
    stageLayout->setContentsMargins(0, 0, 0, 0);
    stageLayout->setSpacing(0);

    auto* header = new QFrame(stage);
    header->setObjectName(QStringLiteral("writingDocumentHeader"));
    auto* headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(30, 15, 30, 12);
    headerLayout->setSpacing(8);

    auto* breadcrumb = new QHBoxLayout;
    breadcrumb->setSpacing(8);
    breadcrumb->addWidget(label(QStringLiteral("⌂"), QStringLiteral("writingBreadcrumbIcon"), header));
    breadcrumb->addWidget(label(QObject::tr("Manuscrito"), QStringLiteral("writingBreadcrumb"), header));
    breadcrumb->addWidget(label(QStringLiteral("›"), QStringLiteral("writingBreadcrumbDim"), header));
    breadcrumb->addWidget(label(QObject::tr("Escena actual"), QStringLiteral("writingBreadcrumb"), header));
    breadcrumb->addStretch(1);
    breadcrumb->addWidget(label(QObject::tr("●  Guardado automáticamente"), QStringLiteral("writingSavedState"), header));
    headerLayout->addLayout(breadcrumb);

    auto* titleLine = new QHBoxLayout;
    auto* sceneTitle = label(QObject::tr("Escena sin título"), QStringLiteral("writingDocumentTitle"), header);
    titleLine->addWidget(sceneTitle);
    titleLine->addStretch(1);
    titleLine->addWidget(label(QObject::tr("POV"), QStringLiteral("writingHeaderMeta"), header));
    headerLayout->addLayout(titleLine);
    stageLayout->addWidget(header);

    editor->setParent(stage);
    stageLayout->addWidget(editor, 1);
    layout->insertWidget(editorIndex, stage, 1);

    if (QLineEdit* source = fieldByPlaceholder(editorPanel->window(), QObject::tr("Título de escena"))) {
        QObject::connect(source, &QLineEdit::textChanged, sceneTitle, [sceneTitle](const QString& text) {
            sceneTitle->setText(text.trimmed().isEmpty() ? QObject::tr("Escena sin título") : text.trimmed());
        });
        if (!source->text().trimmed().isEmpty()) sceneTitle->setText(source->text().trimmed());
    }

    editorPanel->setProperty("wbwEditorialStageStableV1", true);
}

void stylePage(QWidget* page) {
    if (!page || page->property("wbwWritingStyledStableV1").toBool()) return;
    page->setProperty("wbwWritingStyledStableV1", true);
    page->setStyleSheet(QStringLiteral(R"QSS(
#writingPage,#writingEditorTab,#sceneBoardTab{background:#0b1016;color:#d8dee7;}
#writingTabs::pane{border:0;background:#0b1016;}
#writingCommandBar{background:#0d141c;border:0;border-bottom:1px solid #28313c;padding:4px 22px;}
#writingCommandBar QToolButton,#writingCommandBar QPushButton{background:transparent;color:#a7b0ba;border:0;border-radius:5px;padding:5px 9px;}
#writingCommandBar QToolButton:hover,#writingCommandBar QPushButton:hover{background:#18222d;color:#f1f5f8;}
#writingCommandBar QToolButton:checked{background:#17364b;color:#79cfff;}
#indexPanel{background:#0c1219;border:0;border-right:1px solid #25303b;}
#writingProjectName{color:#f0f3f6;font-family:'Georgia';font-size:13pt;font-weight:600;}
#writingProjectSection{color:#768391;font-size:8pt;font-weight:600;letter-spacing:1px;}
#writingSceneSearch{background:#0e161f;color:#dce4eb;border:1px solid #2b3744;border-radius:6px;padding:8px 10px;}
#writingSceneSearch:focus{border-color:#3e82b4;}
#manuscriptTree{background:transparent;color:#c7d0d8;border:0;outline:0;padding:4px 0;}
#manuscriptTree::item{padding:8px 7px;border-radius:5px;margin:1px 0;}
#manuscriptTree::item:hover{background:#131d27;color:#f0f5f8;}
#manuscriptTree::item:selected{background:#12333f;color:#78d4dc;border-left:2px solid #2dcad0;}
#editorPanel,#writingDocumentStage{background:#0b1016;border:0;}
#writingDocumentHeader{background:#0b1016;border:0;border-bottom:1px solid #202a34;}
#writingBreadcrumb,#writingBreadcrumbDim,#writingBreadcrumbIcon{color:#7c8996;font-size:8.5pt;}
#writingSavedState{color:#75967f;font-size:8pt;}
#writingDocumentTitle{color:#f0ece4;font-family:'Georgia';font-size:22pt;font-weight:500;}
#writingHeaderMeta{color:#8c98a4;font-size:8pt;}
#sceneEditor{background:#0b1016;color:#e6dfd5;border:0;padding:0;selection-background-color:#315b72;selection-color:#ffffff;font-family:'Georgia';}
#metadataPanel{background:#0c1219;border:0;border-left:1px solid #25303b;}
#writingContextTab,#writingContextTabActive{padding:12px 0 10px 0;font-size:8.5pt;}
#writingContextTab{color:#7f8994;}
#writingContextTabActive{color:#e6edf2;border-bottom:2px solid #2bc8d0;font-weight:700;}
#writingContextDivider{background:#27313b;border:0;}
#writingSceneDot{color:#ee6470;font-size:10pt;}
#writingSceneHeader{color:#eef2f5;font-family:'Georgia';font-size:12pt;font-weight:600;}
#writingMetaLabel{color:#7f8a95;font-size:7.5pt;font-weight:700;letter-spacing:.7px;}
#writingMetaRow QLineEdit,#writingMetaRow QComboBox{background:#101821;color:#dce3e9;border:1px solid #2b3641;border-radius:5px;padding:7px 9px;}
#writingContextStats{background:#101821;border:1px solid #293540;border-radius:7px;}
#writingContextStatValue{color:#e4ebf0;font-weight:700;}
#writingContextStatName{color:#7d8994;font-size:7.5pt;}
#writingContextSectionTitle{color:#d9e0e6;font-size:8pt;font-weight:700;letter-spacing:.5px;}
#writingContextHint{color:#75818c;font-size:8pt;padding:8px 0;}
#sceneBoardToolbar{background:#0d141c;border:0;border-bottom:1px solid #28313c;}
#sceneBoardOutlinePanel{background:#0c1219;border:0;border-right:1px solid #25303b;}
#sceneBoardHeading{color:#eef2f4;font-family:'Georgia';font-size:18pt;font-weight:600;}
#sceneBoardCanvas{background:#0a0f15;border:0;}
#sceneBoardOutline{background:transparent;color:#bfc9d2;border:0;outline:0;}
#sceneBoardOutline::item{padding:6px;border-radius:5px;}
#sceneBoardOutline::item:selected{background:#172b3d;color:#90cfff;}
QSplitter::handle{background:#25303b;}
)QSS"));
}

class WritingWorkspaceFilter final : public QObject {
public:
    explicit WritingWorkspaceFilter(QMainWindow* window) : QObject(window), window_(window) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        if ((watched == window_ || watched == page_) && (event->type() == QEvent::Show || event->type() == QEvent::Resize))
            schedule();
        return false;
    }

    void apply() {
        pending_ = false;
        if (!window_) return;
        QWidget* page = window_->findChild<QWidget*>(QStringLiteral("writingPage"));
        if (!page) return;
        page_ = page;

        if (!initialized_) {
            if (auto* tabs = page->findChild<QTabWidget*>(QStringLiteral("writingTabs")); tabs && tabs->tabBar()) tabs->tabBar()->hide();
            if (QWidget* hero = page->findChild<QWidget*>(QStringLiteral("writingHero"))) hero->hide();

            QWidget* index = page->findChild<QWidget*>(QStringLiteral("indexPanel"));
            QWidget* editorPanel = page->findChild<QWidget*>(QStringLiteral("editorPanel"));
            QWidget* metadata = page->findChild<QWidget*>(QStringLiteral("metadataPanel"));
            auto* editor = page->findChild<QTextEdit*>(QStringLiteral("sceneEditor"));
            auto* split = index ? qobject_cast<QSplitter*>(index->parentWidget()) : nullptr;
            if (split && metadata && metadata->parentWidget() != split) {
                metadata->setParent(split);
                split->addWidget(metadata);
            }
            buildIndex(index);
            buildMetadata(metadata);
            buildDocumentStage(editorPanel, editor);
            stylePage(page);

            if (QWidget* command = page->findChild<QWidget*>(QStringLiteral("writingCommandBar"))) {
                command->setMinimumHeight(46);
                command->setMaximumHeight(46);
            }
            if (QWidget* board = page->findChild<QWidget*>(QStringLiteral("sceneBoardTab"))) {
                if (QWidget* toolbar = board->findChild<QWidget*>(QStringLiteral("sceneBoardToolbar"))) {
                    toolbar->setMinimumHeight(72);
                    toolbar->setMaximumHeight(78);
                }
                if (auto* view = board->findChild<QGraphicsView*>(QStringLiteral("sceneBoardCanvas"))) view->setFrameShape(QFrame::NoFrame);
            }

            page->installEventFilter(this);
            initialized_ = true;
        }

        QWidget* index = page->findChild<QWidget*>(QStringLiteral("indexPanel"));
        QWidget* metadata = page->findChild<QWidget*>(QStringLiteral("metadataPanel"));
        auto* split = index ? qobject_cast<QSplitter*>(index->parentWidget()) : nullptr;
        const bool wide = window_->width() >= 1240;
        if (index) {
            index->setMinimumWidth(wide ? 286 : 220);
            index->setMaximumWidth(wide ? 336 : 250);
        }
        if (metadata) {
            metadata->setVisible(wide);
            metadata->setMinimumWidth(wide ? 292 : 0);
            metadata->setMaximumWidth(wide ? 340 : 0);
        }
        if (split) {
            split->setHandleWidth(1);
            split->setStretchFactor(0, 0);
            split->setStretchFactor(1, 1);
            split->setStretchFactor(2, 0);
            split->setSizes(wide ? QList<int>{310, 950, 316} : QList<int>{235, 900, 0});
        }

        if (auto* editor = page->findChild<QTextEdit*>(QStringLiteral("sceneEditor"))) {
            QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
            QFont font(settings.value(QStringLiteral("editor/fontFamily"), QStringLiteral("Georgia")).toString());
            font.setPointSize(qBound(11, settings.value(QStringLiteral("editor/fontSize"), 14).toInt(), 24));
            editor->setFont(font);
            editor->setFrameShape(QFrame::NoFrame);
            editor->setLineWrapMode(QTextEdit::WidgetWidth);
            editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        }
    }

private:
    void schedule() {
        if (pending_) return;
        pending_ = true;
        QTimer::singleShot(45, this, [this]() { apply(); });
    }

    QPointer<QMainWindow> window_;
    QPointer<QWidget> page_;
    bool initialized_ = false;
    bool pending_ = false;
};

} // namespace

void installWritingMockupController(QMainWindow* window) {
    if (!window || window->property("wbwWritingWorkspaceStableV1").toBool()) return;
    window->setProperty("wbwWritingWorkspaceStableV1", true);
    auto* filter = new WritingWorkspaceFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
