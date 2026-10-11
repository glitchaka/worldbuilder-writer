#include "ui/CartographicCanvasV3.h"
#include "ui/PilinReyEditor.h"

#include <QApplication>
#include <QJsonArray>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace wbw {
namespace {

QPointF pointFromJson(const QJsonValue& value) {
    const QJsonObject p = value.toObject();
    return QPointF(p.value(QStringLiteral("x")).toDouble(), p.value(QStringLiteral("y")).toDouble());
}

QColor themeColor(const QJsonObject& pilin, const QString& key, const QColor& fallback) {
    QColor c(pilin.value(QStringLiteral("theme")).toObject().value(key).toString());
    return c.isValid() ? c : fallback;
}

QPainterPath smoothPath(const QJsonArray& points, double sx, double sy) {
    QPainterPath path;
    if (points.isEmpty()) return path;
    QPointF prev = pointFromJson(points.first());
    prev = QPointF(prev.x() * sx, prev.y() * sy);
    path.moveTo(prev);
    for (int i = 1; i < points.size(); ++i) {
        QPointF cur = pointFromJson(points.at(i));
        cur = QPointF(cur.x() * sx, cur.y() * sy);
        const QPointF mid = (prev + cur) * .5;
        path.quadTo(prev, mid);
        prev = cur;
    }
    path.lineTo(prev);
    return path;
}

QColor mix(const QColor& a, const QColor& b, double t) {
    t = std::clamp(t, 0.0, 1.0);
    return QColor(qRound(a.red() * (1 - t) + b.red() * t),
                  qRound(a.green() * (1 - t) + b.green() * t),
                  qRound(a.blue() * (1 - t) + b.blue() * t));
}

quint32 visualHash(int x, int y) {
    quint32 h = static_cast<quint32>(x * 374761393u) ^ static_cast<quint32>(y * 668265263u) ^ 0x9e3779b9u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    return h ^ (h >> 16u);
}

QColor regionColor(int index) {
    static const QColor colors[]{
        QColor("#a85f5f"), QColor("#5c7fa5"), QColor("#8a7550"), QColor("#6d8f64"),
        QColor("#82699a"), QColor("#a77952"), QColor("#5d8c8a"), QColor("#9a6f82")
    };
    return colors[std::abs(index) % 8];
}

void drawTree(QPainter& painter, const QPointF& p, double size, const QColor& ink) {
    painter.save();
    painter.setPen(QPen(ink, std::max(.65, size * .11), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QPainterPath crown;
    crown.moveTo(p.x(), p.y() - size);
    crown.lineTo(p.x() - size * .72, p.y() + size * .42);
    crown.lineTo(p.x() - size * .22, p.y() + size * .28);
    crown.lineTo(p.x() - size * .52, p.y() + size * .75);
    crown.lineTo(p.x() + size * .52, p.y() + size * .75);
    crown.lineTo(p.x() + size * .22, p.y() + size * .28);
    crown.lineTo(p.x() + size * .72, p.y() + size * .42);
    crown.closeSubpath();
    painter.setBrush(QColor(ink.red(), ink.green(), ink.blue(), 38));
    painter.drawPath(crown);
    painter.drawLine(QPointF(p.x(), p.y() + size * .34), QPointF(p.x(), p.y() + size));
    painter.restore();
}

void drawMountain(QPainter& painter, const QPointF& p, double size, double skew, const QColor& ink) {
    painter.save();
    painter.setPen(QPen(ink, std::max(.7, size * .075), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const QPointF left(p.x() - size, p.y() + size * .65);
    const QPointF top(p.x() + skew * size * .25, p.y() - size);
    const QPointF right(p.x() + size, p.y() + size * .65);
    QPainterPath body;
    body.moveTo(left);
    body.lineTo(top);
    body.lineTo(right);
    painter.setBrush(QColor(ink.red(), ink.green(), ink.blue(), 25));
    painter.drawPath(body);
    QPainterPath ridge;
    ridge.moveTo(top);
    ridge.lineTo(QPointF(p.x() - size * .08, p.y() - size * .12));
    ridge.lineTo(QPointF(p.x() + size * .16, p.y() + size * .13));
    ridge.lineTo(QPointF(p.x() - size * .04, p.y() + size * .42));
    painter.drawPath(ridge);
    painter.setPen(QPen(QColor(245, 238, 220, 150), std::max(.55, size * .055), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(top, QPointF(p.x() - size * .18, p.y() - size * .25));
    painter.drawLine(top, QPointF(p.x() + size * .24, p.y() - size * .18));
    painter.restore();
}

} // namespace

CartographicCanvasV3::CartographicCanvasV3(PilinReyEditor* editor, QWidget* inputViewport, QWidget* parent)
    : QWidget(parent), editor_(editor), inputViewport_(inputViewport) {
    setObjectName(QStringLiteral("cartographicCanvasV3"));
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

void CartographicCanvasV3::setMap(const QJsonObject& map) {
    map_ = map;
    scheduleRebuild();
}

void CartographicCanvasV3::scheduleRebuild() {
    if (rebuildPending_) return;
    rebuildPending_ = true;
    QTimer::singleShot(110, this, [this]() {
        rebuildPending_ = false;
        rebuildCache();
        update();
    });
}

void CartographicCanvasV3::fitDocument() {
    const QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject();
    const double dw = std::max(1.0, p.value(QStringLiteral("width")).toDouble(4096));
    const double dh = std::max(1.0, p.value(QStringLiteral("height")).toDouble(2304));
    if (width() < 80 || height() < 80) return;
    zoom_ = std::clamp(std::min((width() - 48.0) / dw, (height() - 48.0) / dh), .02, 10.0);
    pan_ = QPointF((width() - dw * zoom_) * .5, (height() - dh * zoom_) * .5);
    update();
}

void CartographicCanvasV3::drawTerrainModel(QPainter& painter, const QJsonObject& pilin, int cw, int ch) {
    const QJsonObject model = pilin.value(QStringLiteral("terrainModel")).toObject();
    const int tw = model.value(QStringLiteral("width")).toInt();
    const int th = model.value(QStringLiteral("height")).toInt();
    const QJsonArray values = model.value(QStringLiteral("values")).toArray();
    const QJsonArray biomes = model.value(QStringLiteral("biomes")).toArray();
    const QJsonArray regions = model.value(QStringLiteral("regions")).toArray();
    if (tw < 4 || th < 4 || values.size() < tw * th) return;

    const double sea = std::clamp(model.value(QStringLiteral("seaLevel")).toDouble(.42), .05, .85);
    QImage field(tw, th, QImage::Format_ARGB32_Premultiplied);

    const QColor deep = themeColor(pilin, QStringLiteral("sea"), QColor("#456c82")).darker(135);
    const QColor shallow = themeColor(pilin, QStringLiteral("sea"), QColor("#8db4bd")).lighter(112);
    const QColor low = themeColor(pilin, QStringLiteral("land"), QColor("#c7b27e"));
    const QColor grass("#899a68"), forest("#4f6f4d"), woodland("#70865d"), desert("#c79e69"), high("#9b8061"), rock("#756b62"), snow("#dedbd1"), tundra("#aaad94");

    auto h = [&](int x, int y) {
        x = std::clamp(x, 0, tw - 1);
        y = std::clamp(y, 0, th - 1);
        return values.at(y * tw + x).toInt() / 255.0;
    };
    auto biomeAt = [&](int x, int y) {
        if (biomes.size() < tw * th) return 1;
        return biomes.at(std::clamp(y, 0, th - 1) * tw + std::clamp(x, 0, tw - 1)).toInt(1);
    };
    auto regionAt = [&](int x, int y) {
        if (regions.size() < tw * th) return -1;
        return regions.at(std::clamp(y, 0, th - 1) * tw + std::clamp(x, 0, tw - 1)).toInt(-1);
    };

    for (int y = 0; y < th; ++y) {
        QRgb* row = reinterpret_cast<QRgb*>(field.scanLine(y));
        for (int x = 0; x < tw; ++x) {
            const double v = h(x, y);
            QColor c;
            if (v < sea) {
                const double t = std::clamp(v / sea, 0.0, 1.0);
                c = mix(deep, shallow, std::pow(t, .72));
            } else {
                const int biome = biomeAt(x, y);
                if (biome == 2) c = forest;
                else if (biome == 3) c = woodland;
                else if (biome == 4) c = desert;
                else if (biome == 7) c = tundra;
                else if (v < .56) c = mix(low, grass, ((v - sea) / std::max(.01, .56 - sea)) * .72);
                else if (v < .70) c = mix(grass, high, (v - .56) / .14);
                else if (v < .84) c = mix(high, rock, (v - .70) / .14);
                else c = mix(rock, snow, (v - .84) / .16);

                const int region = regionAt(x, y);
                if (region >= 0) c = mix(c, regionColor(region), .065);
            }

            const double dx = h(x + 1, y) - h(x - 1, y);
            const double dy = h(x, y + 1) - h(x, y - 1);
            const double shade = std::clamp(.95 - dx * 1.18 - dy * .82, .68, 1.19);
            c.setRed(std::clamp(qRound(c.red() * shade), 0, 255));
            c.setGreen(std::clamp(qRound(c.green() * shade), 0, 255));
            c.setBlue(std::clamp(qRound(c.blue() * shade), 0, 255));
            row[x] = c.rgba();
        }
    }

    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(QRect(0, 0, cw, ch), field);
    painter.restore();

    const double sx = cw / static_cast<double>(tw);
    const double sy = ch / static_cast<double>(th);

    // Bathymetry bands: visually separate deep ocean from shelves without expensive blur passes.
    painter.save();
    painter.setBrush(Qt::NoBrush);
    for (const double band : {sea * .42, sea * .68, sea * .86}) {
        painter.setPen(QPen(QColor(210, 229, 232, 36), std::max(.55, sx * .16)));
        for (int y = 1; y < th - 1; ++y) {
            for (int x = 1; x < tw - 1; ++x) {
                const bool a = h(x, y) >= band;
                if (a != (h(x + 1, y) >= band)) painter.drawLine(QPointF((x + 1) * sx, y * sy), QPointF((x + 1) * sx, (y + 1) * sy));
                if (a != (h(x, y + 1) >= band)) painter.drawLine(QPointF(x * sx, (y + 1) * sy), QPointF((x + 1) * sx, (y + 1) * sy));
            }
        }
    }
    painter.restore();

    // Coastline gets a strong dark ink plus a soft shelf highlight, closer to illustrated atlases.
    painter.save();
    for (int pass = 0; pass < 2; ++pass) {
        painter.setPen(QPen(pass == 0 ? QColor(45, 39, 32, 205) : QColor(233, 224, 196, 92),
                            pass == 0 ? std::max(1.15, sx * .46) : std::max(.55, sx * .18)));
        for (int y = 0; y < th - 1; ++y) {
            for (int x = 0; x < tw - 1; ++x) {
                const bool a = h(x, y) >= sea;
                const bool b = h(x + 1, y) >= sea;
                const bool c = h(x, y + 1) >= sea;
                if (a != b) {
                    QPointF p1((x + 1) * sx, y * sy), p2((x + 1) * sx, (y + 1) * sy);
                    if (pass == 1) { p1 += QPointF(1.2, 1.2); p2 += QPointF(1.2, 1.2); }
                    painter.drawLine(p1, p2);
                }
                if (a != c) {
                    QPointF p1(x * sx, (y + 1) * sy), p2((x + 1) * sx, (y + 1) * sy);
                    if (pass == 1) { p1 += QPointF(1.2, 1.2); p2 += QPointF(1.2, 1.2); }
                    painter.drawLine(p1, p2);
                }
            }
        }
    }
    painter.restore();

    if (regions.size() >= tw * th) {
        painter.save();
        painter.setPen(QPen(QColor(58, 54, 58, 128), std::max(.7, sx * .24), Qt::DashLine, Qt::RoundCap));
        for (int y = 1; y < th - 1; ++y) {
            for (int x = 1; x < tw - 1; ++x) {
                const int r = regionAt(x, y);
                if (r < 0) continue;
                if (regionAt(x + 1, y) >= 0 && regionAt(x + 1, y) != r)
                    painter.drawLine(QPointF((x + 1) * sx, y * sy), QPointF((x + 1) * sx, (y + 1) * sy));
                if (regionAt(x, y + 1) >= 0 && regionAt(x, y + 1) != r)
                    painter.drawLine(QPointF(x * sx, (y + 1) * sy), QPointF((x + 1) * sx, (y + 1) * sy));
            }
        }
        painter.restore();
    }

    if (biomes.size() >= tw * th) {
        painter.save();
        for (int y = 4; y < th - 4; y += 5) {
            for (int x = 4; x < tw - 4; x += 5) {
                const int biome = biomeAt(x, y);
                if ((biome != 2 && biome != 3) || h(x, y) < sea + .02) continue;
                const int densityGate = biome == 2 ? 74 : 44;
                if (static_cast<int>(visualHash(x, y) % 100) > densityGate) continue;
                const double jx = (static_cast<int>(visualHash(x + 3, y + 9) % 100) / 100.0 - .5) * sx * 2.2;
                const double jy = (static_cast<int>(visualHash(x + 11, y + 5) % 100) / 100.0 - .5) * sy * 2.0;
                const QPointF p(x * sx + jx, y * sy + jy);
                const double size = std::clamp(2.7 + (visualHash(x + 7, y + 3) % 26) / 10.0, 2.7, 5.2);
                drawTree(painter, p, size, biome == 2 ? QColor(34, 67, 39, 172) : QColor(58, 82, 50, 142));
            }
        }
        painter.restore();
    }

    painter.save();
    for (int y = 4; y < th - 4; y += 5) {
        for (int x = 4; x < tw - 4; x += 5) {
            const double v = h(x, y);
            if (v < .72) continue;
            const double localSlope = std::hypot(h(x + 1, y) - h(x - 1, y), h(x, y + 1) - h(x, y - 1));
            if (localSlope < .045 && v < .82) continue;
            if (visualHash(x, y) % 100 > 54) continue;
            const QPointF p(x * sx, y * sy);
            const double size = std::clamp(4.8 + (v - .70) * 28.0, 5.0, 10.5);
            const double skew = (static_cast<int>(visualHash(x + 5, y + 13) % 100) / 100.0) * 2.0 - 1.0;
            drawMountain(painter, p, size, skew, QColor(55, 47, 40, 175));
        }
    }
    painter.restore();
}

void CartographicCanvasV3::rebuildCache() {
    const QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject();
    const double dw = std::max(1.0, p.value(QStringLiteral("width")).toDouble(4096));
    const double dh = std::max(1.0, p.value(QStringLiteral("height")).toDouble(2304));
    const double aspect = dw / dh;
    constexpr int maxSide = 1360;
    int cw = aspect >= 1.0 ? maxSide : static_cast<int>(maxSide * aspect);
    int ch = aspect >= 1.0 ? static_cast<int>(maxSide / aspect) : maxSide;
    cw = std::clamp(cw, 680, maxSide);
    ch = std::clamp(ch, 440, maxSide);

    cache_ = QImage(cw, ch, QImage::Format_ARGB32_Premultiplied);
    cache_.fill(themeColor(p, QStringLiteral("sea"), QColor("#7899a4")));
    QPainter painter(&cache_);
    painter.setRenderHint(QPainter::Antialiasing, true);
    if (p.contains(QStringLiteral("terrainModel"))) drawTerrainModel(painter, p, cw, ch);
    else painter.fillRect(cache_.rect(), themeColor(p, QStringLiteral("sea"), QColor("#7899a4")));

    const double sx = cw / dw, sy = ch / dh;
    for (const QJsonValue& lv : p.value(QStringLiteral("layers")).toArray()) {
        const QJsonObject layer = lv.toObject();
        if (!layer.value(QStringLiteral("visible")).toBool(true)) continue;
        painter.save();
        painter.setOpacity(std::clamp(layer.value(QStringLiteral("opacity")).toDouble(1.0), 0.0, 1.0));
        for (const QJsonValue& ov : layer.value(QStringLiteral("objects")).toArray()) drawObject(painter, ov.toObject(), p, sx, sy);
        painter.restore();
    }

    painter.setPen(QPen(QColor(48, 42, 34, 150), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(.5, .5, cw - 1, ch - 1));
}

void CartographicCanvasV3::drawObject(QPainter& painter, const QJsonObject& o, const QJsonObject& p, double sx, double sy) {
    const QString type = o.value(QStringLiteral("type")).toString();
    const QJsonArray points = o.value(QStringLiteral("points")).toArray();
    const double s = (sx + sy) * .5;

    if (type == QStringLiteral("river") || type == QStringLiteral("road") || type == QStringLiteral("border")) {
        const QPainterPath path = smoothPath(points, sx, sy);
        if (path.isEmpty()) return;
        if (type == QStringLiteral("river")) {
            painter.setPen(QPen(QColor(32, 78, 121, 220), std::max(1.6, o.value(QStringLiteral("width")).toDouble(4.0) * s * 1.28), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
            painter.setPen(QPen(QColor(150, 211, 228, 170), std::max(.8, o.value(QStringLiteral("width")).toDouble(4.0) * s * .55), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
        } else if (type == QStringLiteral("road")) {
            painter.setPen(QPen(QColor(240, 219, 173, 118), std::max(2.3, 5.0 * s), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
            painter.setPen(QPen(QColor(113, 77, 45, 230), std::max(1.1, 2.3 * s), Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
        } else {
            painter.setPen(QPen(QColor(69, 57, 52, 190), std::max(1.0, 2.0 * s), Qt::DashDotLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
        }
        return;
    }

    if (type == QStringLiteral("region") && points.size() >= 3) {
        QPainterPath path;
        QPointF first = pointFromJson(points.first());
        path.moveTo(first.x() * sx, first.y() * sy);
        for (int i = 1; i < points.size(); ++i) {
            QPointF v = pointFromJson(points.at(i));
            path.lineTo(v.x() * sx, v.y() * sy);
        }
        path.closeSubpath();
        painter.setPen(QPen(QColor(97, 67, 92, 180), std::max(1.0, 2 * s)));
        painter.setBrush(QColor(133, 81, 154, 22));
        painter.drawPath(path);
        return;
    }

    if (type == QStringLiteral("land") || type == QStringLiteral("sea")) {
        if (points.isEmpty()) return;
        const QPainterPath path = smoothPath(points, sx, sy);
        double r = 0;
        for (const QJsonValue& v : points) r += v.toObject().value(QStringLiteral("radius")).toDouble(220);
        r /= points.size();
        const double w = std::max(8.0, r * 2 * s);
        painter.setPen(QPen(type == QStringLiteral("land") ? themeColor(p, QStringLiteral("land"), QColor("#c9b787")) : themeColor(p, QStringLiteral("sea"), QColor("#7da2ad")), w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);
        return;
    }

    if (type == QStringLiteral("forestArea") || type == QStringLiteral("mountainArea")) {
        if (points.isEmpty()) return;
        const QPainterPath path = smoothPath(points, sx, sy);
        const QColor c = type == QStringLiteral("forestArea") ? QColor(46, 86, 52, 72) : QColor(87, 72, 61, 80);
        painter.setPen(QPen(c, std::max(12.0, 54.0 * s), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);
        return;
    }

    const double x = o.value(QStringLiteral("x")).toDouble() * sx;
    const double y = o.value(QStringLiteral("y")).toDouble() * sy;

    if (type == QStringLiteral("settlement")) {
        const double r = std::max(4.0, o.value(QStringLiteral("size")).toDouble(64) * s * .28);
        painter.setPen(QPen(QColor(52, 43, 36), std::max(1.0, 1.35 * s)));
        painter.setBrush(QColor(241, 220, 166));
        painter.drawEllipse(QPointF(x, y), r, r);
        painter.setBrush(QColor(79, 59, 43));
        painter.drawEllipse(QPointF(x, y), r * .28, r * .28);
        const QString label = o.value(QStringLiteral("label")).toString();
        if (!label.isEmpty()) {
            QFont f(QStringLiteral("Georgia"));
            f.setPointSizeF(std::clamp(11.0 * s, 7.5, 15.0));
            f.setBold(o.value(QStringLiteral("settlementType")).toString() == QObject::tr("Capital"));
            painter.setFont(f);
            const QPointF pos(x + r + 4, y - r * .2);
            painter.setPen(QPen(QColor(246, 236, 211, 220), 3.0));
            painter.drawText(pos + QPointF(1, 1), label);
            painter.setPen(QColor(44, 36, 29));
            painter.drawText(pos, label);
        }
        return;
    }

    if (type == QStringLiteral("stamp")) {
        const double r = std::max(7.0, o.value(QStringLiteral("size")).toDouble(80) * s * .34);
        painter.save();
        painter.translate(x, y);
        painter.rotate(o.value(QStringLiteral("rotation")).toDouble());
        painter.setPen(QPen(QColor(55, 45, 37), std::max(1.0, 1.3 * s)));
        painter.setBrush(QColor(226, 205, 159, 220));
        const QString kind = o.value(QStringLiteral("assetKind")).toString();
        if (kind == QStringLiteral("ship")) {
            painter.drawArc(QRectF(-r, -r * .15, r * 2, r * .9), 200 * 16, 140 * 16);
            painter.drawLine(QPointF(0, -r), QPointF(0, r * .4));
        } else {
            painter.drawRect(QRectF(-r * .55, -r * .25, r * 1.1, r * .7));
            painter.drawRect(QRectF(-r * .8, -r * .6, r * .32, r * 1.05));
            painter.drawRect(QRectF(r * .48, -r * .6, r * .32, r * 1.05));
        }
        painter.restore();
        return;
    }

    if (type == QStringLiteral("label")) {
        const QString text = o.value(QStringLiteral("text")).toString();
        if (text.isEmpty()) return;
        QFont f(QStringLiteral("Georgia"));
        f.setPointSizeF(std::clamp(o.value(QStringLiteral("fontSize")).toDouble(22) * s, 8.0, 32.0));
        f.setBold(o.value(QStringLiteral("bold")).toBool(false));
        painter.setFont(f);
        painter.setPen(QPen(QColor(245, 236, 210, 210), std::max(2.0, 3 * s)));
        painter.drawText(QPointF(x + 1, y + 1), text);
        painter.setPen(QColor(46, 38, 31));
        painter.drawText(QPointF(x, y), text);
    }
}

void CartographicCanvasV3::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#090d11"));
    if (cache_.isNull()) return;
    const QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject();
    const double dw = std::max(1.0, p.value(QStringLiteral("width")).toDouble(4096));
    const double dh = std::max(1.0, p.value(QStringLiteral("height")).toDouble(2304));
    const QRectF page(worldToScreen(QPointF(0, 0)), worldToScreen(QPointF(dw, dh)));
    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 115));
    painter.drawRoundedRect(page.translated(10, 12), 8, 8);
    painter.restore();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(page, cache_);
}

void CartographicCanvasV3::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    if (pan_.isNull()) fitDocument();
}

void CartographicCanvasV3::wheelEvent(QWheelEvent* e) {
    const QPointF cursor = e->position();
    const QPointF before = screenToWorld(cursor);
    zoom_ = std::clamp(zoom_ * (e->angleDelta().y() > 0 ? 1.12 : 1.0 / 1.12), .02, 12.0);
    pan_ = cursor - QPointF(before.x() * zoom_, before.y() * zoom_);
    update();
    e->accept();
}

QPointF CartographicCanvasV3::screenToWorld(const QPointF& p) const {
    return QPointF((p.x() - pan_.x()) / zoom_, (p.y() - pan_.y()) / zoom_);
}

QPointF CartographicCanvasV3::worldToScreen(const QPointF& p) const {
    return QPointF(p.x() * zoom_ + pan_.x(), p.y() * zoom_ + pan_.y());
}

QPointF CartographicCanvasV3::worldToInputScreen(const QPointF& world) const {
    if (!inputViewport_) return world;
    const QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject();
    const double dw = std::max(1.0, p.value(QStringLiteral("width")).toDouble(4096));
    const double dh = std::max(1.0, p.value(QStringLiteral("height")).toDouble(2304));
    const double z = std::clamp(std::min((inputViewport_->width() - 80.0) / dw, (inputViewport_->height() - 80.0) / dh), .03, 6.0);
    const QPointF base((inputViewport_->width() - dw * z) * .5, (inputViewport_->height() - dh * z) * .5);
    return QPointF(world.x() * z + base.x(), world.y() * z + base.y());
}

bool CartographicCanvasV3::panMode() const {
    if (!editor_) return false;
    for (QToolButton* b : editor_->findChildren<QToolButton*>(QStringLiteral("pilinMapTool")))
        if (b->isChecked() && b->toolTip().contains(QObject::tr("Mover lienzo"), Qt::CaseInsensitive)) return true;
    return false;
}

void CartographicCanvasV3::forwardMouse(QMouseEvent* e) {
    if (!inputViewport_) return;
    const QPointF world = screenToWorld(e->position());
    const QPointF local = worldToInputScreen(world);
    QMouseEvent forwarded(e->type(), local, e->globalPosition(), e->button(), e->buttons(), e->modifiers(), e->pointingDevice());
    QApplication::sendEvent(inputViewport_, &forwarded);
}

void CartographicCanvasV3::mousePressEvent(QMouseEvent* e) {
    setFocus(Qt::MouseFocusReason);
    if (e->button() == Qt::MiddleButton || panMode()) {
        panning_ = true;
        lastMouse_ = e->position();
        setCursor(Qt::ClosedHandCursor);
        e->accept();
        return;
    }
    forwardMouse(e);
}

void CartographicCanvasV3::mouseMoveEvent(QMouseEvent* e) {
    if (panning_) {
        pan_ += e->position() - lastMouse_;
        lastMouse_ = e->position();
        update();
        e->accept();
        return;
    }
    forwardMouse(e);
}

void CartographicCanvasV3::mouseReleaseEvent(QMouseEvent* e) {
    if (panning_) {
        panning_ = false;
        setCursor(Qt::ArrowCursor);
        e->accept();
        return;
    }
    forwardMouse(e);
}

void CartographicCanvasV3::mouseDoubleClickEvent(QMouseEvent* e) {
    forwardMouse(e);
}

void CartographicCanvasV3::keyPressEvent(QKeyEvent* e) {
    if (inputViewport_) {
        QKeyEvent forwarded(e->type(), e->key(), e->modifiers(), e->text(), e->isAutoRepeat(), e->count());
        QApplication::sendEvent(inputViewport_, &forwarded);
        if (forwarded.isAccepted()) {
            e->accept();
            return;
        }
    }
    QWidget::keyPressEvent(e);
}

} // namespace wbw
