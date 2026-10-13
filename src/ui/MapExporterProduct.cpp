#include "ui/MapExporter.h"

#include <QByteArray>
#include <QColor>
#include <QFont>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QMarginsF>
#include <QObject>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPolygonF>
#include <QSvgGenerator>

#include <algorithm>
#include <cmath>

namespace wbw {
namespace {

QByteArray dataUrlBytes(const QString& value) {
    if (!value.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive)) return {};
    const qsizetype comma = value.indexOf(QLatin1Char(','));
    if (comma < 0) return {};
    const QString meta = value.mid(5, comma - 5);
    const QByteArray payload = value.mid(comma + 1).toLatin1();
    return meta.contains(QStringLiteral(";base64"), Qt::CaseInsensitive)
        ? QByteArray::fromBase64(payload)
        : QByteArray::fromPercentEncoding(payload);
}

QColor themeColor(const QJsonObject& pilin, const QString& key, const QColor& fallback) {
    const QColor parsed(pilin.value(QStringLiteral("theme")).toObject().value(key).toString());
    return parsed.isValid() ? parsed : fallback;
}

QColor mix(const QColor& a, const QColor& b, double t) {
    t = std::clamp(t, 0.0, 1.0);
    return QColor(qRound(a.red() * (1.0 - t) + b.red() * t),
                  qRound(a.green() * (1.0 - t) + b.green() * t),
                  qRound(a.blue() * (1.0 - t) + b.blue() * t));
}

QPointF jsonPoint(const QJsonValue& value) {
    const QJsonObject p = value.toObject();
    return QPointF(p.value(QStringLiteral("x")).toDouble(), p.value(QStringLiteral("y")).toDouble());
}

QPainterPath smoothPath(const QJsonArray& points, double sx, double sy) {
    QPainterPath path;
    if (points.isEmpty()) return path;
    QPointF previous(jsonPoint(points.first()).x() * sx, jsonPoint(points.first()).y() * sy);
    path.moveTo(previous);
    for (int i = 1; i < points.size(); ++i) {
        const QPointF source = jsonPoint(points.at(i));
        const QPointF current(source.x() * sx, source.y() * sy);
        const QPointF mid = (previous + current) * .5;
        path.quadTo(previous, mid);
        previous = current;
    }
    path.lineTo(previous);
    return path;
}

QImage terrainImage(const QJsonObject& pilin) {
    const QJsonObject model = pilin.value(QStringLiteral("terrainModel")).toObject();
    const int tw = model.value(QStringLiteral("width")).toInt();
    const int th = model.value(QStringLiteral("height")).toInt();
    const QJsonArray values = model.value(QStringLiteral("values")).toArray();
    const QJsonArray biomes = model.value(QStringLiteral("biomes")).toArray();
    if (tw < 2 || th < 2 || values.size() < tw * th) return {};

    const double sea = std::clamp(model.value(QStringLiteral("seaLevel")).toDouble(.42), .05, .85);
    const QColor seaBase = themeColor(pilin, QStringLiteral("sea"), QColor(QStringLiteral("#789fad")));
    const QColor deepSea = seaBase.darker(142);
    const QColor midSea = seaBase;
    const QColor shallowSea(QStringLiteral("#a9c7c4"));
    const QColor beach(QStringLiteral("#cbb98c"));
    const QColor meadow(QStringLiteral("#8d9e70"));
    const QColor forest(QStringLiteral("#526e4e"));
    const QColor woodland(QStringLiteral("#71845d"));
    const QColor dry(QStringLiteral("#b89869"));
    const QColor upland(QStringLiteral("#927b5e"));
    const QColor rock(QStringLiteral("#756d65"));
    const QColor snow(QStringLiteral("#dddcd5"));
    const QColor tundra(QStringLiteral("#a7aa91"));

    auto hAt = [&](int x, int y) {
        x = std::clamp(x, 0, tw - 1);
        y = std::clamp(y, 0, th - 1);
        return values.at(y * tw + x).toInt() / 255.0;
    };
    auto biomeAt = [&](int x, int y) {
        if (biomes.size() < tw * th) return 1;
        x = std::clamp(x, 0, tw - 1);
        y = std::clamp(y, 0, th - 1);
        return biomes.at(y * tw + x).toInt(1);
    };

    QImage image(tw, th, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < th; ++y) {
        QRgb* row = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < tw; ++x) {
            const double h = hAt(x, y);
            QColor color;
            if (h < sea) {
                const double depth = std::clamp(h / sea, 0.0, 1.0);
                color = depth < .66
                    ? mix(deepSea, midSea, depth / .66)
                    : mix(midSea, shallowSea, (depth - .66) / .34);
            } else {
                const int biome = biomeAt(x, y);
                if (h < sea + .018) color = beach;
                else if (biome == 2) color = forest;
                else if (biome == 3) color = woodland;
                else if (biome == 4) color = dry;
                else if (biome == 7) color = tundra;
                else if (h < .57) color = mix(beach, meadow, std::clamp((h - sea) / std::max(.01, .57 - sea), 0.0, 1.0));
                else if (h < .72) color = mix(meadow, upland, (h - .57) / .15);
                else if (h < .86) color = mix(upland, rock, (h - .72) / .14);
                else color = mix(rock, snow, (h - .86) / .14);
            }
            const double dx = hAt(x + 1, y) - hAt(x - 1, y);
            const double dy = hAt(x, y + 1) - hAt(x, y - 1);
            const double shade = std::clamp(.97 - dx * 1.35 - dy * .9, .64, 1.22);
            color.setRed(std::clamp(qRound(color.red() * shade), 0, 255));
            color.setGreen(std::clamp(qRound(color.green() * shade), 0, 255));
            color.setBlue(std::clamp(qRound(color.blue() * shade), 0, 255));
            row[x] = color.rgba();
        }
    }
    return image;
}

void drawGeneratedRegionBoundaries(QPainter& painter, const QJsonObject& pilin, const QSize& size) {
    const QJsonObject model = pilin.value(QStringLiteral("terrainModel")).toObject();
    const int tw = model.value(QStringLiteral("width")).toInt();
    const int th = model.value(QStringLiteral("height")).toInt();
    const QJsonArray regions = model.value(QStringLiteral("regions")).toArray();
    if (tw < 2 || th < 2 || regions.size() < tw * th) return;

    const double sx = static_cast<double>(size.width()) / tw;
    const double sy = static_cast<double>(size.height()) / th;
    auto rAt = [&](int x, int y) {
        x = std::clamp(x, 0, tw - 1);
        y = std::clamp(y, 0, th - 1);
        return regions.at(y * tw + x).toInt(-1);
    };

    painter.save();
    painter.setPen(QPen(QColor(73, 63, 72, 115), std::max(.7, sx * .28), Qt::DashLine));
    for (int y = 1; y < th - 1; ++y) {
        for (int x = 1; x < tw - 1; ++x) {
            const int region = rAt(x, y);
            if (region < 0) continue;
            if (rAt(x + 1, y) >= 0 && rAt(x + 1, y) != region)
                painter.drawLine(QPointF((x + 1) * sx, y * sy), QPointF((x + 1) * sx, (y + 1) * sy));
            if (rAt(x, y + 1) >= 0 && rAt(x, y + 1) != region)
                painter.drawLine(QPointF(x * sx, (y + 1) * sy), QPointF((x + 1) * sx, (y + 1) * sy));
        }
    }
    painter.restore();
}

void drawText(QPainter& painter, const QJsonObject& object, const QString& text, const QPointF& pos, double scale, const QJsonObject& pilin, bool boldFallback = false) {
    if (text.trimmed().isEmpty()) return;
    QFont font(object.value(QStringLiteral("fontFamily")).toString(QStringLiteral("Georgia")));
    font.setPointSizeF(std::clamp(object.value(QStringLiteral("fontSize")).toDouble(54) * scale * .75, 6.0, 240.0));
    font.setBold(object.value(QStringLiteral("bold")).toBool(boldFallback));
    font.setItalic(object.value(QStringLiteral("italic")).toBool(false));
    QPainterPath glyphs;
    glyphs.addText(QPointF(0, 0), font, text);
    QColor fill(object.value(QStringLiteral("color")).toString());
    if (!fill.isValid()) fill = themeColor(pilin, QStringLiteral("text"), QColor(QStringLiteral("#29251f")));
    painter.save();
    painter.translate(pos);
    painter.rotate(object.value(QStringLiteral("rotation")).toDouble());
    painter.setBrush(fill);
    painter.setPen(QPen(QColor(239, 229, 205, 190), std::max(1.0, font.pointSizeF() * .08)));
    painter.drawPath(glyphs);
    painter.setPen(QPen(fill, std::max(.6, font.pointSizeF() * .018)));
    painter.drawPath(glyphs);
    painter.restore();
}

void paintObject(QPainter& painter, const QJsonObject& object, double sx, double sy, const QJsonObject& pilin) {
    const QString type = object.value(QStringLiteral("type")).toString();
    const double scale = (sx + sy) * .5;

    if (type == QStringLiteral("settlement")) {
        const QPointF p(object.value(QStringLiteral("x")).toDouble() * sx, object.value(QStringLiteral("y")).toDouble() * sy);
        const QString kind = object.value(QStringLiteral("settlementType")).toString(object.value(QStringLiteral("kind")).toString());
        const bool capital = kind.contains(QStringLiteral("Capital"), Qt::CaseInsensitive);
        const bool port = kind.contains(QStringLiteral("Puerto"), Qt::CaseInsensitive);
        const double r = std::clamp(7.0 * object.value(QStringLiteral("scale")).toDouble(1.0) * std::sqrt(scale + .18), 3.5, 22.0);
        const QColor ink = themeColor(pilin, QStringLiteral("symbol"), QColor(QStringLiteral("#2c2924")));
        painter.save();
        painter.setPen(QPen(QColor(238, 228, 205, 210), std::max(2.0, 3.3 * scale)));
        painter.setBrush(ink);
        if (capital) painter.drawRect(QRectF(p.x() - r, p.y() - r, r * 2, r * 2));
        else painter.drawEllipse(p, r, r);
        painter.setPen(QPen(ink, std::max(1.0, 1.2 * scale)));
        painter.setBrush(Qt::NoBrush);
        if (capital) painter.drawRect(QRectF(p.x() - r - 3, p.y() - r - 3, r * 2 + 6, r * 2 + 6));
        if (port) painter.drawLine(p + QPointF(0, -r * 1.6), p + QPointF(0, r * 1.6));
        painter.restore();
        QJsonObject labelStyle = object;
        if (!labelStyle.contains(QStringLiteral("fontSize"))) labelStyle.insert(QStringLiteral("fontSize"), capital ? 58 : 46);
        drawText(painter, labelStyle, object.value(QStringLiteral("label")).toString(), p + QPointF(r + 7, -r * .25), scale, pilin, capital);
        return;
    }

    if (type == QStringLiteral("label")) {
        drawText(painter, object, object.value(QStringLiteral("text")).toString(),
                 QPointF(object.value(QStringLiteral("x")).toDouble() * sx, object.value(QStringLiteral("y")).toDouble() * sy),
                 scale, pilin);
        return;
    }

    if (type == QStringLiteral("stamp")) {
        const QString data = object.value(QStringLiteral("dataUrl")).toString();
        if (data.isEmpty()) return;
        QImage image;
        if (!image.loadFromData(dataUrlBytes(data))) return;
        const QPointF p(object.value(QStringLiteral("x")).toDouble() * sx, object.value(QStringLiteral("y")).toDouble() * sy);
        const double s = object.value(QStringLiteral("size")).toDouble(150) * object.value(QStringLiteral("scale")).toDouble(1.0) * scale * .5;
        painter.save();
        painter.translate(p);
        painter.rotate(object.value(QStringLiteral("rotation")).toDouble());
        painter.setOpacity(std::clamp(object.value(QStringLiteral("opacity")).toDouble(1.0), 0.0, 1.0));
        painter.drawImage(QRectF(-s, -s, s * 2, s * 2), image);
        painter.restore();
        return;
    }

    const QJsonArray points = object.value(QStringLiteral("points")).toArray();
    if (points.size() < 2) return;
    const QPainterPath path = smoothPath(points, sx, sy);
    const double width = std::max(1.0, object.value(QStringLiteral("width")).toDouble(5) * scale);

    painter.save();
    painter.setBrush(Qt::NoBrush);
    if (type == QStringLiteral("river")) {
        const QColor river = themeColor(pilin, QStringLiteral("river"), QColor(QStringLiteral("#2d6382")));
        painter.setPen(QPen(river.darker(145), width + 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);
        painter.setPen(QPen(river.lighter(150), std::max(1.0, width * .45), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);
    } else if (type == QStringLiteral("lake")) {
        const QColor water = themeColor(pilin, QStringLiteral("river"), QColor(QStringLiteral("#2d6382")));
        QColor fill = water.lighter(145);
        fill.setAlphaF(std::clamp(object.value(QStringLiteral("fillOpacity")).toDouble(.82), 0.0, 1.0));
        painter.setPen(QPen(water.darker(135), std::max(1.0, width * .7), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(fill);
        painter.drawPath(path);
    } else if (type == QStringLiteral("road")) {
        const QColor road = themeColor(pilin, QStringLiteral("road"), QColor(QStringLiteral("#75512f")));
        painter.setPen(QPen(road.darker(140), width + 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);
        painter.setPen(QPen(road.lighter(160), std::max(1.0, width * .36), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);
    } else if (type == QStringLiteral("border")) {
        painter.setPen(QPen(themeColor(pilin, QStringLiteral("border"), QColor(QStringLiteral("#7c4650"))), width, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);
    } else if (type == QStringLiteral("region")) {
        QColor region = themeColor(pilin, QStringLiteral("region"), QColor(QStringLiteral("#8e6547")));
        QColor fill = region;
        fill.setAlphaF(std::clamp(object.value(QStringLiteral("fillOpacity")).toDouble(.12), 0.0, 1.0));
        painter.setBrush(fill);
        painter.setPen(QPen(region, width, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);
    } else if (type == QStringLiteral("coast")) {
        QColor land = themeColor(pilin, QStringLiteral("land"), QColor(QStringLiteral("#d8c99e")));
        painter.setBrush(land);
        painter.setPen(QPen(themeColor(pilin, QStringLiteral("coast"), QColor(QStringLiteral("#413b30"))), std::max(1.0, width * .5)));
        painter.drawPath(path);
    }
    painter.restore();
}

void paintTemplate(QPainter& painter, const QJsonObject& map, const QJsonObject& pilin, double sx, double sy) {
    const QJsonObject t = pilin.value(QStringLiteral("template")).toObject();
    if (!t.value(QStringLiteral("visible")).toBool(true)) return;
    QString data = t.value(QStringLiteral("dataUrl")).toString();
    if (data.isEmpty()) data = map.value(QStringLiteral("backgroundImageDataUrl")).toString();
    if (data.isEmpty()) return;
    QImage image;
    if (!image.loadFromData(dataUrlBytes(data))) return;
    const double docW = std::max(1.0, pilin.value(QStringLiteral("width")).toDouble(4096));
    const double docH = std::max(1.0, pilin.value(QStringLiteral("height")).toDouble(2304));
    const double scale = std::clamp(t.value(QStringLiteral("scale")).toDouble(1.0), .05, 5.0);
    const double w = docW * sx * scale;
    const double h = docH * sy * scale;
    const double x = t.value(QStringLiteral("x")).toDouble() * sx + (docW * sx - w) * .5;
    const double y = t.value(QStringLiteral("y")).toDouble() * sy + (docH * sy - h) * .5;
    painter.save();
    painter.setOpacity(std::clamp(t.value(QStringLiteral("opacity")).toDouble(.25), 0.0, 1.0));
    painter.translate(x + w * .5, y + h * .5);
    painter.rotate(t.value(QStringLiteral("rotation")).toDouble());
    painter.drawImage(QRectF(-w * .5, -h * .5, w, h), image);
    painter.restore();
}

void paintMap(QPainter& painter, const QJsonObject& map, const QSize& size) {
    const QJsonObject pilin = map.value(QStringLiteral("pilinRey")).toObject();
    const double docW = std::max(1.0, pilin.value(QStringLiteral("width")).toDouble(4096));
    const double docH = std::max(1.0, pilin.value(QStringLiteral("height")).toDouble(2304));
    const double sx = size.width() / docW;
    const double sy = size.height() / docH;

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.fillRect(QRect(QPoint(0, 0), size), themeColor(pilin, QStringLiteral("sea"), QColor(QStringLiteral("#789fad"))));

    const QImage terrain = terrainImage(pilin);
    if (!terrain.isNull()) painter.drawImage(QRect(QPoint(0, 0), size), terrain);
    paintTemplate(painter, map, pilin, sx, sy);
    drawGeneratedRegionBoundaries(painter, pilin, size);

    for (const QJsonValue& layerValue : pilin.value(QStringLiteral("layers")).toArray()) {
        const QJsonObject layer = layerValue.toObject();
        if (!layer.value(QStringLiteral("visible")).toBool(true)) continue;
        painter.save();
        painter.setOpacity(std::clamp(layer.value(QStringLiteral("opacity")).toDouble(1.0), 0.0, 1.0));
        for (const QJsonValue& objectValue : layer.value(QStringLiteral("objects")).toArray())
            paintObject(painter, objectValue.toObject(), sx, sy, pilin);
        painter.restore();
    }
}

bool validExportSize(const QSize& size, QString* error) {
    if (size.isValid() && !size.isEmpty()) return true;
    if (error) *error = QObject::tr("Tamaño de exportación inválido.");
    return false;
}

} // namespace

bool MapExporter::exportPng(const QJsonObject& map, const QString& path, const QSize& pixelSize, QString* error) {
    if (!validExportSize(pixelSize, error)) return false;
    QImage image(pixelSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    paintMap(painter, map, pixelSize);
    painter.end();
    if (!image.save(path, "PNG")) {
        if (error) *error = QObject::tr("No se pudo escribir el PNG.");
        return false;
    }
    return true;
}

bool MapExporter::exportPdf(const QJsonObject& map, const QString& path, const QSize& pixelSize, QString* error) {
    if (!validExportSize(pixelSize, error)) return false;
    QPdfWriter writer(path);
    writer.setResolution(96);
    writer.setPageSize(QPageSize(QSizeF(pixelSize.width() * 25.4 / 96.0, pixelSize.height() * 25.4 / 96.0), QPageSize::Millimeter, QStringLiteral("Worldbuilder Writer Map")));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    QPainter painter(&writer);
    if (!painter.isActive()) {
        if (error) *error = QObject::tr("No se pudo iniciar el exportador PDF.");
        return false;
    }
    paintMap(painter, map, QSize(writer.width(), writer.height()));
    painter.end();
    return true;
}

bool MapExporter::exportSvg(const QJsonObject& map, const QString& path, const QSize& pixelSize, QString* error) {
    if (!validExportSize(pixelSize, error)) return false;
    QSvgGenerator svg;
    svg.setFileName(path);
    svg.setSize(pixelSize);
    svg.setViewBox(QRect(QPoint(0, 0), pixelSize));
    svg.setTitle(QStringLiteral("Worldbuilder Writer Map"));
    QPainter painter(&svg);
    if (!painter.isActive()) {
        if (error) *error = QObject::tr("No se pudo iniciar el exportador SVG.");
        return false;
    }
    paintMap(painter, map, pixelSize);
    painter.end();
    return true;
}

} // namespace wbw
