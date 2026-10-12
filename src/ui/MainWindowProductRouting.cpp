#include "ui/MainWindow.h"

#include "ui/ProjectHubPage.h"
#include "ui/ReviewPage.h"
#include "ui/SettingsWorkspace.h"
#include "ui/WorldPage.h"
#include "ui/WritingPage.h"

#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTabBar>
#include <QTabWidget>

namespace wbw {
namespace {

class ProductRailDelegate final : public QStyledItemDelegate {
public:
    explicit ProductRailDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return QSize(112, 66); }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const QRect r = option.rect.adjusted(5, 3, -5, -3);
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const bool hover = option.state.testFlag(QStyle::State_MouseOver);
        if (selected || hover) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(selected ? QColor(QStringLiteral("#16283b")) : QColor(QStringLiteral("#111923")));
            painter->drawRoundedRect(r, 8, 8);
        }
        if (selected) {
            painter->setBrush(QColor(QStringLiteral("#4aa5ff")));
            painter->drawRoundedRect(QRectF(r.left(), r.top() + 8, 3, r.height() - 16), 1.5, 1.5);
        }

        const QString label = index.data(Qt::DisplayRole).toString();
        QString glyph = QStringLiteral("•");
        if (label == QObject::tr("Biblioteca")) glyph = QStringLiteral("▥");
        else if (label == QObject::tr("Escritura")) glyph = QStringLiteral("▤");
        else if (label == QObject::tr("Tablero de escenas")) glyph = QStringLiteral("▦");
        else if (label == QObject::tr("Atlas / documentación")) glyph = QStringLiteral("◎");
        else if (label == QObject::tr("Mapas")) glyph = QStringLiteral("◇");
        else if (label == QObject::tr("Revisión")) glyph = QStringLiteral("▥");
        else if (label == QObject::tr("Configuración / salida")) glyph = QStringLiteral("⚙");

        QFont iconFont = option.font;
        iconFont.setPointSizeF(15.5);
        iconFont.setWeight(QFont::Medium);
        painter->setFont(iconFont);
        painter->setPen(selected ? QColor(QStringLiteral("#79c1ff")) : QColor(QStringLiteral("#9ca9b6")));
        painter->drawText(QRect(r.left(), r.top() + 7, r.width(), 24), Qt::AlignCenter, glyph);

        QFont labelFont = option.font;
        labelFont.setPointSizeF(7.8);
        labelFont.setWeight(selected ? QFont::DemiBold : QFont::Normal);
        painter->setFont(labelFont);
        painter->setPen(selected ? QColor(QStringLiteral("#cfe8ff")) : QColor(QStringLiteral("#929eaa")));
        QString compact = label;
        if (label == QObject::tr("Tablero de escenas")) compact = QObject::tr("Escenas");
        else if (label == QObject::tr("Atlas / documentación")) compact = QObject::tr("Atlas");
        else if (label == QObject::tr("Configuración / salida")) compact = QObject::tr("Configuración");
        painter->drawText(QRect(r.left() + 2, r.top() + 34, r.width() - 4, 22), Qt::AlignHCenter | Qt::AlignTop, compact);
        painter->restore();
    }
};

void installProductRail(QListWidget* navigation) {
    if (!navigation || navigation->property("wbwProductRailV1").toBool()) return;
    navigation->setProperty("wbwProductRailV1", true);
    navigation->setItemDelegate(new ProductRailDelegate(navigation));
    navigation->setMouseTracking(true);
    navigation->setSpacing(1);
    navigation->setStyleSheet(QStringLiteral(
        "QListWidget#sideNavigation{background:transparent;border:0;outline:0;}"
        "QListWidget#sideNavigation::item{background:transparent;border:0;padding:0;margin:0;}"));
}

} // namespace

void MainWindow::routeProductModule(int row) {
    if (!pages_ || !navigation_) return;
    installProductRail(navigation_);

    auto* writingTabs = writingPage_ ? writingPage_->findChild<QTabWidget*>(QStringLiteral("writingTabs")) : nullptr;
    auto* worldTabs = worldPage_ ? worldPage_->findChild<QTabWidget*>(QStringLiteral("worldTabs")) : nullptr;
    QWidget* mapsTab = worldPage_ ? worldPage_->findChild<QWidget*>(QStringLiteral("mapsTab")) : nullptr;
    QWidget* atlasTab = worldPage_ ? worldPage_->findChild<QWidget*>(QStringLiteral("atlasTab")) : nullptr;

    if (writingTabs && writingTabs->tabBar()) writingTabs->tabBar()->hide();

    switch (row) {
        case 0:
            pages_->setCurrentWidget(hubPage_);
            if (hubPage_) hubPage_->refresh();
            projectTitle_->setText(tr("Biblioteca"));
            saveState_->setText(tr("Tus historias"));
            break;
        case 1:
            pages_->setCurrentWidget(writingPage_);
            if (writingTabs) writingTabs->setCurrentIndex(0);
            projectTitle_->setText(tr("Escritura"));
            saveState_->setText(tr("Manuscrito"));
            break;
        case 2:
            pages_->setCurrentWidget(writingPage_);
            if (writingTabs && writingTabs->count() > 1) writingTabs->setCurrentIndex(1);
            projectTitle_->setText(tr("Tablero de escenas"));
            saveState_->setText(tr("Planificación visual"));
            break;
        case 3:
            pages_->setCurrentWidget(worldPage_);
            if (worldTabs) {
                if (atlasTab && worldTabs->indexOf(atlasTab) >= 0) worldTabs->setCurrentWidget(atlasTab);
                for (int i = 0; i < worldTabs->count(); ++i) worldTabs->setTabVisible(i, worldTabs->widget(i) != mapsTab);
                if (worldTabs->tabBar()) worldTabs->tabBar()->show();
            }
            projectTitle_->setText(tr("Atlas / documentación"));
            saveState_->setText(tr("Mundo, personajes y referencias"));
            break;
        case 4:
            pages_->setCurrentWidget(worldPage_);
            if (worldTabs && mapsTab) {
                const int mapIndex = worldTabs->indexOf(mapsTab);
                for (int i = 0; i < worldTabs->count(); ++i) worldTabs->setTabVisible(i, i == mapIndex);
                if (mapIndex >= 0) worldTabs->setCurrentIndex(mapIndex);
                if (worldTabs->tabBar()) worldTabs->tabBar()->hide();
            }
            projectTitle_->setText(tr("Mapas"));
            saveState_->setText(tr("Pilín Rey"));
            break;
        case 5:
            pages_->setCurrentWidget(reviewPage_);
            projectTitle_->setText(tr("Revisión"));
            saveState_->setText(tr("Diagnóstico y salida editorial"));
            break;
        case 6:
            if (settingsPage_) pages_->setCurrentWidget(settingsPage_);
            else if (auto* settings = findChild<SettingsWorkspace*>()) pages_->setCurrentWidget(settings);
            projectTitle_->setText(tr("Configuración / salida"));
            saveState_->setText(tr("Preferencias, copias y exportación"));
            break;
        default:
            return;
    }

    if (auto* save = findChild<QPushButton*>(QStringLiteral("primarySave"))) save->hide();
    if (auto* focus = findChild<QPushButton*>(QStringLiteral("secondaryAction"))) focus->hide();
    if (auto* top = findChild<QWidget*>(QStringLiteral("topShell"))) top->hide();
}

} // namespace wbw
