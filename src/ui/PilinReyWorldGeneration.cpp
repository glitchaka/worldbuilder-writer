#include "ui/PilinReyEditor.h"
#include "ui/MapTerrainEngine.h"

#include <QDateTime>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QUuid>

namespace wbw {
namespace {
QString generatedId(const QString& prefix) {
    return prefix + QLatin1Char('-') + QUuid::createUuid().toString(QUuid::Id128);
}

QJsonArray keepManualObjects(const QJsonArray& objects) {
    QJsonArray kept;
    for (const QJsonValue& value : objects) {
        const QJsonObject object = value.toObject();
        if (!object.value(QStringLiteral("generated")).toBool(false)) kept.append(object);
    }
    return kept;
}
}

void PilinReyEditor::generateProceduralWorld() {
    if (map_.isEmpty()) return;

    bool ok = false;
    const int seed = QInputDialog::getInt(this, tr("Generar mundo"), tr("Semilla"),
                                         static_cast<int>(QDateTime::currentMSecsSinceEpoch() & 0x7fffffff),
                                         1, 2147483647, 1, &ok);
    if (!ok) return;
    const int continents = QInputDialog::getInt(this, tr("Generar mundo"), tr("Continentes principales"), 3, 1, 8, 1, &ok);
    if (!ok) return;
    const int islands = QInputDialog::getInt(this, tr("Generar mundo"), tr("Islas secundarias"), 12, 0, 40, 1, &ok);
    if (!ok) return;

    if (QMessageBox::question(this, tr("Generar mundo"),
        tr("Se reemplazará el terreno procedural y los ríos generados, pero se conservarán ciudades, assets, etiquetas y objetos creados manualmente. ¿Continuar?")) != QMessageBox::Yes)
        return;

    pushUndo();

    QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
    const double docW = pilin.value(QStringLiteral("width")).toDouble(4096.0);
    const double docH = pilin.value(QStringLiteral("height")).toDouble(2304.0);

    // A 320x180 field is detailed enough for complex coasts while remaining cheap to edit,
    // serialize and rebuild in the cached renderer.
    MapTerrainEngine terrain(320, 180, static_cast<quint32>(seed));
    terrain.generateWorld(continents, islands, 0.62f);
    terrain.erode(2, 0.18f);
    QJsonObject terrainJson = terrain.toJson();
    terrainJson.insert(QStringLiteral("seaLevel"), 0.42);
    terrainJson.insert(QStringLiteral("generatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    terrainJson.insert(QStringLiteral("generator"), QStringLiteral("native-heightmap-v2"));
    pilin.insert(QStringLiteral("terrainModel"), terrainJson);

    const QJsonArray riverNetwork = terrain.traceRivers(qBound(8, continents * 8 + islands / 3, 34), 0.42f);
    QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();
    int hydroIndex = -1;
    for (int i = 0; i < layers.size(); ++i) {
        const QString name = layers.at(i).toObject().value(QStringLiteral("name")).toString();
        if (name.contains(tr("Hidrografía"), Qt::CaseInsensitive) || name.contains(tr("Ríos"), Qt::CaseInsensitive)) {
            hydroIndex = i;
            break;
        }
    }
    if (hydroIndex < 0) {
        layers.append(QJsonObject{{QStringLiteral("id"), generatedId(QStringLiteral("layer"))},
                                  {QStringLiteral("name"), tr("Hidrografía")},
                                  {QStringLiteral("visible"), true},
                                  {QStringLiteral("locked"), false},
                                  {QStringLiteral("opacity"), 1.0},
                                  {QStringLiteral("objects"), QJsonArray()}});
        hydroIndex = layers.size() - 1;
    }

    QJsonObject hydro = layers.at(hydroIndex).toObject();
    QJsonArray hydroObjects = keepManualObjects(hydro.value(QStringLiteral("objects")).toArray());
    for (const QJsonValue& riverValue : riverNetwork) {
        QJsonArray points;
        for (const QJsonValue& pointValue : riverValue.toArray()) {
            const QJsonObject point = pointValue.toObject();
            points.append(QJsonObject{{QStringLiteral("x"), point.value(QStringLiteral("x")).toDouble() * docW},
                                      {QStringLiteral("y"), point.value(QStringLiteral("y")).toDouble() * docH}});
        }
        if (points.size() < 4) continue;
        hydroObjects.append(QJsonObject{{QStringLiteral("id"), generatedId(QStringLiteral("river"))},
                                        {QStringLiteral("type"), QStringLiteral("river")},
                                        {QStringLiteral("points"), points},
                                        {QStringLiteral("width"), 4.0},
                                        {QStringLiteral("generated"), true}});
    }
    hydro.insert(QStringLiteral("objects"), hydroObjects);
    layers.replace(hydroIndex, hydro);

    pilin.insert(QStringLiteral("layers"), layers);
    map_.insert(QStringLiteral("pilinRey"), pilin);
    selectedObjectIds_.clear();
    redoStack_.clear();
    refreshLayers();
    refreshSelectionControls();
    refreshViewport();
    persistToArchive();
    emit mapEdited(map_);
}

} // namespace wbw
