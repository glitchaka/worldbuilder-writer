#include "ui/MainWindow.h"

#include "ui/MapWorkspace.h"
#include "ui/PlanningPage.h"
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
    writing->activatePlanningFilters();
    writing->activateSceneNotes();
    if (auto* tabs = writing->findChild<QTabWidget*>(QStringLiteral("writingTabs"))) {
        if (tabs->tabBar()) tabs->tabBar()->hide();
    }
}

void finalizeNavigationLabels(QListWidget* navigation) {
    if (!navigation) return;
    const QStringList labels{
        QObject::tr("Biblioteca"), QObject::tr("Escritura"), QObject::tr("Tablero"),
        QObject::tr("Atlas"), QObject::tr("Mapas"), QObject::tr("Revisión"), QObject::tr("Configuración")
    };
    for (int i = 0; i < qMin(navigation->count(), labels.size()); ++i) {
        navigation->item(i)->setText(labels.at(i));
        navigation->item(i)->setToolTip(i == 2 ? QObject::tr("Tablero de escenas")
                                   : i == 3 ? QObject::tr("Atlas y documentación")
                                            : labels.at(i));
        navigation->item(i)->setSizeHint(QSize(i == 6 ? 118 : 94, 50));
    }
}

} // namespace

void MainWindow::initializeProductSurface() {
    if (!navigation_) return;
    finalizeNavigationLabels(navigation_);
    prepareWritingSurface(writingPage_);
    if (planningPage_) planningPage_->refreshProductTheme();
    if (worldPage_) worldPage_->installKnowledgeWorkspace();

    if (writingPage_ && planningPage_ && !property("wbwCharacterReferenceBridge").toBool()) {
        setProperty("wbwCharacterReferenceBridge", true);
        connect(writingPage_, &WritingPage::referenceActivated, this,
                [this](const QString& kind, const QString& id) {
            if (kind != QStringLiteral("character") || id.isEmpty() || !planningPage_) return;
            planningPage_->openCharacter(id);
        });
    }

    routeProductModule(qMax(0, navigation_->currentRow()));
}

void MainWindow::routeProductModule(int row) {
    if (!pages_ || !navigation_) return;

    finalizeNavigationLabels(navigation_);
    prepareWritingSurface(writingPage_);
    auto* writingTabs = writingPage_ ? writingPage_->findChild<QTabWidget*>(QStringLiteral("writingTabs")) : nullptr;

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
            projectTitle_->setText(tr("Tablero de escenas"));
            saveState_->setText(tr("Orden visual del manuscrito"));
            break;
        case 3:
            pages_->setCurrentWidget(worldPage_);
            if (planningPage_) planningPage_->refreshProductTheme();
            if (worldPage_) {
                worldPage_->installKnowledgeWorkspace();
                worldPage_->configureAtlasProductSurface();
            }
            projectTitle_->setText(tr("Atlas y documentación"));
            saveState_->setText(tr("Mundo y material de referencia"));
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
            reviewPage_->installWorkspaceNavigation();
            reviewPage_->installManuscriptExportActions();
            reviewPage_->refreshProductTheme();
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
    refreshProductTheme();
    writingPage_->refreshProductChromeTheme();
    if (planningPage_) planningPage_->refreshProductTheme();
    if (worldPage_ && row == 3) worldPage_->installKnowledgeWorkspace();
    if (reviewPage_) reviewPage_->refreshProductTheme();
    if (row == 4 && mapPage_) mapPage_->applyTheme();
    navigation_->viewport()->update();
}

} // namespace wbw
