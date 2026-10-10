#include "ui/MapExporter.h"

#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QSize>
#include <QTemporaryDir>

#include <iostream>

namespace {

bool require(bool condition, const char* message) {
    if (condition) return true;
    std::cerr << "MapExporter smoke failure: " << message << '\n';
    return false;
}

QJsonObject point(double x, double y, double radius = 0.0) {
    QJsonObject p{{QStringLiteral("x"), x}, {QStringLiteral("y"), y}};
    if (radius > 0.0) p.insert(QStringLiteral("radius"), radius);
    return p;
}

QJsonObject sampleMap() {
    const QJsonArray land{
        point(120, 120, 115), point(240, 130, 120), point(360, 170, 130),
        point(470, 250, 140), point(400, 340, 135), point(260, 360, 145), point(135, 300, 130)
    };
    const QJsonArray eraseSea{point(415, 195, 42), point(430, 225, 45)};
    const QJsonArray river{point(260, 145), point(285, 210), point(320, 265), point(345, 330)};
    const QJsonArray road{point(165, 275), point(245, 245), point(330, 250), point(420, 290)};
    const QJsonArray border{point(230, 170), point(235, 245), point(245, 320)};
    const QJsonArray region{point(155, 165), point(300, 145), point(350, 250), point(260, 315), point(155, 165)};
    const QJsonArray forest{point(175, 205, 70), point(205, 225, 75), point(225, 255, 68)};
    const QJsonArray mountains{point(330, 180, 68), point(360, 205, 75), point(385, 235, 70)};

    const QJsonArray terrainObjects{
        QJsonObject{{QStringLiteral("id"), QStringLiteral("land-1")}, {QStringLiteral("type"), QStringLiteral("land")}, {QStringLiteral("points"), land}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("sea-1")}, {QStringLiteral("type"), QStringLiteral("sea")}, {QStringLiteral("points"), eraseSea}}
    };

    const QJsonArray geographyObjects{
        QJsonObject{{QStringLiteral("id"), QStringLiteral("region-1")}, {QStringLiteral("type"), QStringLiteral("region")}, {QStringLiteral("closed"), true}, {QStringLiteral("width"), 4}, {QStringLiteral("points"), region}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("river-1")}, {QStringLiteral("type"), QStringLiteral("river")}, {QStringLiteral("width"), 7}, {QStringLiteral("points"), river}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("road-1")}, {QStringLiteral("type"), QStringLiteral("road")}, {QStringLiteral("width"), 5}, {QStringLiteral("points"), road}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("border-1")}, {QStringLiteral("type"), QStringLiteral("border")}, {QStringLiteral("width"), 4}, {QStringLiteral("points"), border}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("forest-1")}, {QStringLiteral("type"), QStringLiteral("forestArea")}, {QStringLiteral("density"), 70}, {QStringLiteral("symbolSize"), 48}, {QStringLiteral("points"), forest}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("mountains-1")}, {QStringLiteral("type"), QStringLiteral("mountainArea")}, {QStringLiteral("density"), 65}, {QStringLiteral("symbolSize"), 58}, {QStringLiteral("points"), mountains}}
    };

    const QJsonArray annotationObjects{
        QJsonObject{{QStringLiteral("id"), QStringLiteral("capital-1")}, {QStringLiteral("type"), QStringLiteral("settlement")}, {QStringLiteral("kind"), QStringLiteral("Capital")}, {QStringLiteral("label"), QStringLiteral("Puerto Ámbar")}, {QStringLiteral("x"), 275.0}, {QStringLiteral("y"), 265.0}, {QStringLiteral("scale"), 1.2}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("port-1")}, {QStringLiteral("type"), QStringLiteral("settlement")}, {QStringLiteral("kind"), QStringLiteral("Puerto")}, {QStringLiteral("label"), QStringLiteral("Bajamar")}, {QStringLiteral("x"), 420.0}, {QStringLiteral("y"), 300.0}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("stamp-castle")}, {QStringLiteral("type"), QStringLiteral("stamp")}, {QStringLiteral("assetKind"), QStringLiteral("castle")}, {QStringLiteral("x"), 200.0}, {QStringLiteral("y"), 180.0}, {QStringLiteral("size"), 42.0}, {QStringLiteral("scale"), 1.0}, {QStringLiteral("rotation"), -8.0}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("stamp-ship")}, {QStringLiteral("type"), QStringLiteral("stamp")}, {QStringLiteral("assetKind"), QStringLiteral("ship")}, {QStringLiteral("x"), 455.0}, {QStringLiteral("y"), 125.0}, {QStringLiteral("size"), 36.0}, {QStringLiteral("scale"), 1.0}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("stamp-compass")}, {QStringLiteral("type"), QStringLiteral("stamp")}, {QStringLiteral("assetKind"), QStringLiteral("compass")}, {QStringLiteral("x"), 70.0}, {QStringLiteral("y"), 70.0}, {QStringLiteral("size"), 32.0}, {QStringLiteral("scale"), 1.0}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("label-sea")}, {QStringLiteral("type"), QStringLiteral("label")}, {QStringLiteral("text"), QStringLiteral("Mar Interior")}, {QStringLiteral("x"), 395.0}, {QStringLiteral("y"), 85.0}, {QStringLiteral("fontSize"), 34.0}, {QStringLiteral("italic"), true}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("label-region")}, {QStringLiteral("type"), QStringLiteral("label")}, {QStringLiteral("text"), QStringLiteral("Principado de Thet")}, {QStringLiteral("x"), 175.0}, {QStringLiteral("y"), 335.0}, {QStringLiteral("fontSize"), 28.0}, {QStringLiteral("rotation"), -4.0}}
    };

    const QJsonArray layers{
        QJsonObject{{QStringLiteral("id"), QStringLiteral("terrain")}, {QStringLiteral("name"), QStringLiteral("Terreno")}, {QStringLiteral("visible"), true}, {QStringLiteral("locked"), false}, {QStringLiteral("opacity"), 1.0}, {QStringLiteral("objects"), terrainObjects}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("geography")}, {QStringLiteral("name"), QStringLiteral("Geografía")}, {QStringLiteral("visible"), true}, {QStringLiteral("locked"), false}, {QStringLiteral("opacity"), 0.9}, {QStringLiteral("objects"), geographyObjects}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("annotations")}, {QStringLiteral("name"), QStringLiteral("Símbolos y rótulos")}, {QStringLiteral("visible"), true}, {QStringLiteral("locked"), false}, {QStringLiteral("opacity"), 1.0}, {QStringLiteral("objects"), annotationObjects}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("hidden")}, {QStringLiteral("name"), QStringLiteral("Oculta")}, {QStringLiteral("visible"), false}, {QStringLiteral("opacity"), 1.0}, {QStringLiteral("objects"), QJsonArray{QJsonObject{{QStringLiteral("id"), QStringLiteral("hidden-label")}, {QStringLiteral("type"), QStringLiteral("label")}, {QStringLiteral("text"), QStringLiteral("NO DEBE VERSE")}, {QStringLiteral("x"), 250.0}, {QStringLiteral("y"), 200.0}}}}}
    };

    const QJsonObject theme{
        {QStringLiteral("sea"), QStringLiteral("#b9c8cd")},
        {QStringLiteral("land"), QStringLiteral("#d8c99e")},
        {QStringLiteral("coast"), QStringLiteral("#463d31")},
        {QStringLiteral("river"), QStringLiteral("#5d8193")},
        {QStringLiteral("road"), QStringLiteral("#7c5738")},
        {QStringLiteral("border"), QStringLiteral("#8e4a45")},
        {QStringLiteral("forest"), QStringLiteral("#405f3d")},
        {QStringLiteral("mountain"), QStringLiteral("#625849")},
        {QStringLiteral("symbol"), QStringLiteral("#362f29")},
        {QStringLiteral("text"), QStringLiteral("#282521")},
        {QStringLiteral("labelOutline"), QStringLiteral("#efe2c2")}
    };

    const QJsonObject pilin{
        {QStringLiteral("version"), 2},
        {QStringLiteral("width"), 512},
        {QStringLiteral("height"), 384},
        {QStringLiteral("cartographicStyle"), QStringLiteral("Portulano histórico")},
        {QStringLiteral("theme"), theme},
        {QStringLiteral("template"), QJsonObject{{QStringLiteral("visible"), false}, {QStringLiteral("opacity"), 0.35}}},
        {QStringLiteral("layers"), layers},
        {QStringLiteral("activeLayerId"), QStringLiteral("annotations")}
    };
    return QJsonObject{{QStringLiteral("name"), QStringLiteral("Mapa de prueba RC")}, {QStringLiteral("pilinRey"), pilin}};
}

bool validateRaster(const QString& path) {
    QImage image(path);
    if (image.isNull() || image.width() != 1024 || image.height() != 768) return false;
    int nonUniformSamples = 0;
    const QRgb first = image.pixel(0, 0);
    for (int y = 0; y < image.height(); y += 64)
        for (int x = 0; x < image.width(); x += 64)
            if (image.pixel(x, y) != first) ++nonUniformSamples;
    return nonUniformSamples >= 8;
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QApplication app(argc, argv);
    QTemporaryDir directory;
    if (!require(directory.isValid(), "temporary directory could not be created")) return 1;

    const QJsonObject map = sampleMap();
    const QSize outputSize(1024, 768);
    QString error;

    const QString png = directory.filePath(QStringLiteral("map.png"));
    if (!require(wbw::MapExporter::exportPng(map, png, outputSize, &error), "PNG export returned false")) { std::cerr << error.toStdString() << '\n'; return 1; }
    if (!require(QFileInfo(png).size() > 5000, "PNG output is implausibly small")) return 1;
    if (!require(validateRaster(png), "PNG output lacks rendered cartographic content")) return 1;

    const QString svg = directory.filePath(QStringLiteral("map.svg"));
    error.clear();
    if (!require(wbw::MapExporter::exportSvg(map, svg, outputSize, &error), "SVG export returned false")) { std::cerr << error.toStdString() << '\n'; return 1; }
    QFile svgFile(svg);
    if (!require(svgFile.open(QIODevice::ReadOnly), "SVG output cannot be reopened")) return 1;
    const QByteArray svgBytes = svgFile.readAll();
    if (!require(svgBytes.size() > 3000 && svgBytes.contains("<svg"), "SVG output is invalid or empty")) return 1;

    const QString pdf = directory.filePath(QStringLiteral("map.pdf"));
    error.clear();
    if (!require(wbw::MapExporter::exportPdf(map, pdf, outputSize, &error), "PDF export returned false")) { std::cerr << error.toStdString() << '\n'; return 1; }
    if (!require(QFileInfo(pdf).size() > 3000, "PDF output is implausibly small")) return 1;

    std::cout << "MapExporter comprehensive smoke passed\n";
    return 0;
}
