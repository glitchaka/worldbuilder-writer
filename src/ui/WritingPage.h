#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QGraphicsView;
class QLabel;
class QLineEdit;
class QListWidget;
class QTabWidget;
class QTextEdit;
class QToolButton;
class QTreeWidget;

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
    void refreshReferences();
    void updateWordCount();

    ArchiveDocument* document_ = nullptr;
    bool refreshing_ = false;
    QTabWidget* tabs_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    QLineEdit* sceneTitle_ = nullptr;
    QLineEdit* scenePov_ = nullptr;
    QLineEdit* sceneLocation_ = nullptr;
    QLineEdit* sceneLayer_ = nullptr;
    QComboBox* sceneStatus_ = nullptr;
    QTextEdit* editor_ = nullptr;
    QLabel* wordCount_ = nullptr;
    QListWidget* references_ = nullptr;
    QToolButton* bold_ = nullptr;
    QToolButton* italic_ = nullptr;
    QToolButton* underline_ = nullptr;
    QGraphicsView* sceneBoard_ = nullptr;
    QCheckBox* sceneBoardCompact_ = nullptr;
};

} // namespace wbw
