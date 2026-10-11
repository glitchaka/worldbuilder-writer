#include "ui/MainWindow.h"

#include "ui/ProjectHubPage.h"
#include "ui/ReviewPage.h"
#include "ui/SettingsWorkspace.h"
#include "ui/WorldPage.h"
#include "ui/WritingPage.h"

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>

namespace wbw {

void MainWindow::routeProductModule(int row) {
    if (!pages_ || !navigation_) return;
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
                if (worldTabs->tabBar()) worldTabs->tabBar()->show();
                for (int i = 0; i < worldTabs->count(); ++i) worldTabs->setTabVisible(i, worldTabs->widget(i) != mapsTab);
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

    if (auto* save = findChild<QPushButton*>(QStringLiteral("primarySave"))) save->setVisible(false);
    if (auto* focus = findChild<QPushButton*>(QStringLiteral("secondaryAction"))) focus->setVisible(false);

    // Biblioteca, escritura, tablero y Pilín Rey poseen ya una cabecera propia. Mantener además
    // el encabezado genérico duplicaba jerarquías y alejaba la composición de los mockups.
    const bool useGenericHeader = row == 3 || row == 5 || row == 6;
    if (auto* top = findChild<QWidget*>(QStringLiteral("topShell"))) top->setVisible(useGenericHeader);
}

} // namespace wbw
