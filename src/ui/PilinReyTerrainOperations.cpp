#include "ui/PilinReyEditor.h"
#include "ui/MapTerrainEngine.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>

namespace wbw {
namespace {

QJsonObject mergeTerrainMetadata(const MapTerrainEngine& terrain, const QJsonObject& previous) {
    QJsonObject next = terrain.toJson();
    static const QStringList metadataKeys{
        QStringLiteral("seaLevel"),
        QStringLiteral("generatedAt"),
        QStringLiteral("generator"),
        QStringLiteral("biomes"),
        QStringLiteral("regions"),
        QStringLiteral("regionNames")
    };
    for (const QString& key : metadataKeys) {
        if (previous.contains(key)) next.insert(key, previous.value(key));
    }
    return next;
}

} // namespace

void PilinReyEditor::smoothTerrain() {
    if (map_.isEmpty()) return;
    mutateDocument([&](QJsonObject& pilin) {
        const QJsonObject previous = pilin.value(QStringLiteral("terrainModel")).toObject();
        MapTerrainEngine terrain = MapTerrainEngine::fromJson(previous);
        terrain.smooth(2);
        pilin.insert(QStringLiteral("terrainModel"), mergeTerrainMetadata(terrain, previous));
    });
}

void PilinReyEditor::erodeTerrain() {
    if (map_.isEmpty()) return;
    mutateDocument([&](QJsonObject& pilin) {
        const QJsonObject previous = pilin.value(QStringLiteral("terrainModel")).toObject();
        MapTerrainEngine terrain = MapTerrainEngine::fromJson(previous);
        terrain.erode(2, .18f);
        pilin.insert(QStringLiteral("terrainModel"), mergeTerrainMetadata(terrain, previous));
    });
}

void PilinReyEditor::normalizeTerrain() {
    if (map_.isEmpty()) return;
    mutateDocument([&](QJsonObject& pilin) {
        const QJsonObject previous = pilin.value(QStringLiteral("terrainModel")).toObject();
        MapTerrainEngine terrain = MapTerrainEngine::fromJson(previous);
        terrain.normalize();
        terrain.smooth(1);
        pilin.insert(QStringLiteral("terrainModel"), mergeTerrainMetadata(terrain, previous));
    });
}

void PilinReyEditor::applyBakedTerrainMap(const QJsonObject& map) {
    if (map.isEmpty()) return;
    pushUndo();
    map_ = map;
    ensurePilinDocument();
    selectedObjectIds_.clear();
    refreshLayers();
    refreshAssets();
    refreshSelectionControls();
    refreshViewport();
    persistToArchive();
    emit mapEdited(map_);
}

void PilinReyEditor::flushBakedTerrainMap() {
    persistToArchive();
    emit mapEdited(map_);
}

void PilinReyEditor::trimUndoHistory(int maximumEntries) {
    maximumEntries = std::clamp(maximumEntries, 1, 80);
    while (undoStack_.size() > maximumEntries) undoStack_.removeFirst();
    while (redoStack_.size() > maximumEntries) redoStack_.removeFirst();
}

} // namespace wbw
