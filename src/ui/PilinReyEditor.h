#pragma once

#include "core/ArchiveDocument.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QStringList>
#include <QWidget>

#include <functional>

class QCheckBox;
class QFrame;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QResizeEvent;
class QShowEvent;
class QSlider;
class QToolButton;

namespace wbw {

class PilinReyViewport;

class PilinReyEditor final : public QWidget {
    Q_OBJECT
public:
    enum class Tool {
        Select,
        Pan,
        Coast,
        Eraser,
        Region,
        River,
        Road,
        Border,
        Forest,
        Mountain,
        Settlement,
        Stamp,
        Label,
        Measure
    };

    explicit PilinReyEditor(QWidget* parent = nullptr);
    ~PilinReyEditor() override;

    void setMap(const QJsonObject& map);
    QJsonObject map() const { return map_; }
    void nudgeSelection(double dx, double dy);
    void transformSelectedPathGeometry(double scaleFactor, double rotationDegrees);

signals:
    void markerMoved(const QString& id, double x, double y);
    void addMarkerRequested(double x, double y);
    void markerActivated(const QString& id);
    void mapEdited(const QJsonObject& map);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void buildUi();
    void layoutFloatingPanels();
    void ensurePilinDocument();
    void refreshLayers();
    void refreshLayerControls();
    void refreshAssets();
    void refreshSelectionControls();
    void refreshViewport();
    void chooseTemplate();
    void clearTemplate();
    void rotateTemplate(double delta);
    void scaleTemplate(double factor);
    void addLayer();
    void duplicateLayer();
    void removeLayer();
    void moveLayer(int delta);
    void setLayerLocked(bool locked);
    void setLayerOpacity(int value);
    void applyLayerItem(QListWidgetItem* item);
    void setActiveTool(Tool tool);
    void addPathObject(const QString& type, const QJsonArray& points);
    void addSettlement(double x, double y);
    void addStamp(double x, double y);
    void addLabel(double x, double y);
    void selectObjects(const QStringList& ids);
    void movePointObject(const QString& id, double x, double y);
    void movePathPoint(const QString& id, int pointIndex, double x, double y);
    void editSelectedObject();
    void duplicateSelectedObject();
    void deleteSelectedObject();
    void copySelectedObject();
    void pasteCopiedObject();
    void linkSelectedToAtlas();
    void transformSelection(double scaleFactor, double rotationDegrees);
    void importAsset();
    void setThemeColor(const QString& key);
    QJsonObject selectedObject() const;
    QJsonArray selectedObjects() const;
    void mutateDocument(const std::function<void(QJsonObject&)>& mutation);
    void persistToArchive();
    void pushUndo();
    void undo();
    void redo();
    void exportMap();
    QString activeLayerId() const;

    QJsonObject map_;
    QJsonArray clipboardObjects_;
    QList<QJsonObject> undoStack_;
    QList<QJsonObject> redoStack_;
    bool refreshing_ = false;
    QStringList selectedObjectIds_;
    QString selectedAssetKind_ = QStringLiteral("castle");
    QString selectedAssetData_;
    int brushRadius_ = 220;
    int density_ = 58;
    int strokeWidth_ = 5;
    int symbolSize_ = 64;

    QWidget* canvasHost_ = nullptr;
    PilinReyViewport* viewport_ = nullptr;
    QWidget* toolRail_ = nullptr;
    QFrame* layersPopover_ = nullptr;
    QFrame* assetsPopover_ = nullptr;
    QFrame* templatePopover_ = nullptr;
    QFrame* appearancePopover_ = nullptr;
    QFrame* selectionPopover_ = nullptr;
    QFrame* topCommands_ = nullptr;

    QListWidget* layers_ = nullptr;
    QListWidget* assets_ = nullptr;
    QSlider* templateOpacity_ = nullptr;
    QSlider* templateScale_ = nullptr;
    QSlider* templateRotation_ = nullptr;
    QSlider* layerOpacity_ = nullptr;
    QSlider* brushRadiusSlider_ = nullptr;
    QSlider* densitySlider_ = nullptr;
    QSlider* strokeWidthSlider_ = nullptr;
    QSlider* symbolSizeSlider_ = nullptr;
    QCheckBox* layerLocked_ = nullptr;
    QCheckBox* snapCheck_ = nullptr;
    QCheckBox* gridCheck_ = nullptr;
    QLabel* status_ = nullptr;
    QLabel* selectionLabel_ = nullptr;
    QLabel* toolOptionsLabel_ = nullptr;
    QToolButton* undoButton_ = nullptr;
    QToolButton* redoButton_ = nullptr;
    QToolButton* editObjectButton_ = nullptr;
    QToolButton* duplicateObjectButton_ = nullptr;
    QToolButton* linkAtlasButton_ = nullptr;
    QToolButton* deleteObjectButton_ = nullptr;
    QToolButton* rotateLeftButton_ = nullptr;
    QToolButton* rotateRightButton_ = nullptr;
    QToolButton* scaleDownButton_ = nullptr;
    QToolButton* scaleUpButton_ = nullptr;
    QToolButton* layersButton_ = nullptr;
    QToolButton* assetsButton_ = nullptr;
    QToolButton* templateButton_ = nullptr;
    QToolButton* appearanceButton_ = nullptr;
    QToolButton* stampToolButton_ = nullptr;
    QList<QToolButton*> toolButtons_;
};

} // namespace wbw
