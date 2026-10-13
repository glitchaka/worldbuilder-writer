#include "ui/PilinReyEditor.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QUuid>

namespace wbw {
namespace {

QString migrationId(const QString& prefix) {
    return prefix + QLatin1Char('-') + QUuid::createUuid().toString(QUuid::Id128);
}

int settlementLayerIndex(QJsonArray& layers) {
    for (int i = 0; i < layers.size(); ++i) {
        const QJsonObject layer = layers.at(i).toObject();
        if (layer.value(QStringLiteral("name")).toString().compare(QObject::tr("Asentamientos"), Qt::CaseInsensitive) == 0) return i;
    }
    layers.append(QJsonObject{
        {QStringLiteral("id"), migrationId(QStringLiteral("layer"))},
        {QStringLiteral("name"), QObject::tr("Asentamientos")},
        {QStringLiteral("visible"), true},
        {QStringLiteral("locked"), false},
        {QStringLiteral("opacity"), 1.0},
        {QStringLiteral("objects"), QJsonArray()}
    });
    return layers.size() - 1;
}

} // namespace

void PilinReyEditor::migrateLegacyMarkers() {
    const QJsonArray markers = map_.value(QStringLiteral("markers")).toArray();
    if (markers.isEmpty()) return;

    QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
    QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();
    const int layerIndex = settlementLayerIndex(layers);
    QJsonObject layer = layers.at(layerIndex).toObject();
    QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();

    QSet<QString> migratedIds;
    for (const QJsonValue& value : objects) {
        const QJsonObject object = value.toObject();
        const QString legacy = object.value(QStringLiteral("legacyMarkerId")).toString();
        if (!legacy.isEmpty()) migratedIds.insert(legacy);
    }

    bool changed = false;
    for (const QJsonValue& value : markers) {
        const QJsonObject marker = value.toObject();
        const QString legacyId = marker.value(QStringLiteral("id")).toString();
        if (!legacyId.isEmpty() && migratedIds.contains(legacyId)) continue;

        const QString label = marker.value(QStringLiteral("label")).toString(tr("Lugar"));
        const QString kind = marker.value(QStringLiteral("kind")).toString(tr("Lugar"));
        QJsonObject object{
            {QStringLiteral("id"), legacyId.isEmpty() ? migrationId(QStringLiteral("settlement")) : legacyId},
            {QStringLiteral("type"), QStringLiteral("settlement")},
            {QStringLiteral("label"), label},
            {QStringLiteral("settlementType"), kind},
            {QStringLiteral("x"), marker.value(QStringLiteral("x")).toDouble()},
            {QStringLiteral("y"), marker.value(QStringLiteral("y")).toDouble()},
            {QStringLiteral("scale"), 1.0},
            {QStringLiteral("migratedFromLegacyMarker"), true}
        };
        if (!legacyId.isEmpty()) object.insert(QStringLiteral("legacyMarkerId"), legacyId);
        objects.append(object);
        if (!legacyId.isEmpty()) migratedIds.insert(legacyId);
        changed = true;
    }

    if (!changed) return;
    layer.insert(QStringLiteral("objects"), objects);
    layers.replace(layerIndex, layer);
    pilin.insert(QStringLiteral("layers"), layers);
    pilin.insert(QStringLiteral("legacyMarkersImported"), true);
    map_.insert(QStringLiteral("pilinRey"), pilin);

    refreshLayers();
    refreshSelectionControls();
    refreshViewport();
    emit mapEdited(map_);
}

} // namespace wbw
