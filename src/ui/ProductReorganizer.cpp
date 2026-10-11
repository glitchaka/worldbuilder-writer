#include "ui/ProductReorganizer.h"

#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QObject>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace wbw {

namespace {

void rebuildNavigationItem(QListWidget* navigation, int row, const QString& symbol, const QString& label) {
    if (!navigation || row < 0 || row >= navigation->count()) return;
    auto* item = navigation->item(row);
    item->setText(symbol + QStringLiteral("\n") + label);
    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    item->setSizeHint(QSize(108, 72));
}

} // namespace

void applyProductReorganization(QMainWindow* window) {
    if (!window || window->property("wbwProductReorganizedV5").toBool()) return;
    window->setProperty("wbwProductReorganizedV5", true);
    window->setMinimumSize(1040, 680);

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

    QObject::disconnect(navigation, &QListWidget::currentRowChanged, nullptr, nullptr);

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
        QObject::tr("Tablero"),
        QObject::tr("Atlas"),
        QObject::tr("Mapas"),
        QObject::tr("Revisión")
    });
    navigation->setWordWrap(true);
    navigation->setSpacing(2);
    navigation->setIconSize(QSize(20, 20));
    navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navigation->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    rebuildNavigationItem(navigation, 0, QStringLiteral("▥"), QObject::tr("Biblioteca"));
    rebuildNavigationItem(navigation, 1, QStringLiteral("▤"), QObject::tr("Escritura"));
    rebuildNavigationItem(navigation, 2, QStringLiteral("▦"), QObject::tr("Tablero"));
    rebuildNavigationItem(navigation, 3, QStringLiteral("◎"), QObject::tr("Atlas"));
    rebuildNavigationItem(navigation, 4, QStringLiteral("▧"), QObject::tr("Mapas"));
    rebuildNavigationItem(navigation, 5, QStringLiteral("▥"), QObject::tr("Revisión"));
    navigation->setCurrentRow(0);

    if (QWidget* rail = window->findChild<QWidget*>(QStringLiteral("sideRail"))) {
        rail->setFixedWidth(124);
        if (auto* layout = qobject_cast<QVBoxLayout*>(rail->layout())) {
            layout->setContentsMargins(8, 12, 8, 10);
            layout->setSpacing(7);
        }
    }
    if (QWidget* identity = window->findChild<QWidget*>(QStringLiteral("appIdentity"))) {
        identity->setMinimumHeight(60);
        identity->setMaximumHeight(60);
    }
    if (QLabel* name = window->findChild<QLabel*>(QStringLiteral("appName"))) name->hide();
    if (QLabel* mode = window->findChild<QLabel*>(QStringLiteral("appMode"))) mode->hide();
    if (QLabel* mark = window->findChild<QLabel*>(QStringLiteral("appMark"))) {
        mark->setFixedSize(42, 42);
        mark->setText(QStringLiteral("WW"));
    }

    navigation->setStyleSheet(QStringLiteral(R"QSS(
QListWidget#sideNavigation{background:transparent;border:0;outline:0;color:#8e9aa7;font-size:8pt;}
QListWidget#sideNavigation::item{border:0;border-radius:7px;padding:6px 3px;margin:1px 0;}
QListWidget#sideNavigation::item:hover{background:#111a24;color:#d7e2eb;}
QListWidget#sideNavigation::item:selected{background:#14263a;color:#8dcbff;border-left:2px solid #3d9df4;}
)QSS"));

    const int mapIndex = mapsTab ? worldTabs->indexOf(mapsTab) : -1;
    for (int i = 0; i < worldTabs->count(); ++i) worldTabs->setTabVisible(i, i != mapIndex);
    if (worldTabs->tabBar()) worldTabs->tabBar()->show();
}

} // namespace wbw
