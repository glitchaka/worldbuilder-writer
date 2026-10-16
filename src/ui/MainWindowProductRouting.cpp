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
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTextCursor>
#include <QTextEdit>

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
    navigation->setMinimumWidth(510);
    navigation->setMaximumWidth(690);
    const QStringList labels{
        QObject::tr("Biblioteca"), QObject::tr("Escritura"), QObject::tr("Tablero"),
        QObject::tr("Atlas"), QObject::tr("Mapas"), QObject::tr("Revisión"), QObject::tr("Configuración")
    };
    for (int i = 0; i < qMin(navigation->count(), labels.size()); ++i) {
        navigation->item(i)->setText(labels.at(i));
        navigation->item(i)->setToolTip(i == 2 ? QObject::tr("Tablero de escenas")
                                   : i == 3 ? QObject::tr("Atlas y documentación")
                                            : labels.at(i));
        navigation->item(i)->setSizeHint(QSize(i == 0 || i == 6 ? 92 : 78, 50));
    }
}

} // namespace

void MainWindow::initializeProductSurface() {
    if (!navigation_) return;
    finalizeNavigationLabels(navigation_);
    prepareWritingSurface(writingPage_);
    if (planningPage_) planningPage_->refreshProductTheme();
    if (worldPage_) worldPage_->installKnowledgeWorkspace();

    if (auto* globalSearch = findChild<QLineEdit*>(QStringLiteral("globalSearch"))) {
        globalSearch->setMinimumWidth(150);
        globalSearch->setMaximumWidth(310);
    }

    const auto refreshBrand = [this]() {
        auto* brand = findChild<QLabel*>(QStringLiteral("appBrand"));
        if (!brand) return;
        const QString title = document_.storyTitle().trimmed().isEmpty() ? document_.title().trimmed() : document_.storyTitle().trimmed();
        if (navigation_ && navigation_->currentRow() == 0) {
            brand->setText(QStringLiteral("Worldbuilder Writer"));
            brand->setToolTip(QString());
            brand->setMinimumWidth(142);
            brand->setMaximumWidth(190);
            return;
        }
        const QString compact = title.size() > 24 ? title.left(23).trimmed() + QStringLiteral("…") : title;
        brand->setText(compact.isEmpty() ? QStringLiteral("Worldbuilder Writer") : compact);
        brand->setToolTip(title);
        brand->setMinimumWidth(122);
        brand->setMaximumWidth(190);
    };

    if (!property("wbwProductBrandBridge").toBool()) {
        setProperty("wbwProductBrandBridge", true);
        if (planningPage_) connect(planningPage_, &PlanningPage::changed, this, refreshBrand);
        if (writingPage_) connect(writingPage_, &WritingPage::changed, this, refreshBrand);
        if (worldPage_) connect(worldPage_, &WorldPage::changed, this, refreshBrand);
        if (reviewPage_) connect(reviewPage_, &ReviewPage::changed, this, refreshBrand);
    }

    if (writingPage_ && worldPage_ && !property("wbwSemanticReferenceBridgeV2").toBool()) {
        setProperty("wbwSemanticReferenceBridgeV2", true);
        disconnect(writingPage_, &WritingPage::referenceActivated, this, &MainWindow::handleReference);
        connect(writingPage_, &WritingPage::referenceActivated, this,
                [this](const QString& kind, const QString& id) {
            if (id.isEmpty() || !worldPage_ || !navigation_) return;

            if (navigation_->currentRow() != 3) navigation_->setCurrentRow(3);
            else routeProductModule(3);

            auto* tabs = worldPage_->findChild<QTabWidget*>(QStringLiteral("worldTabs"));
            if (!tabs) return;

            if (kind == QStringLiteral("character")) {
                const int planningIndex = tabs->indexOf(planningPage_);
                if (planningIndex >= 0) tabs->setCurrentIndex(planningIndex);
                if (planningPage_) planningPage_->openCharacter(id);
                return;
            }

            // openReference selects the actual record. The product shell then
            // resolves the visible tab by widget identity so inserting the
            // planning page can never shift semantic references to a wrong tab.
            worldPage_->openReference(kind, id);
            QWidget* target = nullptr;
            if (kind == QStringLiteral("world")) target = worldPage_->findChild<QWidget*>(QStringLiteral("atlasTab"));
            else if (kind == QStringLiteral("magic")) target = worldPage_->findChild<QWidget*>(QStringLiteral("magicTab"));
            else if (kind == QStringLiteral("worldText") || kind == QStringLiteral("magicText")) {
                target = worldPage_->findChild<QWidget*>(QStringLiteral("textsTab"));
                if (!target) target = worldPage_->findChild<QWidget*>(QStringLiteral("referenceTextsTab"));
            }
            if (target) {
                const int index = tabs->indexOf(target);
                if (index >= 0) tabs->setCurrentIndex(index);
            }
        });
    }

    if (!property("wbwContextSearchV2").toBool()) {
        if (auto* globalSearch = findChild<QLineEdit*>(QStringLiteral("globalSearch"))) {
            setProperty("wbwContextSearchV2", true);
            QObject::disconnect(globalSearch, &QLineEdit::returnPressed, this, nullptr);
            connect(globalSearch, &QLineEdit::returnPressed, this, [this, globalSearch]() {
                const QString query = globalSearch->text().trimmed();
                if (query.isEmpty() || !navigation_) return;

                switch (navigation_->currentRow()) {
                    case 0: {
                        if (auto* librarySearch = hubPage_ ? hubPage_->findChild<QLineEdit*>(QStringLiteral("hubSearch")) : nullptr) {
                            librarySearch->setText(query);
                            librarySearch->setFocus();
                        }
                        break;
                    }
                    case 1: {
                        if (auto* editor = writingPage_ ? writingPage_->findChild<QTextEdit*>(QStringLiteral("sceneEditor")) : nullptr) {
                            QTextCursor cursor = editor->textCursor();
                            cursor.clearSelection();
                            editor->setTextCursor(cursor);
                            if (!editor->find(query)) {
                                cursor.movePosition(QTextCursor::Start);
                                editor->setTextCursor(cursor);
                                editor->find(query);
                            }
                            editor->setFocus();
                        }
                        break;
                    }
                    case 2: {
                        if (auto* sceneSearch = writingPage_ ? writingPage_->findChild<QLineEdit*>(QStringLiteral("sceneBoardSearch")) : nullptr) {
                            sceneSearch->setText(query);
                            sceneSearch->setFocus();
                        }
                        break;
                    }
                    case 3: {
                        if (auto* atlasSearch = worldPage_ ? worldPage_->findChild<QLineEdit*>(QStringLiteral("atlasSearch")) : nullptr) {
                            atlasSearch->setText(query);
                            atlasSearch->setFocus();
                        }
                        break;
                    }
                    default:
                        break;
                }
            });
        }
    }

    refreshBrand();
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
            reviewPage_->installProfileCompatibilityGuard();
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

    if (auto* brand = findChild<QLabel*>(QStringLiteral("appBrand"))) {
        const QString title = document_.storyTitle().trimmed().isEmpty() ? document_.title().trimmed() : document_.storyTitle().trimmed();
        if (row == 0) {
            brand->setText(QStringLiteral("Worldbuilder Writer"));
            brand->setToolTip(QString());
        } else {
            const QString compact = title.size() > 24 ? title.left(23).trimmed() + QStringLiteral("…") : title;
            brand->setText(compact.isEmpty() ? QStringLiteral("Worldbuilder Writer") : compact);
            brand->setToolTip(title);
        }
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
