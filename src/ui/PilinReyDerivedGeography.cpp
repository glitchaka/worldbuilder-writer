#include "ui/PilinReyEditor.h"

#include "ui/MapTerrainEngine.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace wbw {
namespace {

QString derivedId(const QString& prefix) {
    return prefix + QLatin1Char('-') + QUuid::createUuid().toString(QUuid::Id128);
}

quint32 hashCell(int x, int y, quint32 seed) {
    quint32 h = seed ^ static_cast<quint32>(x * 374761393u) ^ static_cast<quint32>(y * 668265263u);
    h = (h ^ (h >> 13u)) * 1274126177u;
    return h ^ (h >> 16u);
}

double noise01(int x, int y, quint32 seed) {
    return static_cast<double>(hashCell(x, y, seed) & 0xffffu) / 65535.0;
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

QJsonArray floatArray(const QVector<float>& values) {
    QJsonArray result;
    for (float value : values) result.append(std::round(value * 10.0) / 10.0);
    return result;
}

QJsonArray intArray(const QVector<int>& values) {
    QJsonArray result;
    for (int value : values) result.append(value);
    return result;
}

int biomeFromClimate(double temperature, double moisture, double elevation, double seaLevel, bool river) {
    if (elevation < seaLevel) return 0;
    if (temperature < -5.0 || elevation > .92) return 11;
    const double landHeight = (elevation - seaLevel) / std::max(.001, 1.0 - seaLevel);
    if (temperature < 1.5) return 10;
    if (moisture > 36.0 && landHeight < .30) return 12;
    if (moisture < 7.0 && !river) return temperature > 18.0 ? 1 : 2;
    if (moisture > 28.0) return temperature > 19.0 ? 7 : 8;
    if (moisture > 19.0) return temperature > 17.0 ? 5 : 6;
    if (temperature < 7.0) return 9;
    return temperature > 18.0 ? 3 : 4;
}

int visualBiome(int biome) {
    switch (biome) {
        case 0: return 0;
        case 1:
        case 2: return 4;
        case 3:
        case 4: return 1;
        case 5:
        case 9:
        case 12: return 3;
        case 6:
        case 7:
        case 8: return 2;
        case 10: return 7;
        case 11: return 6;
        default: return 1;
    }
}

} // namespace

void PilinReyEditor::recalculateDerivedGeography() {
    if (map_.isEmpty()) return;

    mutateDocument([&](QJsonObject& pilin) {
        QJsonObject terrainJson = pilin.value(QStringLiteral("terrainModel")).toObject();
        MapTerrainEngine terrain = MapTerrainEngine::fromJson(terrainJson);
        if (terrain.width() < 2 || terrain.height() < 2) return;

        const int width = terrain.width();
        const int height = terrain.height();
        const int count = width * height;
        const double seaLevel = std::clamp(terrainJson.value(QStringLiteral("seaLevel")).toDouble(.42), .05, .85);
        const quint32 seed = static_cast<quint32>(terrainJson.value(QStringLiteral("seed")).toInt(42137));

        QVector<float> temperature(count, 0.0f);
        QVector<float> precipitation(count, 0.0f);
        QVector<float> moisture(count, 0.0f);
        QVector<float> flow(count, 0.0f);
        QVector<int> receiver(count, -1);
        QVector<int> biomeIds(count, 0);
        QVector<int> visualBiomes(count, 0);

        auto indexOf = [width](int x, int y) { return y * width + x; };

        for (int y = 0; y < height; ++y) {
            const double latitude = std::abs((static_cast<double>(y) / std::max(1, height - 1)) * 2.0 - 1.0);
            for (int x = 0; x < width; ++x) {
                const double elevation = terrain.at(x, y);
                const double landHeight = std::max(0.0, (elevation - seaLevel) / std::max(.001, 1.0 - seaLevel));
                const double local = (noise01(x / 5, y / 5, seed ^ 0x6a09e667u) - .5) * 3.0;
                temperature[indexOf(x, y)] = static_cast<float>(30.0 - latitude * 35.0 - landHeight * 20.0 + local);
            }
        }

        auto windPass = [&](bool westToEast, float weight) {
            for (int y = 0; y < height; ++y) {
                double humidity = .72 + noise01(y, 17, seed ^ 0xbb67ae85u) * .22;
                double previous = seaLevel;
                for (int step = 0; step < width; ++step) {
                    const int x = westToEast ? step : width - 1 - step;
                    const int cell = indexOf(x, y);
                    const double elevation = terrain.at(x, y);
                    if (elevation < seaLevel) {
                        humidity = std::min(1.0, humidity + .085);
                        previous = elevation;
                        precipitation[cell] += 7.0f * weight;
                        continue;
                    }
                    const double rise = std::max(0.0, elevation - previous);
                    const double fall = std::max(0.0, previous - elevation);
                    const double convection = 3.0 + noise01(x / 3, y / 3, seed ^ 0x3c6ef372u) * 7.0;
                    const double rain = std::clamp((convection + rise * 135.0) * humidity, 0.0, 35.0);
                    precipitation[cell] += static_cast<float>(rain * weight);
                    humidity = std::clamp(humidity - rain / 130.0 - rise * .45 + fall * .08, .05, 1.0);
                    previous = elevation;
                }
            }
        };
        windPass(true, .68f);
        windPass(false, .32f);

        QVector<int> order(count);
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&](int a, int b) {
            return terrain.at(a % width, a / width) > terrain.at(b % width, b / width);
        });

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const int cell = indexOf(x, y);
                const double elevation = terrain.at(x, y);
                if (elevation < seaLevel) continue;
                flow[cell] = 1.0f + precipitation[cell] * .13f;
                double best = elevation;
                int target = -1;
                for (int oy = -1; oy <= 1; ++oy) {
                    for (int ox = -1; ox <= 1; ++ox) {
                        if (!ox && !oy) continue;
                        const int nx = x + ox;
                        const int ny = y + oy;
                        if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;
                        const double candidate = terrain.at(nx, ny);
                        if (candidate < best - 1e-5) {
                            best = candidate;
                            target = indexOf(nx, ny);
                        }
                    }
                }
                receiver[cell] = target;
            }
        }
        for (int cell : order) {
            if (receiver[cell] >= 0) flow[receiver[cell]] += flow[cell];
        }

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const int cell = indexOf(x, y);
                const double elevation = terrain.at(x, y);
                if (elevation < seaLevel) continue;
                double neighborhoodRain = precipitation[cell];
                int samples = 1;
                for (int oy = -1; oy <= 1; ++oy) {
                    for (int ox = -1; ox <= 1; ++ox) {
                        if (!ox && !oy) continue;
                        const int nx = x + ox;
                        const int ny = y + oy;
                        if (nx < 0 || ny < 0 || nx >= width || ny >= height || terrain.at(nx, ny) < seaLevel) continue;
                        neighborhoodRain += precipitation[indexOf(nx, ny)];
                        ++samples;
                    }
                }
                const bool hasRiver = flow[cell] > 18.0f;
                const double wetness = 4.0 + neighborhoodRain / samples + (hasRiver ? std::max(2.0, static_cast<double>(flow[cell]) / 10.0) : 0.0);
                moisture[cell] = static_cast<float>(wetness);
                biomeIds[cell] = biomeFromClimate(temperature[cell], wetness, elevation, seaLevel, hasRiver);
                visualBiomes[cell] = visualBiome(biomeIds[cell]);
            }
        }

        terrainJson.insert(QStringLiteral("temperature"), floatArray(temperature));
        terrainJson.insert(QStringLiteral("precipitation"), floatArray(precipitation));
        terrainJson.insert(QStringLiteral("moisture"), floatArray(moisture));
        terrainJson.insert(QStringLiteral("flow"), floatArray(flow));
        terrainJson.insert(QStringLiteral("biomeIds"), intArray(biomeIds));
        terrainJson.insert(QStringLiteral("biomes"), intArray(visualBiomes));
        terrainJson.insert(QStringLiteral("derivedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        pilin.insert(QStringLiteral("terrainModel"), terrainJson);

        QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();
        const int hydroIndex = hydrologyLayer(layers);
        QJsonObject hydro = layers.at(hydroIndex).toObject();
        QJsonArray objects = manualOnly(hydro.value(QStringLiteral("objects")).toArray());
        const double documentWidth = pilin.value(QStringLiteral("width")).toDouble(4096.0);
        const double documentHeight = pilin.value(QStringLiteral("height")).toDouble(2304.0);

        QVector<int> riverHeads;
        for (int cell = 0; cell < count; ++cell) {
            const int x = cell % width;
            const int y = cell / width;
            if (terrain.at(x, y) > seaLevel + .08 && flow[cell] > 15.0f) riverHeads.append(cell);
        }
        std::sort(riverHeads.begin(), riverHeads.end(), [&](int a, int b) { return flow[a] > flow[b]; });
        QVector<bool> claimed(count, false);
        int riverCount = 0;
        for (int head : riverHeads) {
            if (riverCount >= 36 || claimed[head]) continue;
            QJsonArray points;
            int current = head;
            int steps = 0;
            float maxFlow = 0.0f;
            while (current >= 0 && steps++ < width + height) {
                const int x = current % width;
                const int y = current / width;
                if (claimed[current] && points.size() > 3) break;
                claimed[current] = true;
                maxFlow = std::max(maxFlow, flow[current]);
                if (steps % 2 == 1) {
                    points.append(QJsonObject{{QStringLiteral("x"), static_cast<double>(x) / std::max(1, width - 1) * documentWidth},
                                              {QStringLiteral("y"), static_cast<double>(y) / std::max(1, height - 1) * documentHeight}});
                }
                if (terrain.at(x, y) <= seaLevel) break;
                current = receiver[current];
            }
            if (points.size() >= 5) {
                objects.append(QJsonObject{{QStringLiteral("id"), derivedId(QStringLiteral("river"))},
                                           {QStringLiteral("type"), QStringLiteral("river")},
                                           {QStringLiteral("points"), points},
                                           {QStringLiteral("width"), std::clamp(1.8 + std::log1p(maxFlow) * .78, 2.2, 8.5)},
                                           {QStringLiteral("generated"), true}});
                ++riverCount;
            }
        }

        int lakes = 0;
        for (int y = 4; y < height - 4 && lakes < 16; ++y) {
            for (int x = 4; x < width - 4 && lakes < 16; ++x) {
                const int cell = indexOf(x, y);
                const double elevation = terrain.at(x, y);
                if (receiver[cell] >= 0 || flow[cell] < 38.0f || elevation < seaLevel + .025 || elevation > .72) continue;
                const double radius = std::clamp(10.0 + std::sqrt(flow[cell]) * 2.4, 12.0, 38.0);
                QJsonArray polygon;
                for (int p = 0; p <= 12; ++p) {
                    const double angle = static_cast<double>(p) / 12.0 * 6.283185307179586;
                    const double wobble = .78 + noise01(x * 17 + p, y * 11 + p, seed ^ 0x510e527fu) * .42;
                    polygon.append(QJsonObject{{QStringLiteral("x"), static_cast<double>(x) / std::max(1, width - 1) * documentWidth + std::cos(angle) * radius * wobble},
                                               {QStringLiteral("y"), static_cast<double>(y) / std::max(1, height - 1) * documentHeight + std::sin(angle) * radius * wobble}});
                }
                objects.append(QJsonObject{{QStringLiteral("id"), derivedId(QStringLiteral("lake"))},
                                           {QStringLiteral("type"), QStringLiteral("lake")},
                                           {QStringLiteral("points"), polygon},
                                           {QStringLiteral("width"), 2.4},
                                           {QStringLiteral("fillOpacity"), .82},
                                           {QStringLiteral("generated"), true}});
                ++lakes;
            }
        }

        hydro.insert(QStringLiteral("objects"), objects);
        layers.replace(hydroIndex, hydro);
        pilin.insert(QStringLiteral("layers"), layers);
    });
}

} // namespace wbw
