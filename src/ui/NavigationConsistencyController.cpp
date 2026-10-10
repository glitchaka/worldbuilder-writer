#include "ui/NavigationConsistencyController.h"

#include "ui/ProjectHubPage.h"
#include "ui/WritingPage.h"

#include <QEvent>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QPointer>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QWidget>

namespace wbw {
namespace {

class NavigationConsistency final : public QObject {
public:
    explicit NavigationConsistency(QMainWindow* window) : QObject(window), window_(window) {
        bind();
        if (window_) window_->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == window_ && (event->type() == QEvent::Show || event->type() == QEvent::Resize))
            QTimer::singleShot(0, this, [this]() { polishShell(); enforceRoute(); });
        return QObject::eventFilter(watched, event);
    }

private:
    void bind() {
        if (!window_) return;
        nav_ = window_->findChild<QListWidget*>(QStringLiteral("sideNavigation"));
        pages_ = window_->findChild<QStackedWidget*>(QStringLiteral("pageStack"));
        hub_ = window_->findChild<QWidget*>(QStringLiteral("projectHubPage"));
        writing_ = window_->findChild<QWidget*>(QStringLiteral("writingPage"));
        world_ = window_->findChild<QWidget*>(QStringLiteral("worldPage"));
        review_ = window_->findChild<QWidget*>(QStringLiteral("reviewPage"));
        settings_ = window_->findChild<QWidget*>(QStringLiteral("settingsWorkspace"));
        writingTabs_ = window_->findChild<QTabWidget*>(QStringLiteral("writingTabs"));
        worldTabs_ = window_->findChild<QTabWidget*>(QStringLiteral("worldTabs"));
        atlasTab_ = window_->findChild<QWidget*>(QStringLiteral("atlasTab"));
        mapsTab_ = window_->findChild<QWidget*>(QStringLiteral("mapsTab"));
        rail_ = window_->findChild<QWidget*>(QStringLiteral("sideRail"));
        top_ = window_->findChild<QWidget*>(QStringLiteral("topShell"));
        if (!nav_ || !pages_) return;

        normalizeNavigation();
        QObject::connect(nav_, &QListWidget::currentRowChanged, this, [this](int) { QTimer::singleShot(0, this, [this]() { enforceRoute(); }); });
        QObject::connect(pages_, &QStackedWidget::currentChanged, this, [this](int) { QTimer::singleShot(0, this, [this]() { enforceRoute(); }); });

        if (auto* hub = qobject_cast<ProjectHubPage*>(hub_)) {
            auto goWriting = [this]() { if (nav_) nav_->setCurrentRow(1); };
            QObject::connect(hub, &ProjectHubPage::openProjectRequested, this, [goWriting](const QString&) { goWriting(); });
            QObject::connect(hub, &ProjectHubPage::newProjectRequested, this, goWriting);
            QObject::connect(hub, &ProjectHubPage::importProjectRequested, this, goWriting);
        }

        if (auto* writing = qobject_cast<WritingPage*>(writing_)) {
            QObject::connect(writing, &WritingPage::referenceActivated, this, [this](const QString& kind, const QString&) {
                if (kind == QStringLiteral("character") && nav_) nav_->setCurrentRow(3);
            });
        }

        QTimer::singleShot(0, this, [this]() { polishShell(); enforceRoute(); });
    }

    void normalizeNavigation() {
        if (!nav_) return;
        const QStringList items{
            QObject::tr("Biblioteca"), QObject::tr("Escritura"), QObject::tr("Tablero de escenas"),
            QObject::tr("Atlas / documentación"), QObject::tr("Mapas"), QObject::tr("Revisión"),
            QObject::tr("Configuración / salida")
        };
        if (nav_->count() != items.size()) {
            nav_->clear();
            nav_->addItems(items);
        } else {
            for (int i = 0; i < items.size(); ++i) nav_->item(i)->setText(items.at(i));
        }
        nav_->setSpacing(3);
        nav_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        nav_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        nav_->setMinimumHeight(items.size() * 46 + 14);
        nav_->setMaximumHeight(items.size() * 46 + 14);
        for (int i = 0; i < nav_->count(); ++i) {
            nav_->item(i)->setSizeHint(QSize(176, 43));
            nav_->item(i)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }

    void polishShell() {
        normalizeNavigation();
        if (rail_) {
            const int railWidth = window_ && window_->width() < 1180 ? 188 : 204;
            rail_->setMinimumWidth(railWidth);
            rail_->setMaximumWidth(railWidth);
            rail_->setStyleSheet(QStringLiteral(R"QSS(
#sideRail{background:#090b0e;border-right:1px solid #25251f;}
#appIdentity{background:transparent;border:0;}
#appMark{background:#30261d;color:#e9c489;border:1px solid #6f5437;border-radius:8px;font-family:'Georgia';font-size:11pt;font-weight:700;}
#appName{color:#f1ede4;font-weight:700;font-size:10pt;}
#appMode{color:#756f65;font-size:7pt;font-weight:700;letter-spacing:1.3px;}
QListWidget#sideNavigation{background:transparent;border:0;outline:0;padding:8px 0;}
QListWidget#sideNavigation::item{background:transparent;color:#8f918d;border:0;border-left:2px solid transparent;border-radius:6px;padding:9px 11px;margin:1px 0;}
QListWidget#sideNavigation::item:hover{background:#151719;color:#d8d5ce;}
QListWidget#sideNavigation::item:selected{background:#24211c;color:#e7c28a;border-left:2px solid #b88a52;font-weight:600;}
)QSS"));
        }
        if (top_) {
            top_->setMinimumHeight(54);
            top_->setMaximumHeight(54);
            top_->setStyleSheet(QStringLiteral(R"QSS(
#topShell{background:#0b0e11;border-bottom:1px solid #25251f;}
#projectTitle{color:#f0ece4;font-family:'Georgia';font-size:11pt;font-weight:600;}
#saveState{color:#777b76;font-size:8pt;}
#primarySave{background:#b98a52;color:#11120f;border:0;border-radius:7px;padding:8px 14px;font-weight:700;}
#secondaryAction{background:transparent;color:#b7b2aa;border:1px solid #373730;border-radius:7px;padding:7px 12px;}
#secondaryAction:hover{background:#1b1c1b;color:#fff8ec;}
)QSS"));
        }
    }

    void setHeader(const QString& title, const QString& state) {
        if (!window_) return;
        if (auto* label = window_->findChild<QLabel*>(QStringLiteral("projectTitle"))) label->setText(title);
        if (auto* label = window_->findChild<QLabel*>(QStringLiteral("saveState"))) label->setText(state);
    }

    void routeWorld(bool maps) {
        if (!world_ || !pages_) return;
        pages_->setCurrentWidget(world_);
        if (!worldTabs_) return;
        if (maps && mapsTab_) {
            const int mapIndex = worldTabs_->indexOf(mapsTab_);
            for (int i = 0; i < worldTabs_->count(); ++i) worldTabs_->setTabVisible(i, i == mapIndex);
            if (mapIndex >= 0) worldTabs_->setCurrentIndex(mapIndex);
            if (worldTabs_->tabBar()) worldTabs_->tabBar()->hide();
        } else {
            for (int i = 0; i < worldTabs_->count(); ++i) worldTabs_->setTabVisible(i, worldTabs_->widget(i) != mapsTab_);
            const int atlasIndex = atlasTab_ ? worldTabs_->indexOf(atlasTab_) : -1;
            if (atlasIndex >= 0) worldTabs_->setCurrentIndex(atlasIndex);
            if (worldTabs_->tabBar()) worldTabs_->tabBar()->show();
        }
    }

    void enforceRoute() {
        if (!nav_ || !pages_) return;
        polishShell();
        const int row = nav_->currentRow();
        if (auto* save = window_->findChild<QPushButton*>(QStringLiteral("primarySave"))) save->setVisible(row > 0 && row != 6);
        if (auto* focus = window_->findChild<QPushButton*>(QStringLiteral("secondaryAction"))) focus->setVisible(row == 1);

        switch (row) {
            case 0:
                if (hub_) pages_->setCurrentWidget(hub_);
                setHeader(QObject::tr("Biblioteca"), QObject::tr("Tus historias"));
                break;
            case 1:
                if (writing_) pages_->setCurrentWidget(writing_);
                if (writingTabs_) { writingTabs_->setCurrentIndex(0); if (writingTabs_->tabBar()) writingTabs_->tabBar()->hide(); }
                setHeader(QObject::tr("Escritura"), QObject::tr("Manuscrito"));
                break;
            case 2:
                if (writing_) pages_->setCurrentWidget(writing_);
                if (writingTabs_) { writingTabs_->setCurrentIndex(1); if (writingTabs_->tabBar()) writingTabs_->tabBar()->hide(); }
                setHeader(QObject::tr("Tablero de escenas"), QObject::tr("Estructura visual · arrastra para reordenar"));
                break;
            case 3:
                routeWorld(false);
                setHeader(QObject::tr("Atlas / documentación"), QObject::tr("Personajes, lugares, culturas y referencias"));
                break;
            case 4:
                routeWorld(true);
                setHeader(QObject::tr("Mapas"), QObject::tr("Pilín Rey · editor cartográfico"));
                break;
            case 5:
                if (review_) pages_->setCurrentWidget(review_);
                setHeader(QObject::tr("Revisión"), QObject::tr("Consistencia, lenguaje y estructura"));
                break;
            case 6:
                if (!settings_) settings_ = window_->findChild<QWidget*>(QStringLiteral("settingsWorkspace"));
                if (settings_) pages_->setCurrentWidget(settings_);
                setHeader(QObject::tr("Configuración / salida"), QObject::tr("Preferencias, copias y exportación"));
                break;
            default:
                nav_->setCurrentRow(0);
                break;
        }
    }

    QPointer<QMainWindow> window_;
    QPointer<QListWidget> nav_;
    QPointer<QStackedWidget> pages_;
    QPointer<QWidget> hub_;
    QPointer<QWidget> writing_;
    QPointer<QWidget> world_;
    QPointer<QWidget> review_;
    QPointer<QWidget> settings_;
    QPointer<QWidget> atlasTab_;
    QPointer<QWidget> mapsTab_;
    QPointer<QWidget> rail_;
    QPointer<QWidget> top_;
    QPointer<QTabWidget> writingTabs_;
    QPointer<QTabWidget> worldTabs_;
};

} // namespace

void installNavigationConsistencyController(QMainWindow* window) {
    if (!window || window->property("wbwNavigationConsistencyV3").toBool()) return;
    window->setProperty("wbwNavigationConsistencyV3", true);
    new NavigationConsistency(window);
}

} // namespace wbw
