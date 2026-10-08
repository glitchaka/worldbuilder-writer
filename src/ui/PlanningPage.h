#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QSpinBox;
class QTabWidget;
class QTextEdit;

namespace wbw {

class ArchiveDocument;
class RelationshipBoard;

class PlanningPage final : public QWidget {
    Q_OBJECT
public:
    explicit PlanningPage(QWidget* parent = nullptr);

    void setDocument(ArchiveDocument* document);
    void refresh();

signals:
    void changed();

private:
    QWidget* buildCharactersTab();
    QWidget* buildBoardTab();
    QWidget* buildTheoriesTab();
    QWidget* buildTimelineTab();

    void refreshCharacters();
    void selectCharacter(int row);
    void applyCharacter();
    void addCharacter();
    void removeCharacter();
    void chooseCharacterImage();

    void refreshRelationships();
    void selectRelationship(int row);
    void applyRelationship();
    void addRelationship();
    void removeRelationship();
    void moveCharacter(const QString& id, double x, double y);

    void refreshTheories();
    void selectTheory(int row);
    void applyTheory();
    void addTheory();
    void removeTheory();

    void refreshTimeline();
    void selectTimeline(int row);
    void applyTimeline();
    void addTimeline();
    void removeTimeline();

    ArchiveDocument* document_ = nullptr;
    bool refreshing_ = false;
    QTabWidget* tabs_ = nullptr;

    QListWidget* characterList_ = nullptr;
    QLabel* characterImage_ = nullptr;
    QLineEdit* characterName_ = nullptr;
    QLineEdit* characterAliases_ = nullptr;
    QComboBox* characterCategory_ = nullptr;
    QComboBox* characterStatus_ = nullptr;
    QLineEdit* characterRole_ = nullptr;
    QLineEdit* characterOccupation_ = nullptr;
    QLineEdit* characterOrigin_ = nullptr;
    QLineEdit* characterAffiliation_ = nullptr;
    QTextEdit* characterSummary_ = nullptr;
    QTextEdit* characterBackground_ = nullptr;
    QTextEdit* characterPhysical_ = nullptr;
    QLineEdit* characterTraits_ = nullptr;
    QTextEdit* characterEvidence_ = nullptr;
    QLineEdit* characterPresence_ = nullptr;
    QLineEdit* characterColor_ = nullptr;
    QCheckBox* characterBoardVisible_ = nullptr;

    RelationshipBoard* board_ = nullptr;
    QListWidget* relationshipList_ = nullptr;
    QComboBox* relationshipSource_ = nullptr;
    QComboBox* relationshipTarget_ = nullptr;
    QComboBox* relationshipType_ = nullptr;
    QLineEdit* relationshipLabel_ = nullptr;
    QComboBox* relationshipCertainty_ = nullptr;
    QSpinBox* relationshipStrength_ = nullptr;
    QTextEdit* relationshipDetails_ = nullptr;

    QListWidget* theoryList_ = nullptr;
    QLineEdit* theoryTitle_ = nullptr;
    QComboBox* theoryStatus_ = nullptr;
    QSpinBox* theoryConfidence_ = nullptr;
    QTextEdit* theoryThesis_ = nullptr;
    QTextEdit* theoryEvidence_ = nullptr;
    QTextEdit* theoryCounterpoint_ = nullptr;
    QLineEdit* theoryCharacters_ = nullptr;
    QLineEdit* theoryTags_ = nullptr;

    QListWidget* timelineList_ = nullptr;
    QLineEdit* timelineChapter_ = nullptr;
    QLineEdit* timelineWhen_ = nullptr;
    QLineEdit* timelineTitle_ = nullptr;
    QTextEdit* timelineSummary_ = nullptr;
    QLineEdit* timelineCharacters_ = nullptr;
    QSpinBox* timelineIntensity_ = nullptr;
};

} // namespace wbw
