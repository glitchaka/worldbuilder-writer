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
#include <cmath>

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

quint32 hashCell(int x, int y, quint32 seed) {
    quint32 h = seed ^ static_cast<quint32>(x * 374761393u) ^ static_cast<quint32>(y * 668265263u);
    h = (h ^ (h >> 13u)) * 1274126177u;
    return h ^ (h >> 16u);
}

double noise01(int x, int y, quint32 seed) {
    return static_cast<double>(hashCell(x, y, seed) & 0xffffu) / 65535.0;
}

QString generatedPlaceName(int index, quint32 seed) {
    static const QStringList starts{QStringLiteral("Val"),QStringLiteral("Ar"),QStringLiteral("Bel"),QStringLiteral("Cor"),QStringLiteral("Dor"),QStringLiteral("Eri"),QStringLiteral("Fal"),QStringLiteral("Gal"),QStringLiteral("Ira"),QStringLiteral("Lun"),QStringLiteral("Mor"),QStringLiteral("Nor"),QStringLiteral("Or"),QStringLiteral("Ria"),QStringLiteral("Sol"),QStringLiteral("Tor"),QStringLiteral("Vel")};
    static const QStringList ends{QStringLiteral("oria"),QStringLiteral("en"),QStringLiteral("ar"),QStringLiteral("is"),QStringLiteral("ona"),QStringLiteral("eth"),QStringLiteral("ia"),QStringLiteral("or"),QStringLiteral("una"),QStringLiteral("as"),QStringLiteral("el"),QStringLiteral("in")};
    const quint32 h = hashCell(index, index * 7 + 3, seed);
    return starts.at(static_cast<int>(h % starts.size())) + ends.at(static_cast<int>((h >> 8u) % ends.size()));
}

int ensureLayer(QJsonArray& layers, const QString& name) {
    for (int i = 0; i < layers.size(); ++i)
        if (layers.at(i).toObject().value(QStringLiteral("name")).toString().compare(name, Qt::CaseInsensitive) == 0) return i;
    layers.append(QJsonObject{{QStringLiteral("id"), generatedId(QStringLiteral("layer"))},
                              {QStringLiteral("name"), name},
                              {QStringLiteral("visible"), true},
                              {QStringLiteral("locked"), false},
                              {QStringLiteral("opacity"), 1.0},
                              {QStringLiteral("objects"), QJsonArray()}});
    return layers.size() - 1;
}

QJsonArray buildBiomes(const MapTerrainEngine& terrain, quint32 seed, double seaLevel) {
    QJsonArray biomes;
    const int width = terrain.width();
    const int height = terrain.height();
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const double h = terrain.at(x, y);
            int biome = 0; // water
            if (h >= seaLevel) {
                const double latitude = std::abs((static_cast<double>(y) / std::max(1, height - 1)) * 2.0 - 1.0);
                const double moisture = noise01(x / 4, y / 4, seed ^ 0x91e10da5u) * .68 + noise01(x / 11, y / 11, seed ^ 0x7f4a7c15u) * .32;
                if (h > .84) biome = 6;              // snow / alpine
                else if (h > .72) biome = 5;         // mountain scrub
                else if (latitude > .78) biome = 7;  // tundra
                else if (moisture > .68) biome = 2;  // dense forest
                else if (moisture > .52) biome = 3;  // woodland
                else if (moisture < .25 && latitude < .62) biome = 4; // arid
                else biome = 1;                      // grassland / plains
            }
            biomes.append(biome);
        }
    }
    return biomes;
}

QVector<QPointF> chooseSettlements(const MapTerrainEngine& terrain, quint32 seed, double seaLevel, int count) {
    QVector<QPointF> result;
    const int w = terrain.width(), h = terrain.height();
    struct Candidate { int x; int y; double score; };
    QVector<Candidate> candidates;
    for (int y = 4; y < h - 4; y += 3) {
        for (int x = 4; x < w - 4; x += 3) {
            const double elevation = terrain.at(x, y);
            if (elevation < seaLevel + .025 || elevation > .78) continue;
            int waterNeighbors = 0;
            for (int oy=-2; oy<=2; ++oy) for (int ox=-2; ox<=2; ++ox)
                if (terrain.at(x+ox,y+oy) < seaLevel) ++waterNeighbors;
            const double coastBonus = std::min(1.0, waterNeighbors / 8.0);
            const double lowland = 1.0 - std::abs(elevation - .52) * 2.1;
            const double jitter = noise01(x, y, seed ^ 0xa24baed5u) * .5;
            candidates.append({x,y,coastBonus*.7 + lowland*.5 + jitter});
        }
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a,const Candidate& b){return a.score>b.score;});
    for (const Candidate& c : candidates) {
        if (result.size() >= count) break;
        bool near=false;
        for(const QPointF& p:result) if(std::hypot(p.x()-c.x,p.y()-c.y) < 18.0){near=true;break;}
        if(!near) result.append(QPointF(c.x,c.y));
    }
    return result;
}

QJsonArray roadPoints(const QPointF& a,const QPointF& b,double docW,double docH,int terrainW,int terrainH,quint32 seed,int salt) {
    QJsonArray points;
    const int steps = 8;
    const QPointF delta=b-a;
    const double length=std::hypot(delta.x(),delta.y());
    const QPointF normal = length>0.001 ? QPointF(-delta.y()/length,delta.x()/length) : QPointF();
    for(int i=0;i<=steps;++i){
        const double t=i/(double)steps;
        const double bend=(noise01(i,salt,seed^0x243f6a88u)-.5)*3.2*std::sin(t*3.14159265);
        const QPointF p=a+delta*t+normal*bend;
        points.append(QJsonObject{{QStringLiteral("x"),p.x()/std::max(1,terrainW-1)*docW},{QStringLiteral("y"),p.y()/std::max(1,terrainH-1)*docH}});
    }
    return points;
}
}

void PilinReyEditor::generateProceduralWorld() {
    if (map_.isEmpty()) return;
    bool ok = false;
    const int seed = QInputDialog::getInt(this, tr("Generar mundo"), tr("Semilla"), static_cast<int>(QDateTime::currentMSecsSinceEpoch() & 0x7fffffff), 1, 2147483647, 1, &ok);
    if (!ok) return;
    const int continents = QInputDialog::getInt(this, tr("Generar mundo"), tr("Continentes principales"), 3, 1, 8, 1, &ok);
    if (!ok) return;
    const int islands = QInputDialog::getInt(this, tr("Generar mundo"), tr("Islas secundarias"), 12, 0, 40, 1, &ok);
    if (!ok) return;
    if (QMessageBox::question(this, tr("Generar mundo"), tr("Se reemplazarán terreno, ríos, caminos y asentamientos procedurales. Los objetos creados manualmente se conservarán. ¿Continuar?")) != QMessageBox::Yes) return;

    pushUndo();
    QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
    const double docW = pilin.value(QStringLiteral("width")).toDouble(4096.0);
    const double docH = pilin.value(QStringLiteral("height")).toDouble(2304.0);
    constexpr double seaLevel = 0.42;

    MapTerrainEngine terrain(320, 180, static_cast<quint32>(seed));
    terrain.generateWorld(continents, islands, 0.62f);
    terrain.erode(2, 0.18f);
    QJsonObject terrainJson = terrain.toJson();
    terrainJson.insert(QStringLiteral("seaLevel"), seaLevel);
    terrainJson.insert(QStringLiteral("generatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    terrainJson.insert(QStringLiteral("generator"), QStringLiteral("native-heightmap-v3"));
    terrainJson.insert(QStringLiteral("biomes"), buildBiomes(terrain, static_cast<quint32>(seed), seaLevel));
    pilin.insert(QStringLiteral("terrainModel"), terrainJson);

    QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();
    const int hydroIndex = ensureLayer(layers, tr("Ríos y lagos"));
    const int routesIndex = ensureLayer(layers, tr("Rutas"));
    const int settlementIndex = ensureLayer(layers, tr("Asentamientos"));

    QJsonObject hydro = layers.at(hydroIndex).toObject();
    QJsonArray hydroObjects = keepManualObjects(hydro.value(QStringLiteral("objects")).toArray());
    const QJsonArray riverNetwork = terrain.traceRivers(qBound(8, continents * 8 + islands / 3, 34), seaLevel);
    for (const QJsonValue& riverValue : riverNetwork) {
        QJsonArray points;
        for (const QJsonValue& pointValue : riverValue.toArray()) {
            const QJsonObject point = pointValue.toObject();
            points.append(QJsonObject{{QStringLiteral("x"), point.value(QStringLiteral("x")).toDouble() * docW}, {QStringLiteral("y"), point.value(QStringLiteral("y")).toDouble() * docH}});
        }
        if (points.size() >= 4) hydroObjects.append(QJsonObject{{QStringLiteral("id"), generatedId(QStringLiteral("river"))},{QStringLiteral("type"), QStringLiteral("river")},{QStringLiteral("points"), points},{QStringLiteral("width"), 4.0},{QStringLiteral("generated"), true}});
    }
    hydro.insert(QStringLiteral("objects"),hydroObjects); layers.replace(hydroIndex,hydro);

    const QVector<QPointF> settlements = chooseSettlements(terrain, static_cast<quint32>(seed), seaLevel, qBound(6, continents * 5 + islands / 4, 24));
    QJsonObject settlementLayer = layers.at(settlementIndex).toObject();
    QJsonArray settlementObjects = keepManualObjects(settlementLayer.value(QStringLiteral("objects")).toArray());
    for (int i=0;i<settlements.size();++i) {
        const QPointF p=settlements.at(i); const double elevation=terrain.at(static_cast<int>(p.x()),static_cast<int>(p.y()));
        settlementObjects.append(QJsonObject{{QStringLiteral("id"),generatedId(QStringLiteral("settlement"))},{QStringLiteral("type"),QStringLiteral("settlement")},{QStringLiteral("x"),p.x()/std::max(1,terrain.width()-1)*docW},{QStringLiteral("y"),p.y()/std::max(1,terrain.height()-1)*docH},{QStringLiteral("label"),generatedPlaceName(i,static_cast<quint32>(seed))},{QStringLiteral("settlementType"),i<continents?tr("Capital"):tr("Ciudad")},{QStringLiteral("population"),1200+static_cast<int>(noise01(i,19,seed)*42000)},{QStringLiteral("elevation"),static_cast<int>(elevation*1800)},{QStringLiteral("size"),i<continents?88:64},{QStringLiteral("generated"),true}});
    }
    settlementLayer.insert(QStringLiteral("objects"),settlementObjects); layers.replace(settlementIndex,settlementLayer);

    QJsonObject routes = layers.at(routesIndex).toObject();
    QJsonArray routeObjects = keepManualObjects(routes.value(QStringLiteral("objects")).toArray());
    for(int i=1;i<settlements.size();++i) {
        int nearest=0; double nearestDistance=1e12;
        for(int j=0;j<i;++j){const double d=std::hypot(settlements.at(i).x()-settlements.at(j).x(),settlements.at(i).y()-settlements.at(j).y());if(d<nearestDistance){nearestDistance=d;nearest=j;}}
        if(nearestDistance>95.0) continue;
        routeObjects.append(QJsonObject{{QStringLiteral("id"),generatedId(QStringLiteral("road"))},{QStringLiteral("type"),QStringLiteral("road")},{QStringLiteral("points"),roadPoints(settlements.at(i),settlements.at(nearest),docW,docH,terrain.width(),terrain.height(),static_cast<quint32>(seed),i)},{QStringLiteral("generated"),true}});
    }
    routes.insert(QStringLiteral("objects"),routeObjects); layers.replace(routesIndex,routes);

    pilin.insert(QStringLiteral("layers"), layers);
    map_.insert(QStringLiteral("pilinRey"), pilin);
    selectedObjectIds_.clear(); redoStack_.clear(); refreshLayers(); refreshSelectionControls(); refreshViewport(); persistToArchive(); emit mapEdited(map_);
}

} // namespace wbw
