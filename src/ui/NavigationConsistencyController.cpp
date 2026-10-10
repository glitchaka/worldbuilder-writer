#include "ui/NavigationConsistencyController.h"

#include "ui/ProjectHubPage.h"
#include "ui/WritingPage.h"

#include <QEvent>
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QPointer>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
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
            QTimer::singleShot(0, this, [this]() { enforceReadableRail(); enforceCurrentRoute(); });
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
            QTimer::singleShot(80, this, [this]() { enforceCurrentRoute(); });
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
        nav_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        nav_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        nav_->setMinimumHeight(expected.size() * 44 + 12);
        nav_->setMaximumHeight(expected.size() * 44 + 12);
        const int itemWidth = window_ && window_->width() < 1120 ? 150 : 196;
        for (int i = 0; i < nav_->count(); ++i) {
            nav_->item(i)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            nav_->item(i)->setSizeHint(QSize(itemWidth, 42));
        }
    }

    void enforceReadableRail() {
        if (!window_ || !rail_) return;
        const int width = window_->width();
        const int railWidth = width < 1120 ? 174 : (width < 1360 ? 194 : 220);
        const int itemWidth = railWidth - 24;
        rail_->setMinimumWidth(railWidth);
        rail_->setMaximumWidth(railWidth);
        if (QWidget* identity = window_->findChild<QWidget*>(QStringLiteral("appIdentity")))
            identity->setVisible(width >= 960);
        if (nav_) {
            nav_->setMinimumHeight(nav_->count() * 44 + 12);
            nav_->setMaximumHeight(nav_->count() * 44 + 12);
            for (int i = 0; i < nav_->count(); ++i)
                if (nav_->item(i)) nav_->item(i)->setSizeHint(QSize(itemWidth, 42));
        }
    }

    void setHeader(const QString& title, const QString& state) {
        if (!window_) return;
        if (auto* label = window_->findChild<QLabel*>(QStringLiteral("projectTitle"))) label->setText(title);
        if (auto* label = window_->findChild<QLabel*>(QStringLiteral("saveState"))) label->setText(state);
    }

    void enforceWritingColumns() {
        if (!window_) return;
        QWidget* index = window_->findChild<QWidget*>(QStringLiteral("indexPanel"));
        QWidget* metadata = window_->findChild<QWidget*>(QStringLiteral("metadataPanel"));
        QWidget* commandBar = window_->findChild<QWidget*>(QStringLiteral("writingCommandBar"));
        if (!index || !metadata) return;
        QSplitter* split = qobject_cast<QSplitter*>(index->parentWidget());
        if (!split || split->count() < 3) return;
        const bool showContext = window_->width() >= 900;
        metadata->setVisible(showContext);
        metadata->setMinimumWidth(showContext ? 210 : 0);
        metadata->setMaximumWidth(showContext ? 280 : 0);
        index->setMinimumWidth(window_->width() < 1120 ? 175 : 210);
        index->setMaximumWidth(window_->width() < 1120 ? 205 : 270);
        split->setStretchFactor(0, 0);
        split->setStretchFactor(1, 1);
        split->setStretchFactor(2, 0);
        split->setSizes(showContext ? QList<int>{190, 560, 235} : QList<int>{190, 800, 0});
        if (commandBar) {
            for (QToolButton* button : commandBar->findChildren<QToolButton*>()) {
                if (button->text() != QObject::tr("Detalles")) continue;
                button->setChecked(showContext);
                break;
            }
        }
        metadata->raise();
    }

    void enforceMapPanels() {
        if (!window_) return;
        QWidget* editor = window_->findChild<QWidget*>(QStringLiteral("pilinReyEditor"));
        QWidget* canvas = editor ? editor->findChild<QWidget*>(QStringLiteral("pilinCanvasHost")) : nullptr;
        if (!editor || !canvas || canvas->width() < 300) return;

        QFrame* assets = nullptr;
        QFrame* layers = nullptr;
        const auto frames = editor->findChildren<QFrame*>(QStringLiteral("pilinPopover"));
        for (QFrame* frame : frames) {
            QString title;
            for (QLabel* label : frame->findChildren<QLabel*>()) {
                const QString text = label->text().trimmed();
                if (text == QObject::tr("Assets") || text == QObject::tr("Assets cartográficos") || text == QObject::tr("Assets y sellos")) { title = QStringLiteral("assets"); break; }
                if (text == QObject::tr("Capas")) { title = QStringLiteral("layers"); break; }
            }
            if (title == QStringLiteral("assets")) assets = frame;
            else if (title == QStringLiteral("layers")) layers = frame;
        }

        const int margin = 12;
        const int panelWidth = canvas->width() < 1000 ? 220 : 268;
        int y = 60;
        if (layers && canvas->width() >= 780) {
            layers->setMinimumWidth(panelWidth);
            layers->setMaximumWidth(panelWidth + 20);
            layers->adjustSize();
            layers->show();
            layers->move(qMax(margin, canvas->width() - layers->width() - margin), y);
            layers->raise();
            y = layers->geometry().bottom() + 8;
        }
        if (assets && canvas->width() >= 780) {
            assets->setMinimumWidth(panelWidth);
            assets->setMaximumWidth(panelWidth + 20);
            assets->adjustSize();
            assets->show();
            assets->move(qMax(margin, canvas->width() - assets->width() - margin), y);
            assets->raise();
        }
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
                QTimer::singleShot(0, this, [this]() { enforceWritingColumns(); });
                QTimer::singleShot(80, this, [this]() { enforceWritingColumns(); });
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
                QTimer::singleShot(0, this, [this]() { enforceMapPanels(); });
                QTimer::singleShot(80, this, [this]() { enforceMapPanels(); });
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
