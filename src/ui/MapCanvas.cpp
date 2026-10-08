#include "ui/MapCanvas.h"

#include <QBuffer>
#include <QGraphicsEllipseItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QImage>
#include <QJsonArray>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QWheelEvent>

namespace wbw {
namespace {

constexpr int CanvasWidth = 1200;
constexpr int CanvasHeight = 760;

QColor markerColor(const QString& kind) {
    const QString value = kind.toLower();
    if (value.contains(QStringLiteral("capital"))) return QColor(QStringLiteral("#d8b35c"));
    if (value.contains(QStringLiteral("puerto"))) return QColor(QStringLiteral("#67a9ba"));
    if (value.contains(QStringLiteral("fort"))) return QColor(QStringLiteral("#b8695f"));
    if (value.contains(QStringLiteral("ruina"))) return QColor(QStringLiteral("#8f897b"));
    return QColor(QStringLiteral("#d9d4c8"));
}

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

} // namespace

MapCanvas::MapCanvas(QWidget* parent) : QGraphicsView(parent) {
    setScene(new QGraphicsScene(this));
    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::SmoothPixmapTransform, true);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setBackgroundBrush(QColor(QStringLiteral("#151612")));
    setFrameShape(QFrame::NoFrame);
}

void MapCanvas::setMap(const QJsonObject& map) {
    map_ = map;
    rebuild();
}

QPixmap MapCanvas::generatedBackground() const {
    QImage image(CanvasWidth, CanvasHeight, QImage::Format_ARGB32_Premultiplied);
    const QString style = map_.value(QStringLiteral("style")).toString(QStringLiteral("Pergamino"));
    QColor sea;
    QColor land;
    QColor coast;
    if (style == QStringLiteral("Nocturno")) {
        sea = QColor(QStringLiteral("#151d24"));
        land = QColor(QStringLiteral("#39433d"));
        coast = QColor(QStringLiteral("#81917e"));
    } else if (style == QStringLiteral("Atlas")) {
        sea = QColor(QStringLiteral("#b9d1d7"));
        land = QColor(QStringLiteral("#d9d1aa"));
        coast = QColor(QStringLiteral("#596e63"));
    } else {
        sea = QColor(QStringLiteral("#c7b98d"));
        land = QColor(QStringLiteral("#ddd0a4"));
        coast = QColor(QStringLiteral("#665c43"));
    }
    image.fill(sea);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(coast, 3.0));
    painter.setBrush(land);

    const quint64 seed = static_cast<quint64>(map_.value(QStringLiteral("seed")).toDouble(1.0));
    QRandomGenerator64 random(seed == 0 ? 1 : seed);
    const int continents = qBound(1, map_.value(QStringLiteral("continents")).toInt(3), 12);
    const int islands = qBound(0, map_.value(QStringLiteral("islands")).toInt(8), 80);
    const int roughness = qBound(1, map_.value(QStringLiteral("roughness")).toInt(5), 10);

    auto drawLand = [&](double cx, double cy, double rx, double ry, int points) {
        QPainterPath path;
        for (int i = 0; i < points; ++i) {
            const double angle = (6.283185307179586 * i) / points;
            const double jitter = 0.72 + random.generateDouble() * (0.18 + roughness * 0.025);
            const QPointF point(cx + qCos(angle) * rx * jitter, cy + qSin(angle) * ry * jitter);
            if (i == 0) path.moveTo(point); else path.lineTo(point);
        }
        path.closeSubpath();
        painter.drawPath(path);
    };

    for (int i = 0; i < continents; ++i) {
        const double cx = 120.0 + random.generateDouble() * (CanvasWidth - 240.0);
        const double cy = 100.0 + random.generateDouble() * (CanvasHeight - 200.0);
        const double rx = 110.0 + random.generateDouble() * 170.0;
        const double ry = 80.0 + random.generateDouble() * 120.0;
        drawLand(cx, cy, rx, ry, 18 + roughness * 3);
    }
    for (int i = 0; i < islands; ++i) {
        const double cx = 50.0 + random.generateDouble() * (CanvasWidth - 100.0);
        const double cy = 50.0 + random.generateDouble() * (CanvasHeight - 100.0);
        const double radius = 8.0 + random.generateDouble() * 24.0;
        drawLand(cx, cy, radius * (0.8 + random.generateDouble() * 0.7), radius, 10 + roughness);
    }

    painter.setPen(QPen(coast, 1.0, Qt::DotLine));
    for (int i = 1; i < 6; ++i) painter.drawLine(0, i * CanvasHeight / 6, CanvasWidth, i * CanvasHeight / 6);
    for (int i = 1; i < 8; ++i) painter.drawLine(i * CanvasWidth / 8, 0, i * CanvasWidth / 8, CanvasHeight);
    painter.end();
    return QPixmap::fromImage(image);
}

void MapCanvas::rebuild() {
    scene()->clear();
    if (map_.isEmpty()) {
        scene()->setSceneRect(QRectF(0.0, 0.0, CanvasWidth, CanvasHeight));
        return;
    }

    QPixmap background;
    const QByteArray backgroundBytes = dataUrlBytes(map_.value(QStringLiteral("backgroundImageDataUrl")).toString());
    if (!backgroundBytes.isEmpty()) background.loadFromData(backgroundBytes);
    if (background.isNull()) background = generatedBackground();
    background = background.scaled(CanvasWidth, CanvasHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    auto* pixmap = scene()->addPixmap(background);
    pixmap->setZValue(-10.0);
    scene()->setSceneRect(QRectF(0.0, 0.0, CanvasWidth, CanvasHeight));

    const QJsonArray markers = map_.value(QStringLiteral("markers")).toArray();
    for (const QJsonValue value : markers) {
        const QJsonObject marker = value.toObject();
        const QString id = marker.value(QStringLiteral("id")).toString();
        const double x = qBound(0.0, marker.value(QStringLiteral("x")).toDouble(), 100.0) / 100.0 * CanvasWidth;
        const double y = qBound(0.0, marker.value(QStringLiteral("y")).toDouble(), 100.0) / 100.0 * CanvasHeight;
        const QColor color = markerColor(marker.value(QStringLiteral("kind")).toString());
        auto* dot = scene()->addEllipse(QRectF(-7.0, -7.0, 14.0, 14.0), QPen(QColor(QStringLiteral("#24241f")), 2.0), QBrush(color));
        dot->setPos(x, y);
        dot->setFlag(QGraphicsItem::ItemIsMovable, true);
        dot->setFlag(QGraphicsItem::ItemIsSelectable, true);
        dot->setData(0, id);
        dot->setZValue(2.0);
        auto* label = new QGraphicsSimpleTextItem(marker.value(QStringLiteral("label")).toString(QStringLiteral("Lugar")), dot);
        label->setBrush(QColor(QStringLiteral("#f5f1e8")));
        label->setPos(11.0, -12.0);
        label->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    }
}

void MapCanvas::wheelEvent(QWheelEvent* event) {
    const qreal factor = event->angleDelta().y() > 0 ? 1.12 : 1.0 / 1.12;
    const qreal current = transform().m11();
    if ((factor > 1.0 && current < 4.0) || (factor < 1.0 && current > 0.25)) scale(factor, factor);
    event->accept();
}

void MapCanvas::mouseDoubleClickEvent(QMouseEvent* event) {
    if (auto* item = itemAt(event->pos())) {
        QGraphicsItem* marker = item;
        while (marker && marker->data(0).toString().isEmpty()) marker = marker->parentItem();
        if (marker && !marker->data(0).toString().isEmpty()) {
            emit markerActivated(marker->data(0).toString());
            QGraphicsView::mouseDoubleClickEvent(event);
            return;
        }
    }
    const QPointF point = mapToScene(event->pos());
    if (sceneRect().contains(point)) {
        emit addMarkerRequested(point.x() / CanvasWidth * 100.0, point.y() / CanvasHeight * 100.0);
        event->accept();
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void MapCanvas::mouseReleaseEvent(QMouseEvent* event) {
    QGraphicsView::mouseReleaseEvent(event);
    for (QGraphicsItem* item : scene()->items()) {
        const QString id = item->data(0).toString();
        if (id.isEmpty()) continue;
        const QPointF point = item->pos();
        emit markerMoved(id,
            qBound(0.0, point.x() / CanvasWidth * 100.0, 100.0),
            qBound(0.0, point.y() / CanvasHeight * 100.0, 100.0));
    }
}

} // namespace wbw
