#include "ui/MainWindow.h"

#include "ui/MapWorkspace.h"
#include "ui/ProjectHubPage.h"
#include "ui/ReviewPage.h"
#include "ui/SettingsWorkspace.h"
#include "ui/ThemeManager.h"
#include "ui/WorldPage.h"
#include "ui/WritingPage.h"

#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>

namespace wbw {
namespace {

void polishLibraryControls(ProjectHubPage* hub) {
    if (!hub || hub->property("wbwLibraryControlsV6").toBool()) return;
    hub->setProperty("wbwLibraryControlsV6", true);
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

void prepareWritingSurface(WritingPage* writing) {
    if (!writing) return;
    writing->applyProductChrome();
    if (auto* tabs = writing->findChild<QTabWidget*>(QStringLiteral("writingTabs"))) {
        if (tabs->tabBar()) tabs->tabBar()->hide();
    }
}

} // namespace

void MainWindow::routeProductModule(int row) {
    if (!pages_ || !navigation_) return;

    prepareWritingSurface(writingPage_);
    auto* writingTabs = writingPage_ ? writingPage_->findChild<QTabWidget*>(QStringLiteral("writingTabs")) : nullptr;
    auto* worldTabs = worldPage_ ? worldPage_->findChild<QTabWidget*>(QStringLiteral("worldTabs")) : nullptr;
    QWidget* mapsTab = worldPage_ ? worldPage_->findChild<QWidget*>(QStringLiteral("mapsTab")) : nullptr;
    QWidget* atlasTab = worldPage_ ? worldPage_->findChild<QWidget*>(QStringLiteral("atlasTab")) : nullptr;

    switch (row) {
        case 0:
            pages_->setCurrentWidget(hubPage_);
            if (hubPage_) {
                polishLibraryControls(hubPage_);
                hubPage_->refresh();
            }
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
            projectTitle_->setText(tr("Planificación"));
            saveState_->setText(tr("Tablero de escenas"));
            break;
        case 3:
            pages_->setCurrentWidget(worldPage_);
            if (worldTabs) {
                if (atlasTab && worldTabs->indexOf(atlasTab) >= 0) worldTabs->setCurrentWidget(atlasTab);
                for (int i = 0; i < worldTabs->count(); ++i) worldTabs->setTabVisible(i, worldTabs->widget(i) != mapsTab);
                if (worldTabs->tabBar()) worldTabs->tabBar()->show();
            }
            projectTitle_->setText(tr("Mundo"));
            saveState_->setText(tr("Atlas y documentación"));
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
            saveState_->setText(tr("Diagnóstico editorial"));
            break;
        case 6:
            if (settingsPage_) pages_->setCurrentWidget(settingsPage_);
            projectTitle_->setText(tr("Configuración"));
            saveState_->setText(tr("Preferencias y salida"));
            break;
        default:
            return;
    }

    ThemeManager::applySaved();
    if (row == 4 && mapPage_) mapPage_->applyTheme();
    navigation_->viewport()->update();
}

} // namespace wbw
