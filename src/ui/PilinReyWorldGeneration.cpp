#include "ui/PilinReyEditor.h"
#include "ui/MapTerrainEngine.h"

#include <QDateTime>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QPointF>
#include <QUuid>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <queue>

namespace wbw {
namespace {

// Native C++ adaptation of the dependency order and biome model used by
// Azgaar/Fantasy-Map-Generator (MIT). Worldbuilder Writer never embeds the web
// application: terrain, climate, hydrology, biomes, settlements and politics
// are stored as editable native project data.

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

quint32 hashCell(int x, int y, quint32 seed) {
    quint32 h = seed ^ static_cast<quint32>(x * 374761393u) ^ static_cast<quint32>(y * 668265263u);
    h = (h ^ (h >> 13u)) * 1274126177u;
    return h ^ (h >> 16u);
}

double noise01(int x, int y, quint32 seed) {
    return static_cast<double>(hashCell(x, y, seed) & 0xffffu) / 65535.0;
}

QString generatedPlaceName(int index, quint32 seed) {
    static const QStringList starts{
        QStringLiteral("Val"), QStringLiteral("Ar"), QStringLiteral("Bel"), QStringLiteral("Cor"),
        QStringLiteral("Dor"), QStringLiteral("Eri"), QStringLiteral("Fal"), QStringLiteral("Gal"),
        QStringLiteral("Ira"), QStringLiteral("Lun"), QStringLiteral("Mor"), QStringLiteral("Nor"),
        QStringLiteral("Or"), QStringLiteral("Ria"), QStringLiteral("Sol"), QStringLiteral("Tor"),
        QStringLiteral("Vel")};
    static const QStringList ends{
        QStringLiteral("oria"), QStringLiteral("en"), QStringLiteral("ar"), QStringLiteral("is"),
        QStringLiteral("ona"), QStringLiteral("eth"), QStringLiteral("ia"), QStringLiteral("or"),
        QStringLiteral("una"), QStringLiteral("as"), QStringLiteral("el"), QStringLiteral("in")};
    const quint32 h = hashCell(index, index * 7 + 3, seed);
    return starts.at(static_cast<int>(h % starts.size())) + ends.at(static_cast<int>((h >> 8u) % ends.size()));
}

QString regionNameForCapital(const QString& capital, int index) {
    static const QStringList forms{
        QObject::tr("Reino de %1"), QObject::tr("Principado de %1"), QObject::tr("Marca de %1"),
        QObject::tr("Dominio de %1"), QObject::tr("Liga de %1")};
    return forms.at(index % forms.size()).arg(capital);
}

int ensureLayer(QJsonArray& layers, const QString& name) {
    for (int i = 0; i < layers.size(); ++i) {
        if (layers.at(i).toObject().value(QStringLiteral("name")).toString().compare(name, Qt::CaseInsensitive) == 0)
            return i;
    }
    layers.append(QJsonObject{{QStringLiteral("id"), generatedId(QStringLiteral("layer"))},
                              {QStringLiteral("name"), name},
                              {QStringLiteral("visible"), true},
                              {QStringLiteral("locked"), false},
                              {QStringLiteral("opacity"), 1.0},
                              {QStringLiteral("objects"), QJsonArray()}});
    return layers.size() - 1;
}

struct WorldFields {
    QVector<float> temperature;
    QVector<float> precipitation;
    QVector<float> moisture;
    QVector<float> flow;
    QVector<int> receiver;
    QVector<int> biome;
};

int cellIndex(int x, int y, int width) { return y * width + x; }

int azgaarBiome(double moisture, double temperature, double elevation, double seaLevel, bool hasRiver) {
    // Matrix and thresholds mirror Azgaar's climate-to-biome model, translated
    // to this engine's normalized height scale.
    static constexpr int matrix[5][26] = {
        {1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,10},
        {3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,9,9,9,9,10,10,10},
        {5,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,9,9,9,9,9,10,10,10},
        {5,6,6,6,6,6,6,8,8,8,8,8,8,8,8,8,8,9,9,9,9,9,9,10,10,10},
        {7,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,10,10}
    };

    if (elevation < seaLevel) return 0;
    if (temperature < -5.0) return 11;
    if (temperature >= 25.0 && !hasRiver && moisture < 8.0) return 1;

    const double normalizedLandHeight = (elevation - seaLevel) / std::max(.001, 1.0 - seaLevel);
    if (temperature > -2.0) {
        if (moisture > 40.0 && normalizedLandHeight < .12) return 12;
        if (moisture > 24.0 && normalizedLandHeight >= .12 && normalizedLandHeight < .52) return 12;
    }

    const int moistureBand = std::clamp(static_cast<int>(moisture / 5.0), 0, 4);
    const int temperatureBand = std::clamp(static_cast<int>(20.0 - temperature), 0, 25);
    return matrix[moistureBand][temperatureBand];
}

int visualBiomeClass(int azgaarId) {
    switch (azgaarId) {
        case 0: return 0;   // marine
        case 1: return 4;   // hot desert
        case 2: return 4;   // cold desert
        case 3: return 1;   // savanna
        case 4: return 1;   // grassland
        case 5: return 3;   // tropical seasonal forest
        case 6: return 2;   // temperate deciduous forest
        case 7: return 2;   // tropical rainforest
        case 8: return 2;   // temperate rainforest
        case 9: return 3;   // taiga
        case 10: return 7;  // tundra
        case 11: return 6;  // glacier
        case 12: return 3;  // wetland
        default: return 1;
    }
}

WorldFields buildWorldFields(const MapTerrainEngine& terrain, quint32 seed, double seaLevel) {
    const int width = terrain.width();
    const int height = terrain.height();
    const int count = width * height;
    WorldFields fields;
    fields.temperature.resize(count);
    fields.precipitation.fill(0.0f, count);
    fields.moisture.resize(count);
    fields.flow.fill(0.0f, count);
    fields.receiver.fill(-1, count);
    fields.biome.fill(0, count);

    // Temperature: latitude + lapse rate + small deterministic local variation.
    for (int y = 0; y < height; ++y) {
        const double latitude = std::abs((static_cast<double>(y) / std::max(1, height - 1)) * 2.0 - 1.0);
        for (int x = 0; x < width; ++x) {
            const double elevation = terrain.at(x, y);
            const double aboveSea = std::max(0.0, (elevation - seaLevel) / std::max(.001, 1.0 - seaLevel));
            const double local = (noise01(x / 5, y / 5, seed ^ 0x6a09e667u) - .5) * 3.0;
            fields.temperature[cellIndex(x, y, width)] = static_cast<float>(30.0 - latitude * 35.0 - aboveSea * 20.0 + local);
        }
    }

    // Precipitation: two opposing wind passes. Ocean recharges humidity, rising
    // terrain rains out moisture and descending terrain creates rain shadows.
    auto windPass = [&](bool westToEast, float weight) {
        for (int y = 0; y < height; ++y) {
            double humidity = .72 + noise01(y, 17, seed ^ 0xbb67ae85u) * .22;
            double previousHeight = seaLevel;
            for (int step = 0; step < width; ++step) {
                const int x = westToEast ? step : (width - 1 - step);
                const int i = cellIndex(x, y, width);
                const double elevation = terrain.at(x, y);
                if (elevation < seaLevel) {
                    humidity = std::min(1.0, humidity + .085);
                    previousHeight = elevation;
                    fields.precipitation[i] += 7.0f * weight;
                    continue;
                }

                const double rise = std::max(0.0, elevation - previousHeight);
                const double fall = std::max(0.0, previousHeight - elevation);
                const double convection = 3.0 + noise01(x / 3, y / 3, seed ^ 0x3c6ef372u) * 7.0;
                const double orographic = rise * 135.0;
                const double rain = std::clamp((convection + orographic) * humidity, 0.0, 35.0);
                fields.precipitation[i] += static_cast<float>(rain * weight);
                humidity = std::clamp(humidity - rain / 130.0 - rise * .45 + fall * .08, .05, 1.0);
                previousHeight = elevation;
            }
        }
    };
    windPass(true, .68f);
    windPass(false, .32f);

    // Drainage receivers and flow accumulation. This is the native equivalent
    // of Azgaar's hydrology stage: climate feeds flow, then rivers feed moisture.
    QVector<int> order(count);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return terrain.at(a % width, a / width) > terrain.at(b % width, b / width);
    });

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int i = cellIndex(x, y, width);
            const double elevation = terrain.at(x, y);
            fields.flow[i] = elevation < seaLevel ? 0.0f : 1.0f + fields.precipitation[i] * .13f;
            if (elevation < seaLevel) continue;

            double best = elevation;
            int bestIndex = -1;
            for (int oy = -1; oy <= 1; ++oy) {
                for (int ox = -1; ox <= 1; ++ox) {
                    if (!ox && !oy) continue;
                    const int nx = x + ox, ny = y + oy;
                    if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;
                    const double candidate = terrain.at(nx, ny);
                    if (candidate < best - 1e-5) {
                        best = candidate;
                        bestIndex = cellIndex(nx, ny, width);
                    }
                }
            }
            fields.receiver[i] = bestIndex;
        }
    }

    for (int i : order) {
        const int receiver = fields.receiver[i];
        if (receiver >= 0) fields.flow[receiver] += fields.flow[i];
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int i = cellIndex(x, y, width);
            if (terrain.at(x, y) < seaLevel) {
                fields.moisture[i] = 0.0f;
                fields.biome[i] = 0;
                continue;
            }
            double neighborRain = fields.precipitation[i];
            int samples = 1;
            for (int oy = -1; oy <= 1; ++oy) {
                for (int ox = -1; ox <= 1; ++ox) {
                    if (!ox && !oy) continue;
                    const int nx = x + ox, ny = y + oy;
                    if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;
                    if (terrain.at(nx, ny) < seaLevel) continue;
                    neighborRain += fields.precipitation[cellIndex(nx, ny, width)];
                    ++samples;
                }
            }
            const bool hasRiver = fields.flow[i] > 18.0f;
            const double moisture = 4.0 + neighborRain / samples + (hasRiver ? std::max(2.0, fields.flow[i] / 10.0) : 0.0);
            fields.moisture[i] = static_cast<float>(moisture);
            fields.biome[i] = azgaarBiome(moisture, fields.temperature[i], terrain.at(x, y), seaLevel, hasRiver);
        }
    }

    return fields;
}

QJsonArray floatArray(const QVector<float>& values, double scale = 1.0) {
    QJsonArray array;
    for (float value : values) array.append(std::round(value * scale) / scale);
    return array;
}

QJsonArray intArray(const QVector<int>& values) {
    QJsonArray array;
    for (int value : values) array.append(value);
    return array;
}

QJsonArray visualBiomeArray(const QVector<int>& values) {
    QJsonArray array;
    for (int value : values) array.append(visualBiomeClass(value));
    return array;
}

QVector<QPointF> chooseSettlements(const MapTerrainEngine& terrain, const WorldFields& fields,
                                   quint32 seed, double seaLevel, int wanted) {
    static constexpr std::array<int, 13> habitability{0,4,10,22,30,50,100,80,90,12,4,0,12};
    struct Candidate { int x; int y; double score; };
    QVector<Candidate> candidates;
    const int w = terrain.width(), h = terrain.height();

    for (int y = 3; y < h - 3; y += 2) {
        for (int x = 3; x < w - 3; x += 2) {
            const int i = cellIndex(x, y, w);
            const double elevation = terrain.at(x, y);
            if (elevation < seaLevel + .012 || elevation > .86) continue;
            const int biome = std::clamp(fields.biome[i], 0, 12);
            if (habitability[biome] <= 0) continue;

            int coastNeighbors = 0;
            double slope = 0.0;
            for (int oy = -2; oy <= 2; ++oy) {
                for (int ox = -2; ox <= 2; ++ox) {
                    const int nx = x + ox, ny = y + oy;
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                    if (terrain.at(nx, ny) < seaLevel) ++coastNeighbors;
                    slope = std::max(slope, std::abs(static_cast<double>(terrain.at(nx, ny)) - elevation));
                }
            }
            const double coast = std::min(1.0, coastNeighbors / 6.0);
            const double river = std::min(1.0, static_cast<double>(fields.flow[i]) / 55.0);
            const double climate = habitability[biome] / 100.0;
            const double flatness = std::clamp(1.0 - slope * 7.0, 0.0, 1.0);
            const double jitter = noise01(x, y, seed ^ 0xa24baed5u) * .28;
            candidates.append({x, y, climate * .70 + coast * .42 + river * .38 + flatness * .26 + jitter});
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
    QVector<QPointF> result;
    for (const Candidate& candidate : candidates) {
        if (result.size() >= wanted) break;
        bool tooNear = false;
        for (const QPointF& point : result) {
            if (std::hypot(point.x() - candidate.x, point.y() - candidate.y) < 15.0) {
                tooNear = true;
                break;
            }
        }
        if (!tooNear) result.append(QPointF(candidate.x, candidate.y));
    }
    return result;
}

QJsonArray buildPoliticalRegions(const MapTerrainEngine& terrain, const WorldFields& fields,
                                 const QVector<QPointF>& capitals, double seaLevel) {
    QJsonArray regions;
    const int w = terrain.width(), h = terrain.height();
    if (capitals.isEmpty()) {
        for (int i = 0; i < w * h; ++i) regions.append(-1);
        return regions;
    }

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const double elevation = terrain.at(x, y);
            if (elevation < seaLevel) { regions.append(-1); continue; }
            int best = 0;
            double bestScore = std::numeric_limits<double>::max();
            const int i = cellIndex(x, y, w);
            for (int r = 0; r < capitals.size(); ++r) {
                const double dx = x - capitals.at(r).x();
                const double dy = y - capitals.at(r).y();
                const double mountain = std::max(0.0, elevation - .66) * 1250.0;
                const double wetBarrier = std::min(18.0, static_cast<double>(fields.flow[i]) * .12);
                const double score = dx * dx + dy * dy + mountain + wetBarrier;
                if (score < bestScore) { bestScore = score; best = r; }
            }
            regions.append(best);
        }
    }
    return regions;
}

QJsonArray buildHydrologyObjects(const MapTerrainEngine& terrain, const WorldFields& fields,
                                 double seaLevel, double docW, double docH, int desiredRivers) {
    const int w = terrain.width(), h = terrain.height();
    struct Head { int index; float flow; float elevation; };
    QVector<Head> heads;
    for (int y = 2; y < h - 2; ++y) {
        for (int x = 2; x < w - 2; ++x) {
            const int i = cellIndex(x, y, w);
            const float elevation = terrain.at(x, y);
            if (elevation < seaLevel + .08f || fields.flow[i] < 15.0f) continue;
            heads.append({i, fields.flow[i], elevation});
        }
    }
    std::sort(heads.begin(), heads.end(), [](const Head& a, const Head& b) {
        return a.flow * (.7f + a.elevation) > b.flow * (.7f + b.elevation);
    });

    QJsonArray objects;
    QVector<bool> claimed(w * h, false);
    int riverCount = 0;
    for (const Head& head : heads) {
        if (riverCount >= desiredRivers) break;
        if (claimed[head.index]) continue;

        QJsonArray points;
        int current = head.index;
        int steps = 0;
        float maxFlow = 0.0f;
        while (current >= 0 && steps++ < w + h) {
            const int x = current % w, y = current / w;
            if (claimed[current] && points.size() > 3) break;
            claimed[current] = true;
            maxFlow = std::max(maxFlow, fields.flow[current]);
            if (steps % 2 == 1) {
                points.append(QJsonObject{{QStringLiteral("x"), static_cast<double>(x) / std::max(1, w - 1) * docW},
                                          {QStringLiteral("y"), static_cast<double>(y) / std::max(1, h - 1) * docH}});
            }
            if (terrain.at(x, y) <= seaLevel) break;
            current = fields.receiver[current];
        }

        if (points.size() >= 5) {
            const double width = std::clamp(1.8 + std::log1p(maxFlow) * .78, 2.2, 8.5);
            objects.append(QJsonObject{{QStringLiteral("id"), generatedId(QStringLiteral("river"))},
                                       {QStringLiteral("type"), QStringLiteral("river")},
                                       {QStringLiteral("points"), points},
                                       {QStringLiteral("width"), width},
                                       {QStringLiteral("generated"), true}});
            ++riverCount;
        }
    }

    // Deep inland sinks become editable lake polygons instead of terminating as
    // invisible hydrology failures.
    int lakeCount = 0;
    for (int y = 4; y < h - 4 && lakeCount < 16; ++y) {
        for (int x = 4; x < w - 4 && lakeCount < 16; ++x) {
            const int i = cellIndex(x, y, w);
            if (fields.receiver[i] >= 0 || fields.flow[i] < 38.0f) continue;
            const double elevation = terrain.at(x, y);
            if (elevation < seaLevel + .025 || elevation > .72) continue;
            const double radius = std::clamp(10.0 + std::sqrt(fields.flow[i]) * 2.4, 12.0, 38.0);
            QJsonArray polygon;
            for (int p = 0; p <= 12; ++p) {
                const double angle = static_cast<double>(p) / 12.0 * 6.283185307179586;
                const double wobble = .78 + noise01(x * 17 + p, y * 11 + p, 0x510e527fu) * .42;
                const double wx = static_cast<double>(x) / std::max(1, w - 1) * docW + std::cos(angle) * radius * wobble;
                const double wy = static_cast<double>(y) / std::max(1, h - 1) * docH + std::sin(angle) * radius * wobble;
                polygon.append(QJsonObject{{QStringLiteral("x"), wx}, {QStringLiteral("y"), wy}});
            }
            objects.append(QJsonObject{{QStringLiteral("id"), generatedId(QStringLiteral("lake"))},
                                       {QStringLiteral("type"), QStringLiteral("lake")},
                                       {QStringLiteral("points"), polygon},
                                       {QStringLiteral("width"), 2.4},
                                       {QStringLiteral("fillOpacity"), .82},
                                       {QStringLiteral("generated"), true}});
            ++lakeCount;
        }
    }
    return objects;
}

QJsonArray terrainAwareRoad(const QPointF& start, const QPointF& goal, const MapTerrainEngine& terrain,
                            double seaLevel, double docW, double docH) {
    const int w = terrain.width(), h = terrain.height(), count = w * h;
    auto idx = [w](int x, int y) { return y * w + x; };
    const int sx = std::clamp(static_cast<int>(std::round(start.x())), 0, w - 1);
    const int sy = std::clamp(static_cast<int>(std::round(start.y())), 0, h - 1);
    const int gx = std::clamp(static_cast<int>(std::round(goal.x())), 0, w - 1);
    const int gy = std::clamp(static_cast<int>(std::round(goal.y())), 0, h - 1);
    struct Node { double f; int index; bool operator<(const Node& other) const { return f > other.f; } };
    std::priority_queue<Node> open;
    QVector<double> g(count, std::numeric_limits<double>::infinity());
    QVector<int> parent(count, -1);
    const int startIndex = idx(sx, sy), goalIndex = idx(gx, gy);
    g[startIndex] = 0;
    open.push({std::hypot(gx - sx, gy - sy), startIndex});
    static constexpr int dirs[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};

    while (!open.empty()) {
        const int current = open.top().index;
        open.pop();
        if (current == goalIndex) break;
        const int cx = current % w, cy = current / w;
        for (const auto& d : dirs) {
            const int nx = cx + d[0], ny = cy + d[1];
            if (nx < 1 || ny < 1 || nx >= w - 1 || ny >= h - 1) continue;
            const double nh = terrain.at(nx, ny);
            if (nh < seaLevel + .006) continue;
            const double ch = terrain.at(cx, cy);
            const double slope = std::abs(nh - ch);
            const double highland = std::max(0.0, nh - .62);
            const double step = (d[0] && d[1]) ? 1.414 : 1.0;
            const double cost = step * (1.0 + slope * 18.0 + highland * 5.0);
            const int ni = idx(nx, ny);
            const double tentative = g[current] + cost;
            if (tentative >= g[ni]) continue;
            g[ni] = tentative;
            parent[ni] = current;
            open.push({tentative + std::hypot(gx - nx, gy - ny), ni});
        }
    }

    if (parent[goalIndex] < 0) return {};
    QVector<QPointF> reversed;
    for (int current = goalIndex; current >= 0; current = parent[current]) {
        reversed.append(QPointF(current % w, current / w));
        if (current == startIndex) break;
    }
    std::reverse(reversed.begin(), reversed.end());
    QJsonArray points;
    const int stride = std::max(1, static_cast<int>(reversed.size()) / 22);
    for (int i = 0; i < reversed.size(); i += stride) {
        const QPointF p = reversed.at(i);
        points.append(QJsonObject{{QStringLiteral("x"), p.x() / std::max(1, w - 1) * docW},
                                  {QStringLiteral("y"), p.y() / std::max(1, h - 1) * docH}});
    }
    if (!reversed.isEmpty()) {
        const QPointF p = reversed.last();
        points.append(QJsonObject{{QStringLiteral("x"), p.x() / std::max(1, w - 1) * docW},
                                  {QStringLiteral("y"), p.y() / std::max(1, h - 1) * docH}});
    }
    return points;
}

} // namespace

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
        tr("Se regenerarán terreno, clima, hidrología, biomas, regiones, rutas y asentamientos procedurales. Tus objetos manuales se conservan. ¿Continuar?")) != QMessageBox::Yes) return;

    pushUndo();
    QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
    const double docW = pilin.value(QStringLiteral("width")).toDouble(4096.0);
    const double docH = pilin.value(QStringLiteral("height")).toDouble(2304.0);
    constexpr double seaLevel = .42;

    // Ordered native generation pipeline: heightmap -> climate -> hydrology ->
    // biomes -> settlements -> politics -> routes. Generation is only the
    // starting point; every produced layer remains editable in the map editor.
    MapTerrainEngine terrain(320, 180, static_cast<quint32>(seed));
    terrain.generateWorld(continents, islands, .66f);
    terrain.erode(3, .19f);
    const WorldFields fields = buildWorldFields(terrain, static_cast<quint32>(seed), seaLevel);

    const int settlementTarget = qBound(8, continents * 7 + islands / 3, 34);
    const QVector<QPointF> settlements = chooseSettlements(terrain, fields, static_cast<quint32>(seed), seaLevel, settlementTarget);
    QVector<QPointF> capitals;
    for (int i = 0; i < qMin(continents, settlements.size()); ++i) capitals.append(settlements.at(i));

    QJsonObject terrainJson = terrain.toJson();
    terrainJson.insert(QStringLiteral("seaLevel"), seaLevel);
    terrainJson.insert(QStringLiteral("generatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    terrainJson.insert(QStringLiteral("generator"), QStringLiteral("azgaar-native-port-v1"));
    terrainJson.insert(QStringLiteral("sourceProject"), QStringLiteral("Azgaar/Fantasy-Map-Generator"));
    terrainJson.insert(QStringLiteral("temperature"), floatArray(fields.temperature, 10.0));
    terrainJson.insert(QStringLiteral("precipitation"), floatArray(fields.precipitation, 10.0));
    terrainJson.insert(QStringLiteral("moisture"), floatArray(fields.moisture, 10.0));
    terrainJson.insert(QStringLiteral("flow"), floatArray(fields.flow, 10.0));
    terrainJson.insert(QStringLiteral("biomeIds"), intArray(fields.biome));
    terrainJson.insert(QStringLiteral("biomes"), visualBiomeArray(fields.biome));
    terrainJson.insert(QStringLiteral("regions"), buildPoliticalRegions(terrain, fields, capitals, seaLevel));

    QJsonArray regionNames;
    for (int i = 0; i < capitals.size(); ++i)
        regionNames.append(regionNameForCapital(generatedPlaceName(i, static_cast<quint32>(seed)), i));
    terrainJson.insert(QStringLiteral("regionNames"), regionNames);
    pilin.insert(QStringLiteral("terrainModel"), terrainJson);

    QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();
    const int hydroIndex = ensureLayer(layers, tr("Ríos y lagos"));
    const int routesIndex = ensureLayer(layers, tr("Rutas"));
    const int settlementIndex = ensureLayer(layers, tr("Asentamientos"));

    QJsonObject hydroLayer = layers.at(hydroIndex).toObject();
    QJsonArray hydroObjects = keepManualObjects(hydroLayer.value(QStringLiteral("objects")).toArray());
    const QJsonArray generatedHydrology = buildHydrologyObjects(terrain, fields, seaLevel, docW, docH,
                                                                 qBound(10, continents * 10 + islands / 2, 46));
    for (const QJsonValue& value : generatedHydrology) hydroObjects.append(value);
    hydroLayer.insert(QStringLiteral("objects"), hydroObjects);
    layers.replace(hydroIndex, hydroLayer);

    QStringList placeNames;
    for (int i = 0; i < settlements.size(); ++i)
        placeNames.append(generatedPlaceName(i, static_cast<quint32>(seed)));

    QJsonObject settlementLayer = layers.at(settlementIndex).toObject();
    QJsonArray settlementObjects = keepManualObjects(settlementLayer.value(QStringLiteral("objects")).toArray());
    for (int i = 0; i < settlements.size(); ++i) {
        const QPointF p = settlements.at(i);
        const int gx = static_cast<int>(p.x()), gy = static_cast<int>(p.y());
        const int cell = cellIndex(gx, gy, terrain.width());
        const bool capital = i < capitals.size();
        const bool riverTown = fields.flow[cell] > 22.0f;
        int coastNeighbors = 0;
        for (int oy = -2; oy <= 2; ++oy)
            for (int ox = -2; ox <= 2; ++ox)
                if (terrain.at(gx + ox, gy + oy) < seaLevel) ++coastNeighbors;
        const bool port = coastNeighbors >= 3;
        const QString kind = capital ? tr("Capital") : (port ? tr("Puerto") : (riverTown ? tr("Ciudad") : tr("Pueblo")));
        const double elevation = terrain.at(gx, gy);
        const double growth = std::clamp(static_cast<double>(fields.flow[cell]) / 45.0 + noise01(i, 19, seed), .25, 2.4);
        const int population = static_cast<int>((capital ? 18000.0 : 2200.0) * growth + noise01(i, 23, seed) * (capital ? 48000.0 : 12000.0));

        settlementObjects.append(QJsonObject{
            {QStringLiteral("id"), generatedId(QStringLiteral("settlement"))},
            {QStringLiteral("type"), QStringLiteral("settlement")},
            {QStringLiteral("x"), p.x() / std::max(1, terrain.width() - 1) * docW},
            {QStringLiteral("y"), p.y() / std::max(1, terrain.height() - 1) * docH},
            {QStringLiteral("label"), placeNames.at(i)},
            {QStringLiteral("group"), capital && i < regionNames.size() ? regionNames.at(i).toString() : QString()},
            {QStringLiteral("settlementType"), kind},
            {QStringLiteral("population"), population},
            {QStringLiteral("elevation"), static_cast<int>(elevation * 1800)},
            {QStringLiteral("biomeId"), fields.biome[cell]},
            {QStringLiteral("size"), capital ? 92 : (port ? 74 : 62)},
            {QStringLiteral("generated"), true}});
    }
    settlementLayer.insert(QStringLiteral("objects"), settlementObjects);
    layers.replace(settlementIndex, settlementLayer);

    QJsonObject routesLayer = layers.at(routesIndex).toObject();
    QJsonArray routeObjects = keepManualObjects(routesLayer.value(QStringLiteral("objects")).toArray());
    for (int i = 1; i < settlements.size(); ++i) {
        int nearest = 0;
        double nearestDistance = std::numeric_limits<double>::max();
        for (int j = 0; j < i; ++j) {
            const double distance = std::hypot(settlements.at(i).x() - settlements.at(j).x(),
                                               settlements.at(i).y() - settlements.at(j).y());
            if (distance < nearestDistance) { nearestDistance = distance; nearest = j; }
        }
        if (nearestDistance > 118.0) continue;
        const QJsonArray points = terrainAwareRoad(settlements.at(i), settlements.at(nearest), terrain, seaLevel, docW, docH);
        if (points.size() < 2) continue;
        routeObjects.append(QJsonObject{{QStringLiteral("id"), generatedId(QStringLiteral("road"))},
                                        {QStringLiteral("type"), QStringLiteral("road")},
                                        {QStringLiteral("points"), points},
                                        {QStringLiteral("width"), 3.0},
                                        {QStringLiteral("generated"), true}});
    }
    routesLayer.insert(QStringLiteral("objects"), routeObjects);
    layers.replace(routesIndex, routesLayer);

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
