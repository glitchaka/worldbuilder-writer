#include "ui/WritingMockupController.h"

#include <QEvent>
#include <QFont>
#include <QFrame>
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
#include <QWidget>

namespace wbw {
namespace {

QPushButton* buttonByText(QWidget* page, const QString& text) {
    if (!page) return nullptr;
    for (QPushButton* button : page->findChildren<QPushButton*>()) if (button->text() == text) return button;
    return nullptr;
}

void ensureWritingShortcuts(QWidget* page, const QSettings& settings) {
    if (!page) return;
    auto* focus = page->findChild<QShortcut*>(QStringLiteral("wbwFocusShortcut"));
    if (!focus) {
        focus = new QShortcut(page);
        focus->setObjectName(QStringLiteral("wbwFocusShortcut"));
        focus->setContext(Qt::WindowShortcut);
        if (QPushButton* button = buttonByText(page, QObject::tr("Enfoque"))) QObject::connect(focus, &QShortcut::activated, button, &QPushButton::click);
    }
    focus->setKey(QKeySequence(settings.value(QStringLiteral("shortcut/focus"), QStringLiteral("Ctrl+Shift+F")).toString()));

    auto* proof = page->findChild<QShortcut*>(QStringLiteral("wbwProofShortcut"));
    if (!proof) {
        proof = new QShortcut(page);
        proof->setObjectName(QStringLiteral("wbwProofShortcut"));
        proof->setContext(Qt::WindowShortcut);
        if (QPushButton* button = buttonByText(page, QObject::tr("Revisar"))) QObject::connect(proof, &QShortcut::activated, button, &QPushButton::click);
    }
    proof->setKey(QKeySequence(settings.value(QStringLiteral("shortcut/proofread"), QStringLiteral("F7")).toString()));
}

class WritingWorkspaceFilter final : public QObject {
public:
    explicit WritingWorkspaceFilter(QMainWindow* window) : QObject(window), window_(window) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::Show || event->type() == QEvent::Resize || event->type() == QEvent::LayoutRequest ||
            event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
            QTimer::singleShot(0, this, [this]() { apply(); });
        return QObject::eventFilter(watched, event);
    }

    void apply() {
        if (!window_) return;
        QWidget* page = window_->findChild<QWidget*>(QStringLiteral("writingPage"));
        if (!page) return;

        if (auto* tabs = page->findChild<QTabWidget*>(QStringLiteral("writingTabs"))) if (tabs->tabBar()) tabs->tabBar()->hide();
        if (QWidget* hero = page->findChild<QWidget*>(QStringLiteral("writingHero"))) hero->hide();

        QWidget* index = page->findChild<QWidget*>(QStringLiteral("indexPanel"));
        QWidget* editorPanel = page->findChild<QWidget*>(QStringLiteral("editorPanel"));
        QWidget* metadata = page->findChild<QWidget*>(QStringLiteral("metadataPanel"));
        QSplitter* split = index ? qobject_cast<QSplitter*>(index->parentWidget()) : nullptr;

        if (split && editorPanel && metadata && metadata->parentWidget() != split) {
            const bool visible = metadata->isVisible();
            metadata->setParent(split);
            split->addWidget(metadata);
            metadata->setVisible(visible);
            split->setStretchFactor(0, 0); split->setStretchFactor(1, 1); split->setStretchFactor(2, 0);
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
        ensureWritingShortcuts(page, settings);

        page->setStyleSheet(QStringLiteral(
            "#writingPage,#writingEditorTab,#sceneBoardTab{background:palette(window);color:palette(window-text);}"
            "#writingTabs::pane{border:0;background:palette(window);}"
            "#writingHero{border:0;background:transparent;}"
            "#writingCommandBar,#focusBar,#sceneBoardToolbar,#sceneBoardOutlinePanel{background:palette(base);border:1px solid palette(mid);border-radius:9px;}"
            "#indexPanel,#editorPanel,#metadataPanel{background:palette(base);border:1px solid palette(mid);border-radius:8px;}"
            "#indexTitle,#editorPanelTitle,#sectionLabel{color:#b98a53;font-size:8pt;font-weight:700;letter-spacing:1px;}"
            "#manuscriptTree,#sceneBoardOutline{background:palette(base);color:palette(text);border:0;padding:5px;}"
            "#manuscriptTree::item,#sceneBoardOutline::item{padding:6px;border-radius:5px;}"
            "#manuscriptTree::item:selected,#sceneBoardOutline::item:selected{background:palette(highlight);color:palette(highlighted-text);}"
            "#sceneEditor{background:palette(base);color:palette(text);border:0;padding:42px 72px;selection-background-color:palette(highlight);selection-color:palette(highlighted-text);}"
            "#metadataPanel QLineEdit,#metadataPanel QComboBox{background:palette(base);color:palette(text);border:1px solid palette(mid);border-radius:6px;padding:7px;}"
            "#writingPrimary{background:#c59a5d;color:#121418;border:1px solid #c59a5d;border-radius:6px;font-weight:700;padding:7px 11px;}"
            "#writingSubtle,QToolButton{background:palette(alternate-base);color:palette(button-text);border:1px solid palette(mid);border-radius:6px;padding:6px 9px;}"
            "#writingSubtle:hover,QToolButton:hover{background:palette(highlight);color:palette(highlighted-text);}"
            "#proofState,#wordCount,#focusSceneName,#sceneBoardSubheading,#sceneBoardSelection{color:palette(window-text);font-size:8.5pt;}"
            "#sceneBoardHeading{font-size:15pt;font-weight:700;color:palette(text);}"
            "#sceneBoardCanvas{background:palette(window);border:1px solid palette(mid);border-radius:8px;}"
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
