#pragma once

#include <QJsonObject>
#include <QString>

class QSize;

namespace wbw {

class MapExporter final {
public:
    static bool exportPng(const QJsonObject& map, const QString& path, const QSize& pixelSize, QString* error = nullptr);
    static bool exportPdf(const QJsonObject& map, const QString& path, const QSize& pixelSize, QString* error = nullptr);
    static bool exportSvg(const QJsonObject& map, const QString& path, const QSize& pixelSize, QString* error = nullptr);
};

} // namespace wbw
