#include "ui/ProductReorganizer.h"

#include <QListWidget>
#include <QMainWindow>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QWidget>

namespace wbw {

void applyProductReorganization(QMainWindow* window) {
    if (!window || window->property("wbwProductReorganizedV4").toBool()) return;
    window->setProperty("wbwProductReorganizedV4", true);
    window->setMinimumSize(960, 640);

    auto* navigation = window->findChild<QListWidget*>(QStringLiteral("sideNavigation"));
    auto* pages = window->findChild<QStackedWidget*>(QStringLiteral("pageStack"));
    auto* hub = window->findChild<QWidget*>(QStringLiteral("projectHubPage"));
    auto* planning = window->findChild<QWidget*>(QStringLiteral("planningPage"));
    auto* writing = window->findChild<QWidget*>(QStringLiteral("writingPage"));
    auto* world = window->findChild<QWidget*>(QStringLiteral("worldPage"));
    auto* review = window->findChild<QWidget*>(QStringLiteral("reviewPage"));
    auto* writingTabs = window->findChild<QTabWidget*>(QStringLiteral("writingTabs"));
    auto* worldTabs = window->findChild<QTabWidget*>(QStringLiteral("worldTabs"));
    auto* mapsTab = window->findChild<QWidget*>(QStringLiteral("mapsTab"));

    if (!navigation || !pages || !hub || !writing || !world || !review || !worldTabs) return;

    // Planning becomes a documentation sub-workspace. This is a structural move only:
    // no widgets are painted over, hidden as a visual patch, or duplicated.
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

    if (writingTabs && writingTabs->tabBar()) writingTabs->tabBar()->hide();

    // The stacked widget contains real product workspaces only. Navigation routing is owned
    // by NavigationConsistencyController so there is a single source of truth.
    while (pages->count()) pages->removeWidget(pages->widget(0));
    pages->addWidget(hub);
    pages->addWidget(writing);
    pages->addWidget(world);
    pages->addWidget(review);
    pages->setCurrentWidget(hub);

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
        navigation->item(i)->setSizeHint(QSize(176, 43));
    }
    navigation->setCurrentRow(0);

    // Atlas route exposes all documentation tabs except maps; the dedicated Maps route
    // switches to the map tab and hides its tab bar in NavigationConsistencyController.
    const int mapIndex = mapsTab ? worldTabs->indexOf(mapsTab) : -1;
    for (int i = 0; i < worldTabs->count(); ++i) worldTabs->setTabVisible(i, i != mapIndex);
    if (worldTabs->tabBar()) worldTabs->tabBar()->show();
}

} // namespace wbw
