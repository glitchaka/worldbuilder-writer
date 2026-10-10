#include "ui/ProductReorganizer.h"

#include <QApplication>
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QScreen>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QWidget>

namespace wbw {
namespace {

class ResponsiveShellFilter final : public QObject {
public:
    ResponsiveShellFilter(QMainWindow* window, QWidget* rail)
        : QObject(window), window_(window), rail_(rail) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == window_ && event->type() == QEvent::Resize) update();
        return QObject::eventFilter(watched, event);
    }

private:
    void update() {
        if (!window_ || !rail_) return;
        const int w = window_->width();
        const int railWidth = w < 1040 ? 104 : (w < 1320 ? 124 : 142);
        rail_->setMinimumWidth(railWidth);
        rail_->setMaximumWidth(railWidth);

        if (QWidget* identity = window_->findChild<QWidget*>(QStringLiteral("appIdentity")))
            identity->setVisible(w >= 1040);

        const auto splitters = window_->findChildren<QSplitter*>();
        for (QSplitter* splitter : splitters) {
            splitter->setChildrenCollapsible(true);
            splitter->setHandleWidth(w < 1180 ? 3 : 5);
        }

        const QStringList compactPanels{
            QStringLiteral("indexPanel"),
            QStringLiteral("sceneBoardOutlinePanel"),
            QStringLiteral("worldIndexPanel"),
            QStringLiteral("magicIndexPanel"),
            QStringLiteral("planningIndex"),
            QStringLiteral("planningRelationPanel")
        };
        for (const QString& name : compactPanels) {
            if (QWidget* panel = window_->findChild<QWidget*>(name)) {
                panel->setMinimumWidth(0);
                panel->setMaximumWidth(w < 1180 ? 214 : 286);
            }
        }
    }

    QMainWindow* window_ = nullptr;
    QWidget* rail_ = nullptr;
};

void setOnlyTabVisible(QTabWidget* tabs, int wanted, bool showTabBar) {
    if (!tabs) return;
    for (int i = 0; i < tabs->count(); ++i) tabs->setTabVisible(i, i == wanted);
    tabs->setCurrentIndex(wanted);
    if (tabs->tabBar()) tabs->tabBar()->setVisible(showTabBar);
}

void restoreDocumentationTabs(QTabWidget* tabs, int mapIndex) {
    if (!tabs) return;
    for (int i = 0; i < tabs->count(); ++i) tabs->setTabVisible(i, i != mapIndex);
    if (tabs->tabBar()) tabs->tabBar()->show();
    if (tabs->currentIndex() == mapIndex || tabs->currentIndex() < 0) tabs->setCurrentIndex(0);
}

void removeLegacyChrome(QMainWindow* window) {
    const QStringList hideNames{
        QStringLiteral("writingHero"),
        QStringLiteral("planningHero"),
        QStringLiteral("worldHero"),
        QStringLiteral("reviewHero")
    };
    for (const QString& name : hideNames)
        if (QWidget* widget = window->findChild<QWidget*>(name)) widget->hide();
}

void applyApprovedVisualLanguage(QMainWindow* window) {
    if (!window) return;
    window->setStyleSheet(window->styleSheet() + QStringLiteral(R"QSS(
#appRoot, #workspaceShell, #contentShell, #pageStack,
#writingPage, #writingEditorTab, #sceneBoardTab,
#planningPage, #planningCharacters, #planningBoard, #planningTheories, #planningTimeline,
#worldPage, #reviewPage { background:#111315; color:#d9dde3; }

#sideRail { background:#0b0d0f; border-right:1px solid #24282d; }
#appMark { background:#b88a52; color:#111315; border-radius:8px; font-weight:800; }
#appName { color:#f0f1f3; font-weight:700; }
#appMode { color:#727982; font-size:8pt; letter-spacing:1px; }
#sideNavigation { background:transparent; border:0; outline:0; color:#858c95; }
#sideNavigation::item { border-radius:7px; padding:0 10px; margin:2px 0; }
#sideNavigation::item:hover { background:#171a1e; color:#e8eaed; }
#sideNavigation::item:selected { background:#20242a; color:#f4f5f6; border-left:2px solid #c49358; }
#settingsAction { background:transparent; border:1px solid #282d33; color:#9da4ad; border-radius:7px; }
#settingsAction:hover { background:#171a1e; color:#f0f1f3; }

#topShell { background:#111315; border-bottom:1px solid #24282d; }
#projectTitle { color:#f1f2f4; font-size:11pt; font-weight:700; }
#saveState { color:#737a83; font-size:8pt; }
#primarySave { background:#b88952; color:#111315; border:0; border-radius:7px; padding:7px 13px; font-weight:700; }
#secondaryAction { background:#171a1e; color:#c8cdd3; border:1px solid #30353c; border-radius:7px; padding:7px 12px; }

#writingTabs::pane, #planningTabs::pane, #worldTabs::pane { border:0; background:#111315; }
#writingTabs QTabBar::tab, #planningTabs QTabBar::tab, #worldTabs QTabBar::tab {
    background:transparent; color:#7f8790; border:0; padding:9px 14px; margin-right:2px;
}
#planningTabs QTabBar::tab:selected, #worldTabs QTabBar::tab:selected {
    color:#eceef0; border-bottom:2px solid #b88952;
}

#writingCommandBar, #sceneBoardToolbar {
    background:#14171a; border:1px solid #282d33; border-radius:9px;
}
#writingCommandBar QToolButton, #writingCommandBar QPushButton,
#sceneBoardToolbar QToolButton, #sceneBoardToolbar QPushButton {
    background:transparent; color:#aeb4bc; border:1px solid transparent; border-radius:6px; padding:6px 8px;
}
#writingCommandBar QToolButton:hover, #writingCommandBar QPushButton:hover,
#sceneBoardToolbar QToolButton:hover, #sceneBoardToolbar QPushButton:hover {
    background:#20242a; color:#f1f2f4; border-color:#30353c;
}
#writingCommandBar QToolButton:checked { background:#2a251f; color:#d6a96e; border-color:#5a4935; }

#indexPanel, #editorPanel, #metadataPanel, #sceneBoardOutlinePanel,
#planningIndex, #planningEditor, #planningRelationPanel,
#worldIndexPanel, #magicIndexPanel {
    background:#14171a; border:1px solid #282d33; border-radius:10px;
}
#indexTitle, #editorPanelTitle, #planningIndexTitle, #planningField, #sectionLabel {
    color:#8a919a; font-size:8pt; font-weight:700; letter-spacing:.6px;
}

#sceneEditor {
    background:#151719; color:#dedbd3; border:0; border-radius:9px;
    padding:34px 56px; font-family:'Georgia'; font-size:12pt;
    selection-background-color:#5b4833; selection-color:#fff8ec;
}
#sceneEditor QScrollBar:vertical { background:#151719; width:9px; }
#sceneEditor QScrollBar::handle:vertical { background:#34383e; border-radius:4px; min-height:30px; }

#planningCard { background:#171a1e; border:1px solid #2a2f35; border-radius:10px; }
#planningCardTitle { color:#eceef0; font-family:'Georgia'; font-size:13pt; }
#characterImage { background:#111315; border:1px solid #30353c; border-radius:8px; color:#737a83; }

QLineEdit, QTextEdit, QPlainTextEdit, QComboBox, QSpinBox {
    background:#111315; color:#d7dbe0; border:1px solid #30353c; border-radius:7px; padding:7px 9px;
    selection-background-color:#5b4833;
}
QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QComboBox:focus, QSpinBox:focus { border-color:#8c6944; }
QListWidget, QTreeWidget {
    background:#14171a; color:#c9ced4; border:0; outline:0;
}
QListWidget::item, QTreeWidget::item { padding:6px; border-radius:5px; }
QListWidget::item:selected, QTreeWidget::item:selected { background:#272c32; color:#f3f4f5; }

#sceneBoardCanvas { background:#0f1113; border:1px solid #282d33; border-radius:9px; }
#sceneBoardHeading { color:#f0f1f3; }
#sceneBoardSubheading, #sceneBoardSelection, #proofState, #wordCount { color:#7f8790; }

#worldTabs, #planningTabs { background:#111315; }
#mapsTab { background:#0d0f11; }
#pilinToolOptions { background:#171a1e; border:1px solid #343a42; border-radius:9px; }

QPushButton { background:#1a1e22; color:#c9ced4; border:1px solid #30353c; border-radius:7px; padding:7px 11px; }
QPushButton:hover { background:#23282e; color:#f1f2f4; }
#writingPrimary, #planningPrimary { background:#b88952; color:#111315; border:0; font-weight:700; }
#writingSubtle { background:#171a1e; color:#b8bec6; border:1px solid #30353c; }
)QSS"));
}

} // namespace

void applyProductReorganization(QMainWindow* window) {
    if (!window || window->property("wbwProductReorganized").toBool()) return;
    window->setProperty("wbwProductReorganized", true);
    window->setMinimumSize(900, 580);

    auto* navigation = window->findChild<QListWidget*>(QStringLiteral("sideNavigation"));
    auto* pages = window->findChild<QStackedWidget*>(QStringLiteral("pageStack"));
    auto* hub = window->findChild<QWidget*>(QStringLiteral("projectHubPage"));
    auto* planning = window->findChild<QWidget*>(QStringLiteral("planningPage"));
    auto* writing = window->findChild<QWidget*>(QStringLiteral("writingPage"));
    auto* world = window->findChild<QWidget*>(QStringLiteral("worldPage"));
    auto* review = window->findChild<QWidget*>(QStringLiteral("reviewPage"));
    auto* writingTabs = window->findChild<QTabWidget*>(QStringLiteral("writingTabs"));
    auto* worldTabs = window->findChild<QTabWidget*>(QStringLiteral("worldTabs"));
    auto* rail = window->findChild<QWidget*>(QStringLiteral("sideRail"));
    auto* projectTitle = window->findChild<QLabel*>(QStringLiteral("projectTitle"));
    auto* saveState = window->findChild<QLabel*>(QStringLiteral("saveState"));
    auto* quickSave = window->findChild<QPushButton*>(QStringLiteral("primarySave"));
    auto* focus = window->findChild<QPushButton*>(QStringLiteral("secondaryAction"));

    if (!navigation || !pages || !hub || !writing || !world || !review || !worldTabs) return;

    if (planning && planning->parentWidget() == pages) {
        pages->removeWidget(planning);
        worldTabs->insertTab(0, planning, QObject::tr("Personajes y tramas"));
    }

    if (worldTabs->count() >= 5) {
        worldTabs->setTabText(0, QObject::tr("Personajes y tramas"));
        worldTabs->setTabText(1, QObject::tr("Atlas"));
        worldTabs->setTabText(2, QObject::tr("Magia"));
        worldTabs->setTabText(3, QObject::tr("Mapas"));
        worldTabs->setTabText(4, QObject::tr("Textos de referencia"));
    }
    const int mapIndex = worldTabs->indexOf(window->findChild<QWidget*>(QStringLiteral("mapsTab")));

    if (writingTabs && writingTabs->tabBar()) writingTabs->tabBar()->hide();

    QObject::disconnect(navigation, &QListWidget::currentRowChanged, nullptr, nullptr);
    navigation->clear();
    navigation->addItems({
        QObject::tr("Biblioteca"),
        QObject::tr("Escritura"),
        QObject::tr("Tablero de escenas"),
        QObject::tr("Atlas / documentación"),
        QObject::tr("Mapas"),
        QObject::tr("Revisión")
    });
    for (int i = 0; i < navigation->count(); ++i) {
        navigation->item(i)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        navigation->item(i)->setSizeHint(QSize(136, 38));
    }

    while (pages->count()) pages->removeWidget(pages->widget(0));
    pages->addWidget(hub);
    pages->addWidget(writing);
    pages->addWidget(world);
    pages->addWidget(review);

    QObject::connect(navigation, &QListWidget::currentRowChanged, window,
        [=](int row) {
            if (quickSave) quickSave->setVisible(row > 0);
            if (focus) focus->setVisible(row == 1);

            switch (row) {
                case 0:
                    pages->setCurrentWidget(hub);
                    if (projectTitle) projectTitle->setText(QObject::tr("Biblioteca"));
                    if (saveState) saveState->setText(QObject::tr("Proyectos locales"));
                    break;
                case 1:
                    pages->setCurrentWidget(writing);
                    if (writingTabs) writingTabs->setCurrentIndex(0);
                    if (projectTitle) projectTitle->setText(QObject::tr("Escritura"));
                    if (saveState) saveState->setText(QObject::tr("Manuscrito"));
                    break;
                case 2:
                    pages->setCurrentWidget(writing);
                    if (writingTabs) writingTabs->setCurrentIndex(1);
                    if (projectTitle) projectTitle->setText(QObject::tr("Tablero de escenas"));
                    if (saveState) saveState->setText(QObject::tr("Estructura visual · arrastrar para reordenar"));
                    break;
                case 3:
                    pages->setCurrentWidget(world);
                    restoreDocumentationTabs(worldTabs, mapIndex);
                    if (projectTitle) projectTitle->setText(QObject::tr("Atlas / documentación"));
                    if (saveState) saveState->setText(QObject::tr("Personajes, lugares, mundo y referencias"));
                    break;
                case 4:
                    pages->setCurrentWidget(world);
                    if (mapIndex >= 0) setOnlyTabVisible(worldTabs, mapIndex, false);
                    if (projectTitle) projectTitle->setText(QObject::tr("Mapas"));
                    if (saveState) saveState->setText(QObject::tr("Pilín Rey"));
                    break;
                case 5:
                    pages->setCurrentWidget(review);
                    if (projectTitle) projectTitle->setText(QObject::tr("Revisión"));
                    if (saveState) saveState->setText(QObject::tr("Consistencia, lenguaje y salida"));
                    break;
                default:
                    break;
            }
        });

    auto* responsive = new ResponsiveShellFilter(window, rail);
    window->installEventFilter(responsive);

    if (rail) {
        rail->setMinimumWidth(124);
        rail->setMaximumWidth(124);
    }
    const auto splitters = window->findChildren<QSplitter*>();
    for (QSplitter* splitter : splitters) {
        splitter->setChildrenCollapsible(true);
        splitter->setHandleWidth(4);
    }

    removeLegacyChrome(window);
    applyApprovedVisualLanguage(window);
    restoreDocumentationTabs(worldTabs, mapIndex);
    navigation->setCurrentRow(0);

    QTimer::singleShot(0, window, [window]() {
        removeLegacyChrome(window);
        applyApprovedVisualLanguage(window);
        const QSize available = window->screen() ? window->screen()->availableGeometry().size() : QSize(1366, 768);
        if (window->width() > available.width() || window->height() > available.height())
            window->resize(qMin(window->width(), available.width()), qMin(window->height(), available.height()));
    });
}

} // namespace wbw
