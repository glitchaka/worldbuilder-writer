#include "ui/RcVisualFixture.h"

#include "ui/MapTerrainEngine.h"
#include "ui/PilinReyEditor.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QMainWindow>

namespace wbw {
namespace {

QJsonObject p(double x, double y, double radius = 0.0) {
    QJsonObject value{{QStringLiteral("x"), x}, {QStringLiteral("y"), y}};
    if (radius > 0.0) value.insert(QStringLiteral("radius"), radius);
    return value;
}

QJsonObject layer(const QString& id, const QString& name, const QJsonArray& objects) {
    return QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("name"), name}, {QStringLiteral("visible"), true}, {QStringLiteral("locked"), false}, {QStringLiteral("opacity"), 1.0}, {QStringLiteral("objects"), objects}};
}

QJsonObject pathObject(const QString& id, const QString& type, const QJsonArray& points, int width = 5) {
    return QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("type"), type}, {QStringLiteral("points"), points}, {QStringLiteral("width"), width}, {QStringLiteral("density"), 64}, {QStringLiteral("symbolSize"), 68}};
}

QJsonArray scaledRiver(const QJsonArray& normalized, double width, double height) {
    QJsonArray points;
    for (const QJsonValue& value : normalized) {
        const QJsonObject q = value.toObject();
        points.append(p(q.value(QStringLiteral("x")).toDouble() * width, q.value(QStringLiteral("y")).toDouble() * height));
    }
    return points;
}

} // namespace

void applyRcVisualFixture(QMainWindow* window) {
    if (!window || window->property("wbwRcVisualFixture").toBool()) return;
    window->setProperty("wbwRcVisualFixture", true);
    auto* mapEditor = window->findChild<PilinReyEditor*>(QStringLiteral("pilinReyEditor"));
    if (!mapEditor) return;

    constexpr double docW = 4096.0;
    constexpr double docH = 2304.0;
    MapTerrainEngine terrain(320, 180, 19770817u);
    terrain.generateWorld(3, 16, .62f);
    terrain.addRange(QPointF(105, 52), QPointF(218, 98), 8.0f, .18f, .16f);
    terrain.erode(2, .17f);
    QJsonObject terrainModel = terrain.toJson();
    terrainModel.insert(QStringLiteral("seaLevel"), .42);
    terrainModel.insert(QStringLiteral("generator"), QStringLiteral("rc-heightmap-v3"));

    QJsonArray hydro;
    int riverIndex = 0;
    for (const QJsonValue& value : terrain.traceRivers(22, .42f)) {
        const QJsonArray points = scaledRiver(value.toArray(), docW, docH);
        if (points.size() < 4) continue;
        hydro.append(pathObject(QStringLiteral("river-%1").arg(++riverIndex), QStringLiteral("river"), points, 8));
    }

    QJsonArray roads;
    roads.append(pathObject(QStringLiteral("road-main"), QStringLiteral("road"), QJsonArray{p(920,1080),p(1220,1010),p(1560,1120),p(1870,1200),p(2180,1110),p(2510,980)}, 7));
    roads.append(pathObject(QStringLiteral("road-east"), QStringLiteral("road"), QJsonArray{p(2510,980),p(2780,1040),p(3020,1190)}, 6));

    QJsonArray borders;
    QJsonObject north = pathObject(QStringLiteral("region-north"), QStringLiteral("region"), QJsonArray{p(760,650),p(1490,430),p(2180,720),p(1990,1260),p(1200,1320),p(760,650)}, 4);
    north.insert(QStringLiteral("closed"), true);
    borders.append(north);
    borders.append(pathObject(QStringLiteral("border-east"), QStringLiteral("border"), QJsonArray{p(2260,520),p(2370,850),p(2310,1190),p(2450,1510)}, 4));

    QJsonArray settlements{
        QJsonObject{{QStringLiteral("id"), QStringLiteral("city-amber")}, {QStringLiteral("type"), QStringLiteral("settlement")}, {QStringLiteral("settlementType"), QStringLiteral("Capital")}, {QStringLiteral("label"), QStringLiteral("Puerto Ámbar")}, {QStringLiteral("group"), QStringLiteral("Principado de Thet")}, {QStringLiteral("population"), 38600}, {QStringLiteral("elevation"), 24}, {QStringLiteral("x"), 1710}, {QStringLiteral("y"), 1320}, {QStringLiteral("size"), 92}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("city-north")}, {QStringLiteral("type"), QStringLiteral("settlement")}, {QStringLiteral("settlementType"), QStringLiteral("Fortaleza")}, {QStringLiteral("label"), QStringLiteral("Valdoria")}, {QStringLiteral("x"), 1280}, {QStringLiteral("y"), 820}, {QStringLiteral("size"), 72}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("city-east")}, {QStringLiteral("type"), QStringLiteral("settlement")}, {QStringLiteral("settlementType"), QStringLiteral("Puerto")}, {QStringLiteral("label"), QStringLiteral("Iskanat")}, {QStringLiteral("x"), 2810}, {QStringLiteral("y"), 1090}, {QStringLiteral("size"), 70}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("stamp-ship")}, {QStringLiteral("type"), QStringLiteral("stamp")}, {QStringLiteral("assetKind"), QStringLiteral("ship")}, {QStringLiteral("x"), 2270}, {QStringLiteral("y"), 1680}, {QStringLiteral("rotation"), -8.0}, {QStringLiteral("size"), 210}, {QStringLiteral("opacity"), .9}}
    };

    QJsonArray labels{
        QJsonObject{{QStringLiteral("id"), QStringLiteral("label-thet")}, {QStringLiteral("type"), QStringLiteral("label")}, {QStringLiteral("text"), QStringLiteral("PRINCIPADO DE THET")}, {QStringLiteral("x"), 1050}, {QStringLiteral("y"), 560}, {QStringLiteral("fontSize"), 64}, {QStringLiteral("rotation"), -4.0}, {QStringLiteral("bold"), true}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("label-sea")}, {QStringLiteral("type"), QStringLiteral("label")}, {QStringLiteral("text"), QStringLiteral("Mar de Oaos")}, {QStringLiteral("x"), 2260}, {QStringLiteral("y"), 1880}, {QStringLiteral("fontSize"), 58}, {QStringLiteral("rotation"), -8.0}, {QStringLiteral("bold"), false}}
    };

    const QJsonArray layers{
        layer(QStringLiteral("terrain"), QStringLiteral("Terreno"), QJsonArray()),
        layer(QStringLiteral("hydro"), QStringLiteral("Ríos y lagos"), hydro),
        layer(QStringLiteral("borders"), QStringLiteral("Fronteras y regiones"), borders),
        layer(QStringLiteral("roads"), QStringLiteral("Rutas"), roads),
        layer(QStringLiteral("places"), QStringLiteral("Asentamientos"), settlements),
        layer(QStringLiteral("labels"), QStringLiteral("Etiquetas"), labels)
    };

    QJsonObject pilin{
        {QStringLiteral("version"), 8}, {QStringLiteral("width"), static_cast<int>(docW)}, {QStringLiteral("height"), static_cast<int>(docH)},
        {QStringLiteral("activeLayerId"), QStringLiteral("terrain")}, {QStringLiteral("terrainModel"), terrainModel}, {QStringLiteral("layers"), layers}, {QStringLiteral("assets"), QJsonArray()},
        {QStringLiteral("theme"), QJsonObject{{QStringLiteral("land"), QStringLiteral("#cdbb86")}, {QStringLiteral("sea"), QStringLiteral("#769baa")}, {QStringLiteral("coast"), QStringLiteral("#4a4033")}, {QStringLiteral("river"), QStringLiteral("#295f7d")}, {QStringLiteral("road"), QStringLiteral("#81562f")}, {QStringLiteral("border"), QStringLiteral("#744d58")}, {QStringLiteral("forest"), QStringLiteral("#426445")}, {QStringLiteral("mountain"), QStringLiteral("#5d5146")}, {QStringLiteral("region"), QStringLiteral("#8b6c56")}, {QStringLiteral("symbol"), QStringLiteral("#302820")}, {QStringLiteral("text"), QStringLiteral("#29251f")}, {QStringLiteral("labelOutline"), QStringLiteral("#eadfbe")}}}
    };

    mapEditor->setMap(QJsonObject{{QStringLiteral("id"), QStringLiteral("rc-map")}, {QStringLiteral("name"), QStringLiteral("Puerto Ámbar y el golfo de Oaos")}, {QStringLiteral("pilinRey"), pilin}});
}

} // namespace wbw
