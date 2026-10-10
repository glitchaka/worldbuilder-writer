#pragma once

#include "core/ArchiveDocument.h"

#include <QKeySequence>
#include <QLineEdit>
#include <QMainWindow>
#include <QStatusBar>

class QCloseEvent;
class QLabel;
class QListWidget;
class QStackedWidget;
class QTimer;

namespace wbw {

class PlanningPage;
class ProjectHubPage;
class ReviewPage;
class WorldPage;
class WritingPage;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    QTimer* autosaveTimer() const { return autosaveTimer_; }

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void createShell();
    void loadStartupProject();
    void showLibrary();
    void newProject();
    void openLibrary();
    void openProject();
    void importWbw();
    bool loadPath(const QString& path);
    bool saveProject(bool quiet = false);
    bool saveProjectAs();
    void exportWbw();
    void exportPdf();
    void createBackup();
    bool confirmDiscard();
    void setDocument(ArchiveDocument document);
    void refreshPages();
    void onDocumentChanged();
    void updateWindowTitle();
    void handleReference(const QString& kind, const QString& id);

    ArchiveDocument document_;
    QListWidget* navigation_ = nullptr;
    QStackedWidget* pages_ = nullptr;
    QLabel* projectTitle_ = nullptr;
    QLabel* saveState_ = nullptr;
    ProjectHubPage* hubPage_ = nullptr;
    PlanningPage* planningPage_ = nullptr;
    WritingPage* writingPage_ = nullptr;
    WorldPage* worldPage_ = nullptr;
    ReviewPage* reviewPage_ = nullptr;
    QTimer* autosaveTimer_ = nullptr;
};

} // namespace wbw
