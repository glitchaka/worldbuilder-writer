#include "ui/RcVisualFixture.h"

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
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("visible"), true},
        {QStringLiteral("locked"), false},
        {QStringLiteral("opacity"), 1.0},
        {QStringLiteral("objects"), objects}
    };
}

QJsonObject pathObject(const QString& id, const QString& type, const QJsonArray& points, int width = 5) {
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("type"), type},
        {QStringLiteral("points"), points},
        {QStringLiteral("width"), width},
        {QStringLiteral("density"), 64},
        {QStringLiteral("symbolSize"), 68}
    };
}

} // namespace

void applyRcVisualFixture(QMainWindow* window) {
    if (!window || window->property("wbwRcVisualFixture").toBool()) return;
    window->setProperty("wbwRcVisualFixture", true);

    auto* mapEditor = window->findChild<PilinReyEditor*>(QStringLiteral("pilinReyEditor"));
    if (!mapEditor) return;

    QJsonArray terrain;
    terrain.append(pathObject(QStringLiteral("land-main"), QStringLiteral("land"), QJsonArray{
        p(730, 720, 360), p(1060, 620, 430), p(1460, 700, 410), p(1820, 900, 380),
        p(2050, 1220, 330), p(1830, 1510, 360), p(1440, 1600, 430), p(1020, 1500, 390), p(760, 1180, 350)
    }));
    terrain.append(pathObject(QStringLiteral("land-east"), QStringLiteral("land"), QJsonArray{
        p(2580, 820, 280), p(2870, 740, 310), p(3160, 900, 300), p(3090, 1220, 260), p(2760, 1300, 300)
    }));

    QJsonArray relief;
    relief.append(pathObject(QStringLiteral("mountains"), QStringLiteral("mountainArea"), QJsonArray{
        p(1180, 820, 260), p(1400, 920, 250), p(1600, 1030, 230), p(1780, 1130, 210)
    }));
    relief.append(pathObject(QStringLiteral("forest-west"), QStringLiteral("forestArea"), QJsonArray{
        p(860, 1160, 250), p(1050, 1280, 240), p(1260, 1350, 230)
    }));
    relief.append(pathObject(QStringLiteral("forest-east"), QStringLiteral("forestArea"), QJsonArray{
        p(2720, 980, 200), p(2920, 1050, 190)
    }));

    QJsonArray hydro;
    hydro.append(pathObject(QStringLiteral("river-main"), QStringLiteral("river"), QJsonArray{
        p(1490, 760), p(1570, 940), p(1500, 1110), p(1610, 1270), p(1700, 1490)
    }, 12));
    hydro.append(pathObject(QStringLiteral("river-east"), QStringLiteral("river"), QJsonArray{
        p(2840, 780), p(2890, 920), p(2830, 1070), p(2780, 1260)
    }, 8));

    QJsonArray roads;
    roads.append(pathObject(QStringLiteral("road-main"), QStringLiteral("road"), QJsonArray{
        p(940, 1080), p(1280, 1050), p(1580, 1120), p(1940, 1190), p(2240, 1150)
    }, 10));

    QJsonArray borders;
    QJsonObject region = pathObject(QStringLiteral("region-north"), QStringLiteral("region"), QJsonArray{
        p(930, 760), p(1410, 560), p(1980, 790), p(1880, 1170), p(1260, 1210), p(930, 760)
    }, 5);
    region.insert(QStringLiteral("closed"), true);
    region.insert(QStringLiteral("fillOpacity"), 0.10);
    borders.append(region);
    borders.append(pathObject(QStringLiteral("border-east"), QStringLiteral("border"), QJsonArray{
        p(2360, 620), p(2440, 900), p(2390, 1210), p(2470, 1510)
    }, 5));

    QJsonArray settlements{
        QJsonObject{{QStringLiteral("id"), QStringLiteral("city-amber")}, {QStringLiteral("type"), QStringLiteral("settlement")}, {QStringLiteral("kind"), QStringLiteral("Capital")}, {QStringLiteral("label"), QStringLiteral("Puerto Ámbar")}, {QStringLiteral("x"), 1710}, {QStringLiteral("y"), 1320}, {QStringLiteral("scale"), 1.35}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("city-north")}, {QStringLiteral("type"), QStringLiteral("settlement")}, {QStringLiteral("kind"), QStringLiteral("Fortaleza")}, {QStringLiteral("label"), QStringLiteral("Valdoria")}, {QStringLiteral("x"), 1280}, {QStringLiteral("y"), 820}, {QStringLiteral("scale"), 1.0}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("city-east")}, {QStringLiteral("type"), QStringLiteral("settlement")}, {QStringLiteral("kind"), QStringLiteral("Puerto")}, {QStringLiteral("label"), QStringLiteral("Iskanat")}, {QStringLiteral("x"), 2810}, {QStringLiteral("y"), 1090}, {QStringLiteral("scale"), 1.0}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("stamp-compass")}, {QStringLiteral("type"), QStringLiteral("stamp")}, {QStringLiteral("assetKind"), QStringLiteral("compass")}, {QStringLiteral("x"), 3440}, {QStringLiteral("y"), 1760}, {QStringLiteral("scale"), 1.0}, {QStringLiteral("rotation"), 0.0}, {QStringLiteral("size"), 260}, {QStringLiteral("opacity"), 0.9}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("stamp-ship")}, {QStringLiteral("type"), QStringLiteral("stamp")}, {QStringLiteral("assetKind"), QStringLiteral("ship")}, {QStringLiteral("x"), 2270}, {QStringLiteral("y"), 1680}, {QStringLiteral("scale"), 1.0}, {QStringLiteral("rotation"), -8.0}, {QStringLiteral("size"), 210}, {QStringLiteral("opacity"), 0.9}}
    };

    QJsonArray labels{
        QJsonObject{{QStringLiteral("id"), QStringLiteral("label-thet")}, {QStringLiteral("type"), QStringLiteral("label")}, {QStringLiteral("text"), QStringLiteral("PRINCIPADO DE THET")}, {QStringLiteral("x"), 1240}, {QStringLiteral("y"), 560}, {QStringLiteral("fontSize"), 72}, {QStringLiteral("rotation"), -4.0}, {QStringLiteral("bold"), true}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("label-sea")}, {QStringLiteral("type"), QStringLiteral("label")}, {QStringLiteral("text"), QStringLiteral("Mar de Oaos")}, {QStringLiteral("x"), 2250}, {QStringLiteral("y"), 1880}, {QStringLiteral("fontSize"), 62}, {QStringLiteral("rotation"), -8.0}, {QStringLiteral("bold"), false}}
    };

    const QJsonArray layers{
        layer(QStringLiteral("terrain"), QStringLiteral("Terreno"), terrain),
        layer(QStringLiteral("hydro"), QStringLiteral("Hidrografía"), hydro),
        layer(QStringLiteral("relief"), QStringLiteral("Relieve y vegetación"), relief),
        layer(QStringLiteral("borders"), QStringLiteral("Fronteras y regiones"), borders),
        layer(QStringLiteral("roads"), QStringLiteral("Caminos"), roads),
        layer(QStringLiteral("places"), QStringLiteral("Asentamientos y assets"), settlements),
        layer(QStringLiteral("labels"), QStringLiteral("Etiquetas"), labels)
    };

    QJsonObject pilin{
        {QStringLiteral("version"), 7},
        {QStringLiteral("width"), 4096},
        {QStringLiteral("height"), 2304},
        {QStringLiteral("activeLayerId"), QStringLiteral("terrain")},
        {QStringLiteral("layers"), layers},
        {QStringLiteral("assets"), QJsonArray()},
        {QStringLiteral("theme"), QJsonObject{
            {QStringLiteral("land"), QStringLiteral("#d7c799")},
            {QStringLiteral("sea"), QStringLiteral("#aebfbe")},
            {QStringLiteral("coast"), QStringLiteral("#4a4033")},
            {QStringLiteral("river"), QStringLiteral("#254f6a")},
            {QStringLiteral("road"), QStringLiteral("#7a5431")},
            {QStringLiteral("border"), QStringLiteral("#8c443b")},
            {QStringLiteral("forest"), QStringLiteral("#385b38")},
            {QStringLiteral("mountain"), QStringLiteral("#51483e")},
            {QStringLiteral("region"), QStringLiteral("#a76d44")},
            {QStringLiteral("symbol"), QStringLiteral("#302820")},
            {QStringLiteral("text"), QStringLiteral("#29251f")},
            {QStringLiteral("labelOutline"), QStringLiteral("#eadfbe")}
        }}
    };

    mapEditor->setMap(QJsonObject{
        {QStringLiteral("id"), QStringLiteral("rc-map")},
        {QStringLiteral("name"), QStringLiteral("Puerto Ámbar y el golfo de Oaos")},
        {QStringLiteral("pilinRey"), pilin}
    });
}

} // namespace wbw
