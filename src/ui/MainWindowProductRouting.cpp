#include "ui/MainWindow.h"

#include "ui/MapWorkspace.h"
#include "ui/ProjectHubPage.h"
#include "ui/ReviewPage.h"
#include "ui/SettingsWorkspace.h"
#include "ui/ThemeManager.h"
#include "ui/WorldPage.h"
#include "ui/WritingPage.h"

#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
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
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return QSize(80, 62); }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        const QRect r = option.rect.adjusted(4, 3, -4, -3);
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const bool hover = option.state.testFlag(QStyle::State_MouseOver);
        const bool dark = qApp && qApp->property("wbwDarkMode").toBool();

        const QColor hoverFill = dark ? QColor(QStringLiteral("#111923")) : QColor(QStringLiteral("#e7e1d8"));
        const QColor selectedFill = dark ? QColor(QStringLiteral("#16283b")) : QColor(QStringLiteral("#d8e4ec"));
        const QColor selectionBar = dark ? QColor(QStringLiteral("#4aa5ff")) : QColor(QStringLiteral("#457aa3"));
        const QColor icon = dark ? QColor(QStringLiteral("#9ca9b6")) : QColor(QStringLiteral("#746e65"));
        const QColor selectedIcon = dark ? QColor(QStringLiteral("#79c1ff")) : QColor(QStringLiteral("#315f78"));
        const QColor text = dark ? QColor(QStringLiteral("#929eaa")) : QColor(QStringLiteral("#6d675f"));
        const QColor selectedText = dark ? QColor(QStringLiteral("#cfe8ff")) : QColor(QStringLiteral("#315f78"));

        if (selected || hover) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(selected ? selectedFill : hoverFill);
            painter->drawRoundedRect(r, 8, 8);
        }
        if (selected) {
            painter->setBrush(selectionBar);
            painter->drawRoundedRect(QRectF(r.left(), r.top() + 7, 3, r.height() - 14), 1.5, 1.5);
        }

        const QString value = index.data(Qt::DisplayRole).toString();
        QString glyph = QStringLiteral("•");
        QString compact = value;
        if (value == QObject::tr("Biblioteca")) glyph = QStringLiteral("▥");
        else if (value == QObject::tr("Escritura")) glyph = QStringLiteral("▤");
        else if (value == QObject::tr("Tablero de escenas")) { glyph = QStringLiteral("▦"); compact = QObject::tr("Escenas"); }
        else if (value == QObject::tr("Atlas / documentación")) { glyph = QStringLiteral("◎"); compact = QObject::tr("Atlas"); }
        else if (value == QObject::tr("Mapas")) glyph = QStringLiteral("◇");
        else if (value == QObject::tr("Revisión")) glyph = QStringLiteral("✓");
        else if (value == QObject::tr("Configuración / salida")) { glyph = QStringLiteral("⚙"); compact = QObject::tr("Ajustes"); }

        QFont iconFont = option.font;
        iconFont.setPointSizeF(15.0);
        iconFont.setWeight(QFont::Medium);
        painter->setFont(iconFont);
        painter->setPen(selected ? selectedIcon : icon);
        painter->drawText(QRect(r.left(), r.top() + 6, r.width(), 23), Qt::AlignCenter, glyph);

        QFont textFont = option.font;
        textFont.setPointSizeF(7.2);
        textFont.setWeight(selected ? QFont::DemiBold : QFont::Normal);
        painter->setFont(textFont);
        painter->setPen(selected ? selectedText : text);
        painter->drawText(QRect(r.left() + 2, r.top() + 33, r.width() - 4, 20), Qt::AlignHCenter | Qt::AlignTop, compact);
        painter->restore();
    }
};

void installProductRail(QListWidget* navigation) {
    if (!navigation || navigation->property("wbwProductRailV5").toBool()) return;
    navigation->setProperty("wbwProductRailV5", true);

    // The original shell attached a QWidget to every QListWidget item, which meant the
    // polished delegate never painted at all. Remove those old widgets instead of layering
    // yet another visual patch over them.
    for (int i = 0; i < navigation->count(); ++i) {
        if (QListWidgetItem* item = navigation->item(i)) navigation->removeItemWidget(item);
    }

    navigation->setItemDelegate(new ProductRailDelegate(navigation));
    navigation->setMouseTracking(true);
    navigation->setSpacing(1);
    if (QWidget* rail = navigation->parentWidget()) rail->setFixedWidth(90);
    navigation->setStyleSheet(QStringLiteral(
        "QListWidget#sideNavigation{background:transparent;border:0;outline:0;}"
        "QListWidget#sideNavigation::item{background:transparent;border:0;padding:0;margin:0;}"));
}

void polishLibraryControls(ProjectHubPage* hub) {
    if (!hub || hub->property("wbwLibraryControlsV5").toBool()) return;
    hub->setProperty("wbwLibraryControlsV5", true);
    if (auto* search = hub->findChild<QLineEdit*>(QStringLiteral("hubSearch"))) {
        search->show();
        search->setMaximumWidth(520);
    }
    for (QPushButton* button : hub->findChildren<QPushButton*>(QStringLiteral("hubViewButton"))) button->hide();
    if (auto* sort = hub->findChild<QComboBox*>(QStringLiteral("hubSort"))) sort->hide();
    if (auto* genre = hub->findChild<QComboBox*>(QStringLiteral("hubGenreFilter"))) genre->hide();
    for (QPushButton* button : hub->findChildren<QPushButton*>(QStringLiteral("newProjectOption"))) {
        if (button->text().contains(QObject::tr("Desde plantilla"), Qt::CaseInsensitive)) button->hide();
    }
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
            if (hubPage_) { polishLibraryControls(hubPage_); hubPage_->refresh(); }
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
            if (!mapPage_) {
                mapPage_ = new MapWorkspace(pages_);
                pages_->addWidget(mapPage_);
                connect(mapPage_, &MapWorkspace::changed, this, &MainWindow::onDocumentChanged);
            }
            mapPage_->setDocument(&document_);
            pages_->setCurrentWidget(mapPage_);
            projectTitle_->setText(tr("Mapas"));
            saveState_->setText(tr("Cartografía"));
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

    ThemeManager::applySaved();
    if (navigation_) navigation_->viewport()->update();
}

} // namespace wbw
