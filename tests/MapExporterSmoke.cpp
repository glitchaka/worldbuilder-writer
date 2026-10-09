#include "ui/MapExporter.h"

#include <QApplication>
#include <QFile>
#include <QFileInfo>
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

QJsonObject sampleMap() {
    const QJsonArray coastPoints{
        QJsonObject{{QStringLiteral("x"), 80.0}, {QStringLiteral("y"), 80.0}},
        QJsonObject{{QStringLiteral("x"), 250.0}, {QStringLiteral("y"), 120.0}},
        QJsonObject{{QStringLiteral("x"), 420.0}, {QStringLiteral("y"), 220.0}}
    };
    const QJsonArray riverPoints{
        QJsonObject{{QStringLiteral("x"), 220.0}, {QStringLiteral("y"), 100.0}},
        QJsonObject{{QStringLiteral("x"), 250.0}, {QStringLiteral("y"), 200.0}},
        QJsonObject{{QStringLiteral("x"), 290.0}, {QStringLiteral("y"), 360.0}}
    };
    const QJsonArray objects{
        QJsonObject{{QStringLiteral("id"), QStringLiteral("coast-1")}, {QStringLiteral("type"), QStringLiteral("coast")}, {QStringLiteral("points"), coastPoints}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("river-1")}, {QStringLiteral("type"), QStringLiteral("river")}, {QStringLiteral("points"), riverPoints}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("city-1")}, {QStringLiteral("type"), QStringLiteral("settlement")}, {QStringLiteral("label"), QStringLiteral("Puerto Prueba")}, {QStringLiteral("x"), 310.0}, {QStringLiteral("y"), 260.0}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("label-1")}, {QStringLiteral("type"), QStringLiteral("label")}, {QStringLiteral("text"), QStringLiteral("Mar Interior")}, {QStringLiteral("x"), 120.0}, {QStringLiteral("y"), 300.0}}
    };
    const QJsonArray layers{
        QJsonObject{{QStringLiteral("id"), QStringLiteral("layer-1")}, {QStringLiteral("name"), QStringLiteral("Cartografía")}, {QStringLiteral("visible"), true}, {QStringLiteral("opacity"), 1.0}, {QStringLiteral("objects"), objects}}
    };
    const QJsonObject pilin{
        {QStringLiteral("version"), 2},
        {QStringLiteral("width"), 512},
        {QStringLiteral("height"), 384},
        {QStringLiteral("cartographicStyle"), QStringLiteral("Portulano")},
        {QStringLiteral("template"), QJsonObject{{QStringLiteral("visible"), false}, {QStringLiteral("opacity"), 0.35}}},
        {QStringLiteral("layers"), layers},
        {QStringLiteral("activeLayerId"), QStringLiteral("layer-1")}
    };
    return QJsonObject{{QStringLiteral("name"), QStringLiteral("Mapa de prueba")}, {QStringLiteral("pilinRey"), pilin}};
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
    if (!require(wbw::MapExporter::exportPng(map, png, outputSize, &error), "PNG export returned false")) {
        std::cerr << error.toStdString() << '\n';
        return 1;
    }
    if (!require(QFileInfo(png).size() > 100, "PNG output is empty")) return 1;

    const QString svg = directory.filePath(QStringLiteral("map.svg"));
    error.clear();
    if (!require(wbw::MapExporter::exportSvg(map, svg, outputSize, &error), "SVG export returned false")) {
        std::cerr << error.toStdString() << '\n';
        return 1;
    }
    QFile svgFile(svg);
    if (!require(svgFile.open(QIODevice::ReadOnly), "SVG output cannot be reopened")) return 1;
    const QByteArray svgBytes = svgFile.readAll();
    if (!require(svgBytes.size() > 100 && svgBytes.contains("<svg"), "SVG output is invalid")) return 1;

    const QString pdf = directory.filePath(QStringLiteral("map.pdf"));
    error.clear();
    if (!require(wbw::MapExporter::exportPdf(map, pdf, outputSize, &error), "PDF export returned false")) {
        std::cerr << error.toStdString() << '\n';
        return 1;
    }
    if (!require(QFileInfo(pdf).size() > 100, "PDF output is empty")) return 1;

    std::cout << "MapExporter smoke passed\n";
    return 0;
}
