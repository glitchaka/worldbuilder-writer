#include "ui/MapExporter.h"

#include <QBuffer>
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
#include <QPdfWriter>
#include <QPolygon>
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

QColor paperColor(const QString& style) {
    if (style == QStringLiteral("Portulano")) return QColor(225, 213, 174);
    if (style == QStringLiteral("Mappa mundi")) return QColor(214, 194, 147);
    if (style == QStringLiteral("Ptolemaico")) return QColor(232, 220, 187);
    if (style == QStringLiteral("Islámico medieval")) return QColor(222, 207, 161);
    if (style == QStringLiteral("Señorial")) return QColor(225, 210, 171);
    if (style == QStringLiteral("Militar")) return QColor(218, 217, 197);
    if (style == QStringLiteral("Político")) return QColor(228, 224, 207);
    return QColor(224, 210, 174);
}

QColor objectColor(const QString& type) {
    if (type == QStringLiteral("river")) return QColor(66, 108, 130);
    if (type == QStringLiteral("road")) return QColor(115, 87, 57);
    if (type == QStringLiteral("border")) return QColor(153, 70, 57);
    if (type == QStringLiteral("forest")) return QColor(61, 99, 61);
    if (type == QStringLiteral("mountain")) return QColor(82, 73, 65);
    if (type == QStringLiteral("settlement")) return QColor(84, 52, 38);
    if (type == QStringLiteral("label")) return QColor(45, 42, 37);
    return QColor(58, 51, 43);
}

QPointF projectPoint(const QJsonObject& point, double sx, double sy) {
    return QPointF(point.value(QStringLiteral("x")).toDouble() * sx,
                   point.value(QStringLiteral("y")).toDouble() * sy);
}

void paintObject(QPainter& painter, const QJsonObject& object, double sx, double sy, double opacity) {
    const QString type = object.value(QStringLiteral("type")).toString();
    QColor color = objectColor(type);
    color.setAlphaF(std::clamp(opacity, 0.0, 1.0));
    QPen pen(color, std::max(1.0, 2.0 * (sx + sy) * 0.5));
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    if (type == QStringLiteral("settlement")) {
        const QPointF p(object.value(QStringLiteral("x")).toDouble() * sx,
                        object.value(QStringLiteral("y")).toDouble() * sy);
        const qreal r = std::clamp(6.0 * (sx + sy) * 0.5, 3.0, 12.0);
        painter.setBrush(color);
        painter.drawEllipse(p, r, r);
        painter.setBrush(Qt::NoBrush);
        return;
    }

    if (type == QStringLiteral("label")) {
        const QPointF p(object.value(QStringLiteral("x")).toDouble() * sx,
                        object.value(QStringLiteral("y")).toDouble() * sy);
        QFont font = painter.font();
        font.setFamily(QStringLiteral("Georgia"));
        font.setPointSizeF(std::clamp(11.0 * (sx + sy) * 0.5, 8.0, 28.0));
        painter.setFont(font);
        painter.drawText(p, object.value(QStringLiteral("text")).toString());
        return;
    }

    const QJsonArray points = object.value(QStringLiteral("points")).toArray();
    if (points.isEmpty()) return;
    QPolygonF polyline;
    polyline.reserve(points.size());
    for (const QJsonValue value : points) polyline.append(projectPoint(value.toObject(), sx, sy));

    if (type == QStringLiteral("forest")) {
        const qreal size = std::clamp(9.0 * (sx + sy) * 0.5, 4.0, 22.0);
        for (int i = 0; i < polyline.size(); i += 4) {
            const QPointF p = polyline.at(i);
            painter.drawLine(p + QPointF(0, -size), p + QPointF(-size * .55, size * .35));
            painter.drawLine(p + QPointF(0, -size), p + QPointF(size * .55, size * .35));
            painter.drawLine(p + QPointF(-size * .42, 0), p + QPointF(size * .42, 0));
            painter.drawLine(p + QPointF(0, size * .35), p + QPointF(0, size * .8));
        }
        return;
    }

    if (type == QStringLiteral("mountain")) {
        const qreal size = std::clamp(13.0 * (sx + sy) * 0.5, 5.0, 30.0);
        for (int i = 0; i < polyline.size(); i += 5) {
            const QPointF p = polyline.at(i);
            painter.drawLine(p + QPointF(-size, size * .55), p + QPointF(0, -size));
            painter.drawLine(p + QPointF(0, -size), p + QPointF(size, size * .55));
            painter.drawLine(p + QPointF(-size * .32, -size * .02), p + QPointF(0, size * .22));
            painter.drawLine(p + QPointF(0, size * .22), p + QPointF(size * .27, -size * .08));
        }
        return;
    }

    if (polyline.size() > 1) painter.drawPolyline(polyline);
}

void paintTemplate(QPainter& painter, const QJsonObject& map, const QJsonObject& pilin,
                   const QSize& outputSize, double sx, double sy) {
    const QJsonObject templ = pilin.value(QStringLiteral("template")).toObject();
    if (!templ.value(QStringLiteral("visible")).toBool(true)) return;

    QString dataUrl = templ.value(QStringLiteral("dataUrl")).toString();
    if (dataUrl.isEmpty()) dataUrl = map.value(QStringLiteral("backgroundImageDataUrl")).toString();
    if (dataUrl.isEmpty()) return;

    QImage image;
    if (!image.loadFromData(dataUrlBytes(dataUrl))) return;

    const double docW = std::max(1.0, pilin.value(QStringLiteral("width")).toDouble(4096.0));
    const double docH = std::max(1.0, pilin.value(QStringLiteral("height")).toDouble(2304.0));
    const double x = templ.value(QStringLiteral("x")).toDouble(0.0) * sx;
    const double y = templ.value(QStringLiteral("y")).toDouble(0.0) * sy;
    const double scale = std::clamp(templ.value(QStringLiteral("scale")).toDouble(1.0), 0.01, 100.0);
    const double rotation = templ.value(QStringLiteral("rotation")).toDouble(0.0);
    const double width = docW * sx * scale;
    const double height = docH * sy * scale;

    painter.save();
    painter.setOpacity(std::clamp(templ.value(QStringLiteral("opacity")).toDouble(.35), 0.0, 1.0));
    painter.translate(x + width * .5, y + height * .5);
    painter.rotate(rotation);
    painter.drawImage(QRectF(-width * .5, -height * .5, width, height), image);
    painter.restore();
}

void paintMap(QPainter& painter, const QJsonObject& map, const QSize& outputSize) {
    const QJsonObject pilin = map.value(QStringLiteral("pilinRey")).toObject();
    const double docW = std::max(1.0, pilin.value(QStringLiteral("width")).toDouble(4096.0));
    const double docH = std::max(1.0, pilin.value(QStringLiteral("height")).toDouble(2304.0));
    const double sx = outputSize.width() / docW;
    const double sy = outputSize.height() / docH;

    painter.fillRect(QRect(QPoint(0, 0), outputSize), paperColor(pilin.value(QStringLiteral("cartographicStyle")).toString(QStringLiteral("Fantasía clásica"))));
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    paintTemplate(painter, map, pilin, outputSize, sx, sy);

    for (const QJsonValue layerValue : pilin.value(QStringLiteral("layers")).toArray()) {
        const QJsonObject layer = layerValue.toObject();
        if (!layer.value(QStringLiteral("visible")).toBool(true)) continue;
        const double opacity = layer.value(QStringLiteral("opacity")).toDouble(1.0);
        for (const QJsonValue objectValue : layer.value(QStringLiteral("objects")).toArray())
            paintObject(painter, objectValue.toObject(), sx, sy, opacity);
    }
}

QString svgPointList(const QJsonArray& points, double sx, double sy) {
    QStringList values;
    values.reserve(points.size());
    for (const QJsonValue value : points) {
        const QJsonObject p = value.toObject();
        values.append(QStringLiteral("%1,%2").arg(p.value(QStringLiteral("x")).toDouble() * sx, 0, 'f', 2)
                                             .arg(p.value(QStringLiteral("y")).toDouble() * sy, 0, 'f', 2));
    }
    return values.join(QLatin1Char(' '));
}

} // namespace

bool MapExporter::exportPng(const QJsonObject& map, const QString& path, const QSize& pixelSize, QString* error) {
    if (!pixelSize.isValid() || pixelSize.isEmpty()) {
        if (error) *error = QObject::tr("Tamaño de exportación inválido.");
        return false;
    }
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
    if (!pixelSize.isValid() || pixelSize.isEmpty()) {
        if (error) *error = QObject::tr("Tamaño de exportación inválido.");
        return false;
    }
    QPdfWriter writer(path);
    writer.setResolution(96);
    const QSizeF millimeters(pixelSize.width() * 25.4 / 96.0, pixelSize.height() * 25.4 / 96.0);
    writer.setPageSize(QPageSize(millimeters, QPageSize::Millimeter, QStringLiteral("Pilín Rey")));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    QPainter painter(&writer);
    if (!painter.isActive()) {
        if (error) *error = QObject::tr("No se pudo iniciar el exportador PDF.");
        return false;
    }
    const QSize target(writer.width(), writer.height());
    paintMap(painter, map, target);
    painter.end();
    return true;
}

bool MapExporter::exportSvg(const QJsonObject& map, const QString& path, const QSize& pixelSize, QString* error) {
    if (!pixelSize.isValid() || pixelSize.isEmpty()) {
        if (error) *error = QObject::tr("Tamaño de exportación inválido.");
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    const QJsonObject pilin = map.value(QStringLiteral("pilinRey")).toObject();
    const double docW = std::max(1.0, pilin.value(QStringLiteral("width")).toDouble(4096.0));
    const double docH = std::max(1.0, pilin.value(QStringLiteral("height")).toDouble(2304.0));
    const double sx = pixelSize.width() / docW;
    const double sy = pixelSize.height() / docH;
    QTextStream out(&file);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" width=\"" << pixelSize.width()
        << "\" height=\"" << pixelSize.height() << "\" viewBox=\"0 0 " << pixelSize.width() << ' ' << pixelSize.height() << "\">\n";
    out << "<rect width=\"100%\" height=\"100%\" fill=\"" << paperColor(pilin.value(QStringLiteral("cartographicStyle")).toString()).name() << "\"/>\n";

    const QJsonObject templ = pilin.value(QStringLiteral("template")).toObject();
    if (templ.value(QStringLiteral("visible")).toBool(true)) {
        QString dataUrl = templ.value(QStringLiteral("dataUrl")).toString();
        if (dataUrl.isEmpty()) dataUrl = map.value(QStringLiteral("backgroundImageDataUrl")).toString();
        if (!dataUrl.isEmpty()) {
            const double x = templ.value(QStringLiteral("x")).toDouble(0.0) * sx;
            const double y = templ.value(QStringLiteral("y")).toDouble(0.0) * sy;
            const double scale = std::clamp(templ.value(QStringLiteral("scale")).toDouble(1.0), 0.01, 100.0);
            const double rotation = templ.value(QStringLiteral("rotation")).toDouble(0.0);
            const double width = docW * sx * scale;
            const double height = docH * sy * scale;
            const double cx = x + width * .5;
            const double cy = y + height * .5;
            out << "<image x=\"" << x << "\" y=\"" << y << "\" width=\"" << width << "\" height=\"" << height
                << "\" opacity=\"" << std::clamp(templ.value(QStringLiteral("opacity")).toDouble(.35),0.0,1.0)
                << "\" preserveAspectRatio=\"none\" transform=\"rotate(" << rotation << ' ' << cx << ' ' << cy << ")\" xlink:href=\""
                << dataUrl.toHtmlEscaped() << "\"/>\n";
        }
    }

    for (const QJsonValue layerValue : pilin.value(QStringLiteral("layers")).toArray()) {
        const QJsonObject layer = layerValue.toObject();
        if (!layer.value(QStringLiteral("visible")).toBool(true)) continue;
        const double opacity = std::clamp(layer.value(QStringLiteral("opacity")).toDouble(1.0),0.0,1.0);
        out << "<g opacity=\"" << opacity << "\" data-layer=\"" << layer.value(QStringLiteral("name")).toString().toHtmlEscaped() << "\">\n";
        for (const QJsonValue objectValue : layer.value(QStringLiteral("objects")).toArray()) {
            const QJsonObject object = objectValue.toObject();
            const QString type = object.value(QStringLiteral("type")).toString();
            const QString color = objectColor(type).name();
            if (type == QStringLiteral("settlement")) {
                const double x = object.value(QStringLiteral("x")).toDouble()*sx, y = object.value(QStringLiteral("y")).toDouble()*sy;
                out << "<circle cx=\"" << x << "\" cy=\"" << y << "\" r=\"6\" fill=\"" << color << "\"/>\n";
            } else if (type == QStringLiteral("label")) {
                const double x = object.value(QStringLiteral("x")).toDouble()*sx, y = object.value(QStringLiteral("y")).toDouble()*sy;
                out << "<text x=\"" << x << "\" y=\"" << y << "\" fill=\"" << color << "\" font-family=\"Georgia, serif\" font-size=\"14\">"
                    << object.value(QStringLiteral("text")).toString().toHtmlEscaped() << "</text>\n";
            } else {
                const QJsonArray points = object.value(QStringLiteral("points")).toArray();
                if (points.size() > 1)
                    out << "<polyline points=\"" << svgPointList(points,sx,sy) << "\" fill=\"none\" stroke=\"" << color << "\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>\n";
            }
        }
        out << "</g>\n";
    }
    out << "</svg>\n";
    return true;
}

} // namespace wbw
