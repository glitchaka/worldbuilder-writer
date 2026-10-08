#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QWidget>

#include <functional>

class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
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
        River,
        Road,
        Border,
        Forest,
        Mountain,
        Settlement
    };

    explicit PilinReyEditor(QWidget* parent = nullptr);
    ~PilinReyEditor() override;

    void setMap(const QJsonObject& map);
    QJsonObject map() const { return map_; }

signals:
    // Compatibility signals kept only so old WorldPage connections remain source-compatible.
    void markerMoved(const QString& id, double x, double y);
    void addMarkerRequested(double x, double y);
    void markerActivated(const QString& id);
    void mapEdited(const QJsonObject& map);

private:
    void buildUi();
    void ensurePilinDocument();
    void refreshLayers();
    void refreshViewport();
    void chooseTemplate();
    void clearTemplate();
    void addLayer();
    void removeLayer();
    void moveLayer(int delta);
    void setActiveTool(Tool tool);
    void addPathObject(const QString& type, const QJsonArray& points);
    void addSettlement(double x, double y);
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

    PilinReyViewport* viewport_ = nullptr;
    QListWidget* layers_ = nullptr;
    QComboBox* style_ = nullptr;
    QSlider* templateOpacity_ = nullptr;
    QLabel* status_ = nullptr;
    QPushButton* undoButton_ = nullptr;
    QPushButton* redoButton_ = nullptr;
    QList<QToolButton*> toolButtons_;
};

} // namespace wbw
