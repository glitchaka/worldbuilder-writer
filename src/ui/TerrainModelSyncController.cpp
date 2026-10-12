#include "ui/TerrainModelSyncController.h"

#include "ui/MapTerrainEngine.h"
#include "ui/PilinReyEditor.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QMainWindow>
#include <QPointer>
#include <QTimer>

#include <algorithm>
#include <functional>

namespace wbw {
namespace {

QJsonObject terrainOperationResult(const QJsonObject& source, const std::function<void(MapTerrainEngine&)>& operation) {
    QJsonObject pilin = source.value(QStringLiteral("pilinRey")).toObject();
    const QJsonObject terrainJson = pilin.value(QStringLiteral("terrainModel")).toObject();
    if (terrainJson.isEmpty()) return {};

    MapTerrainEngine terrain = MapTerrainEngine::fromJson(terrainJson);
    operation(terrain);
    QJsonObject updatedTerrain = terrain.toJson();
    for (const QString& key : {QStringLiteral("seaLevel"), QStringLiteral("generatedAt"), QStringLiteral("generator"),
                               QStringLiteral("biomes"), QStringLiteral("regions"), QStringLiteral("regionNames")}) {
        if (terrainJson.contains(key)) updatedTerrain.insert(key, terrainJson.value(key));
    }
    pilin.insert(QStringLiteral("terrainModel"), updatedTerrain);
    QJsonObject result = source;
    result.insert(QStringLiteral("pilinRey"), pilin);
    return result;
}

bool isBakedTerrainType(const QString& type) {
    return type == QStringLiteral("land") || type == QStringLiteral("sea") || type == QStringLiteral("mountainArea");
}

} // namespace

void PilinReyEditor::applyBakedTerrainMap(const QJsonObject& updated) {
    if (updated.isEmpty()) return;
    map_ = updated;
    refreshViewport();
    emit mapEdited(map_);
}

void PilinReyEditor::flushBakedTerrainMap() {
    persistToArchive();
}

void PilinReyEditor::trimUndoHistory(int maximumEntries) {
    maximumEntries = std::clamp(maximumEntries, 3, 16);
    while (undoStack_.size() > maximumEntries) undoStack_.removeFirst();
    while (redoStack_.size() > maximumEntries) redoStack_.removeFirst();
}

void PilinReyEditor::smoothTerrain() {
    const QJsonObject updated = terrainOperationResult(map_, [](MapTerrainEngine& terrain) { terrain.smooth(2); });
    if (updated.isEmpty()) return;
    pushUndo();
    applyBakedTerrainMap(updated);
    persistToArchive();
}

void PilinReyEditor::erodeTerrain() {
    const QJsonObject updated = terrainOperationResult(map_, [](MapTerrainEngine& terrain) { terrain.erode(2, .18f); });
    if (updated.isEmpty()) return;
    pushUndo();
    applyBakedTerrainMap(updated);
    persistToArchive();
}

void PilinReyEditor::normalizeTerrain() {
    const QJsonObject updated = terrainOperationResult(map_, [](MapTerrainEngine& terrain) { terrain.normalize(); });
    if (updated.isEmpty()) return;
    pushUndo();
    applyBakedTerrainMap(updated);
    persistToArchive();
}

namespace {

class TerrainModelSync final : public QObject {
public:
    explicit TerrainModelSync(QMainWindow* window) : QObject(window), window_(window) {
        editor_ = window ? window->findChild<PilinReyEditor*>(QStringLiteral("pilinReyEditor")) : nullptr;
        if (!editor_) return;

        bakeTimer_.setSingleShot(true);
        bakeTimer_.setInterval(190);
        persistTimer_.setSingleShot(true);
        persistTimer_.setInterval(1400);

        connect(&bakeTimer_, &QTimer::timeout, this, [this]() {
            const QJsonObject current = pending_;
            pending_ = QJsonObject();
            if (editor_ && editor_->isVisible() && !current.isEmpty()) synchronize(current);
        });
        connect(&persistTimer_, &QTimer::timeout, this, [this]() {
            if (editor_) editor_->flushBakedTerrainMap();
        });
        connect(editor_, &PilinReyEditor::mapEdited, this, [this](const QJsonObject& map) {
            if (busy_ || !editor_) return;
            editor_->trimUndoHistory(6);
            if (!editor_->isVisible()) {
                pending_ = QJsonObject();
                bakeTimer_.stop();
                return;
            }
            pending_ = map;
            bakeTimer_.start();
        });
    }

private:
    static bool hasBakedBrushes(const QJsonArray& layers) {
        for (const QJsonValue& layerValue : layers) {
            for (const QJsonValue& objectValue : layerValue.toObject().value(QStringLiteral("objects")).toArray()) {
                if (isBakedTerrainType(objectValue.toObject().value(QStringLiteral("type")).toString())) return true;
            }
        }
        return false;
    }

    void synchronize(const QJsonObject& incoming) {
        if (busy_ || !editor_ || !editor_->isVisible()) return;

        QJsonObject pilin = incoming.value(QStringLiteral("pilinRey")).toObject();
        const QJsonObject originalTerrain = pilin.value(QStringLiteral("terrainModel")).toObject();
        if (originalTerrain.isEmpty()) return;

        QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();
        if (!hasBakedBrushes(layers)) return;

        MapTerrainEngine terrain = MapTerrainEngine::fromJson(originalTerrain);
        const double docW = std::max(1.0, pilin.value(QStringLiteral("width")).toDouble(4096.0));
        const double docH = std::max(1.0, pilin.value(QStringLiteral("height")).toDouble(2304.0));
        bool changed = false;

        for (qsizetype layerIndex = 0; layerIndex < layers.size(); ++layerIndex) {
            QJsonObject layer = layers.at(layerIndex).toObject();
            const QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
            QJsonArray kept;
            bool layerChanged = false;

            for (const QJsonValue& value : objects) {
                const QJsonObject object = value.toObject();
                const QString type = object.value(QStringLiteral("type")).toString();
                if (!isBakedTerrainType(type)) {
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
                terrainPoints.reserve(static_cast<int>(points.size()));
                double averageRadius = 0.0;
                for (const QJsonValue& pointValue : points) {
                    const QJsonObject point = pointValue.toObject();
                    terrainPoints.append(QPointF(point.value(QStringLiteral("x")).toDouble() / docW * (terrain.width() - 1),
                                                  point.value(QStringLiteral("y")).toDouble() / docH * (terrain.height() - 1)));
                    averageRadius += point.value(QStringLiteral("radius")).toDouble(220.0);
                }
                averageRadius /= static_cast<double>(std::max<qsizetype>(1, points.size()));
                const double radiusGrid = std::max(1.5, averageRadius * .5 * (terrain.width() / docW + terrain.height() / docH));

                if (type == QStringLiteral("land")) {
                    for (const QPointF& point : terrainPoints) terrain.addHill(point, radiusGrid, .18f);
                } else if (type == QStringLiteral("sea")) {
                    for (const QPointF& point : terrainPoints) terrain.addPit(point, radiusGrid, .22f);
                } else if (terrainPoints.size() == 1) {
                    terrain.addHill(terrainPoints.first(), radiusGrid * .72, .24f);
                } else {
                    for (qsizetype i = 1; i < terrainPoints.size(); ++i)
                        terrain.addRange(terrainPoints.at(i - 1), terrainPoints.at(i), radiusGrid * .55, .16f, .16f);
                }
            }

            if (layerChanged) {
                layer.insert(QStringLiteral("objects"), kept);
                layers.replace(layerIndex, layer);
            }
        }

        if (!changed) return;
        terrain.smooth(1);

        QJsonObject newTerrain = terrain.toJson();
        for (const QString& key : {QStringLiteral("seaLevel"), QStringLiteral("generatedAt"), QStringLiteral("generator"),
                                   QStringLiteral("biomes"), QStringLiteral("regions"), QStringLiteral("regionNames")}) {
            if (originalTerrain.contains(key)) newTerrain.insert(key, originalTerrain.value(key));
        }

        pilin.insert(QStringLiteral("terrainModel"), newTerrain);
        pilin.insert(QStringLiteral("layers"), layers);
        QJsonObject updated = incoming;
        updated.insert(QStringLiteral("pilinRey"), pilin);

        busy_ = true;
        editor_->applyBakedTerrainMap(updated);
        busy_ = false;
        persistTimer_.start();
    }

    QPointer<QMainWindow> window_;
    QPointer<PilinReyEditor> editor_;
    QTimer bakeTimer_;
    QTimer persistTimer_;
    QJsonObject pending_;
    bool busy_ = false;
};

} // namespace

void installTerrainModelSyncController(QMainWindow* window) {
    if (!window || window->property("wbwTerrainModelSyncV8").toBool()) return;
    window->setProperty("wbwTerrainModelSyncV8", true);
    new TerrainModelSync(window);
}

} // namespace wbw
