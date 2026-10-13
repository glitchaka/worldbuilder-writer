#include "ui/PilinReyEditor.h"

#include "ui/MapTerrainEngine.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>

#include <algorithm>
#include <cmath>

namespace wbw {
namespace {

QString derivedId(const QString& prefix) {
    return prefix + QLatin1Char('-') + QUuid::createUuid().toString(QUuid::Id128);
}

int hydrologyLayer(QJsonArray& layers) {
    for (int i = 0; i < layers.size(); ++i) {
        const QString name = layers.at(i).toObject().value(QStringLiteral("name")).toString();
        if (name.contains(QObject::tr("Ríos"), Qt::CaseInsensitive)
            || name.contains(QStringLiteral("Hidrograf"), Qt::CaseInsensitive)) return i;
    }
    layers.append(QJsonObject{{QStringLiteral("id"), derivedId(QStringLiteral("layer"))},
                              {QStringLiteral("name"), QObject::tr("Ríos y lagos")},
                              {QStringLiteral("visible"), true},
                              {QStringLiteral("locked"), false},
                              {QStringLiteral("opacity"), 1.0},
                              {QStringLiteral("objects"), QJsonArray()}});
    return layers.size() - 1;
}

QJsonArray manualOnly(const QJsonArray& values) {
    QJsonArray result;
    for (const QJsonValue& value : values) {
        const QJsonObject object = value.toObject();
        if (!object.value(QStringLiteral("generated")).toBool(false)) result.append(object);
    }
    return result;
}

} // namespace

void PilinReyEditor::recalculateDerivedGeography() {
    mutateDocument([&](QJsonObject& pilin) {
        QJsonObject terrainJson = pilin.value(QStringLiteral("terrainModel")).toObject();
        MapTerrainEngine terrain = MapTerrainEngine::fromJson(terrainJson);
        if (terrain.width() < 2 || terrain.height() < 2) return;

        const int width = terrain.width();
        const int height = terrain.height();
        const int count = width * height;
        const double seaLevel = std::clamp(terrainJson.value(QStringLiteral("seaLevel")).toDouble(.42), .05, .85);
        const QJsonArray temperatures = terrainJson.value(QStringLiteral("temperature")).toArray();
        const QJsonArray moisture = terrainJson.value(QStringLiteral("moisture")).toArray();
        const QJsonArray precipitation = terrainJson.value(QStringLiteral("precipitation")).toArray();

        QJsonArray visualBiomes;
        QJsonArray biomeIds;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const int index = y * width + x;
                const double elevation = terrain.at(x, y);
                if (elevation < seaLevel) {
                    visualBiomes.append(0);
                    biomeIds.append(0);
                    continue;
                }

                const double latitude = std::abs((static_cast<double>(y) / std::max(1, height - 1)) * 2.0 - 1.0);
                const double landHeight = (elevation - seaLevel) / std::max(.001, 1.0 - seaLevel);
                const double temperature = temperatures.size() == count
                    ? temperatures.at(index).toDouble()
                    : 30.0 - latitude * 35.0 - landHeight * 20.0;
                const double wetness = moisture.size() == count
                    ? moisture.at(index).toDouble()
                    : (precipitation.size() == count ? precipitation.at(index).toDouble() + 4.0 : 13.0);

                int azgaarBiome = 4;
                int visual = 1;
                if (temperature < -5.0 || elevation > .91) {
                    azgaarBiome = 11; visual = 6;
                } else if (latitude > .78 || temperature < 1.5) {
                    azgaarBiome = 10; visual = 7;
                } else if (wetness < 7.5) {
                    azgaarBiome = temperature > 17.0 ? 1 : 2; visual = 4;
                } else if (wetness > 31.0 && landHeight < .34) {
                    azgaarBiome = 12; visual = 3;
                } else if (wetness > 23.0) {
                    azgaarBiome = temperature > 19.0 ? 7 : 8; visual = 2;
                } else if (wetness > 16.0) {
                    azgaarBiome = temperature > 18.0 ? 5 : 6; visual = temperature > 18.0 ? 3 : 2;
                } else if (temperature < 7.0) {
                    azgaarBiome = 9; visual = 3;
                }
                biomeIds.append(azgaarBiome);
                visualBiomes.append(visual);
            }
        }
        terrainJson.insert(QStringLiteral("biomeIds"), biomeIds);
        terrainJson.insert(QStringLiteral("biomes"), visualBiomes);
        terrainJson.insert(QStringLiteral("derivedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        pilin.insert(QStringLiteral("terrainModel"), terrainJson);

        QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();
        const int hydroIndex = hydrologyLayer(layers);
        QJsonObject hydro = layers.at(hydroIndex).toObject();
        QJsonArray objects = manualOnly(hydro.value(QStringLiteral("objects")).toArray());
        const double documentWidth = pilin.value(QStringLiteral("width")).toDouble(4096.0);
        const double documentHeight = pilin.value(QStringLiteral("height")).toDouble(2304.0);
        const QJsonArray rivers = terrain.traceRivers(34, static_cast<float>(seaLevel));
        for (const QJsonValue& riverValue : rivers) {
            QJsonArray points;
            for (const QJsonValue& pointValue : riverValue.toArray()) {
                const QJsonObject point = pointValue.toObject();
                points.append(QJsonObject{{QStringLiteral("x"), point.value(QStringLiteral("x")).toDouble() * documentWidth},
                                          {QStringLiteral("y"), point.value(QStringLiteral("y")).toDouble() * documentHeight}});
            }
            if (points.size() < 4) continue;
            objects.append(QJsonObject{{QStringLiteral("id"), derivedId(QStringLiteral("river"))},
                                       {QStringLiteral("type"), QStringLiteral("river")},
                                       {QStringLiteral("points"), points},
                                       {QStringLiteral("width"), 3.2},
                                       {QStringLiteral("generated"), true}});
        }
        hydro.insert(QStringLiteral("objects"), objects);
        layers.replace(hydroIndex, hydro);
        pilin.insert(QStringLiteral("layers"), layers);
    });
}

} // namespace wbw
