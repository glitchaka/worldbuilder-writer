#pragma once

#include "ui/SemanticTextEdit.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileInfo>
#include <QGraphicsView>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QShortcut>
#include <QSplitter>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace wbw {

class ArchiveDocument;

class WritingPage final : public QWidget {
    Q_OBJECT
public:
    explicit WritingPage(QWidget* parent = nullptr);

    void setDocument(ArchiveDocument* document);
    void refresh();
    void openFocusMode();

signals:
    void changed();
    void referenceActivated(const QString& kind, const QString& id);

private:
    QWidget* buildEditorTab();
    QWidget* buildSceneBoardTab();
    void refreshTree();
    void refreshSceneBoard();
    void selectItem();
    void applyScene();
    void addChapter();
    void addScene();
    void removeItem();
    void moveItem(int delta);
    void renameChapter();
    void importManuscript();
    void updateFormattingState();
    void applyCharacterFormat(int property, bool enabled);
    void updateWordCount();
    void loadSceneContent(const QJsonObject& scene);
    QJsonArray serializeFormatting() const;
    void applyFormatting(const QJsonArray& formatting);
    void closeFocusMode();

    ArchiveDocument* document_ = nullptr;
    bool refreshing_ = false;
    bool focusActive_ = false;
    bool indexWasVisible_ = true;
    bool detailsWereVisible_ = false;
    bool tabBarWasVisible_ = true;
    bool topShellWasVisible_ = true;
    Qt::WindowStates previousWindowState_ = Qt::WindowNoState;

    QTabWidget* tabs_ = nullptr;
    QWidget* editorTab_ = nullptr;
    QVBoxLayout* editorOuterLayout_ = nullptr;
    QWidget* heroPanel_ = nullptr;
    QWidget* commandBarPanel_ = nullptr;
    QWidget* focusBar_ = nullptr;
    QLabel* focusSceneName_ = nullptr;
    QShortcut* focusEscape_ = nullptr;

    QSplitter* editorSplit_ = nullptr;
    QWidget* indexPanel_ = nullptr;
    QWidget* metadataPanel_ = nullptr;
    QToolButton* toggleIndex_ = nullptr;
    QToolButton* toggleDetails_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    QLineEdit* sceneTitle_ = nullptr;
    QLineEdit* scenePov_ = nullptr;
    QLineEdit* sceneLocation_ = nullptr;
    QLineEdit* sceneLayer_ = nullptr;
    QComboBox* sceneStatus_ = nullptr;
    SemanticTextEdit* editor_ = nullptr;
    QLabel* wordCount_ = nullptr;
    QLabel* proofState_ = nullptr;
    QToolButton* bold_ = nullptr;
    QToolButton* italic_ = nullptr;
    QToolButton* underline_ = nullptr;
    QGraphicsView* sceneBoard_ = nullptr;
    QCheckBox* sceneBoardCompact_ = nullptr;
};

} // namespace wbw
