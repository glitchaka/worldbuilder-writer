#pragma once

#include <QJsonObject>
#include <QWidget>

class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;

namespace wbw {

class ArchiveDocument;
class PilinReyEditor;

class MapWorkspace final : public QWidget {
    Q_OBJECT
public:
    explicit MapWorkspace(QWidget* parent = nullptr);

    void setDocument(ArchiveDocument* document);
    void refresh();
    void applyTheme();

signals:
    void changed();

private:
    void rebuildPicker(int preferredRow = -1);
    void selectMap(int row);
    void createMap();
    void duplicateMap();
    void removeMap();
    void renameMap();
    void editMetadata();
    void applyEditorMap(const QJsonObject& editedMap);
    void updateEmptyState();

    ArchiveDocument* document_ = nullptr;
    bool refreshing_ = false;
    int currentRow_ = -1;

    QFrame* chrome_ = nullptr;
    QComboBox* picker_ = nullptr;
    QLineEdit* name_ = nullptr;
    QLabel* emptyTitle_ = nullptr;
    QLabel* emptyCopy_ = nullptr;
    QPushButton* emptyCreate_ = nullptr;
    PilinReyEditor* editor_ = nullptr;
};

} // namespace wbw
