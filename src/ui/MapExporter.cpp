#include "ui/MapExporter.h"

#include <QColor>
#include <QFile>
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
#include <QSize>
#include <QSizeF>
#include <QTextStream>

#include <algorithm>

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

QColor paperColor() { return QColor(224, 210, 174); }
QColor landColor() { return QColor(197, 182, 143); }
QColor regionFill() { return QColor(188, 169, 128, 92); }

QColor objectColor(const QString& type) {
    if (type == QStringLiteral("river")) return QColor(55, 105, 147);
    if (type == QStringLiteral("road")) return QColor(112, 78, 46);
    if (type == QStringLiteral("border")) return QColor(145, 63, 55);
    if (type == QStringLiteral("forest")) return QColor(47, 88, 52);
    if (type == QStringLiteral("mountain")) return QColor(73, 65, 59);
    if (type == QStringLiteral("settlement")) return QColor(67, 45, 34);
    if (type == QStringLiteral("label")) return QColor(37, 35, 32);
    if (type == QStringLiteral("region")) return QColor(113, 91, 57);
    return QColor(47, 45, 41);
}

QPointF projectPoint(const QJsonObject& point, double sx, double sy) {
    return QPointF(point.value(QStringLiteral("x")).toDouble() * sx,
                   point.value(QStringLiteral("y")).toDouble() * sy);
}

QPolygonF polygonFor(const QJsonArray& points, double sx, double sy) {
    QPolygonF polygon;
    polygon.reserve(points.size());
    for (const QJsonValue value : points) polygon.append(projectPoint(value.toObject(), sx, sy));
    return polygon;
}

QPainterPath pathFor(const QPolygonF& polygon) {
    QPainterPath path;
    if (polygon.isEmpty()) return path;
    path.moveTo(polygon.first());
    for (int i = 1; i < polygon.size(); ++i) path.lineTo(polygon.at(i));
    return path;
}

void drawForest(QPainter& painter, const QPolygonF& points, double scale) {
    const qreal size = std::clamp(9.0 * scale, 4.0, 22.0);
    for (int i = 0; i < points.size(); i += 4) {
        const QPointF p = points.at(i);
        painter.drawLine(p + QPointF(0, -size), p + QPointF(-size * .55, size * .35));
        painter.drawLine(p + QPointF(0, -size), p + QPointF(size * .55, size * .35));
        painter.drawLine(p + QPointF(-size * .42, 0), p + QPointF(size * .42, 0));
        painter.drawLine(p + QPointF(0, size * .35), p + QPointF(0, size * .8));
    }
}

void drawMountains(QPainter& painter, const QPolygonF& points, double scale) {
    const qreal size = std::clamp(13.0 * scale, 5.0, 30.0);
    for (int i = 0; i < points.size(); i += 5) {
        const QPointF p = points.at(i);
        painter.drawLine(p + QPointF(-size, size * .55), p + QPointF(0, -size));
        painter.drawLine(p + QPointF(0, -size), p + QPointF(size, size * .55));
        painter.drawLine(p + QPointF(-size * .32, -size * .02), p + QPointF(0, size * .22));
        painter.drawLine(p + QPointF(0, size * .22), p + QPointF(size * .27, -size * .08));
    }
}

void paintObject(QPainter& painter, const QJsonObject& object, double sx, double sy, double opacity) {
    const QString type = object.value(QStringLiteral("type")).toString();
    const double scale = (sx + sy) * .5;
    QColor color = objectColor(type);
    color.setAlphaF(std::clamp(opacity, 0.0, 1.0));

    if (type == QStringLiteral("settlement")) {
        const QPointF p(object.value(QStringLiteral("x")).toDouble() * sx, object.value(QStringLiteral("y")).toDouble() * sy);
        const qreal r = std::clamp(6.0 * scale, 3.0, 12.0);
        painter.setPen(QPen(color, std::max(1.0, 1.5 * scale)));
        const QString kind = object.value(QStringLiteral("kind")).toString();
        if (kind.contains(QStringLiteral("Puerto"), Qt::CaseInsensitive)) {
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(QRectF(p.x() - r, p.y() - r, r * 2, r * 2));
        } else {
            painter.setBrush(color);
            painter.drawEllipse(p, r, r);
        }
        if (kind.contains(QStringLiteral("Capital"), Qt::CaseInsensitive)) {
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(p, r + 3.0, r + 3.0);
        }
        QFont font(QStringLiteral("Georgia"));
        font.setPointSizeF(std::clamp(10.0 * scale, 8.0, 20.0));
        font.setBold(kind.contains(QStringLiteral("Capital"), Qt::CaseInsensitive));
        painter.setFont(font);
        painter.setBrush(Qt::NoBrush);
        painter.drawText(p + QPointF(r + 5.0, 4.0), object.value(QStringLiteral("label")).toString());
        return;
    }

    if (type == QStringLiteral("label")) {
        const QPointF p(object.value(QStringLiteral("x")).toDouble() * sx, object.value(QStringLiteral("y")).toDouble() * sy);
        QFont font(QStringLiteral("Georgia"));
        font.setPointSizeF(std::clamp(12.0 * scale, 9.0, 28.0));
        painter.setFont(font);
        painter.setPen(color);
        painter.drawText(p, object.value(QStringLiteral("text")).toString());
        return;
    }

    const QJsonArray raw = object.value(QStringLiteral("points")).toArray();
    if (raw.size() < 2) return;
    const QPolygonF polyline = polygonFor(raw, sx, sy);
    QPainterPath path = pathFor(polyline);
    const bool closed = object.value(QStringLiteral("closed")).toBool(false) || type == QStringLiteral("coast") || type == QStringLiteral("region");
    if (closed) path.closeSubpath();

    if (type == QStringLiteral("coast")) {
        QColor fill = landColor(); fill.setAlphaF(std::clamp(opacity * .78, 0.0, 1.0));
        painter.fillPath(path, fill);
        painter.setPen(QPen(QColor(40, 42, 39, color.alpha()), std::max(1.0, 2.2 * scale), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);
        painter.setPen(QPen(QColor(121, 104, 73, qRound(color.alpha() * .5)), std::max(0.8, 1.0 * scale), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);
        return;
    }

    if (type == QStringLiteral("region")) {
        QColor fill = regionFill(); fill.setAlphaF(std::clamp(opacity * .36, 0.0, 1.0));
        painter.fillPath(path, fill);
        painter.setPen(QPen(color, std::max(1.0, 1.5 * scale), Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);
        return;
    }

    painter.setBrush(Qt::NoBrush);
    if (type == QStringLiteral("forest")) {
        painter.setPen(QPen(color, std::max(1.0, 1.1 * scale), Qt::SolidLine, Qt::RoundCap));
        drawForest(painter, polyline, scale);
        return;
    }
    if (type == QStringLiteral("mountain")) {
        painter.setPen(QPen(color, std::max(1.0, 1.1 * scale), Qt::SolidLine, Qt::RoundCap));
        drawMountains(painter, polyline, scale);
        return;
    }
    if (type == QStringLiteral("river")) {
        painter.setPen(QPen(QColor(42, 86, 124, color.alpha()), std::max(1.5, 3.6 * scale), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPolyline(polyline);
        painter.setPen(QPen(QColor(99, 151, 190, qRound(color.alpha() * .9)), std::max(1.0, 1.8 * scale), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPolyline(polyline);
        return;
    }
    if (type == QStringLiteral("road")) {
        painter.setPen(QPen(QColor(73, 54, 37, color.alpha()), std::max(1.5, 3.0 * scale), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPolyline(polyline);
        painter.setPen(QPen(QColor(169, 137, 91, color.alpha()), std::max(1.0, 1.2 * scale), Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPolyline(polyline);
        return;
    }
    if (type == QStringLiteral("border")) {
        painter.setPen(QPen(color, std::max(1.0, 1.6 * scale), Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPolyline(polyline);
        return;
    }
    painter.setPen(QPen(color, std::max(1.0, 1.5 * scale), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPolyline(polyline);
}

void paintTemplate(QPainter& painter, const QJsonObject& map, const QJsonObject& pilin, double sx, double sy) {
    const QJsonObject templ = pilin.value(QStringLiteral("template")).toObject();
    if (!templ.value(QStringLiteral("visible")).toBool(true)) return;
    QString dataUrl = templ.value(QStringLiteral("dataUrl")).toString();
    if (dataUrl.isEmpty()) dataUrl = map.value(QStringLiteral("backgroundImageDataUrl")).toString();
    if (dataUrl.isEmpty()) return;
    QImage image;
    if (!image.loadFromData(dataUrlBytes(dataUrl))) return;
    const double docW = std::max(1.0, pilin.value(QStringLiteral("width")).toDouble(4096.0));
    const double docH = std::max(1.0, pilin.value(QStringLiteral("height")).toDouble(2304.0));
    const double x = templ.value(QStringLiteral("x")).toDouble() * sx;
    const double y = templ.value(QStringLiteral("y")).toDouble() * sy;
    const double scale = std::clamp(templ.value(QStringLiteral("scale")).toDouble(1.0), .01, 100.0);
    const double width = docW * sx * scale;
    const double height = docH * sy * scale;
    painter.save();
    painter.setOpacity(std::clamp(templ.value(QStringLiteral("opacity")).toDouble(.35), 0.0, 1.0));
    painter.translate(x + width * .5, y + height * .5);
    painter.rotate(templ.value(QStringLiteral("rotation")).toDouble());
    painter.drawImage(QRectF(-width * .5, -height * .5, width, height), image);
    painter.restore();
}

void paintMap(QPainter& painter, const QJsonObject& map, const QSize& outputSize) {
    const QJsonObject pilin = map.value(QStringLiteral("pilinRey")).toObject();
    const double docW = std::max(1.0, pilin.value(QStringLiteral("width")).toDouble(4096.0));
    const double docH = std::max(1.0, pilin.value(QStringLiteral("height")).toDouble(2304.0));
    const double sx = outputSize.width() / docW;
    const double sy = outputSize.height() / docH;
    painter.fillRect(QRect(QPoint(0, 0), outputSize), paperColor());
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    paintTemplate(painter, map, pilin, sx, sy);
    for (const QJsonValue layerValue : pilin.value(QStringLiteral("layers")).toArray()) {
        const QJsonObject layer = layerValue.toObject();
        if (!layer.value(QStringLiteral("visible")).toBool(true)) continue;
        const double opacity = layer.value(QStringLiteral("opacity")).toDouble(1.0);
        for (const QJsonValue objectValue : layer.value(QStringLiteral("objects")).toArray()) paintObject(painter, objectValue.toObject(), sx, sy, opacity);
    }
}

QString svgPointList(const QJsonArray& points, double sx, double sy) {
    QStringList values;
    for (const QJsonValue value : points) {
        const QJsonObject p = value.toObject();
        values.append(QStringLiteral("%1,%2").arg(p.value(QStringLiteral("x")).toDouble() * sx, 0, 'f', 2).arg(p.value(QStringLiteral("y")).toDouble() * sy, 0, 'f', 2));
    }
    return values.join(QLatin1Char(' '));
}

QString esc(const QString& text) { return text.toHtmlEscaped(); }

} // namespace

bool MapExporter::exportPng(const QJsonObject& map, const QString& path, const QSize& pixelSize, QString* error) {
    if (!pixelSize.isValid() || pixelSize.isEmpty()) { if (error) *error = QObject::tr("Tamaño de exportación inválido."); return false; }
    QImage image(pixelSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    paintMap(painter, map, pixelSize);
    painter.end();
    if (!image.save(path, "PNG")) { if (error) *error = QObject::tr("No se pudo escribir el PNG."); return false; }
    return true;
}

bool MapExporter::exportPdf(const QJsonObject& map, const QString& path, const QSize& pixelSize, QString* error) {
    if (!pixelSize.isValid() || pixelSize.isEmpty()) { if (error) *error = QObject::tr("Tamaño de exportación inválido."); return false; }
    QPdfWriter writer(path);
    writer.setResolution(96);
    writer.setPageSize(QPageSize(QSizeF(pixelSize.width() * 25.4 / 96.0, pixelSize.height() * 25.4 / 96.0), QPageSize::Millimeter, QStringLiteral("Pilín Rey")));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    QPainter painter(&writer);
    if (!painter.isActive()) { if (error) *error = QObject::tr("No se pudo iniciar el exportador PDF."); return false; }
    paintMap(painter, map, QSize(writer.width(), writer.height()));
    painter.end();
    return true;
}

bool MapExporter::exportSvg(const QJsonObject& map, const QString& path, const QSize& pixelSize, QString* error) {
    if (!pixelSize.isValid() || pixelSize.isEmpty()) { if (error) *error = QObject::tr("Tamaño de exportación inválido."); return false; }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) { if (error) *error = file.errorString(); return false; }

    const QJsonObject pilin = map.value(QStringLiteral("pilinRey")).toObject();
    const double docW = std::max(1.0, pilin.value(QStringLiteral("width")).toDouble(4096.0));
    const double docH = std::max(1.0, pilin.value(QStringLiteral("height")).toDouble(2304.0));
    const double sx = pixelSize.width() / docW;
    const double sy = pixelSize.height() / docH;
    QTextStream out(&file);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" width=\"" << pixelSize.width() << "\" height=\"" << pixelSize.height() << "\" viewBox=\"0 0 " << pixelSize.width() << ' ' << pixelSize.height() << "\">\n";
    out << "<rect width=\"100%\" height=\"100%\" fill=\"" << paperColor().name() << "\"/>\n";

    const QJsonObject templ = pilin.value(QStringLiteral("template")).toObject();
    if (templ.value(QStringLiteral("visible")).toBool(true)) {
        QString dataUrl = templ.value(QStringLiteral("dataUrl")).toString();
        if (dataUrl.isEmpty()) dataUrl = map.value(QStringLiteral("backgroundImageDataUrl")).toString();
        if (!dataUrl.isEmpty()) {
            const double x = templ.value(QStringLiteral("x")).toDouble() * sx;
            const double y = templ.value(QStringLiteral("y")).toDouble() * sy;
            const double scale = std::clamp(templ.value(QStringLiteral("scale")).toDouble(1.0), .01, 100.0);
            const double w = docW * sx * scale, h = docH * sy * scale;
            const double cx = x + w * .5, cy = y + h * .5;
            out << "<image x=\"" << x << "\" y=\"" << y << "\" width=\"" << w << "\" height=\"" << h << "\" opacity=\"" << std::clamp(templ.value(QStringLiteral("opacity")).toDouble(.35), 0.0, 1.0) << "\" preserveAspectRatio=\"none\" transform=\"rotate(" << templ.value(QStringLiteral("rotation")).toDouble() << ' ' << cx << ' ' << cy << ")\" xlink:href=\"" << esc(dataUrl) << "\"/>\n";
        }
    }

    for (const QJsonValue layerValue : pilin.value(QStringLiteral("layers")).toArray()) {
        const QJsonObject layer = layerValue.toObject();
        if (!layer.value(QStringLiteral("visible")).toBool(true)) continue;
        const double opacity = std::clamp(layer.value(QStringLiteral("opacity")).toDouble(1.0), 0.0, 1.0);
        out << "<g opacity=\"" << opacity << "\" data-layer=\"" << esc(layer.value(QStringLiteral("name")).toString()) << "\">\n";
        for (const QJsonValue objectValue : layer.value(QStringLiteral("objects")).toArray()) {
            const QJsonObject object = objectValue.toObject();
            const QString type = object.value(QStringLiteral("type")).toString();
            const QString color = objectColor(type).name();
            if (type == QStringLiteral("settlement")) {
                const double x = object.value(QStringLiteral("x")).toDouble() * sx, y = object.value(QStringLiteral("y")).toDouble() * sy;
                const QString label = esc(object.value(QStringLiteral("label")).toString());
                out << "<circle cx=\"" << x << "\" cy=\"" << y << "\" r=\"4\" fill=\"" << color << "\"/>\n";
                if (!label.isEmpty()) out << "<text x=\"" << x + 7 << "\" y=\"" << y + 4 << "\" font-family=\"Georgia\" font-size=\"12\" fill=\"#252320\">" << label << "</text>\n";
                continue;
            }
            if (type == QStringLiteral("label")) {
                out << "<text x=\"" << object.value(QStringLiteral("x")).toDouble() * sx << "\" y=\"" << object.value(QStringLiteral("y")).toDouble() * sy << "\" font-family=\"Georgia\" font-size=\"14\" fill=\"" << color << "\">" << esc(object.value(QStringLiteral("text")).toString()) << "</text>\n";
                continue;
            }
            const QJsonArray points = object.value(QStringLiteral("points")).toArray();
            if (points.size() < 2) continue;
            const QString list = svgPointList(points, sx, sy);
            if (type == QStringLiteral("coast")) {
                out << "<polygon points=\"" << list << "\" fill=\"" << landColor().name() << "\" fill-opacity=\"0.78\" stroke=\"#282a27\" stroke-width=\"2.2\" stroke-linejoin=\"round\"/>\n";
            } else if (type == QStringLiteral("region")) {
                out << "<polygon points=\"" << list << "\" fill=\"" << regionFill().name() << "\" fill-opacity=\"0.36\" stroke=\"" << color << "\" stroke-width=\"1.5\" stroke-dasharray=\"6 5\"/>\n";
            } else if (type == QStringLiteral("river")) {
                out << "<polyline points=\"" << list << "\" fill=\"none\" stroke=\"#2a567c\" stroke-width=\"4\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>\n";
                out << "<polyline points=\"" << list << "\" fill=\"none\" stroke=\"#6397be\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>\n";
            } else if (type == QStringLiteral("road")) {
                out << "<polyline points=\"" << list << "\" fill=\"none\" stroke=\"#493625\" stroke-width=\"3\" stroke-linecap=\"round\"/>\n";
                out << "<polyline points=\"" << list << "\" fill=\"none\" stroke=\"#a9895b\" stroke-width=\"1.2\" stroke-dasharray=\"7 5\"/>\n";
            } else {
                const QString dash = type == QStringLiteral("border") ? QStringLiteral(" stroke-dasharray=\"7 5\"") : QString();
                out << "<polyline points=\"" << list << "\" fill=\"none\" stroke=\"" << color << "\" stroke-width=\"1.6\"" << dash << "/>\n";
            }
        }
        out << "</g>\n";
    }
    out << "</svg>\n";
    file.close();
    return true;
}

} // namespace wbw
