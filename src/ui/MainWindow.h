#pragma once

#include "core/ArchiveDocument.h"

#include <QMainWindow>

class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QStackedWidget;
class QTreeWidget;
class QTextEdit;

namespace wbw {

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    QWidget* buildPlanningPage();
    QWidget* buildWritingPage();
    QWidget* buildWorldPage();
    QWidget* buildReviewPage();

    void createMenus();
    void createShell();
    void newProject();
    void openProject();
    bool saveProject();
    bool saveProjectAs();
    bool confirmDiscard();
    void setDocument(ArchiveDocument document);
    void refreshAll();
    void refreshPlanning();
    void refreshWriting();
    void refreshWorld();
    void refreshReview();
    void updateWindowTitle();
    void setSaveLabel();

    void selectCharacter(int row);
    void applyCharacterEdits();
    void addCharacter();
    void removeCharacter();

    void selectScene();
    void applySceneEdits();
    void addChapter();
    void addScene();
    void removeWritingItem();
    void openFocusMode();

    void selectWorldRecord(int row);
    void applyWorldEdits();
    void addWorldRecord();
    void removeWorldRecord();

    void selectMagicRecord(int row);
    void applyMagicEdits();
    void addMagicRecord();
    void removeMagicRecord();

    void exportManuscriptPdf();

    ArchiveDocument document_;
    bool refreshing_ = false;

    QListWidget* navigation_ = nullptr;
    QStackedWidget* pages_ = nullptr;
    QLabel* projectLabel_ = nullptr;
    QLabel* saveLabel_ = nullptr;

    QListWidget* characterList_ = nullptr;
    QLineEdit* characterName_ = nullptr;
    QLineEdit* characterRole_ = nullptr;
    QLineEdit* characterOrigin_ = nullptr;
    QTextEdit* characterSummary_ = nullptr;

    QTreeWidget* manuscriptTree_ = nullptr;
    QLineEdit* sceneTitle_ = nullptr;
    QLineEdit* scenePov_ = nullptr;
    QLineEdit* sceneLocation_ = nullptr;
    QLineEdit* sceneStatus_ = nullptr;
    QTextEdit* sceneEditor_ = nullptr;

    QListWidget* worldList_ = nullptr;
    QLineEdit* worldName_ = nullptr;
    QLineEdit* worldKind_ = nullptr;
    QTextEdit* worldSummary_ = nullptr;
    QTextEdit* worldNotes_ = nullptr;

    QListWidget* magicList_ = nullptr;
    QLineEdit* magicName_ = nullptr;
    QLineEdit* magicCategory_ = nullptr;
    QTextEdit* magicPrinciple_ = nullptr;
    QTextEdit* magicLimits_ = nullptr;

    QLabel* reviewStats_ = nullptr;
};

} // namespace wbw
