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

QPointF jsonPoint(const QJsonValue& value) {
    const QJsonObject p = value.toObject();
    return QPointF(p.value(QStringLiteral("x")).toDouble(), p.value(QStringLiteral("y")).toDouble());
}

QColor themeColor(const QJsonObject& pilin, const QString& key, const QColor& fallback) {
    QColor color(pilin.value(QStringLiteral("theme")).toObject().value(key).toString());
    return color.isValid() ? color : fallback;
}

QColor mix(const QColor& a, const QColor& b, double t) {
    t = std::clamp(t, 0.0, 1.0);
    return QColor(qRound(a.red() * (1.0 - t) + b.red() * t),
                  qRound(a.green() * (1.0 - t) + b.green() * t),
                  qRound(a.blue() * (1.0 - t) + b.blue() * t));
}

quint32 hash2(int x, int y) {
    quint32 h = static_cast<quint32>(x * 374761393u) ^ static_cast<quint32>(y * 668265263u) ^ 0x9e3779b9u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    return h ^ (h >> 16u);
}

QColor regionTint(int index) {
    static const QColor colors[]{QColor("#9b6660"), QColor("#6381a0"), QColor("#9a805e"), QColor("#668760"),
                                 QColor("#806d91"), QColor("#a87a5c"), QColor("#5e8987"), QColor("#976f80")};
    return colors[std::abs(index) % 8];
}

QPainterPath smoothPath(const QJsonArray& points, double sx, double sy) {
    QPainterPath path;
    if (points.isEmpty()) return path;
    QPointF previous = jsonPoint(points.first());
    previous = QPointF(previous.x() * sx, previous.y() * sy);
    path.moveTo(previous);
    for (int i = 1; i < points.size(); ++i) {
        QPointF current = jsonPoint(points.at(i));
        current = QPointF(current.x() * sx, current.y() * sy);
        const QPointF mid = (previous + current) * .5;
        path.quadTo(previous, mid);
        previous = current;
    }
    path.lineTo(previous);
    return path;
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
        if (!isVisible()) return;
        rebuildCache();
        update();
    });
}

void CartographicCanvasV3::fitDocument() {
    const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
    const double dw = std::max(1.0, pilin.value(QStringLiteral("width")).toDouble(4096));
    const double dh = std::max(1.0, pilin.value(QStringLiteral("height")).toDouble(2304));
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
    const QColor deepSea = themeColor(pilin, QStringLiteral("sea"), QColor("#567f92")).darker(145);
    const QColor midSea = themeColor(pilin, QStringLiteral("sea"), QColor("#6f99aa"));
    const QColor shallowSea = QColor("#a7c7c8");
    const QColor beach("#c9b889");
    const QColor meadow("#8da06d");
    const QColor forest("#536f4d");
    const QColor woodland("#71865b");
    const QColor dry("#bb9868");
    const QColor upland("#927b5d");
    const QColor rock("#746c63");
    const QColor snow("#dddcd5");
    const QColor tundra("#a4a88e");

    auto heightAt = [&](int x, int y) {
        x = std::clamp(x, 0, tw - 1);
        y = std::clamp(y, 0, th - 1);
        return values.at(y * tw + x).toInt() / 255.0;
    };
    auto biomeAt = [&](int x, int y) {
        if (biomes.size() < tw * th) return 1;
        x = std::clamp(x, 0, tw - 1); y = std::clamp(y, 0, th - 1);
        return biomes.at(y * tw + x).toInt(1);
    };
    auto regionAt = [&](int x, int y) {
        if (regions.size() < tw * th) return -1;
        x = std::clamp(x, 0, tw - 1); y = std::clamp(y, 0, th - 1);
        return regions.at(y * tw + x).toInt(-1);
    };

    QImage field(tw, th, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < th; ++y) {
        QRgb* row = reinterpret_cast<QRgb*>(field.scanLine(y));
        for (int x = 0; x < tw; ++x) {
            const double h = heightAt(x, y);
            QColor color;
            if (h < sea) {
                const double depth = std::clamp(h / sea, 0.0, 1.0);
                color = depth < .65 ? mix(deepSea, midSea, depth / .65) : mix(midSea, shallowSea, (depth - .65) / .35);
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

                const int region = regionAt(x, y);
                if (region >= 0) color = mix(color, regionTint(region), .07);
            }

            // Continuous hillshade, not repeated mountain glyphs.
            const double dx = heightAt(x + 1, y) - heightAt(x - 1, y);
            const double dy = heightAt(x, y + 1) - heightAt(x, y - 1);
            const double shade = std::clamp(.96 - dx * 1.25 - dy * .82, .66, 1.22);
            color.setRed(std::clamp(qRound(color.red() * shade), 0, 255));
            color.setGreen(std::clamp(qRound(color.green() * shade), 0, 255));
            color.setBlue(std::clamp(qRound(color.blue() * shade), 0, 255));
            row[x] = color.rgba();
        }
    }

    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(QRect(0, 0, cw, ch), field);
    painter.restore();

    const double sx = cw / static_cast<double>(tw);
    const double sy = ch / static_cast<double>(th);

    // Coastline and lake shores.
    painter.save();
    painter.setPen(QPen(QColor(45, 42, 37, 175), std::max(.75, sx * .34), Qt::SolidLine, Qt::RoundCap));
    for (int y = 0; y < th - 1; ++y) {
        for (int x = 0; x < tw - 1; ++x) {
            const bool land = heightAt(x, y) >= sea;
            if (land != (heightAt(x + 1, y) >= sea)) painter.drawLine(QPointF((x + 1) * sx, y * sy), QPointF((x + 1) * sx, (y + 1) * sy));
            if (land != (heightAt(x, y + 1) >= sea)) painter.drawLine(QPointF(x * sx, (y + 1) * sy), QPointF((x + 1) * sx, (y + 1) * sy));
        }
    }
    painter.restore();

    // Sparse elevation contours give readable relief without the old hatch/symbol explosion.
    painter.save();
    painter.setPen(QPen(QColor(67, 59, 49, 52), std::max(.45, sx * .16)));
    constexpr double contourStep = .055;
    for (int y = 1; y < th - 1; ++y) {
        for (int x = 1; x < tw - 1; ++x) {
            const double h = heightAt(x, y);
            if (h < sea + .05) continue;
            const int band = static_cast<int>((h - sea) / contourStep);
            const int right = static_cast<int>((heightAt(x + 1, y) - sea) / contourStep);
            const int down = static_cast<int>((heightAt(x, y + 1) - sea) / contourStep);
            if (right != band) painter.drawLine(QPointF((x + 1) * sx, y * sy), QPointF((x + 1) * sx, (y + 1) * sy));
            if (down != band) painter.drawLine(QPointF(x * sx, (y + 1) * sy), QPointF((x + 1) * sx, (y + 1) * sy));
        }
    }
    painter.restore();

    // Political borders from generated region raster.
    if (regions.size() >= tw * th) {
        painter.save();
        painter.setPen(QPen(QColor(63, 57, 67, 120), std::max(.7, sx * .24), Qt::DashLine));
        for (int y = 1; y < th - 1; ++y) {
            for (int x = 1; x < tw - 1; ++x) {
                const int region = regionAt(x, y);
                if (region < 0) continue;
                if (regionAt(x + 1, y) >= 0 && regionAt(x + 1, y) != region) painter.drawLine(QPointF((x + 1) * sx, y * sy), QPointF((x + 1) * sx, (y + 1) * sy));
                if (regionAt(x, y + 1) >= 0 && regionAt(x, y + 1) != region) painter.drawLine(QPointF(x * sx, (y + 1) * sy), QPointF((x + 1) * sx, (y + 1) * sy));
            }
        }
        painter.restore();
    }

    // Forest texture as sparse canopy clusters instead of thousands of V-shaped trees.
    if (biomes.size() >= tw * th) {
        painter.save();
        painter.setPen(QPen(QColor(35, 62, 38, 80), .65));
        painter.setBrush(QColor(42, 74, 44, 58));
        for (int y = 4; y < th - 4; y += 6) {
            for (int x = 4; x < tw - 4; x += 6) {
                const int biome = biomeAt(x, y);
                if (biome != 2 && biome != 3) continue;
                const quint32 h = hash2(x, y);
                if ((h % 100) > (biome == 2 ? 58u : 31u)) continue;
                const QPointF center((x + ((h >> 8) % 5 - 2) * .28) * sx, (y + ((h >> 12) % 5 - 2) * .28) * sy);
                const double r = std::clamp(1.2 + (h % 35) / 22.0, 1.2, 2.8);
                painter.drawEllipse(center, r * sx * .55, r * sy * .43);
            }
        }
        painter.restore();
    }
}

void CartographicCanvasV3::rebuildCache() {
    const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
    const double dw = std::max(1.0, pilin.value(QStringLiteral("width")).toDouble(4096));
    const double dh = std::max(1.0, pilin.value(QStringLiteral("height")).toDouble(2304));
    const double aspect = dw / dh;
    constexpr int maxSide = 1280;
    int cw = aspect >= 1.0 ? maxSide : static_cast<int>(maxSide * aspect);
    int ch = aspect >= 1.0 ? static_cast<int>(maxSide / aspect) : maxSide;
    cw = std::clamp(cw, 640, maxSide);
    ch = std::clamp(ch, 420, maxSide);

    cache_ = QImage(cw, ch, QImage::Format_ARGB32_Premultiplied);
    cache_.fill(themeColor(pilin, QStringLiteral("sea"), QColor("#7899a4")));
    QPainter painter(&cache_);
    painter.setRenderHint(QPainter::Antialiasing, true);
    if (pilin.contains(QStringLiteral("terrainModel"))) drawTerrainModel(painter, pilin, cw, ch);

    const double sx = cw / dw, sy = ch / dh;
    for (const QJsonValue& layerValue : pilin.value(QStringLiteral("layers")).toArray()) {
        const QJsonObject layer = layerValue.toObject();
        if (!layer.value(QStringLiteral("visible")).toBool(true)) continue;
        painter.save();
        painter.setOpacity(std::clamp(layer.value(QStringLiteral("opacity")).toDouble(1.0), 0.0, 1.0));
        for (const QJsonValue& objectValue : layer.value(QStringLiteral("objects")).toArray()) drawObject(painter, objectValue.toObject(), pilin, sx, sy);
        painter.restore();
    }

    painter.setPen(QPen(QColor(48, 42, 34, 145), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(.5, .5, cw - 1, ch - 1));
}

void CartographicCanvasV3::drawObject(QPainter& painter, const QJsonObject& object, const QJsonObject& pilin, double sx, double sy) {
    const QString type = object.value(QStringLiteral("type")).toString();
    const QJsonArray points = object.value(QStringLiteral("points")).toArray();
    const double scale = (sx + sy) * .5;

    if (type == QStringLiteral("river") || type == QStringLiteral("road") || type == QStringLiteral("border")) {
        const QPainterPath path = smoothPath(points, sx, sy);
        if (path.isEmpty()) return;
        if (type == QStringLiteral("river")) {
            painter.setPen(QPen(QColor(48, 111, 158, 235), std::max(1.1, object.value(QStringLiteral("width")).toDouble(4.0) * scale), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
            painter.setPen(QPen(QColor(197, 234, 239, 95), std::max(.55, .8 * scale), Qt::SolidLine, Qt::RoundCap));
            painter.drawPath(path);
        } else if (type == QStringLiteral("road")) {
            painter.setPen(QPen(QColor(116, 80, 49, 225), std::max(1.0, 2.6 * scale), Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
        } else {
            painter.setPen(QPen(QColor(73, 61, 57, 205), std::max(.9, 1.8 * scale), Qt::DashDotLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
        }
        return;
    }

    if (type == QStringLiteral("region") && points.size() >= 3) {
        QPainterPath path;
        QPointF first = jsonPoint(points.first()); path.moveTo(first.x() * sx, first.y() * sy);
        for (int i = 1; i < points.size(); ++i) { const QPointF p = jsonPoint(points.at(i)); path.lineTo(p.x() * sx, p.y() * sy); }
        path.closeSubpath();
        painter.setPen(QPen(QColor(94, 70, 95, 180), std::max(.9, 1.6 * scale), Qt::DashLine));
        painter.setBrush(QColor(133, 81, 154, 22));
        painter.drawPath(path);
        return;
    }

    if ((type == QStringLiteral("land") || type == QStringLiteral("sea") || type == QStringLiteral("forestArea") || type == QStringLiteral("mountainArea")) && !points.isEmpty()) {
        // Temporary edit preview. The terrain synchronizer bakes terrain/relief strokes into the heightmap.
        const QPainterPath path = smoothPath(points, sx, sy);
        double radius = 0.0;
        for (const QJsonValue& value : points) radius += value.toObject().value(QStringLiteral("radius")).toDouble(220.0);
        radius /= points.size();
        QColor color;
        if (type == QStringLiteral("land")) color = QColor(194, 178, 128, 150);
        else if (type == QStringLiteral("sea")) color = QColor(92, 145, 164, 155);
        else if (type == QStringLiteral("forestArea")) color = QColor(45, 84, 49, 110);
        else color = QColor(104, 88, 70, 105);
        painter.setPen(QPen(color, std::max(8.0, radius * 2.0 * scale), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);
        return;
    }

    const double x = object.value(QStringLiteral("x")).toDouble() * sx;
    const double y = object.value(QStringLiteral("y")).toDouble() * sy;
    if (type == QStringLiteral("settlement")) {
        const QString settlementType = object.value(QStringLiteral("settlementType")).toString();
        const bool capital = settlementType.contains(QStringLiteral("Capital"), Qt::CaseInsensitive);
        const double r = std::max(3.4, object.value(QStringLiteral("size")).toDouble(64) * scale * (capital ? .34 : .25));
        painter.setPen(QPen(QColor(54, 46, 38), std::max(.9, 1.2 * scale)));
        painter.setBrush(capital ? QColor(238, 205, 122) : QColor(238, 221, 177));
        painter.drawEllipse(QPointF(x, y), r, r);
        if (capital) painter.drawEllipse(QPointF(x, y), r * .42, r * .42);
        const QString text = object.value(QStringLiteral("label")).toString();
        if (!text.isEmpty()) {
            QFont font(QStringLiteral("Georgia")); font.setPointSizeF(std::clamp(10.5 * scale, 7.0, 14.0)); font.setBold(capital); painter.setFont(font);
            const QPointF pos(x + r + 4, y - r * .15);
            painter.setPen(QPen(QColor(246, 238, 217, 210), 2.6)); painter.drawText(pos + QPointF(1, 1), text);
            painter.setPen(QColor(45, 38, 31)); painter.drawText(pos, text);
        }
        return;
    }

    if (type == QStringLiteral("stamp")) {
        const double r = std::max(6.0, object.value(QStringLiteral("size")).toDouble(80) * scale * .30);
        painter.save(); painter.translate(x, y); painter.rotate(object.value(QStringLiteral("rotation")).toDouble());
        painter.setPen(QPen(QColor(55, 45, 37), std::max(.8, 1.1 * scale))); painter.setBrush(QColor(225, 205, 160, 220));
        const QString kind = object.value(QStringLiteral("assetKind")).toString();
        if (kind == QStringLiteral("ship")) {
            painter.drawArc(QRectF(-r, -r * .15, r * 2, r * .9), 200 * 16, 140 * 16); painter.drawLine(QPointF(0, -r), QPointF(0, r * .4));
        } else {
            painter.drawRect(QRectF(-r * .55, -r * .25, r * 1.1, r * .7)); painter.drawRect(QRectF(-r * .8, -r * .6, r * .32, r * 1.05)); painter.drawRect(QRectF(r * .48, -r * .6, r * .32, r * 1.05));
        }
        painter.restore();
        return;
    }

    if (type == QStringLiteral("label")) {
        const QString text = object.value(QStringLiteral("text")).toString();
        if (text.isEmpty()) return;
        QFont font(QStringLiteral("Georgia")); font.setPointSizeF(std::clamp(object.value(QStringLiteral("fontSize")).toDouble(22) * scale, 8.0, 30.0)); font.setBold(object.value(QStringLiteral("bold")).toBool(false)); painter.setFont(font);
        painter.setPen(QPen(QColor(245, 236, 211, 195), std::max(1.8, 2.7 * scale))); painter.drawText(QPointF(x + 1, y + 1), text);
        painter.setPen(QColor(47, 40, 34)); painter.drawText(QPointF(x, y), text);
    }
}

void CartographicCanvasV3::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#090d11"));
    if (cache_.isNull()) return;
    const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
    const double dw = std::max(1.0, pilin.value(QStringLiteral("width")).toDouble(4096));
    const double dh = std::max(1.0, pilin.value(QStringLiteral("height")).toDouble(2304));
    const QRectF page(worldToScreen(QPointF(0, 0)), worldToScreen(QPointF(dw, dh)));
    painter.setPen(Qt::NoPen); painter.setBrush(QColor(0, 0, 0, 110)); painter.drawRoundedRect(page.translated(9, 11), 8, 8);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true); painter.drawImage(page, cache_);
}

void CartographicCanvasV3::resizeEvent(QResizeEvent* event) { QWidget::resizeEvent(event); if (pan_.isNull()) fitDocument(); }
void CartographicCanvasV3::wheelEvent(QWheelEvent* event) { const QPointF cursor = event->position(); const QPointF before = screenToWorld(cursor); zoom_ = std::clamp(zoom_ * (event->angleDelta().y() > 0 ? 1.12 : 1.0 / 1.12), .02, 12.0); pan_ = cursor - QPointF(before.x() * zoom_, before.y() * zoom_); update(); event->accept(); }
QPointF CartographicCanvasV3::screenToWorld(const QPointF& p) const { return QPointF((p.x() - pan_.x()) / zoom_, (p.y() - pan_.y()) / zoom_); }
QPointF CartographicCanvasV3::worldToScreen(const QPointF& p) const { return QPointF(p.x() * zoom_ + pan_.x(), p.y() * zoom_ + pan_.y()); }
QPointF CartographicCanvasV3::worldToInputScreen(const QPointF& world) const {
    if (!inputViewport_) return world;
    const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
    const double dw = std::max(1.0, pilin.value(QStringLiteral("width")).toDouble(4096));
    const double dh = std::max(1.0, pilin.value(QStringLiteral("height")).toDouble(2304));
    const double z = std::clamp(std::min((inputViewport_->width() - 80.0) / dw, (inputViewport_->height() - 80.0) / dh), .03, 6.0);
    const QPointF base((inputViewport_->width() - dw * z) * .5, (inputViewport_->height() - dh * z) * .5);
    return QPointF(world.x() * z + base.x(), world.y() * z + base.y());
}
bool CartographicCanvasV3::panMode() const { if (!editor_) return false; for (QToolButton* b : editor_->findChildren<QToolButton*>(QStringLiteral("pilinMapTool"))) if (b->isChecked() && b->toolTip().contains(QObject::tr("Mover lienzo"), Qt::CaseInsensitive)) return true; return false; }
void CartographicCanvasV3::forwardMouse(QMouseEvent* event) { if (!inputViewport_) return; const QPointF local = worldToInputScreen(screenToWorld(event->position())); QMouseEvent forwarded(event->type(), local, event->globalPosition(), event->button(), event->buttons(), event->modifiers(), event->pointingDevice()); QApplication::sendEvent(inputViewport_, &forwarded); }
void CartographicCanvasV3::mousePressEvent(QMouseEvent* event) { setFocus(Qt::MouseFocusReason); if (event->button() == Qt::MiddleButton || panMode()) { panning_ = true; lastMouse_ = event->position(); setCursor(Qt::ClosedHandCursor); event->accept(); return; } forwardMouse(event); }
void CartographicCanvasV3::mouseMoveEvent(QMouseEvent* event) { if (panning_) { pan_ += event->position() - lastMouse_; lastMouse_ = event->position(); update(); event->accept(); return; } forwardMouse(event); }
void CartographicCanvasV3::mouseReleaseEvent(QMouseEvent* event) { if (panning_) { panning_ = false; setCursor(Qt::ArrowCursor); event->accept(); return; } forwardMouse(event); }
void CartographicCanvasV3::mouseDoubleClickEvent(QMouseEvent* event) { forwardMouse(event); }
void CartographicCanvasV3::keyPressEvent(QKeyEvent* event) { if (inputViewport_) { QKeyEvent forwarded(event->type(), event->key(), event->modifiers(), event->text(), event->isAutoRepeat(), event->count()); QApplication::sendEvent(inputViewport_, &forwarded); if (forwarded.isAccepted()) { event->accept(); return; } } QWidget::keyPressEvent(event); }

} // namespace wbw
