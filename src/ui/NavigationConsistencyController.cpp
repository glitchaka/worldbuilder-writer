#include "ui/NavigationConsistencyController.h"

#include "ui/ProjectHubPage.h"
#include "ui/WritingPage.h"

#include <QListWidget>
#include <QMainWindow>
#include <QPointer>
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
        settings_ = window_->findChild<QWidget*>(QStringLiteral("settingsWorkspace"));
        if (!nav_ || !pages_) return;

        connect(pages_, &QStackedWidget::currentChanged, this, [this](int) {
            QTimer::singleShot(0, this, [this]() { enforceCurrentRoute(); });
        });

        if (auto* writingPage = qobject_cast<WritingPage*>(writing_)) {
            connect(writingPage, &WritingPage::referenceActivated, this,
                [this](const QString& kind, const QString&) {
                    if (kind != QStringLiteral("character")) return;
                    QTimer::singleShot(0, this, [this]() {
                        if (nav_) nav_->setCurrentRow(3);
                        if (worldTabs_) worldTabs_->setCurrentIndex(0);
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

        QTimer::singleShot(0, this, [this]() { enforceCurrentRoute(); });
    }

    void enforceCurrentRoute() {
        if (!window_ || !nav_ || !pages_) return;
        const int row = nav_->currentRow();
        switch (row) {
            case 0:
                if (hub_ && pages_->currentWidget() != hub_) pages_->setCurrentWidget(hub_);
                break;
            case 1:
                if (writing_ && pages_->currentWidget() != writing_) pages_->setCurrentWidget(writing_);
                if (writingTabs_) writingTabs_->setCurrentIndex(0);
                break;
            case 2:
                if (writing_ && pages_->currentWidget() != writing_) pages_->setCurrentWidget(writing_);
                if (writingTabs_) writingTabs_->setCurrentIndex(1);
                break;
            case 3:
                if (world_ && pages_->currentWidget() != world_) pages_->setCurrentWidget(world_);
                if (worldTabs_) {
                    for (int i = 0; i < worldTabs_->count(); ++i) worldTabs_->setTabVisible(i, worldTabs_->widget(i) != mapsTab_);
                    if (worldTabs_->currentWidget() == mapsTab_ || worldTabs_->currentIndex() < 0) worldTabs_->setCurrentIndex(0);
                    if (worldTabs_->tabBar()) worldTabs_->tabBar()->show();
                }
                break;
            case 4:
                if (world_ && pages_->currentWidget() != world_) pages_->setCurrentWidget(world_);
                if (worldTabs_ && mapsTab_) {
                    const int mapIndex = worldTabs_->indexOf(mapsTab_);
                    for (int i = 0; i < worldTabs_->count(); ++i) worldTabs_->setTabVisible(i, i == mapIndex);
                    if (mapIndex >= 0) worldTabs_->setCurrentIndex(mapIndex);
                    if (worldTabs_->tabBar()) worldTabs_->tabBar()->hide();
                }
                break;
            case 5:
                if (review_ && pages_->currentWidget() != review_) pages_->setCurrentWidget(review_);
                break;
            case 6:
                if (!settings_) settings_ = window_->findChild<QWidget*>(QStringLiteral("settingsWorkspace"));
                if (settings_ && pages_->currentWidget() != settings_) pages_->setCurrentWidget(settings_);
                break;
            default:
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
