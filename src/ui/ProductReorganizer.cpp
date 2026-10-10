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
        const int railWidth = w < 1180 ? 148 : (w < 1450 ? 164 : 184);
        rail_->setMinimumWidth(railWidth);
        rail_->setMaximumWidth(railWidth);

        const auto splitters = window_->findChildren<QSplitter*>();
        for (QSplitter* splitter : splitters) {
            splitter->setChildrenCollapsible(true);
            splitter->setHandleWidth(w < 1180 ? 4 : 6);
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
                if (w < 1180) panel->setMaximumWidth(230);
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

    // Planning is not a top-level product area. Fold its complete functionality
    // into Atlas / documentation as the first documentation section.
    if (planning && planning->parentWidget() == pages) {
        pages->removeWidget(planning);
        worldTabs->insertTab(0, planning, QObject::tr("Personajes y tramas"));
    }

    // Normalize the world/documentation sections after the insertion above.
    if (worldTabs->count() >= 5) {
        worldTabs->setTabText(0, QObject::tr("Personajes y tramas"));
        worldTabs->setTabText(1, QObject::tr("Atlas"));
        worldTabs->setTabText(2, QObject::tr("Magia"));
        worldTabs->setTabText(3, QObject::tr("Mapas"));
        worldTabs->setTabText(4, QObject::tr("Textos de referencia"));
    }
    const int mapIndex = worldTabs->indexOf(window->findChild<QWidget*>(QStringLiteral("mapsTab")));

    // The manuscript and scene board are independent product modules. Their
    // internal tab bar no longer competes with top-level navigation.
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
        navigation->item(i)->setSizeHint(QSize(150, 38));
    }

    // Rebuild the page stack order after moving Planning under World.
    while (pages->count()) pages->removeWidget(pages->widget(0));
    pages->addWidget(hub);      // 0
    pages->addWidget(writing);  // 1
    pages->addWidget(world);    // 2
    pages->addWidget(review);   // 3

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
                    if (saveState) saveState->setText(QObject::tr("Estructura visual y drag & drop"));
                    break;
                case 3:
                    pages->setCurrentWidget(world);
                    restoreDocumentationTabs(worldTabs, mapIndex);
                    if (projectTitle) projectTitle->setText(QObject::tr("Atlas / documentación"));
                    if (saveState) saveState->setText(QObject::tr("Fichas, mundo, magia y referencias"));
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
                    if (saveState) saveState->setText(QObject::tr("Análisis y salida"));
                    break;
                default:
                    break;
            }
        });

    auto* responsive = new ResponsiveShellFilter(window, rail);
    window->installEventFilter(responsive);

    // Remove layout assumptions that caused clipping on 1366x768 and smaller.
    if (rail) {
        rail->setMinimumWidth(164);
        rail->setMaximumWidth(164);
    }
    const auto splitters = window->findChildren<QSplitter*>();
    for (QSplitter* splitter : splitters) {
        splitter->setChildrenCollapsible(true);
        splitter->setHandleWidth(5);
    }

    // Atlas/documentation starts in its editorial area. Mapas is available only
    // from the top-level Mapas module.
    restoreDocumentationTabs(worldTabs, mapIndex);
    navigation->setCurrentRow(0);

    QTimer::singleShot(0, window, [window]() {
        const QSize available = window->screen() ? window->screen()->availableGeometry().size() : QSize(1366, 768);
        if (window->width() > available.width() || window->height() > available.height()) {
            window->resize(qMin(window->width(), available.width()), qMin(window->height(), available.height()));
        }
    });
}

} // namespace wbw
