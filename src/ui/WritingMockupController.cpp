#include "ui/WritingMockupController.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
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

QPushButton* buttonByText(QWidget* page, const QString& text) {
    if (!page) return nullptr;
    for (QPushButton* button : page->findChildren<QPushButton*>())
        if (button->text() == text) return button;
    return nullptr;
}

void ensureWritingShortcuts(QWidget* page, const QSettings& settings) {
    if (!page) return;
    auto* focus = page->findChild<QShortcut*>(QStringLiteral("wbwFocusShortcut"));
    if (!focus) {
        focus = new QShortcut(page);
        focus->setObjectName(QStringLiteral("wbwFocusShortcut"));
        focus->setContext(Qt::WindowShortcut);
        if (QPushButton* button = buttonByText(page, QObject::tr("Enfoque")))
            QObject::connect(focus, &QShortcut::activated, button, &QPushButton::click);
    }
    focus->setKey(QKeySequence(settings.value(QStringLiteral("shortcut/focus"), QStringLiteral("Ctrl+Shift+F")).toString()));

    auto* proof = page->findChild<QShortcut*>(QStringLiteral("wbwProofShortcut"));
    if (!proof) {
        proof = new QShortcut(page);
        proof->setObjectName(QStringLiteral("wbwProofShortcut"));
        proof->setContext(Qt::WindowShortcut);
        if (QPushButton* button = buttonByText(page, QObject::tr("Revisar")))
            QObject::connect(proof, &QShortcut::activated, button, &QPushButton::click);
    }
    proof->setKey(QKeySequence(settings.value(QStringLiteral("shortcut/proofread"), QStringLiteral("F7")).toString()));
}

void polishButton(QAbstractButton* button) {
    if (!button) return;
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(30);
}

class WritingWorkspaceFilter final : public QObject {
public:
    explicit WritingWorkspaceFilter(QMainWindow* window) : QObject(window), window_(window) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == window_ && (event->type() == QEvent::Show || event->type() == QEvent::Resize))
            QTimer::singleShot(0, this, [this]() { apply(); });
        return QObject::eventFilter(watched, event);
    }

    void apply() {
        if (!window_) return;
        QWidget* page = window_->findChild<QWidget*>(QStringLiteral("writingPage"));
        if (!page) return;

        if (auto* tabs = page->findChild<QTabWidget*>(QStringLiteral("writingTabs"))) {
            if (tabs->tabBar()) tabs->tabBar()->hide();
            tabs->setDocumentMode(true);
        }
        if (QWidget* hero = page->findChild<QWidget*>(QStringLiteral("writingHero"))) hero->hide();

        QWidget* commandBar = page->findChild<QWidget*>(QStringLiteral("writingCommandBar"));
        QWidget* index = page->findChild<QWidget*>(QStringLiteral("indexPanel"));
        QWidget* editorPanel = page->findChild<QWidget*>(QStringLiteral("editorPanel"));
        QWidget* metadata = page->findChild<QWidget*>(QStringLiteral("metadataPanel"));
        QSplitter* split = index ? qobject_cast<QSplitter*>(index->parentWidget()) : nullptr;

        if (split && editorPanel && metadata && metadata->parentWidget() != split) {
            metadata->setParent(split);
            split->addWidget(metadata);
        }
        if (split) {
            split->setChildrenCollapsible(true);
            split->setHandleWidth(1);
            split->setStretchFactor(0, 0);
            split->setStretchFactor(1, 1);
            split->setStretchFactor(2, 0);
            if (!splitInitialized_) {
                split->setSizes({236, 960, 286});
                splitInitialized_ = true;
            }
        }

        if (index) {
            index->setMinimumWidth(205);
            index->setMaximumWidth(285);
        }
        if (metadata) {
            metadata->setMinimumWidth(245);
            metadata->setMaximumWidth(310);
            metadata->show();
        }
        if (editorPanel) editorPanel->setMinimumWidth(430);

        if (commandBar) {
            commandBar->setMaximumHeight(42);
            for (QToolButton* b : commandBar->findChildren<QToolButton*>()) polishButton(b);
            for (QPushButton* b : commandBar->findChildren<QPushButton*>()) polishButton(b);
        }

        QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
        if (auto* editor = page->findChild<QTextEdit*>(QStringLiteral("sceneEditor"))) {
            QFont font(settings.value(QStringLiteral("editor/fontFamily"), QStringLiteral("Georgia")).toString());
            font.setPointSize(qBound(10, settings.value(QStringLiteral("editor/fontSize"), 13).toInt(), 24));
            editor->setFont(font);
            editor->setFrameShape(QFrame::NoFrame);
            editor->setLineWrapMode(QTextEdit::WidgetWidth);
            editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        }

        QWidget* board = page->findChild<QWidget*>(QStringLiteral("sceneBoardTab"));
        if (board) {
            if (auto* toolbar = board->findChild<QWidget*>(QStringLiteral("sceneBoardToolbar"))) {
                toolbar->setMinimumHeight(54);
                toolbar->setMaximumHeight(64);
                for (QPushButton* b : toolbar->findChildren<QPushButton*>()) polishButton(b);
            }
            if (auto* outline = board->findChild<QWidget*>(QStringLiteral("sceneBoardOutlinePanel"))) {
                outline->setMinimumWidth(210);
                outline->setMaximumWidth(260);
            }
            if (auto* view = board->findChild<QGraphicsView*>(QStringLiteral("sceneBoardCanvas"))) {
                view->setFrameShape(QFrame::NoFrame);
                view->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
                view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            }
            const auto splitters = board->findChildren<QSplitter*>();
            for (QSplitter* boardSplit : splitters) {
                boardSplit->setChildrenCollapsible(true);
                boardSplit->setHandleWidth(1);
                boardSplit->setStretchFactor(0, 0);
                boardSplit->setStretchFactor(1, 1);
                if (!boardSplitInitialized_) {
                    boardSplit->setSizes({230, 1150});
                    boardSplitInitialized_ = true;
                }
            }
        }

        ensureWritingShortcuts(page, settings);

        page->setStyleSheet(QStringLiteral(R"QSS(
#writingPage,#writingEditorTab,#sceneBoardTab{background:#0e1116;color:#d9dee7;}
#writingTabs::pane{border:0;background:#0e1116;}
#writingHero{display:none;border:0;background:transparent;}
#writingCommandBar{background:#12171d;border:0;border-bottom:1px solid #252d38;border-radius:0;padding:3px 8px;}
#writingCommandBar QToolButton,#writingCommandBar QPushButton{background:transparent;color:#aeb7c4;border:0;border-radius:6px;padding:5px 8px;min-width:28px;}
#writingCommandBar QToolButton:hover,#writingCommandBar QPushButton:hover{background:#1c232c;color:#f0f3f7;}
#writingCommandBar QToolButton:checked{background:#2b241c;color:#d7ad72;}
#indexPanel{background:#11161d;border:0;border-right:1px solid #252d38;border-radius:0;}
#editorPanel{background:#0e1116;border:0;border-radius:0;}
#metadataPanel{background:#11161d;border:0;border-left:1px solid #252d38;border-radius:0;}
#indexTitle,#editorPanelTitle,#sectionLabel{color:#c8a16d;font-size:8pt;font-weight:700;letter-spacing:1px;}
#manuscriptTree,#sceneBoardOutline{background:#11161d;color:#cfd6df;border:0;padding:7px;outline:0;}
#manuscriptTree::item,#sceneBoardOutline::item{padding:8px 7px;border-radius:6px;margin:1px 0;}
#manuscriptTree::item:hover,#sceneBoardOutline::item:hover{background:#171e27;}
#manuscriptTree::item:selected,#sceneBoardOutline::item:selected{background:#222b36;color:#ffffff;}
#sceneEditor{background:#0e1116;color:#e0ddd5;border:0;padding:64px 11%;selection-background-color:#5a4936;selection-color:#fff8ec;font-family:'Georgia';}
#sceneEditor QScrollBar:vertical{background:#0e1116;width:9px;margin:4px 2px;}
#sceneEditor QScrollBar::handle:vertical{background:#343b45;border-radius:4px;min-height:34px;}
#sceneEditor QScrollBar::add-line:vertical,#sceneEditor QScrollBar::sub-line:vertical{height:0;}
#metadataPanel QLineEdit,#metadataPanel QComboBox{background:#0d1116;color:#d9dee7;border:1px solid #2b3440;border-radius:7px;padding:7px 9px;}
#metadataPanel QLineEdit:focus,#metadataPanel QComboBox:focus{border-color:#8e6c46;}
#writingPrimary{background:#c59a5d;color:#111315;border:0;border-radius:6px;font-weight:700;padding:7px 11px;}
#writingSubtle{background:transparent;color:#aeb7c4;border:1px solid #303946;border-radius:6px;padding:6px 9px;}
#writingSubtle:hover{background:#1a212a;color:#f1f4f8;}
#proofState,#wordCount,#focusSceneName,#sceneBoardSubheading,#sceneBoardSelection{color:#788596;font-size:8.5pt;}
#focusBar{background:#0b0e12;border:0;border-bottom:1px solid #252d38;}
#focusExit{background:transparent;border:0;color:#8f99a7;}
#sceneBoardToolbar{background:#11161d;border:0;border-bottom:1px solid #252d38;border-radius:0;padding:4px 8px;}
#sceneBoardOutlinePanel{background:#11161d;border:0;border-right:1px solid #252d38;border-radius:0;}
#sceneBoardHeading{font-family:'Georgia';font-size:17pt;font-weight:700;color:#edf1f5;}
#sceneBoardSubheading{color:#7f8997;font-size:8.5pt;}
#sceneBoardSearch,#sceneBoardStatus{background:#0d1116;color:#dbe0e7;border:1px solid #2c3541;border-radius:7px;padding:6px 8px;}
#sceneBoardSearch:focus,#sceneBoardStatus:focus{border-color:#8e6c46;}
#sceneBoardCanvas{background:#0a0d11;border:0;border-radius:0;}
#sceneBoardCanvas QScrollBar:horizontal,#sceneBoardCanvas QScrollBar:vertical{background:#0a0d11;}
#sceneBoardCanvas QScrollBar::handle:horizontal,#sceneBoardCanvas QScrollBar::handle:vertical{background:#333b45;border-radius:4px;min-width:28px;min-height:28px;}
QCheckBox{color:#9ca6b4;spacing:6px;}
QSplitter::handle{background:#252d38;}
QPushButton{background:#171d25;color:#cbd2dc;border:1px solid #303946;border-radius:6px;padding:6px 9px;}
QPushButton:hover{background:#202833;color:#f3f5f7;}
)QSS"));
    }

private:
    QPointer<QMainWindow> window_;
    bool splitInitialized_ = false;
    bool boardSplitInitialized_ = false;
};

} // namespace

void installWritingMockupController(QMainWindow* window) {
    if (!window || window->property("wbwWritingMockupController").toBool()) return;
    window->setProperty("wbwWritingMockupController", true);
    auto* filter = new WritingWorkspaceFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
