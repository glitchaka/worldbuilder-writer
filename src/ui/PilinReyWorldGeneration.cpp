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
#include <limits>
#include <queue>

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

QString regionNameForCapital(const QString& capital, int index) {
    static const QStringList forms{QObject::tr("Reino de %1"), QObject::tr("Principado de %1"), QObject::tr("Marca de %1"), QObject::tr("Dominio de %1"), QObject::tr("Liga de %1")};
    return forms.at(index % forms.size()).arg(capital);
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
            int biome = 0;
            if (h >= seaLevel) {
                const double latitude = std::abs((static_cast<double>(y) / std::max(1, height - 1)) * 2.0 - 1.0);
                const double moisture = noise01(x / 4, y / 4, seed ^ 0x91e10da5u) * .68 + noise01(x / 11, y / 11, seed ^ 0x7f4a7c15u) * .32;
                if (h > .84) biome = 6;
                else if (h > .72) biome = 5;
                else if (latitude > .78) biome = 7;
                else if (moisture > .68) biome = 2;
                else if (moisture > .52) biome = 3;
                else if (moisture < .25 && latitude < .62) biome = 4;
                else biome = 1;
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

QJsonArray buildPoliticalRegions(const MapTerrainEngine& terrain, const QVector<QPointF>& capitals, double seaLevel) {
    QJsonArray regions;
    const int w = terrain.width(), h = terrain.height();
    if (capitals.isEmpty()) {
        for (int i=0;i<w*h;++i) regions.append(-1);
        return regions;
    }
    for(int y=0;y<h;++y){
        for(int x=0;x<w;++x){
            const double elevation=terrain.at(x,y);
            if(elevation<seaLevel){regions.append(-1);continue;}
            int best=0;double bestScore=std::numeric_limits<double>::max();
            for(int r=0;r<capitals.size();++r){
                const double dx=x-capitals.at(r).x(),dy=y-capitals.at(r).y();
                double score=dx*dx+dy*dy;
                const double ridgePenalty=std::max(0.0,elevation-.68)*900.0;
                score += ridgePenalty;
                if(score<bestScore){bestScore=score;best=r;}
            }
            regions.append(best);
        }
    }
    return regions;
}

QJsonArray terrainAwareRoad(const QPointF& start, const QPointF& goal, const MapTerrainEngine& terrain, double seaLevel, double docW, double docH) {
    const int w=terrain.width(),h=terrain.height(),count=w*h;
    auto idx=[w](int x,int y){return y*w+x;};
    const int sx=std::clamp(static_cast<int>(std::round(start.x())),0,w-1),sy=std::clamp(static_cast<int>(std::round(start.y())),0,h-1);
    const int gx=std::clamp(static_cast<int>(std::round(goal.x())),0,w-1),gy=std::clamp(static_cast<int>(std::round(goal.y())),0,h-1);
    struct Node{double f;int index;bool operator<(const Node& other)const{return f>other.f;}};
    std::priority_queue<Node> open;
    QVector<double> g(count,std::numeric_limits<double>::infinity());
    QVector<int> parent(count,-1);
    const int startIndex=idx(sx,sy),goalIndex=idx(gx,gy);g[startIndex]=0;open.push({std::hypot(gx-sx,gy-sy),startIndex});
    static const int dirs[8][2]={{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
    while(!open.empty()){
        const int current=open.top().index;open.pop();if(current==goalIndex)break;const int cx=current%w,cy=current/w;
        for(const auto& d:dirs){const int nx=cx+d[0],ny=cy+d[1];if(nx<1||ny<1||nx>=w-1||ny>=h-1)continue;const double nh=terrain.at(nx,ny);if(nh<seaLevel+.006)continue;const double ch=terrain.at(cx,cy);const double slope=std::abs(nh-ch);const double highland=std::max(0.0,nh-.62);const double step=(d[0]&&d[1])?1.414:1.0;const double cost=step*(1.0+slope*18.0+highland*5.0);const int ni=idx(nx,ny);const double tentative=g[current]+cost;if(tentative>=g[ni])continue;g[ni]=tentative;parent[ni]=current;const double heuristic=std::hypot(gx-nx,gy-ny);open.push({tentative+heuristic,ni});}
    }
    if(parent[goalIndex]<0)return {};
    QVector<QPointF> reversed;for(int current=goalIndex;current>=0;current=parent[current]){reversed.append(QPointF(current%w,current/w));if(current==startIndex)break;}
    std::reverse(reversed.begin(),reversed.end());QJsonArray points;
    for(int i=0;i<reversed.size();i+=qMax(1,reversed.size()/18)){const QPointF p=reversed.at(i);points.append(QJsonObject{{QStringLiteral("x"),p.x()/std::max(1,w-1)*docW},{QStringLiteral("y"),p.y()/std::max(1,h-1)*docH}});}if(!reversed.isEmpty()){const QPointF p=reversed.last();const QJsonObject last=points.isEmpty()?QJsonObject():points.last().toObject();const double x=p.x()/std::max(1,w-1)*docW,y=p.y()/std::max(1,h-1)*docH;if(last.value(QStringLiteral("x")).toDouble()!=x||last.value(QStringLiteral("y")).toDouble()!=y)points.append(QJsonObject{{QStringLiteral("x"),x},{QStringLiteral("y"),y}});}return points;
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
    if (QMessageBox::question(this, tr("Generar mundo"), tr("Se reemplazarán terreno, ríos, regiones, caminos y asentamientos procedurales. Los objetos creados manualmente se conservarán. ¿Continuar?")) != QMessageBox::Yes) return;

    pushUndo();
    QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
    const double docW = pilin.value(QStringLiteral("width")).toDouble(4096.0);
    const double docH = pilin.value(QStringLiteral("height")).toDouble(2304.0);
    constexpr double seaLevel = 0.42;

    MapTerrainEngine terrain(320, 180, static_cast<quint32>(seed));
    terrain.generateWorld(continents, islands, 0.62f);
    terrain.erode(2, 0.18f);
    const QVector<QPointF> settlements = chooseSettlements(terrain, static_cast<quint32>(seed), seaLevel, qBound(6, continents * 5 + islands / 4, 24));
    QVector<QPointF> capitals;
    for(int i=0;i<qMin(continents,settlements.size());++i)capitals.append(settlements.at(i));

    QJsonObject terrainJson = terrain.toJson();
    terrainJson.insert(QStringLiteral("seaLevel"), seaLevel);
    terrainJson.insert(QStringLiteral("generatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    terrainJson.insert(QStringLiteral("generator"), QStringLiteral("native-heightmap-v4"));
    terrainJson.insert(QStringLiteral("biomes"), buildBiomes(terrain, static_cast<quint32>(seed), seaLevel));
    terrainJson.insert(QStringLiteral("regions"), buildPoliticalRegions(terrain, capitals, seaLevel));
    QJsonArray regionNames;
    for(int i=0;i<capitals.size();++i)regionNames.append(regionNameForCapital(generatedPlaceName(i,static_cast<quint32>(seed)),i));
    terrainJson.insert(QStringLiteral("regionNames"),regionNames);
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

    QStringList placeNames;
    for(int i=0;i<settlements.size();++i)placeNames.append(generatedPlaceName(i,static_cast<quint32>(seed)));
    QJsonObject settlementLayer = layers.at(settlementIndex).toObject();
    QJsonArray settlementObjects = keepManualObjects(settlementLayer.value(QStringLiteral("objects")).toArray());
    for (int i=0;i<settlements.size();++i) {
        const QPointF p=settlements.at(i); const double elevation=terrain.at(static_cast<int>(p.x()),static_cast<int>(p.y()));
        settlementObjects.append(QJsonObject{{QStringLiteral("id"),generatedId(QStringLiteral("settlement"))},{QStringLiteral("type"),QStringLiteral("settlement")},{QStringLiteral("x"),p.x()/std::max(1,terrain.width()-1)*docW},{QStringLiteral("y"),p.y()/std::max(1,terrain.height()-1)*docH},{QStringLiteral("label"),placeNames.at(i)},{QStringLiteral("group"),i<capitals.size()?regionNames.at(i).toString():QString()},{QStringLiteral("settlementType"),i<continents?tr("Capital"):tr("Ciudad")},{QStringLiteral("population"),1200+static_cast<int>(noise01(i,19,seed)*42000)},{QStringLiteral("elevation"),static_cast<int>(elevation*1800)},{QStringLiteral("size"),i<continents?88:64},{QStringLiteral("generated"),true}});
    }
    settlementLayer.insert(QStringLiteral("objects"),settlementObjects); layers.replace(settlementIndex,settlementLayer);

    QJsonObject routes = layers.at(routesIndex).toObject();
    QJsonArray routeObjects = keepManualObjects(routes.value(QStringLiteral("objects")).toArray());
    for(int i=1;i<settlements.size();++i) {
        int nearest=0; double nearestDistance=1e12;
        for(int j=0;j<i;++j){const double d=std::hypot(settlements.at(i).x()-settlements.at(j).x(),settlements.at(i).y()-settlements.at(j).y());if(d<nearestDistance){nearestDistance=d;nearest=j;}}
        if(nearestDistance>110.0) continue;
        const QJsonArray points=terrainAwareRoad(settlements.at(i),settlements.at(nearest),terrain,seaLevel,docW,docH);
        if(points.size()<2)continue;
        routeObjects.append(QJsonObject{{QStringLiteral("id"),generatedId(QStringLiteral("road"))},{QStringLiteral("type"),QStringLiteral("road")},{QStringLiteral("points"),points},{QStringLiteral("generated"),true}});
    }
    routes.insert(QStringLiteral("objects"),routeObjects); layers.replace(routesIndex,routes);

    pilin.insert(QStringLiteral("layers"), layers);
    map_.insert(QStringLiteral("pilinRey"), pilin);
    selectedObjectIds_.clear(); redoStack_.clear(); refreshLayers(); refreshSelectionControls(); refreshViewport(); persistToArchive(); emit mapEdited(map_);
}

} // namespace wbw
