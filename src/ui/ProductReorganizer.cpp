#include "ui/ProductReorganizer.h"

#include <QColor>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QObject>
#include <QPainter>
#include <QPixmap>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace wbw {

namespace {

QIcon navIcon(const QString& glyph) {
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QFont font(QStringLiteral("Segoe UI Symbol"));
    font.setPixelSize(18);
    painter.setFont(font);
    painter.setPen(QColor(QStringLiteral("#91a4b7")));
    painter.drawText(pixmap.rect(), Qt::AlignCenter, glyph);
    return QIcon(pixmap);
}

void rebuildNavigationItem(QListWidget* navigation, int row, const QString& glyph, const QString& label) {
    if (!navigation || row < 0 || row >= navigation->count()) return;
    auto* item = navigation->item(row);
    item->setText(label);
    item->setIcon(navIcon(glyph));
    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    item->setSizeHint(QSize(108, 70));
}

} // namespace

void applyProductReorganization(QMainWindow* window) {
    if (!window || window->property("wbwProductReorganizedV6").toBool()) return;
    window->setProperty("wbwProductReorganizedV6", true);
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
        QObject::tr("Tablero de escenas"),
        QObject::tr("Atlas / documentación"),
        QObject::tr("Mapas"),
        QObject::tr("Revisión")
    });
    navigation->setWordWrap(true);
    navigation->setSpacing(2);
    navigation->setIconSize(QSize(22, 22));
    navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navigation->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    rebuildNavigationItem(navigation, 0, QStringLiteral("▥"), QObject::tr("Biblioteca"));
    rebuildNavigationItem(navigation, 1, QStringLiteral("▤"), QObject::tr("Escritura"));
    rebuildNavigationItem(navigation, 2, QStringLiteral("▦"), QObject::tr("Tablero de escenas"));
    rebuildNavigationItem(navigation, 3, QStringLiteral("◎"), QObject::tr("Atlas / documentación"));
    rebuildNavigationItem(navigation, 4, QStringLiteral("▧"), QObject::tr("Mapas"));
    rebuildNavigationItem(navigation, 5, QStringLiteral("▥"), QObject::tr("Revisión"));
    navigation->setCurrentRow(0);

    if (QWidget* rail = window->findChild<QWidget*>(QStringLiteral("sideRail"))) {
        rail->setFixedWidth(128);
        if (auto* layout = qobject_cast<QVBoxLayout*>(rail->layout())) {
            layout->setContentsMargins(8, 12, 8, 10);
            layout->setSpacing(6);
        }
    }
    if (QWidget* identity = window->findChild<QWidget*>(QStringLiteral("appIdentity"))) {
        identity->setMinimumHeight(58);
        identity->setMaximumHeight(58);
    }
    if (QLabel* name = window->findChild<QLabel*>(QStringLiteral("appName"))) name->hide();
    if (QLabel* mode = window->findChild<QLabel*>(QStringLiteral("appMode"))) mode->hide();
    if (QLabel* mark = window->findChild<QLabel*>(QStringLiteral("appMark"))) {
        mark->setFixedSize(42, 42);
        mark->setText(QStringLiteral("WW"));
    }

    navigation->setStyleSheet(QStringLiteral(R"QSS(
QListWidget#sideNavigation{background:transparent;border:0;outline:0;color:#8e9aa7;font-size:7.8pt;}
QListWidget#sideNavigation::item{border:0;border-radius:8px;padding:5px 3px;margin:1px 0;}
QListWidget#sideNavigation::item:hover{background:#111a24;color:#d7e2eb;}
QListWidget#sideNavigation::item:selected{background:#14263a;color:#8dcbff;border-left:2px solid #3d9df4;}
)QSS"));

    const int mapIndex = mapsTab ? worldTabs->indexOf(mapsTab) : -1;
    for (int i = 0; i < worldTabs->count(); ++i) worldTabs->setTabVisible(i, i != mapIndex);
    if (worldTabs->tabBar()) worldTabs->tabBar()->show();
}

} // namespace wbw
