#include "ui/WritingMockupController.h"

#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QMainWindow>
#include <QPointer>
#include <QSettings>
#include <QSplitter>
#include <QTabBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QWidget>

namespace wbw {
namespace {

class WritingWorkspaceFilter final : public QObject {
public:
    explicit WritingWorkspaceFilter(QMainWindow* window) : QObject(window), window_(window) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::Show || event->type() == QEvent::Resize || event->type() == QEvent::LayoutRequest)
            QTimer::singleShot(0, this, [this]() { apply(); });
        return QObject::eventFilter(watched, event);
    }

    void apply() {
        if (!window_) return;
        QWidget* page = window_->findChild<QWidget*>(QStringLiteral("writingPage"));
        if (!page) return;

        if (auto* tabs = page->findChild<QTabWidget*>(QStringLiteral("writingTabs"))) {
            if (tabs->tabBar()) tabs->tabBar()->hide();
        }
        if (QWidget* hero = page->findChild<QWidget*>(QStringLiteral("writingHero"))) hero->hide();

        QWidget* index = page->findChild<QWidget*>(QStringLiteral("indexPanel"));
        QWidget* editorPanel = page->findChild<QWidget*>(QStringLiteral("editorPanel"));
        QWidget* metadata = page->findChild<QWidget*>(QStringLiteral("metadataPanel"));
        QSplitter* split = nullptr;
        if (index) split = qobject_cast<QSplitter*>(index->parentWidget());

        if (split && editorPanel && metadata && metadata->parentWidget() != split) {
            const bool visible = metadata->isVisible();
            metadata->setParent(split);
            split->addWidget(metadata);
            metadata->setVisible(visible);
            split->setStretchFactor(0, 0);
            split->setStretchFactor(1, 1);
            split->setStretchFactor(2, 0);
            split->setSizes({250, 900, visible ? 285 : 0});
        }

        if (index) { index->setMinimumWidth(220); index->setMaximumWidth(290); }
        if (metadata) { metadata->setMinimumWidth(250); metadata->setMaximumWidth(310); }

        QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
        if (auto* editor = page->findChild<QTextEdit*>(QStringLiteral("sceneEditor"))) {
            QFont font(settings.value(QStringLiteral("editor/fontFamily"), QStringLiteral("Georgia")).toString());
            font.setPointSize(qBound(10, settings.value(QStringLiteral("editor/fontSize"), 12).toInt(), 24));
            editor->setFont(font);
        }

        page->setStyleSheet(QStringLiteral(
            "#writingPage,#writingEditorTab,#sceneBoardTab{background:#111318;}"
            "#writingTabs::pane{border:0;background:#111318;}"
            "#writingHero{border:0;background:transparent;}"
            "#writingCommandBar,#focusBar,#sceneBoardToolbar,#sceneBoardOutlinePanel{background:#171a20;border:1px solid #2b3038;border-radius:9px;}"
            "#indexPanel,#editorPanel,#metadataPanel{background:#15181d;border:1px solid #2a2f37;border-radius:8px;}"
            "#indexTitle,#editorPanelTitle,#sectionLabel{color:#c59a5d;font-size:8pt;font-weight:700;letter-spacing:1px;}"
            "#manuscriptTree,#sceneBoardOutline{background:#15181d;color:#d8dde5;border:0;padding:5px;}"
            "#manuscriptTree::item,#sceneBoardOutline::item{padding:6px;border-radius:5px;}"
            "#manuscriptTree::item:selected,#sceneBoardOutline::item:selected{background:#292f38;color:#ffffff;}"
            "#sceneEditor{background:#121419;color:#e6e8ec;border:0;padding:42px 72px;selection-background-color:#5a4935;}"
            "#metadataPanel QLineEdit,#metadataPanel QComboBox{background:#111318;color:#e4e7ec;border:1px solid #333943;border-radius:6px;padding:7px;}"
            "#writingPrimary{background:#c59a5d;color:#121418;border:1px solid #c59a5d;border-radius:6px;font-weight:700;padding:7px 11px;}"
            "#writingSubtle,QToolButton{background:#1b1f25;color:#d9dde4;border:1px solid #303640;border-radius:6px;padding:6px 9px;}"
            "#writingSubtle:hover,QToolButton:hover{background:#252a32;}"
            "#proofState,#wordCount,#focusSceneName,#sceneBoardSubheading,#sceneBoardSelection{color:#979faa;font-size:8.5pt;}"
            "#sceneBoardHeading{font-size:15pt;font-weight:700;color:#eef0f3;}"
            "#sceneBoardCanvas{background:#101217;border:1px solid #2b3038;border-radius:8px;}"
        ));
    }

private:
    QPointer<QMainWindow> window_;
};

} // namespace

void installWritingMockupController(QMainWindow* window) {
    if (!window || window->property("wbwWritingMockupController").toBool()) return;
    window->setProperty("wbwWritingMockupController", true);
    auto* filter = new WritingWorkspaceFilter(window);
    window->installEventFilter(filter);
    const auto widgets = window->findChildren<QWidget*>();
    for (QWidget* widget : widgets) widget->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
