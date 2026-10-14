#include "ui/PilinReyEditor.h"

#include "ui/MapTerrainEngine.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QPointF>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <limits>

namespace wbw {
namespace {

QString generatedId(const QString& prefix) {
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

QString placeName(int index, quint32 seed) {
    static const QStringList starts{
        QStringLiteral("Ael"), QStringLiteral("Bel"), QStringLiteral("Cor"), QStringLiteral("Dor"),
        QStringLiteral("Eir"), QStringLiteral("Fal"), QStringLiteral("Gal"), QStringLiteral("Ira"),
        QStringLiteral("Lor"), QStringLiteral("Mor"), QStringLiteral("Nor"), QStringLiteral("Or"),
        QStringLiteral("Rav"), QStringLiteral("Sol"), QStringLiteral("Tor"), QStringLiteral("Val"),
        QStringLiteral("Vel")};
    static const QStringList ends{
        QStringLiteral("ara"), QStringLiteral("en"), QStringLiteral("or"), QStringLiteral("ia"),
        QStringLiteral("eth"), QStringLiteral("una"), QStringLiteral("al"), QStringLiteral("is"),
        QStringLiteral("on"), QStringLiteral("ar"), QStringLiteral("em")};
    const quint32 h = hashCell(index, index * 13 + 7, seed);
    return starts.at(static_cast<int>(h % starts.size()))
        + ends.at(static_cast<int>((h >> 8u) % ends.size()));
}

int ensureLayer(QJsonArray& layers, const QString& name) {
    for (int i = 0; i < layers.size(); ++i) {
        if (layers.at(i).toObject().value(QStringLiteral("name")).toString().compare(name, Qt::CaseInsensitive) == 0)
            return i;
    }
    layers.append(QJsonObject{
        {QStringLiteral("id"), generatedId(QStringLiteral("layer"))},
        {QStringLiteral("name"), name},
        {QStringLiteral("visible"), true},
        {QStringLiteral("locked"), false},
        {QStringLiteral("opacity"), 1.0},
        {QStringLiteral("objects"), QJsonArray()}
    });
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

struct SettlementCandidate {
    int x = 0;
    int y = 0;
    double score = 0.0;
};

QVector<SettlementCandidate> chooseSettlements(const MapTerrainEngine& terrain,
                                                const QJsonObject& model,
                                                quint32 seed,
                                                int wanted) {
    const int w = terrain.width();
    const int h = terrain.height();
    const double sea = std::clamp(model.value(QStringLiteral("seaLevel")).toDouble(.42), .05, .85);
    const QJsonArray flow = model.value(QStringLiteral("flow")).toArray();
    const QJsonArray moisture = model.value(QStringLiteral("moisture")).toArray();
    const QJsonArray biomes = model.value(QStringLiteral("biomeIds")).toArray();

    QVector<SettlementCandidate> candidates;
    for (int y = 3; y < h - 3; y += 2) {
        for (int x = 3; x < w - 3; x += 2) {
            const double elevation = terrain.at(x, y);
            if (elevation <= sea + .015 || elevation > .83) continue;
            const int i = y * w + x;
            const double river = i < flow.size() ? flow.at(i).toDouble() : 0.0;
            const double wet = i < moisture.size() ? moisture.at(i).toDouble() : 12.0;
            const int biome = i < biomes.size() ? biomes.at(i).toInt() : 4;
            if (biome == 11) continue;

            int waterNearby = 0;
            for (int oy = -3; oy <= 3; ++oy) {
                for (int ox = -3; ox <= 3; ++ox) {
                    if (terrain.at(x + ox, y + oy) < sea) ++waterNearby;
                }
            }
            const double coast = std::min(1.0, waterNearby / 9.0);
            const double lowland = std::clamp(1.0 - std::abs(elevation - (sea + .13)) * 3.4, 0.0, 1.0);
            const double riverScore = std::min(1.35, std::log1p(std::max(0.0, river)) / 4.2);
            const double climate = std::clamp(1.0 - std::abs(wet - 22.0) / 35.0, .15, 1.0);
            const double jitter = noise01(x, y, seed ^ 0xa24baed5u) * .42;
            candidates.append({x, y, coast * .85 + lowland * .65 + riverScore * .9 + climate * .32 + jitter});
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) { return a.score > b.score; });
    QVector<SettlementCandidate> selected;
    for (const SettlementCandidate& candidate : candidates) {
        if (selected.size() >= wanted) break;
        bool near = false;
        for (const SettlementCandidate& existing : selected) {
            if (std::hypot(candidate.x - existing.x, candidate.y - existing.y) < 14.0) {
                near = true;
                break;
            }
        }
        if (!near) selected.append(candidate);
    }
    return selected;
}

QJsonArray buildRegions(const MapTerrainEngine& terrain,
                        const QVector<SettlementCandidate>& capitals,
                        double sea) {
    QJsonArray regions;
    for (int y = 0; y < terrain.height(); ++y) {
        for (int x = 0; x < terrain.width(); ++x) {
            if (terrain.at(x, y) < sea || capitals.isEmpty()) {
                regions.append(-1);
                continue;
            }
            int best = 0;
            double bestScore = std::numeric_limits<double>::max();
            for (int i = 0; i < capitals.size(); ++i) {
                const double dx = x - capitals.at(i).x;
                const double dy = y - capitals.at(i).y;
                const double ridgePenalty = std::max(0.0, static_cast<double>(terrain.at(x, y)) - .66) * 1000.0;
                const double score = dx * dx + dy * dy + ridgePenalty;
                if (score < bestScore) {
                    bestScore = score;
                    best = i;
                }
            }
            regions.append(best);
        }
    }
    return regions;
}

QJsonArray routePolyline(const SettlementCandidate& a, const SettlementCandidate& b,
                         double docW, double docH, int gridW, int gridH,
                         quint32 seed, int routeIndex) {
    QJsonArray points;
    const QPointF start(static_cast<double>(a.x) / std::max(1, gridW - 1) * docW,
                        static_cast<double>(a.y) / std::max(1, gridH - 1) * docH);
    const QPointF end(static_cast<double>(b.x) / std::max(1, gridW - 1) * docW,
                      static_cast<double>(b.y) / std::max(1, gridH - 1) * docH);
    const QPointF delta = end - start;
    const QPointF normal(-delta.y(), delta.x());
    const double length = std::max(1.0, std::hypot(delta.x(), delta.y()));
    const QPointF unitNormal = normal / length;
    const double bend = (noise01(routeIndex, 47, seed) - .5) * std::min(160.0, length * .20);

    for (int i = 0; i <= 8; ++i) {
        const double t = i / 8.0;
        const double curve = std::sin(t * 3.141592653589793) * bend;
        const QPointF p = start + delta * t + unitNormal * curve;
        points.append(QJsonObject{{QStringLiteral("x"), p.x()}, {QStringLiteral("y"), p.y()}});
    }
    return points;
}

} // namespace

void PilinReyEditor::generateAzgaarTemplateWorld(const QString& templateId) {
    if (map_.isEmpty()) return;
    if (QMessageBox::question(
            this,
            tr("Generar con Azgaar"),
            tr("Se regenerará la geografía procedural usando la plantilla seleccionada. Tus objetos manuales se conservarán. ¿Continuar?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes) != QMessageBox::Yes) return;

    const quint32 seed = static_cast<quint32>(qMax(1, property("wbwGeneratorSeed").toInt()));
    const double seaLevel = property("wbwGeneratorSeaLevel").isValid()
        ? std::clamp(property("wbwGeneratorSeaLevel").toInt() / 100.0, .20, .70)
        : .42;
    const int politicalCenters = property("wbwGeneratorContinents").isValid()
        ? qBound(1, property("wbwGeneratorContinents").toInt(), 8)
        : 3;
    const int settlementTarget = qBound(10,
        politicalCenters * 6 + qMax(0, property("wbwGeneratorIslands").toInt()) / 2,
        34);

    MapTerrainEngine terrain(320, 180, seed);
    terrain.generateAzgaarTemplate(templateId);
    terrain.erode(2, .16f);

    mutateDocument([&](QJsonObject& pilin) {
        QJsonObject model = terrain.toJson();
        model.insert(QStringLiteral("seaLevel"), seaLevel);
        model.insert(QStringLiteral("generator"), QStringLiteral("azgaar-native-template-v1"));
        model.insert(QStringLiteral("sourceProject"), QStringLiteral("Azgaar/Fantasy-Map-Generator"));
        model.insert(QStringLiteral("heightmapTemplate"), templateId);
        model.insert(QStringLiteral("generatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        pilin.insert(QStringLiteral("terrainModel"), model);
    });

    // Derive climate, moisture, flow, biomes, rivers and lakes from the newly
    // generated heightmap. This same operation is available after manual edits.
    recalculateDerivedGeography();

    const QJsonObject pilinNow = map_.value(QStringLiteral("pilinRey")).toObject();
    QJsonObject terrainJson = pilinNow.value(QStringLiteral("terrainModel")).toObject();
    terrain = MapTerrainEngine::fromJson(terrainJson);
    const QVector<SettlementCandidate> settlements = chooseSettlements(terrain, terrainJson, seed, settlementTarget);
    QVector<SettlementCandidate> capitals;
    for (int i = 0; i < qMin(politicalCenters, static_cast<int>(settlements.size())); ++i) capitals.append(settlements.at(i));

    const double docW = pilinNow.value(QStringLiteral("width")).toDouble(4096.0);
    const double docH = pilinNow.value(QStringLiteral("height")).toDouble(2304.0);
    const QJsonArray flow = terrainJson.value(QStringLiteral("flow")).toArray();
    const QJsonArray biomeIds = terrainJson.value(QStringLiteral("biomeIds")).toArray();

    mutateDocument([&](QJsonObject& pilin) {
        QJsonObject model = pilin.value(QStringLiteral("terrainModel")).toObject();
        model.insert(QStringLiteral("regions"), buildRegions(terrain, capitals, seaLevel));
        QJsonArray regionNames;
        for (int i = 0; i < capitals.size(); ++i) regionNames.append(tr("Dominio de %1").arg(placeName(i, seed)));
        model.insert(QStringLiteral("regionNames"), regionNames);
        pilin.insert(QStringLiteral("terrainModel"), model);

        QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();
        const int settlementsIndex = ensureLayer(layers, tr("Asentamientos"));
        const int routesIndex = ensureLayer(layers, tr("Rutas"));

        QJsonObject settlementLayer = layers.at(settlementsIndex).toObject();
        QJsonArray settlementObjects = manualOnly(settlementLayer.value(QStringLiteral("objects")).toArray());
        for (int i = 0; i < settlements.size(); ++i) {
            const SettlementCandidate& s = settlements.at(i);
            const int cell = s.y * terrain.width() + s.x;
            const double riverFlow = cell < flow.size() ? flow.at(cell).toDouble() : 0.0;
            const int biome = cell < biomeIds.size() ? biomeIds.at(cell).toInt() : 0;
            int water = 0;
            for (int oy = -2; oy <= 2; ++oy)
                for (int ox = -2; ox <= 2; ++ox)
                    if (terrain.at(s.x + ox, s.y + oy) < seaLevel) ++water;
            const bool capital = i < capitals.size();
            const bool port = water >= 3;
            const bool riverCity = riverFlow > 22.0;
            const QString kind = capital ? tr("Capital") : port ? tr("Puerto") : riverCity ? tr("Ciudad") : tr("Pueblo");
            const int population = static_cast<int>((capital ? 25000.0 : 2800.0)
                * (.55 + s.score * .42 + noise01(i, 31, seed) * .55));

            settlementObjects.append(QJsonObject{
                {QStringLiteral("id"), generatedId(QStringLiteral("settlement"))},
                {QStringLiteral("type"), QStringLiteral("settlement")},
                {QStringLiteral("label"), placeName(i, seed)},
                {QStringLiteral("settlementType"), kind},
                {QStringLiteral("population"), population},
                {QStringLiteral("biomeId"), biome},
                {QStringLiteral("x"), static_cast<double>(s.x) / std::max(1, terrain.width() - 1) * docW},
                {QStringLiteral("y"), static_cast<double>(s.y) / std::max(1, terrain.height() - 1) * docH},
                {QStringLiteral("generated"), true}
            });
        }
        settlementLayer.insert(QStringLiteral("objects"), settlementObjects);
        layers.replace(settlementsIndex, settlementLayer);

        QJsonObject routesLayer = layers.at(routesIndex).toObject();
        QJsonArray routes = manualOnly(routesLayer.value(QStringLiteral("objects")).toArray());
        for (int i = 1; i < settlements.size(); ++i) {
            int nearest = 0;
            double nearestDistance = std::numeric_limits<double>::max();
            for (int j = 0; j < i; ++j) {
                const double distance = std::hypot(settlements.at(i).x - settlements.at(j).x,
                                                   settlements.at(i).y - settlements.at(j).y);
                if (distance < nearestDistance) {
                    nearestDistance = distance;
                    nearest = j;
                }
            }
            if (nearestDistance > 70.0 && i >= capitals.size()) continue;
            routes.append(QJsonObject{
                {QStringLiteral("id"), generatedId(QStringLiteral("route"))},
                {QStringLiteral("type"), QStringLiteral("road")},
                {QStringLiteral("points"), routePolyline(settlements.at(i), settlements.at(nearest),
                                                          docW, docH, terrain.width(), terrain.height(), seed, i)},
                {QStringLiteral("width"), i < capitals.size() ? 4.0 : 2.6},
                {QStringLiteral("generated"), true}
            });
        }
        routesLayer.insert(QStringLiteral("objects"), routes);
        layers.replace(routesIndex, routesLayer);
        pilin.insert(QStringLiteral("layers"), layers);
    });

    if (status_) status_->setText(tr("Azgaar · %1 · mundo editable generado").arg(templateId));
}

} // namespace wbw
