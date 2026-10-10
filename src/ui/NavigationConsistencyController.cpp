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
    explicit NavigationConsistency(QMainWindow* window)
        : QObject(window), window_(window) {
        bind();
        if (window_) window_->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == window_ && (event->type() == QEvent::Resize || event->type() == QEvent::Show))
            QTimer::singleShot(0, this, [this]() { enforceReadableRail(); });
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
        writingTabs_ = window_->findChild<QTabWidget*>(QStringLiteral("writingTabs"));
        worldTabs_ = window_->findChild<QTabWidget*>(QStringLiteral("worldTabs"));
        mapsTab_ = window_->findChild<QWidget*>(QStringLiteral("mapsTab"));
        atlasTab_ = window_->findChild<QWidget*>(QStringLiteral("atlasTab"));
        settings_ = window_->findChild<QWidget*>(QStringLiteral("settingsWorkspace"));
        rail_ = window_->findChild<QWidget*>(QStringLiteral("sideRail"));
        if (!nav_ || !pages_) return;

        normalizeNavigation();
        connect(nav_, &QListWidget::currentRowChanged, this, [this](int) {
            QTimer::singleShot(0, this, [this]() { enforceCurrentRoute(); });
        });
        connect(pages_, &QStackedWidget::currentChanged, this, [this](int) {
            QTimer::singleShot(0, this, [this]() { enforceCurrentRoute(); });
        });

        if (auto* writingPage = qobject_cast<WritingPage*>(writing_)) {
            connect(writingPage, &WritingPage::referenceActivated, this,
                [this](const QString& kind, const QString&) {
                    if (kind != QStringLiteral("character")) return;
                    QTimer::singleShot(0, this, [this]() {
                        if (nav_) nav_->setCurrentRow(3);
                        if (worldTabs_ && worldTabs_->count() > 0) worldTabs_->setCurrentIndex(0);
                    });
                });
        }

        if (auto* hub = qobject_cast<ProjectHubPage*>(hub_)) {
            const auto goWriting = [this]() {
                QTimer::singleShot(0, this, [this]() {
                    if (nav_) nav_->setCurrentRow(1);
                    enforceCurrentRoute();
                });
            };
            connect(hub, &ProjectHubPage::openProjectRequested, this, [goWriting](const QString&) { goWriting(); });
            connect(hub, &ProjectHubPage::newProjectRequested, this, goWriting);
            connect(hub, &ProjectHubPage::importProjectRequested, this, goWriting);
        }

        QTimer::singleShot(0, this, [this]() { enforceCurrentRoute(); enforceReadableRail(); });
    }

    void normalizeNavigation() {
        if (!nav_) return;
        const QStringList expected{
            QObject::tr("Biblioteca"),
            QObject::tr("Escritura"),
            QObject::tr("Tablero de escenas"),
            QObject::tr("Atlas / documentación"),
            QObject::tr("Mapas"),
            QObject::tr("Revisión"),
            QObject::tr("Configuración / salida")
        };
        if (nav_->count() != expected.size()) {
            nav_->clear();
            nav_->addItems(expected);
        } else {
            for (int i = 0; i < expected.size(); ++i) nav_->item(i)->setText(expected.at(i));
        }
        for (int i = 0; i < nav_->count(); ++i) {
            nav_->item(i)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            nav_->item(i)->setSizeHint(QSize(190, 42));
        }
    }

    void enforceReadableRail() {
        if (!window_ || !rail_) return;
        const int width = window_->width();
        const int railWidth = width < 1040 ? 168 : (width < 1320 ? 188 : 204);
        rail_->setMinimumWidth(railWidth);
        rail_->setMaximumWidth(railWidth);
        if (nav_) {
            for (int i = 0; i < nav_->count(); ++i)
                if (nav_->item(i)) nav_->item(i)->setSizeHint(QSize(railWidth - 20, 42));
        }
    }

    void setHeader(const QString& title, const QString& state) {
        if (!window_) return;
        if (auto* label = window_->findChild<QLabel*>(QStringLiteral("projectTitle"))) label->setText(title);
        if (auto* label = window_->findChild<QLabel*>(QStringLiteral("saveState"))) label->setText(state);
    }

    void enforceCurrentRoute() {
        if (!window_ || !nav_ || !pages_) return;
        normalizeNavigation();
        enforceReadableRail();
        const int row = nav_->currentRow();
        if (auto* save = window_->findChild<QPushButton*>(QStringLiteral("primarySave"))) save->setVisible(row > 0 && row != 6);
        if (auto* focus = window_->findChild<QPushButton*>(QStringLiteral("secondaryAction"))) focus->setVisible(row == 1);

        switch (row) {
            case 0:
                if (hub_ && pages_->currentWidget() != hub_) pages_->setCurrentWidget(hub_);
                setHeader(QObject::tr("Biblioteca"), QObject::tr("Tus historias"));
                break;
            case 1:
                if (writing_ && pages_->currentWidget() != writing_) pages_->setCurrentWidget(writing_);
                if (writingTabs_) { writingTabs_->setCurrentIndex(0); if (writingTabs_->tabBar()) writingTabs_->tabBar()->hide(); }
                setHeader(QObject::tr("Escritura"), QObject::tr("Manuscrito"));
                break;
            case 2:
                if (writing_ && pages_->currentWidget() != writing_) pages_->setCurrentWidget(writing_);
                if (writingTabs_) { writingTabs_->setCurrentIndex(1); if (writingTabs_->tabBar()) writingTabs_->tabBar()->hide(); }
                setHeader(QObject::tr("Tablero de escenas"), QObject::tr("Estructura visual · arrastra para reordenar"));
                break;
            case 3:
                if (world_ && pages_->currentWidget() != world_) pages_->setCurrentWidget(world_);
                if (worldTabs_) {
                    for (int i = 0; i < worldTabs_->count(); ++i) worldTabs_->setTabVisible(i, worldTabs_->widget(i) != mapsTab_);
                    if (worldTabs_->tabBar()) worldTabs_->tabBar()->show();
                    const int atlasIndex = atlasTab_ ? worldTabs_->indexOf(atlasTab_) : -1;
                    worldTabs_->setCurrentIndex(atlasIndex >= 0 ? atlasIndex : qMin(1, worldTabs_->count() - 1));
                }
                setHeader(QObject::tr("Atlas / documentación"), QObject::tr("Personajes, lugares, culturas, relaciones y referencias"));
                break;
            case 4:
                if (world_ && pages_->currentWidget() != world_) pages_->setCurrentWidget(world_);
                if (worldTabs_ && mapsTab_) {
                    const int mapIndex = worldTabs_->indexOf(mapsTab_);
                    for (int i = 0; i < worldTabs_->count(); ++i) worldTabs_->setTabVisible(i, i == mapIndex);
                    if (mapIndex >= 0) worldTabs_->setCurrentIndex(mapIndex);
                    if (worldTabs_->tabBar()) worldTabs_->tabBar()->hide();
                }
                setHeader(QObject::tr("Mapas"), QObject::tr("Pilín Rey"));
                break;
            case 5:
                if (review_ && pages_->currentWidget() != review_) pages_->setCurrentWidget(review_);
                setHeader(QObject::tr("Revisión"), QObject::tr("Consistencia, lenguaje y salida"));
                break;
            case 6:
                if (!settings_) settings_ = window_->findChild<QWidget*>(QStringLiteral("settingsWorkspace"));
                if (settings_ && pages_->currentWidget() != settings_) pages_->setCurrentWidget(settings_);
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
    QPointer<QWidget> mapsTab_;
    QPointer<QWidget> atlasTab_;
    QPointer<QWidget> rail_;
    QPointer<QTabWidget> writingTabs_;
    QPointer<QTabWidget> worldTabs_;
};

} // namespace

void installNavigationConsistencyController(QMainWindow* window) {
    if (!window || window->property("wbwNavigationConsistency").toBool()) return;
    window->setProperty("wbwNavigationConsistency", true);
    new NavigationConsistency(window);
}

} // namespace wbw
