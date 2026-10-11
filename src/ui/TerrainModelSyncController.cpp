#include "ui/TerrainModelSyncController.h"

#include "ui/MapTerrainEngine.h"
#include "ui/PilinReyEditor.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QMainWindow>
#include <QPointer>
#include <QTimer>

#include <algorithm>

namespace wbw {

void PilinReyEditor::applyBakedTerrainMap(const QJsonObject& updated) {
    if (updated.isEmpty()) return;

    // Terrain baking changes the height field and consumes temporary brush objects, but it does
    // not change the layer list, asset library or selection model. Rebuilding every panel here
    // on every mouse move was one of the main sources of UI stalls and memory churn.
    map_ = updated;
    refreshViewport();
    persistToArchive();
    emit mapEdited(map_);
}

void PilinReyEditor::trimUndoHistory(int maximumEntries) {
    maximumEntries = std::clamp(maximumEntries, 4, 24);
    while (undoStack_.size() > maximumEntries) undoStack_.removeFirst();
    while (redoStack_.size() > maximumEntries) redoStack_.removeFirst();
}

namespace {

class TerrainModelSync final : public QObject {
public:
    explicit TerrainModelSync(QMainWindow* window) : QObject(window), window_(window) {
        editor_ = window ? window->findChild<PilinReyEditor*>(QStringLiteral("pilinReyEditor")) : nullptr;
        if (!editor_) return;

        bakeTimer_.setSingleShot(true);
        bakeTimer_.setInterval(90);
        connect(&bakeTimer_, &QTimer::timeout, this, [this]() {
            if (!pending_.isEmpty()) synchronize(pending_);
            pending_ = QJsonObject();
        });

        connect(editor_, &PilinReyEditor::mapEdited, this, [this](const QJsonObject& map) {
            if (busy_ || !editor_) return;
            editor_->trimUndoHistory(10);

            // Coalesce the stream generated while dragging a terrain brush. Only the newest
            // state is baked, instead of rebuilding a 320x180 heightmap dozens of times/sec.
            pending_ = map;
            if (!bakeTimer_.isActive()) bakeTimer_.start();
            else bakeTimer_.start(90);
        });
    }

private:
    static bool containsTerrainBrushObjects(const QJsonArray& layers) {
        for (const QJsonValue& layerValue : layers) {
            const QJsonArray objects = layerValue.toObject().value(QStringLiteral("objects")).toArray();
            for (const QJsonValue& objectValue : objects) {
                const QString type = objectValue.toObject().value(QStringLiteral("type")).toString();
                if (type == QStringLiteral("land") || type == QStringLiteral("sea") || type == QStringLiteral("mountainArea"))
                    return true;
            }
        }
        return false;
    }

    void synchronize(const QJsonObject& incoming) {
        if (busy_ || !editor_) return;

        QJsonObject pilin = incoming.value(QStringLiteral("pilinRey")).toObject();
        QJsonObject terrainJson = pilin.value(QStringLiteral("terrainModel")).toObject();
        if (terrainJson.isEmpty()) return;

        QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();
        if (!containsTerrainBrushObjects(layers)) return;

        MapTerrainEngine terrain = MapTerrainEngine::fromJson(terrainJson);
        const double docW = std::max(1.0, pilin.value(QStringLiteral("width")).toDouble(4096.0));
        const double docH = std::max(1.0, pilin.value(QStringLiteral("height")).toDouble(2304.0));
        bool changed = false;

        for (int li = 0; li < layers.size(); ++li) {
            QJsonObject layer = layers.at(li).toObject();
            QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
            QJsonArray kept;
            bool layerChanged = false;

            for (const QJsonValue& value : objects) {
                const QJsonObject object = value.toObject();
                const QString type = object.value(QStringLiteral("type")).toString();
                if (type != QStringLiteral("land") && type != QStringLiteral("sea") && type != QStringLiteral("mountainArea")) {
                    kept.append(object);
                    continue;
                }

                const QJsonArray points = object.value(QStringLiteral("points")).toArray();
                if (points.isEmpty()) {
                    kept.append(object);
                    continue;
                }

                changed = true;
                layerChanged = true;

                QVector<QPointF> terrainPoints;
                terrainPoints.reserve(points.size());
                double averageRadius = 0.0;
                for (const QJsonValue& pointValue : points) {
                    const QJsonObject point = pointValue.toObject();
                    terrainPoints.append(QPointF(point.value(QStringLiteral("x")).toDouble() / docW * (terrain.width() - 1),
                                                  point.value(QStringLiteral("y")).toDouble() / docH * (terrain.height() - 1)));
                    averageRadius += point.value(QStringLiteral("radius")).toDouble(220.0);
                }
                averageRadius /= std::max(1, points.size());
                const double radiusGrid = std::max(1.5, averageRadius * 0.5 * (terrain.width() / docW + terrain.height() / docH));

                if (type == QStringLiteral("land") || type == QStringLiteral("sea")) {
                    for (const QPointF& p : terrainPoints) {
                        if (type == QStringLiteral("land")) terrain.addHill(p, radiusGrid, 0.18f);
                        else terrain.addPit(p, radiusGrid, 0.22f);
                    }
                } else if (terrainPoints.size() == 1) {
                    terrain.addHill(terrainPoints.first(), radiusGrid * .72, .24f);
                } else {
                    for (int i = 1; i < terrainPoints.size(); ++i)
                        terrain.addRange(terrainPoints.at(i - 1), terrainPoints.at(i), radiusGrid * .55, .16f, .16f);
                }
            }

            if (layerChanged) {
                layer.insert(QStringLiteral("objects"), kept);
                layers.replace(li, layer);
            }
        }

        if (!changed) return;

        // One light post-process per coalesced stroke, rather than per input event.
        terrain.smooth(1);

        QJsonObject newTerrain = terrain.toJson();
        for (const QString& key : {QStringLiteral("seaLevel"), QStringLiteral("generatedAt"), QStringLiteral("generator"),
                                   QStringLiteral("biomes"), QStringLiteral("regions"), QStringLiteral("regionNames")}) {
            if (terrainJson.contains(key)) newTerrain.insert(key, terrainJson.value(key));
        }

        pilin.insert(QStringLiteral("terrainModel"), newTerrain);
        pilin.insert(QStringLiteral("layers"), layers);
        QJsonObject updated = incoming;
        updated.insert(QStringLiteral("pilinRey"), pilin);

        busy_ = true;
        editor_->applyBakedTerrainMap(updated);
        busy_ = false;
    }

    QPointer<QMainWindow> window_;
    QPointer<PilinReyEditor> editor_;
    QTimer bakeTimer_;
    QJsonObject pending_;
    bool busy_ = false;
};

} // namespace

void installTerrainModelSyncController(QMainWindow* window) {
    if (!window || window->property("wbwTerrainModelSyncV4").toBool()) return;
    window->setProperty("wbwTerrainModelSyncV4", true);
    new TerrainModelSync(window);
}

} // namespace wbw
