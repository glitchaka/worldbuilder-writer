#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QWidget>

#include <functional>

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QShowEvent;
class QSlider;
class QToolButton;

namespace wbw {

class PilinReyViewport;

class PilinReyEditor : public QWidget {
    Q_OBJECT
public:
    enum class Tool {
        Select,
        Pan,
        Coast,
        River,
        Road,
        Border,
        Forest,
        Mountain,
        Settlement,
        Label
    };

    explicit PilinReyEditor(QWidget* parent = nullptr);
    ~PilinReyEditor() override;

    void setMap(const QJsonObject& map);
    QJsonObject map() const { return map_; }

signals:
    void markerMoved(const QString& id, double x, double y);
    void addMarkerRequested(double x, double y);
    void markerActivated(const QString& id);
    void mapEdited(const QJsonObject& map);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void buildUi();
    void applyEditorChrome();
    void ensureExportAction();
    void ensurePilinDocument();
    void refreshLayers();
    void refreshLayerControls();
    void refreshSelectionControls();
    void refreshViewport();
    void chooseTemplate();
    void clearTemplate();
    void addLayer();
    void duplicateLayer();
    void removeLayer();
    void moveLayer(int delta);
    void setLayerLocked(bool locked);
    void setLayerOpacity(int value);
    void setActiveTool(Tool tool);
    void addPathObject(const QString& type, const QJsonArray& points);
    void addSettlement(double x, double y);
    void addLabel(double x, double y);
    void selectObject(const QString& id);
    void movePointObject(const QString& id, double x, double y);
    void editSelectedObject();
    void deleteSelectedObject();
    void linkSelectedToAtlas();
    QJsonObject selectedObject() const;
    void mutateDocument(const std::function<void(QJsonObject&)>& mutation);
    void persistToArchive();
    void pushUndo();
    void undo();
    void redo();
    QString activeLayerId() const;

    QJsonObject map_;
    QList<QJsonObject> undoStack_;
    QList<QJsonObject> redoStack_;
    bool refreshing_ = false;
    bool chromeApplied_ = false;
    QString selectedObjectId_;

    PilinReyViewport* viewport_ = nullptr;
    QListWidget* layers_ = nullptr;
    QComboBox* style_ = nullptr;
    QSlider* templateOpacity_ = nullptr;
    QSlider* layerOpacity_ = nullptr;
    QCheckBox* layerLocked_ = nullptr;
    QLabel* status_ = nullptr;
    QPushButton* undoButton_ = nullptr;
    QPushButton* redoButton_ = nullptr;
    QPushButton* editObjectButton_ = nullptr;
    QPushButton* linkAtlasButton_ = nullptr;
    QPushButton* deleteObjectButton_ = nullptr;
    QPushButton* exportButton_ = nullptr;
    QList<QToolButton*> toolButtons_;
};

} // namespace wbw
