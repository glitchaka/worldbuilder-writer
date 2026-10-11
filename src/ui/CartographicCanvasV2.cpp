#include "ui/CartographicCanvasV2.h"
#include "ui/PilinReyEditor.h"

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

double pointRadius(const QJsonValue& value, double fallback) {
    return value.toObject().value(QStringLiteral("radius")).toDouble(fallback);
}

QColor themeColor(const QJsonObject& pilin, const QString& key, const QColor& fallback) {
    const QColor c(pilin.value(QStringLiteral("theme")).toObject().value(key).toString());
    return c.isValid() ? c : fallback;
}

QPainterPath smoothPolyline(const QJsonArray& points, double sx, double sy) {
    QPainterPath path;
    if (points.isEmpty()) return path;
    QPointF prev(pointFromJson(points.first()).x() * sx, pointFromJson(points.first()).y() * sy);
    path.moveTo(prev);
    for (int i = 1; i < points.size(); ++i) {
        const QPointF w = pointFromJson(points.at(i));
        const QPointF cur(w.x() * sx, w.y() * sy);
        const QPointF mid = (prev + cur) * 0.5;
        path.quadTo(prev, mid);
        prev = cur;
    }
    path.lineTo(prev);
    return path;
}

quint32 seeded(const QString& id, int salt) {
    quint32 h = qHash(id) ^ static_cast<quint32>(salt * 2654435761u);
    h ^= h >> 16; h *= 0x7feb352dU; h ^= h >> 15; h *= 0x846ca68bU; h ^= h >> 16;
    return h;
}

double jitter(const QString& id, int salt) {
    return (seeded(id, salt) & 0xffffu) / 65535.0 - 0.5;
}

} // namespace

CartographicCanvasV2::CartographicCanvasV2(PilinReyEditor* editor, QWidget* inputViewport, QWidget* parent)
    : QWidget(parent), editor_(editor), inputViewport_(inputViewport) {
    setObjectName(QStringLiteral("cartographicCanvasV2"));
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

void CartographicCanvasV2::setMap(const QJsonObject& map) {
    map_ = map;
    scheduleRebuild();
}

void CartographicCanvasV2::scheduleRebuild() {
    if (rebuildPending_) return;
    rebuildPending_ = true;
    QTimer::singleShot(35, this, [this]() {
        rebuildPending_ = false;
        rebuildCache();
        update();
    });
}

void CartographicCanvasV2::fitDocument() {
    const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
    const double docW = std::max(1.0, pilin.value(QStringLiteral("width")).toDouble(4096));
    const double docH = std::max(1.0, pilin.value(QStringLiteral("height")).toDouble(2304));
    if (width() < 80 || height() < 80) return;
    zoom_ = std::clamp(std::min((width() - 56.0) / docW, (height() - 56.0) / docH), 0.02, 10.0);
    pan_ = QPointF((width() - docW * zoom_) * 0.5, (height() - docH * zoom_) * 0.5);
    update();
}

void CartographicCanvasV2::rebuildCache() {
    const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
    const double docW = std::max(1.0, pilin.value(QStringLiteral("width")).toDouble(4096));
    const double docH = std::max(1.0, pilin.value(QStringLiteral("height")).toDouble(2304));

    const double aspect = docW / docH;
    int cw = aspect >= 1.0 ? 1800 : static_cast<int>(1800 * aspect);
    int ch = aspect >= 1.0 ? static_cast<int>(1800 / aspect) : 1800;
    cw = std::clamp(cw, 640, 1800);
    ch = std::clamp(ch, 420, 1800);

    cache_ = QImage(cw, ch, QImage::Format_ARGB32_Premultiplied);
    cache_.fill(themeColor(pilin, QStringLiteral("sea"), QColor(QStringLiteral("#afc5c3"))));

    QPainter painter(&cache_);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const double sx = cw / docW;
    const double sy = ch / docH;

    painter.save();
    painter.setPen(QPen(QColor(45, 63, 63, 26), 1.0));
    for (int y = 22; y < ch; y += 30) {
        QPainterPath wave;
        wave.moveTo(0, y);
        for (int x = 0; x < cw; x += 48) wave.cubicTo(x + 12, y - 2.5, x + 26, y + 2.5, x + 48, y);
        painter.drawPath(wave);
    }
    painter.restore();

    const QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();
    for (const QJsonValue& layerValue : layers) {
        const QJsonObject layer = layerValue.toObject();
        if (!layer.value(QStringLiteral("visible")).toBool(true)) continue;
        painter.save();
        painter.setOpacity(std::clamp(layer.value(QStringLiteral("opacity")).toDouble(1.0), 0.0, 1.0));
        const QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
        for (const QJsonValue& objectValue : objects) drawObject(painter, objectValue.toObject(), pilin, sx, sy);
        painter.restore();
    }

    painter.setPen(QPen(QColor(59, 48, 36, 110), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(0.5, 0.5, cw - 1.0, ch - 1.0));
}

void CartographicCanvasV2::drawObject(QPainter& painter, const QJsonObject& object, const QJsonObject& pilin, double sx, double sy) {
    const QString type = object.value(QStringLiteral("type")).toString();
    const QString id = object.value(QStringLiteral("id")).toString(type);
    const QJsonArray points = object.value(QStringLiteral("points")).toArray();
    const double s = (sx + sy) * 0.5;

    const QColor ink = themeColor(pilin, QStringLiteral("ink"), QColor(QStringLiteral("#40362c")));
    const QColor land = themeColor(pilin, QStringLiteral("land"), QColor(QStringLiteral("#d8c79e")));
    const QColor sea = themeColor(pilin, QStringLiteral("sea"), QColor(QStringLiteral("#afc5c3")));
    const QColor forest = themeColor(pilin, QStringLiteral("forest"), QColor(QStringLiteral("#55785a")));
    const QColor mountain = themeColor(pilin, QStringLiteral("mountain"), QColor(QStringLiteral("#6b6258")));

    if (type == QStringLiteral("land") || type == QStringLiteral("sea")) {
        if (points.isEmpty()) return;
        double avgRadius = 0.0;
        for (const QJsonValue& v : points) avgRadius += pointRadius(v, 220.0);
        avgRadius /= points.size();
        const double width = std::max(8.0, avgRadius * 2.0 * s);
        const QPainterPath path = smoothPolyline(points, sx, sy);
        painter.save();
        painter.setBrush(Qt::NoBrush);
        if (type == QStringLiteral("land")) {
            painter.setPen(QPen(QColor(58, 47, 35, 165), width + std::max(2.0, 4.0 * s), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
            painter.setPen(QPen(land, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
        } else {
            painter.setPen(QPen(sea, width + std::max(2.0, 3.0 * s), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
        }
        painter.restore();
        return;
    }

    if (type == QStringLiteral("region") && points.size() >= 3) {
        QPainterPath region;
        const QPointF first = pointFromJson(points.first());
        region.moveTo(first.x() * sx, first.y() * sy);
        for (int i = 1; i < points.size(); ++i) {
            const QPointF p = pointFromJson(points.at(i));
            region.lineTo(p.x() * sx, p.y() * sy);
        }
        region.closeSubpath();
        painter.setPen(QPen(QColor(104, 74, 45, 160), std::max(1.0, 2.0 * s)));
        painter.setBrush(QColor(166, 119, 70, 34));
        painter.drawPath(region);
        return;
    }

    if (type == QStringLiteral("river") || type == QStringLiteral("road") || type == QStringLiteral("border")) {
        const QPainterPath path = smoothPolyline(points, sx, sy);
        if (path.isEmpty()) return;
        if (type == QStringLiteral("river")) {
            painter.setPen(QPen(QColor(61, 95, 111, 210), std::max(1.4, object.value(QStringLiteral("width")).toDouble(5.0) * s), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
            painter.setPen(QPen(QColor(201, 221, 220, 120), std::max(0.8, 1.2 * s), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
        } else if (type == QStringLiteral("road")) {
            painter.setPen(QPen(QColor(133, 91, 54, 210), std::max(1.2, 3.2 * s), Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
        } else {
            painter.setPen(QPen(QColor(82, 64, 49, 190), std::max(1.0, 2.2 * s), Qt::DashDotLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
        }
        return;
    }

    if ((type == QStringLiteral("forestArea") || type == QStringLiteral("mountainArea")) && !points.isEmpty()) {
        const int maxSymbols = type == QStringLiteral("forestArea") ? 72 : 42;
        const int count = std::min(maxSymbols, std::max(8, points.size() * 2));
        for (int i = 0; i < count; ++i) {
            const int pi = (i * points.size()) / count;
            const QPointF w = pointFromJson(points.at(std::min(pi, points.size() - 1)));
            const double radius = pointRadius(points.at(std::min(pi, points.size() - 1)), 180.0);
            const double jx = jitter(id, i * 2 + 1) * radius * 1.25;
            const double jy = jitter(id, i * 2 + 2) * radius * 1.25;
            const QPointF p((w.x() + jx) * sx, (w.y() + jy) * sy);
            if (type == QStringLiteral("forestArea")) drawTree(painter, p, std::max(6.0, 19.0 * s), forest.darker(125));
            else drawMountain(painter, p, std::max(8.0, 28.0 * s), mountain.darker(112));
        }
        return;
    }

    const double x = object.value(QStringLiteral("x")).toDouble() * sx;
    const double y = object.value(QStringLiteral("y")).toDouble() * sy;
    if (type == QStringLiteral("settlement")) {
        const double size = std::max(5.0, object.value(QStringLiteral("size")).toDouble(64.0) * s * 0.35);
        painter.setPen(QPen(ink, std::max(1.2, 1.8 * s)));
        painter.setBrush(QColor(238, 226, 194, 220));
        painter.drawEllipse(QPointF(x, y), size, size);
        painter.drawLine(QPointF(x - size * 1.5, y), QPointF(x + size * 1.5, y));
        return;
    }

    if (type == QStringLiteral("stamp")) {
        const double size = std::max(10.0, object.value(QStringLiteral("size")).toDouble(80.0) * s * 0.45);
        painter.save();
        painter.translate(x, y);
        painter.rotate(object.value(QStringLiteral("rotation")).toDouble());
        painter.setPen(QPen(ink, std::max(1.1, 1.6 * s)));
        painter.setBrush(QColor(236, 221, 184, 200));
        const QString kind = object.value(QStringLiteral("assetKind")).toString();
        if (kind == QStringLiteral("ship")) {
            painter.drawArc(QRectF(-size, -size * .15, size * 2.0, size * .9), 200 * 16, 140 * 16);
            painter.drawLine(QPointF(0, -size), QPointF(0, size * .45));
            QPainterPath sail; sail.moveTo(0, -size * .92); sail.lineTo(size * .72, -size * .15); sail.lineTo(0, -size * .05); sail.closeSubpath(); painter.drawPath(sail);
        } else {
            painter.drawRect(QRectF(-size * .6, -size * .35, size * 1.2, size * .85));
            painter.drawRect(QRectF(-size * .85, -size * .65, size * .38, size * 1.15));
            painter.drawRect(QRectF(size * .47, -size * .65, size * .38, size * 1.15));
        }
        painter.restore();
        return;
    }

    if (type == QStringLiteral("label")) {
        const QString text = object.value(QStringLiteral("text")).toString();
        if (text.isEmpty()) return;
        QFont font(QStringLiteral("Georgia"));
        font.setPointSizeF(std::clamp(object.value(QStringLiteral("fontSize")).toDouble(22.0) * s, 7.0, 34.0));
        font.setBold(object.value(QStringLiteral("bold")).toBool(false));
        painter.setFont(font);
        painter.setPen(QPen(QColor(250, 244, 225, 170), std::max(2.0, 3.0 * s)));
        painter.drawText(QPointF(x + 1, y + 1), text);
        painter.setPen(QPen(ink, 1.0));
        painter.drawText(QPointF(x, y), text);
    }
}

void CartographicCanvasV2::drawTree(QPainter& painter, const QPointF& p, double size, const QColor& ink) {
    painter.save();
    painter.setPen(QPen(ink, std::max(1.0, size * .07), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(QColor(ink.red(), ink.green(), ink.blue(), 18));
    QPainterPath crown;
    crown.moveTo(p.x(), p.y() - size);
    crown.lineTo(p.x() - size * .52, p.y() - size * .05);
    crown.lineTo(p.x() - size * .22, p.y() - size * .12);
    crown.lineTo(p.x() - size * .42, p.y() + size * .44);
    crown.lineTo(p.x() + size * .42, p.y() + size * .44);
    crown.lineTo(p.x() + size * .22, p.y() - size * .12);
    crown.lineTo(p.x() + size * .52, p.y() - size * .05);
    crown.closeSubpath();
    painter.drawPath(crown);
    painter.drawLine(QPointF(p.x(), p.y() + size * .38), QPointF(p.x(), p.y() + size * .78));
    painter.restore();
}

void CartographicCanvasV2::drawMountain(QPainter& painter, const QPointF& p, double size, const QColor& ink) {
    painter.save();
    painter.setPen(QPen(ink, std::max(1.0, size * .055), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(QColor(ink.red(), ink.green(), ink.blue(), 15));
    QPainterPath m;
    m.moveTo(p.x() - size, p.y() + size * .55);
    m.lineTo(p.x() - size * .28, p.y() - size * .22);
    m.lineTo(p.x(), p.y() - size);
    m.lineTo(p.x() + size * .32, p.y() - size * .18);
    m.lineTo(p.x() + size, p.y() + size * .55);
    m.closeSubpath();
    painter.drawPath(m);
    painter.drawLine(QPointF(p.x() - size * .18, p.y() - size * .28), QPointF(p.x(), p.y() - size));
    painter.drawLine(QPointF(p.x(), p.y() - size), QPointF(p.x() + size * .2, p.y() - size * .22));
    painter.restore();
}

void CartographicCanvasV2::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(QStringLiteral("#11100d")));
    if (cache_.isNull()) return;
    const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
    const double docW = std::max(1.0, pilin.value(QStringLiteral("width")).toDouble(4096));
    const double docH = std::max(1.0, pilin.value(QStringLiteral("height")).toDouble(2304));
    const QRectF target(pan_.x(), pan_.y(), docW * zoom_, docH * zoom_);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 90));
    painter.drawRoundedRect(target.translated(8, 10), 8, 8);
    painter.drawImage(target, cache_);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(83, 68, 51, 145), 1.0));
    painter.drawRect(target);
}

void CartographicCanvasV2::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (pan_.isNull()) fitDocument();
}

void CartographicCanvasV2::wheelEvent(QWheelEvent* event) {
    const QPointF cursor = event->position();
    const QPointF before = screenToWorld(cursor);
    zoom_ = std::clamp(zoom_ * (event->angleDelta().y() > 0 ? 1.12 : 1.0 / 1.12), 0.02, 12.0);
    pan_ = cursor - QPointF(before.x() * zoom_, before.y() * zoom_);
    update();
    event->accept();
}

QPointF CartographicCanvasV2::screenToWorld(const QPointF& p) const {
    return QPointF((p.x() - pan_.x()) / std::max(0.0001, zoom_), (p.y() - pan_.y()) / std::max(0.0001, zoom_));
}

QPointF CartographicCanvasV2::worldToScreen(const QPointF& p) const {
    return QPointF(p.x() * zoom_ + pan_.x(), p.y() * zoom_ + pan_.y());
}

QPointF CartographicCanvasV2::worldToInputScreen(const QPointF& world) const {
    if (!inputViewport_) return world;
    const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
    const double docW = std::max(1.0, pilin.value(QStringLiteral("width")).toDouble(4096));
    const double docH = std::max(1.0, pilin.value(QStringLiteral("height")).toDouble(2304));
    const double z = std::clamp(std::min((inputViewport_->width() - 80.0) / docW, (inputViewport_->height() - 80.0) / docH), 0.03, 6.0);
    const QPointF basePan((inputViewport_->width() - docW * z) * 0.5, (inputViewport_->height() - docH * z) * 0.5);
    return QPointF(world.x() * z + basePan.x(), world.y() * z + basePan.y());
}

bool CartographicCanvasV2::panMode() const {
    if (!editor_) return false;
    for (QToolButton* b : editor_->findChildren<QToolButton*>(QStringLiteral("pilinMapTool"))) {
        if (!b->isChecked()) continue;
        const QString tip = b->toolTip().toLower();
        if (tip.contains(QStringLiteral("mover")) || tip.contains(QStringLiteral("pan"))) return true;
    }
    return false;
}

void CartographicCanvasV2::forwardMouse(QMouseEvent* event) {
    if (!inputViewport_) return;
    const QPointF world = screenToWorld(event->position());
    const QPointF local = worldToInputScreen(world);
    QMouseEvent forwarded(event->type(), local, event->globalPosition(), event->button(), event->buttons(), event->modifiers(), event->pointingDevice());
    QApplication::sendEvent(inputViewport_, &forwarded);
}

void CartographicCanvasV2::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);
    if (event->button() == Qt::MiddleButton || panMode()) {
        panning_ = true;
        lastMouse_ = event->position();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    forwardMouse(event);
}

void CartographicCanvasV2::mouseMoveEvent(QMouseEvent* event) {
    if (panning_) {
        pan_ += event->position() - lastMouse_;
        lastMouse_ = event->position();
        update();
        return;
    }
    forwardMouse(event);
}

void CartographicCanvasV2::mouseReleaseEvent(QMouseEvent* event) {
    if (panning_) {
        panning_ = false;
        unsetCursor();
        return;
    }
    forwardMouse(event);
}

void CartographicCanvasV2::mouseDoubleClickEvent(QMouseEvent* event) {
    forwardMouse(event);
}

void CartographicCanvasV2::keyPressEvent(QKeyEvent* event) {
    if (inputViewport_) QApplication::sendEvent(inputViewport_, event);
    else QWidget::keyPressEvent(event);
}

} // namespace wbw
