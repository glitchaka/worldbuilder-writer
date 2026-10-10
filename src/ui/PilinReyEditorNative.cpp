#include "ui/PilinReyEditor.h"

#include "core/ArchiveDocument.h"
#include "ui/MapExportDialog.h"
#include "ui/MapExporter.h"
#include "ui/WorldPage.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QShortcut>
#include <QSlider>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <functional>

namespace wbw {
namespace {

constexpr double kPi = 3.14159265358979323846;

QString uid(const QString& prefix) {
    return prefix + QLatin1Char('-') + QUuid::createUuid().toString(QUuid::Id128);
}

QPointF jsonPoint(const QJsonValue& value) {
    const QJsonObject point = value.toObject();
    return QPointF(point.value(QStringLiteral("x")).toDouble(), point.value(QStringLiteral("y")).toDouble());
}

QJsonObject pointJson(const QPointF& point, double radius = 0.0) {
    QJsonObject value{{QStringLiteral("x"), point.x()}, {QStringLiteral("y"), point.y()}};
    if (radius > 0.0) value.insert(QStringLiteral("radius"), radius);
    return value;
}

double pointRadius(const QJsonValue& value, double fallback) {
    return value.toObject().value(QStringLiteral("radius")).toDouble(fallback);
}

QColor themeColor(const QJsonObject& pilin, const QString& key, const QColor& fallback) {
    const QColor color(pilin.value(QStringLiteral("theme")).toObject().value(key).toString());
    return color.isValid() ? color : fallback;
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

QString imageToDataUrl(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QString mime = QStringLiteral("image/png");
    const QString lower = path.toLower();
    if (lower.endsWith(QStringLiteral(".jpg")) || lower.endsWith(QStringLiteral(".jpeg"))) mime = QStringLiteral("image/jpeg");
    else if (lower.endsWith(QStringLiteral(".webp"))) mime = QStringLiteral("image/webp");
    return QStringLiteral("data:%1;base64,%2").arg(mime, QString::fromLatin1(file.readAll().toBase64()));
}

QString typeForTool(PilinReyEditor::Tool tool) {
    switch (tool) {
        case PilinReyEditor::Tool::Coast: return QStringLiteral("land");
        case PilinReyEditor::Tool::Eraser: return QStringLiteral("sea");
        case PilinReyEditor::Tool::Region: return QStringLiteral("region");
        case PilinReyEditor::Tool::River: return QStringLiteral("river");
        case PilinReyEditor::Tool::Road: return QStringLiteral("road");
        case PilinReyEditor::Tool::Border: return QStringLiteral("border");
        case PilinReyEditor::Tool::Forest: return QStringLiteral("forestArea");
        case PilinReyEditor::Tool::Mountain: return QStringLiteral("mountainArea");
        default: return {};
    }
}

bool brushTool(PilinReyEditor::Tool tool) {
    return tool == PilinReyEditor::Tool::Coast || tool == PilinReyEditor::Tool::Eraser ||
           tool == PilinReyEditor::Tool::Forest || tool == PilinReyEditor::Tool::Mountain;
}

bool nodeTool(PilinReyEditor::Tool tool) {
    return tool == PilinReyEditor::Tool::Region || tool == PilinReyEditor::Tool::River ||
           tool == PilinReyEditor::Tool::Road || tool == PilinReyEditor::Tool::Border;
}

QToolButton* toolButton(const QString& text, const QString& tip, QWidget* parent, const QString& name) {
    auto* button = new QToolButton(parent);
    button->setObjectName(name);
    button->setText(text);
    button->setToolTip(tip);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    button->setMinimumHeight(30);
    return button;
}

QPainterPath smoothPath(const QJsonArray& points, const std::function<QPointF(const QPointF&)>& transform) {
    QPainterPath path;
    if (points.isEmpty()) return path;
    QPointF previous = transform(jsonPoint(points.first()));
    path.moveTo(previous);
    for (int i = 1; i < points.size(); ++i) {
        const QPointF current = transform(jsonPoint(points.at(i)));
        const QPointF mid = (previous + current) * 0.5;
        path.quadTo(previous, mid);
        previous = current;
    }
    path.quadTo(previous, previous);
    return path;
}

QPainterPath brushRegion(const QJsonArray& points, double fallbackRadius, const std::function<QPointF(const QPointF&)>& transform, double zoom) {
    QPainterPath region;
    for (int i = 0; i < points.size(); ++i) {
        const QPointF world = jsonPoint(points.at(i));
        const QPointF center = transform(world);
        const double radius = std::max(4.0, pointRadius(points.at(i), fallbackRadius) * zoom);
        QPainterPath disc;
        disc.addEllipse(center, radius, radius);
        region = region.isEmpty() ? disc : region.united(disc);
        if (i > 0) {
            const QPointF previous = transform(jsonPoint(points.at(i - 1)));
            QPainterPath bridge;
            QPolygonF quad;
            const QPointF delta = center - previous;
            const double len = std::hypot(delta.x(), delta.y());
            if (len > 0.01) {
                const QPointF normal(-delta.y() / len * radius, delta.x() / len * radius);
                quad << previous + normal << center + normal << center - normal << previous - normal;
                bridge.addPolygon(quad);
                region = region.united(bridge);
            }
        }
    }
    return region.simplified();
}

quint32 stableHash(int a, int b, int c) {
    quint32 x = static_cast<quint32>(a * 73856093) ^ static_cast<quint32>(b * 19349663) ^ static_cast<quint32>(c * 83492791);
    x ^= x >> 13; x *= 0x5bd1e995; x ^= x >> 15;
    return x;
}

double hash01(int a, int b, int c) {
    return (stableHash(a, b, c) & 0xffffu) / 65535.0;
}

} // namespace

class PilinReyViewport final : public QWidget {
public:
    using Tool = PilinReyEditor::Tool;

    std::function<void(const QString&, const QJsonArray&)> onPath;
    std::function<void(double, double)> onSettlement;
    std::function<void(double, double)> onStamp;
    std::function<void(double, double)> onLabel;
    std::function<void(const QStringList&)> onSelection;
    std::function<void(const QString&, double, double)> onMovePoint;
    std::function<void()> onDeleteSelection;
    std::function<void(const QString&)> onStatus;

    explicit PilinReyViewport(QWidget* parent = nullptr) : QWidget(parent) {
        setObjectName(QStringLiteral("pilinQtViewport"));
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setMinimumSize(360, 260);
    }

    void setDocument(const QJsonObject& map) {
        map_ = map;
        templateImage_ = QImage();
        const QString data = map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("template")).toObject().value(QStringLiteral("dataUrl")).toString();
        if (!data.isEmpty()) templateImage_.loadFromData(dataUrlBytes(data));
        if (!viewInitialized_) fitDocument();
        update();
    }
    void setTool(Tool tool) {
        tool_ = tool; stroke_ = {}; nodes_ = {}; drawing_ = false; draggingId_.clear();
        setCursor(tool == Tool::Pan ? Qt::OpenHandCursor : tool == Tool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
        update();
    }
    void setSelection(const QStringList& ids) { selected_ = ids; update(); }
    void setSnap(bool enabled) { snap_ = enabled; update(); }
    void setGrid(bool enabled) { grid_ = enabled; update(); }
    void setBrushRadius(int value) { brushRadius_ = qBound(20, value, 700); update(); }
    void setDensity(int value) { density_ = qBound(10, value, 100); update(); }
    void setStrokeWidth(int value) { strokeWidth_ = qBound(1, value, 24); update(); }
    void setSymbolSize(int value) { symbolSize_ = qBound(16, value, 160); update(); }

    void fitDocument() {
        const QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject();
        const double docW = std::max(1.0, p.value(QStringLiteral("width")).toDouble(4096));
        const double docH = std::max(1.0, p.value(QStringLiteral("height")).toDouble(2304));
        if (width() < 80 || height() < 80) return;
        zoom_ = std::clamp(std::min((width() - 80.0) / docW, (height() - 80.0) / docH), 0.03, 6.0);
        pan_ = QPointF((width() - docW * zoom_) * 0.5, (height() - docH * zoom_) * 0.5);
        viewInitialized_ = true;
        update();
    }

protected:
    void resizeEvent(QResizeEvent* event) override { QWidget::resizeEvent(event); if (autoFit_) fitDocument(); }
    void wheelEvent(QWheelEvent* event) override {
        autoFit_ = false;
        const QPointF cursor = event->position();
        const QPointF before = screenToWorld(cursor);
        zoom_ = std::clamp(zoom_ * (event->angleDelta().y() > 0 ? 1.12 : 1.0 / 1.12), 0.025, 12.0);
        pan_ = cursor - QPointF(before.x() * zoom_, before.y() * zoom_);
        update();
        event->accept();
    }
    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Delete && onDeleteSelection) { onDeleteSelection(); event->accept(); return; }
        if (event->key() == Qt::Key_Escape) { stroke_ = {}; nodes_ = {}; drawing_ = false; draggingId_.clear(); update(); event->accept(); return; }
        if (event->key() == Qt::Key_Backspace && !nodes_.isEmpty()) { nodes_.removeLast(); update(); event->accept(); return; }
        QWidget::keyPressEvent(event);
    }
    void mousePressEvent(QMouseEvent* event) override {
        setFocus(Qt::MouseFocusReason);
        lastMouse_ = event->position();
        cursorWorld_ = snapped(screenToWorld(event->position()));
        if (event->button() == Qt::MiddleButton || tool_ == Tool::Pan) {
            panning_ = true; autoFit_ = false; setCursor(Qt::ClosedHandCursor); return;
        }
        if (event->button() == Qt::RightButton && nodeTool(tool_)) { finishNodes(); return; }
        if (event->button() != Qt::LeftButton) return;
        if (tool_ == Tool::Select) {
            const QString id = hitTest(cursorWorld_);
            const bool extend = event->modifiers().testFlag(Qt::ShiftModifier) || event->modifiers().testFlag(Qt::ControlModifier);
            if (!extend) selected_.clear();
            if (!id.isEmpty()) {
                if (extend && selected_.contains(id)) selected_.removeAll(id); else if (!selected_.contains(id)) selected_.append(id);
                const QJsonObject object = objectById(id);
                if (object.contains(QStringLiteral("x"))) {
                    draggingId_ = id;
                    dragOrigin_ = QPointF(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble());
                    dragStart_ = cursorWorld_;
                }
            }
            if (onSelection) onSelection(selected_);
            update(); return;
        }
        if (brushTool(tool_)) {
            drawing_ = true;
            stroke_ = QJsonArray{pointJson(cursorWorld_, brushRadius_)};
            update(); return;
        }
        if (nodeTool(tool_)) {
            nodes_.append(pointJson(cursorWorld_));
            if (onStatus) onStatus(QObject::tr("Clic para añadir nodos · clic derecho para terminar"));
            update(); return;
        }
        if (tool_ == Tool::Settlement && onSettlement) onSettlement(cursorWorld_.x(), cursorWorld_.y());
        else if (tool_ == Tool::Stamp && onStamp) onStamp(cursorWorld_.x(), cursorWorld_.y());
        else if (tool_ == Tool::Label && onLabel) onLabel(cursorWorld_.x(), cursorWorld_.y());
        else if (tool_ == Tool::Measure) {
            measureA_ = cursorWorld_; measureB_ = cursorWorld_; measuring_ = true; update();
        }
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        cursorWorld_ = snapped(screenToWorld(event->position()));
        if (panning_) { pan_ += event->position() - lastMouse_; lastMouse_ = event->position(); update(); return; }
        if (drawing_) {
            if (stroke_.isEmpty() || QLineF(jsonPoint(stroke_.last()), cursorWorld_).length() > std::max(12.0, brushRadius_ * .22)) stroke_.append(pointJson(cursorWorld_, brushRadius_));
            update(); return;
        }
        if (measuring_) { measureB_ = cursorWorld_; update(); }
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (panning_) { panning_ = false; setCursor(tool_ == Tool::Pan ? Qt::OpenHandCursor : Qt::ArrowCursor); return; }
        if (event->button() != Qt::LeftButton) return;
        if (drawing_) {
            drawing_ = false;
            if (onPath && !stroke_.isEmpty()) onPath(typeForTool(tool_), stroke_);
            stroke_ = {}; update(); return;
        }
        if (!draggingId_.isEmpty()) {
            const QPointF target = dragOrigin_ + (cursorWorld_ - dragStart_);
            if (onMovePoint) onMovePoint(draggingId_, target.x(), target.y());
            draggingId_.clear(); return;
        }
        measuring_ = false;
    }
    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && nodeTool(tool_)) { nodes_.append(pointJson(snapped(screenToWorld(event->position())))); finishNodes(); event->accept(); return; }
        QWidget::mouseDoubleClickEvent(event);
    }
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), QColor(QStringLiteral("#14110d")));
        const QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject();
        const double docW = p.value(QStringLiteral("width")).toDouble(4096);
        const double docH = p.value(QStringLiteral("height")).toDouble(2304);
        const QRectF page(worldToScreen(QPointF(0, 0)), worldToScreen(QPointF(docW, docH)));

        painter.save();
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 105));
        painter.drawRoundedRect(page.translated(9, 12), 8, 8);
        painter.restore();

        const QColor sea = themeColor(p, QStringLiteral("sea"), QColor(QStringLiteral("#9fb7b6")));
        painter.fillRect(page, sea);
        drawSeaTexture(painter, page);
        drawTemplate(painter, page, p);
        if (grid_) drawGrid(painter, page, docW, docH);

        for (const QJsonValue& layerValue : p.value(QStringLiteral("layers")).toArray()) {
            const QJsonObject layer = layerValue.toObject();
            if (!layer.value(QStringLiteral("visible")).toBool(true)) continue;
            painter.save();
            painter.setOpacity(std::clamp(layer.value(QStringLiteral("opacity")).toDouble(1.0), 0.0, 1.0));
            for (const QJsonValue& objectValue : layer.value(QStringLiteral("objects")).toArray()) drawObject(painter, objectValue.toObject(), p);
            painter.restore();
        }
        drawPreview(painter, p);

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(QStringLiteral("#4a4033")), 1.2));
        painter.drawRect(page);
        painter.setPen(QPen(QColor(255, 248, 224, 45), 1.0));
        painter.drawRect(page.adjusted(7, 7, -7, -7));
    }

private:
    QPointF worldToScreen(const QPointF& p) const { return QPointF(p.x() * zoom_ + pan_.x(), p.y() * zoom_ + pan_.y()); }
    QPointF screenToWorld(const QPointF& p) const { return QPointF((p.x() - pan_.x()) / zoom_, (p.y() - pan_.y()) / zoom_); }
    QPointF snapped(const QPointF& p) const {
        if (!snap_) return p;
        constexpr double step = 25.0;
        return QPointF(std::round(p.x() / step) * step, std::round(p.y() / step) * step);
    }
    void finishNodes() {
        if (nodes_.size() >= 2 && onPath) {
            QJsonArray value = nodes_;
            if (tool_ == Tool::Region && value.size() >= 3) value.append(value.first());
            onPath(typeForTool(tool_), value);
        }
        nodes_ = {}; update();
    }
    QJsonObject objectById(const QString& id) const {
        for (const QJsonValue& layerValue : map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray())
            for (const QJsonValue& objectValue : layerValue.toObject().value(QStringLiteral("objects")).toArray()) {
                const QJsonObject object = objectValue.toObject();
                if (object.value(QStringLiteral("id")).toString() == id) return object;
            }
        return {};
    }
    QString hitTest(const QPointF& point) const {
        const double tolerance = 18.0 / std::max(.03, zoom_);
        const QJsonArray layers = map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray();
        for (int li = layers.size() - 1; li >= 0; --li) {
            const QJsonObject layer = layers.at(li).toObject();
            if (!layer.value(QStringLiteral("visible")).toBool(true) || layer.value(QStringLiteral("locked")).toBool(false)) continue;
            const QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
            for (int oi = objects.size() - 1; oi >= 0; --oi) {
                const QJsonObject object = objects.at(oi).toObject();
                if (object.contains(QStringLiteral("x"))) {
                    const QPointF center(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble());
                    if (QLineF(center, point).length() <= std::max(tolerance, object.value(QStringLiteral("size")).toDouble(80) * .5)) return object.value(QStringLiteral("id")).toString();
                } else {
                    const QJsonArray points = object.value(QStringLiteral("points")).toArray();
                    for (const QJsonValue& p : points) if (QLineF(jsonPoint(p), point).length() <= tolerance) return object.value(QStringLiteral("id")).toString();
                }
            }
        }
        return {};
    }
    void drawSeaTexture(QPainter& painter, const QRectF& page) {
        painter.save();
        painter.setClipRect(page);
        painter.setPen(QPen(QColor(54, 76, 76, 30), 1));
        for (double y = page.top() + 26; y < page.bottom(); y += 32) {
            QPainterPath wave;
            wave.moveTo(page.left(), y);
            for (double x = page.left(); x < page.right(); x += 42) wave.cubicTo(x + 10, y - 3, x + 22, y + 3, x + 42, y);
            painter.drawPath(wave);
        }
        painter.setPen(QPen(QColor(255, 255, 244, 16), 1));
        for (double x = page.left(); x < page.right(); x += 37) painter.drawLine(QPointF(x, page.top()), QPointF(x, page.bottom()));
        painter.restore();
    }
    void drawTemplate(QPainter& painter, const QRectF& page, const QJsonObject& pilin) {
        if (templateImage_.isNull()) return;
        const QJsonObject t = pilin.value(QStringLiteral("template")).toObject();
        if (!t.value(QStringLiteral("visible")).toBool(true)) return;
        const double scale = std::clamp(t.value(QStringLiteral("scale")).toDouble(1.0), .05, 5.0);
        const QSizeF size(page.width() * scale, page.height() * scale);
        const QPointF center = page.center() + QPointF(t.value(QStringLiteral("x")).toDouble() * zoom_, t.value(QStringLiteral("y")).toDouble() * zoom_);
        const QRectF target(center.x() - size.width() / 2.0, center.y() - size.height() / 2.0, size.width(), size.height());
        painter.save(); painter.setOpacity(std::clamp(t.value(QStringLiteral("opacity")).toDouble(.35), 0.0, 1.0)); painter.translate(target.center()); painter.rotate(t.value(QStringLiteral("rotation")).toDouble()); painter.translate(-target.center()); painter.drawImage(target, templateImage_); painter.restore();
    }
    void drawGrid(QPainter& painter, const QRectF& page, double docW, double docH) {
        painter.save(); painter.setClipRect(page); painter.setPen(QPen(QColor(40, 56, 54, 55), 1));
        for (double x = 0; x <= docW; x += 100) painter.drawLine(worldToScreen(QPointF(x, 0)), worldToScreen(QPointF(x, docH)));
        for (double y = 0; y <= docH; y += 100) painter.drawLine(worldToScreen(QPointF(0, y)), worldToScreen(QPointF(docW, y)));
        painter.restore();
    }
    void drawTree(QPainter& painter, const QPointF& p, double s, const QColor& ink) {
        painter.setPen(QPen(ink, std::max(1.0, s * .08), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(QColor(ink.red(), ink.green(), ink.blue(), 18));
        QPainterPath canopy; canopy.moveTo(p.x(), p.y() - s); canopy.lineTo(p.x() - s * .6, p.y() + s * .2); canopy.lineTo(p.x() - s * .25, p.y() + s * .08); canopy.lineTo(p.x() - s * .48, p.y() + s * .52); canopy.lineTo(p.x() + s * .48, p.y() + s * .52); canopy.lineTo(p.x() + s * .25, p.y() + s * .08); canopy.lineTo(p.x() + s * .6, p.y() + s * .2); canopy.closeSubpath(); painter.drawPath(canopy); painter.drawLine(QPointF(p.x(), p.y() + s * .45), QPointF(p.x(), p.y() + s * .82));
    }
    void drawMountain(QPainter& painter, const QPointF& p, double s, const QColor& ink) {
        painter.setPen(QPen(ink, std::max(1.0, s * .07), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); painter.setBrush(QColor(ink.red(), ink.green(), ink.blue(), 14));
        QPainterPath mountain; mountain.moveTo(p.x() - s, p.y() + s * .55); mountain.lineTo(p.x() - s * .25, p.y() - s * .28); mountain.lineTo(p.x(), p.y() - s); mountain.lineTo(p.x() + s * .27, p.y() - s * .22); mountain.lineTo(p.x() + s, p.y() + s * .55); mountain.closeSubpath(); painter.drawPath(mountain);
        painter.drawLine(QPointF(p.x() - s * .23, p.y() - s * .27), QPointF(p.x(), p.y() - s)); painter.drawLine(QPointF(p.x(), p.y() - s), QPointF(p.x() + s * .22, p.y() - s * .18));
        painter.setPen(QPen(QColor(255, 250, 230, 90), std::max(1.0, s * .035))); painter.drawLine(QPointF(p.x() - s * .05, p.y() - s * .85), QPointF(p.x() - s * .42, p.y() + s * .25));
    }
    void drawBuiltinStamp(QPainter& painter, const QString& kind, double size) {
        const QColor ink(QStringLiteral("#342c23")); painter.setPen(QPen(ink, std::max(1.2, size * .035), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); painter.setBrush(QColor(235, 218, 177, 155));
        if (kind == QStringLiteral("compass")) {
            painter.drawEllipse(QPointF(0, 0), size * .38, size * .38); for (int i = 0; i < 8; ++i) { const double a = i * kPi / 4.0; painter.drawLine(QPointF(std::cos(a) * size * .08, std::sin(a) * size * .08), QPointF(std::cos(a) * size * .48, std::sin(a) * size * .48)); } return;
        }
        if (kind == QStringLiteral("ship")) {
            painter.drawArc(QRectF(-size * .45, -size * .04, size * .9, size * .42), 200 * 16, 140 * 16); painter.drawLine(QPointF(0, -size * .48), QPointF(0, size * .22)); QPainterPath sail; sail.moveTo(0, -size * .44); sail.lineTo(size * .34, -size * .08); sail.lineTo(0, -size * .02); sail.closeSubpath(); painter.drawPath(sail); return;
        }
        if (kind == QStringLiteral("temple")) {
            QPainterPath roof; roof.moveTo(-size * .43, -size * .13); roof.lineTo(0, -size * .44); roof.lineTo(size * .43, -size * .13); roof.closeSubpath(); painter.drawPath(roof); for (int i = -2; i <= 2; ++i) painter.drawLine(QPointF(i * size * .14, -size * .1), QPointF(i * size * .14, size * .3)); painter.drawLine(QPointF(-size * .48, size * .31), QPointF(size * .48, size * .31)); return;
        }
        if (kind == QStringLiteral("ruin")) {
            painter.drawRect(QRectF(-size * .34, -size * .2, size * .22, size * .5)); painter.drawRect(QRectF(size * .04, -size * .02, size * .26, size * .32)); painter.drawLine(QPointF(-size * .45, size * .31), QPointF(size * .43, size * .31)); return;
        }
        painter.drawRect(QRectF(-size * .31, -size * .16, size * .62, size * .47)); painter.drawRect(QRectF(-size * .46, -size * .34, size * .22, size * .65)); painter.drawRect(QRectF(size * .24, -size * .34, size * .22, size * .65)); painter.drawLine(QPointF(-size * .46, -size * .34), QPointF(-size * .35, -size * .48)); painter.drawLine(QPointF(-size * .24, -size * .34), QPointF(-size * .35, -size * .48)); painter.drawLine(QPointF(size * .24, -size * .34), QPointF(size * .35, -size * .48)); painter.drawLine(QPointF(size * .46, -size * .34), QPointF(size * .35, -size * .48));
    }
    void drawObject(QPainter& painter, const QJsonObject& object, const QJsonObject& pilin) {
        const QString type = object.value(QStringLiteral("type")).toString();
        const bool selected = selected_.contains(object.value(QStringLiteral("id")).toString());
        if (type == QStringLiteral("land") || type == QStringLiteral("sea")) {
            const QJsonArray points = object.value(QStringLiteral("points")).toArray(); if (points.isEmpty()) return;
            const QPainterPath region = brushRegion(points, brushRadius_, [this](const QPointF& p) { return worldToScreen(p); }, zoom_);
            const QColor fill = type == QStringLiteral("land") ? themeColor(pilin, QStringLiteral("land"), QColor(QStringLiteral("#d7c798"))) : themeColor(pilin, QStringLiteral("sea"), QColor(QStringLiteral("#9fb7b6")));
            const QColor coast = themeColor(pilin, QStringLiteral("coast"), QColor(QStringLiteral("#4a4033")));
            painter.save(); painter.setBrush(fill); painter.setPen(type == QStringLiteral("land") ? QPen(coast, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin) : Qt::NoPen); painter.drawPath(region);
            if (type == QStringLiteral("land")) { painter.setBrush(Qt::NoBrush); painter.setPen(QPen(QColor(255, 247, 220, 70), 1)); painter.drawPath(region.translated(-2.5, -2.5)); }
            painter.restore(); return;
        }
        if (type == QStringLiteral("forestArea") || type == QStringLiteral("mountainArea")) {
            const QJsonArray points = object.value(QStringLiteral("points")).toArray(); if (points.isEmpty()) return;
            const QColor ink = type == QStringLiteral("forestArea") ? themeColor(pilin, QStringLiteral("forest"), QColor(QStringLiteral("#35563a"))) : themeColor(pilin, QStringLiteral("mountain"), QColor(QStringLiteral("#4e463d")));
            painter.save();
            for (int i = 0; i < points.size(); ++i) {
                const QPointF center = jsonPoint(points.at(i)); const double radius = pointRadius(points.at(i), brushRadius_); const int count = qBound(4, object.value(QStringLiteral("density")).toInt(density_) / 5, 20);
                for (int j = 0; j < count; ++j) {
                    const double a = hash01(qRound(center.x()), qRound(center.y()), j) * kPi * 2.0;
                    const double rr = radius * std::sqrt(hash01(qRound(center.x()) + 7, qRound(center.y()) + 11, j + 3)) * .92;
                    const QPointF pos = worldToScreen(center + QPointF(std::cos(a) * rr, std::sin(a) * rr));
                    const double base = object.value(QStringLiteral("symbolSize")).toDouble(symbolSize_) * zoom_ * (0.30 + hash01(i, j, 17) * .18);
                    const double s = std::clamp(base, 5.0, 22.0);
                    if (type == QStringLiteral("forestArea")) drawTree(painter, pos, s, ink); else drawMountain(painter, pos, s, ink);
                }
            }
            painter.restore(); return;
        }
        if (type == QStringLiteral("settlement")) {
            const QPointF p = worldToScreen(QPointF(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble()));
            const double s = std::clamp(object.value(QStringLiteral("scale")).toDouble(1.0) * symbolSize_ * zoom_ * .45, 5.0, 18.0);
            const QColor ink = themeColor(pilin, QStringLiteral("symbol"), QColor(QStringLiteral("#302820")));
            painter.save(); painter.setPen(QPen(ink, 1.5)); painter.setBrush(QColor(238, 223, 188)); painter.drawEllipse(p, s * .48, s * .48); painter.setBrush(ink); painter.drawEllipse(p, s * .13, s * .13);
            QFont font(QStringLiteral("Georgia")); font.setPointSizeF(std::clamp(8.0 + zoom_ * 7.0, 8.0, 12.0)); font.setBold(true); painter.setFont(font); painter.setPen(ink); painter.drawText(QPointF(p.x() + s * .8, p.y() + 4), object.value(QStringLiteral("label")).toString()); painter.restore();
            if (selected) { painter.setPen(QPen(QColor(QStringLiteral("#d3a15f")), 1.5, Qt::DashLine)); painter.setBrush(Qt::NoBrush); painter.drawEllipse(p, s * 1.3, s * 1.3); } return;
        }
        if (type == QStringLiteral("stamp")) {
            const QPointF p = worldToScreen(QPointF(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble()));
            const double size = std::clamp(object.value(QStringLiteral("size")).toDouble(symbolSize_ * 2.2) * object.value(QStringLiteral("scale")).toDouble(1.0) * zoom_, 12.0, 100.0);
            painter.save(); painter.translate(p); painter.rotate(object.value(QStringLiteral("rotation")).toDouble()); painter.setOpacity(std::clamp(object.value(QStringLiteral("opacity")).toDouble(1.0), 0.0, 1.0));
            const QString data = object.value(QStringLiteral("dataUrl")).toString(); QImage image; if (!data.isEmpty()) image.loadFromData(dataUrlBytes(data));
            if (!image.isNull()) painter.drawImage(QRectF(-size / 2, -size / 2, size, size), image); else drawBuiltinStamp(painter, object.value(QStringLiteral("assetKind")).toString(), size);
            painter.restore(); return;
        }
        if (type == QStringLiteral("label")) {
            const QPointF p = worldToScreen(QPointF(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble()));
            painter.save(); painter.translate(p); painter.rotate(object.value(QStringLiteral("rotation")).toDouble()); QFont font(QStringLiteral("Georgia")); font.setPointSizeF(std::clamp(object.value(QStringLiteral("fontSize")).toDouble(54) * zoom_ * .55, 8.0, 34.0)); font.setBold(object.value(QStringLiteral("bold")).toBool(false)); font.setLetterSpacing(QFont::PercentageSpacing, 104); painter.setFont(font);
            QPainterPath glyphs; glyphs.addText(QPointF(0,0), font, object.value(QStringLiteral("text")).toString()); painter.setBrush(themeColor(pilin, QStringLiteral("text"), QColor(QStringLiteral("#29251f")))); painter.setPen(QPen(themeColor(pilin, QStringLiteral("labelOutline"), QColor(QStringLiteral("#eadfbe"))), 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); painter.drawPath(glyphs); painter.setPen(QPen(themeColor(pilin, QStringLiteral("text"), QColor(QStringLiteral("#29251f"))), .8)); painter.drawPath(glyphs); painter.restore(); return;
        }
        const QJsonArray points = object.value(QStringLiteral("points")).toArray(); if (points.size() < 2) return;
        QPainterPath path = smoothPath(points, [this](const QPointF& p) { return worldToScreen(p); });
        const double w = std::max(1.0, object.value(QStringLiteral("width")).toDouble(strokeWidth_) * zoom_);
        painter.save(); painter.setBrush(Qt::NoBrush);
        if (type == QStringLiteral("river")) { const QColor c = themeColor(pilin, QStringLiteral("river"), QColor(QStringLiteral("#24506d"))); painter.setPen(QPen(c.darker(145), w + 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); painter.drawPath(path); painter.setPen(QPen(c.lighter(150), std::max(1.0, w * .46), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); painter.drawPath(path); }
        else if (type == QStringLiteral("road")) { const QColor c = themeColor(pilin, QStringLiteral("road"), QColor(QStringLiteral("#795532"))); painter.setPen(QPen(c.darker(155), w + 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); painter.drawPath(path); painter.setPen(QPen(c.lighter(155), std::max(1.0, w * .42), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); painter.drawPath(path); }
        else if (type == QStringLiteral("border")) { painter.setPen(QPen(themeColor(pilin, QStringLiteral("border"), QColor(QStringLiteral("#8c443b"))), std::max(1.0, w), Qt::DashLine, Qt::RoundCap, Qt::RoundJoin)); painter.drawPath(path); }
        else if (type == QStringLiteral("region")) { QColor c = themeColor(pilin, QStringLiteral("region"), QColor(QStringLiteral("#a76d44"))); QColor fill = c; fill.setAlphaF(std::clamp(object.value(QStringLiteral("fillOpacity")).toDouble(.12), 0.0, .6)); painter.setPen(QPen(c.darker(120), std::max(1.0, w), Qt::DashLine)); painter.setBrush(fill); painter.drawPath(path); }
        painter.restore();
        if (selected) { painter.setPen(QPen(QColor(QStringLiteral("#d3a15f")), 1.4)); painter.setBrush(QColor(QStringLiteral("#d3a15f"))); for (const QJsonValue& v : points) { const QPointF pt = worldToScreen(jsonPoint(v)); painter.drawRect(QRectF(pt.x()-3, pt.y()-3, 6, 6)); } }
    }
    void drawPreview(QPainter& painter, const QJsonObject& pilin) {
        if (!nodes_.isEmpty()) { QJsonArray preview = nodes_; preview.append(pointJson(cursorWorld_)); QJsonObject object{{QStringLiteral("type"), typeForTool(tool_)},{QStringLiteral("points"), preview},{QStringLiteral("width"), strokeWidth_}}; drawObject(painter, object, pilin); }
        if (drawing_ && !stroke_.isEmpty()) { QJsonObject object{{QStringLiteral("type"), typeForTool(tool_)},{QStringLiteral("points"), stroke_},{QStringLiteral("density"), density_},{QStringLiteral("symbolSize"), symbolSize_}}; drawObject(painter, object, pilin); }
        if (brushTool(tool_)) { const QPointF center = worldToScreen(cursorWorld_); painter.setBrush(Qt::NoBrush); painter.setPen(QPen(QColor(211, 161, 95, 190), 1.2, Qt::DashLine)); painter.drawEllipse(center, brushRadius_ * zoom_, brushRadius_ * zoom_); }
        if (measuring_) { painter.setPen(QPen(QColor(QStringLiteral("#d3a15f")), 1.6, Qt::DashLine)); painter.drawLine(worldToScreen(measureA_), worldToScreen(measureB_)); }
    }

    QJsonObject map_;
    QImage templateImage_;
    Tool tool_ = Tool::Select;
    QStringList selected_;
    QJsonArray stroke_;
    QJsonArray nodes_;
    bool drawing_ = false;
    bool panning_ = false;
    bool measuring_ = false;
    bool snap_ = false;
    bool grid_ = false;
    bool viewInitialized_ = false;
    bool autoFit_ = true;
    int brushRadius_ = 220;
    int density_ = 58;
    int strokeWidth_ = 5;
    int symbolSize_ = 64;
    double zoom_ = .2;
    QPointF pan_{40,40};
    QPointF lastMouse_;
    QPointF cursorWorld_;
    QPointF measureA_;
    QPointF measureB_;
    QString draggingId_;
    QPointF dragStart_;
    QPointF dragOrigin_;
};

PilinReyEditor::PilinReyEditor(QWidget* parent) : QWidget(parent) { buildUi(); }
PilinReyEditor::~PilinReyEditor() = default;

void PilinReyEditor::buildUi() {
    setObjectName(QStringLiteral("pilinReyEditor"));
    auto* root = new QVBoxLayout(this); root->setContentsMargins(0,0,0,0); root->setSpacing(0);
    canvasHost_ = new QWidget(this); canvasHost_->setObjectName(QStringLiteral("pilinCanvasHost"));
    auto* canvasLayout = new QVBoxLayout(canvasHost_); canvasLayout->setContentsMargins(0,0,0,0);
    viewport_ = new PilinReyViewport(canvasHost_); canvasLayout->addWidget(viewport_); root->addWidget(canvasHost_,1);

    toolRail_ = new QFrame(canvasHost_); toolRail_->setObjectName(QStringLiteral("pilinToolPalette"));
    auto* tools = new QVBoxLayout(toolRail_); tools->setContentsMargins(10,48,10,10); tools->setSpacing(2);
    auto* eyebrow = new QLabel(tr("PILÍN REY"), toolRail_); eyebrow->setObjectName(QStringLiteral("mapPaletteEyebrow"));
    auto* title = new QLabel(tr("Pinceles y terreno"), toolRail_); title->setObjectName(QStringLiteral("mapPaletteTitle"));
    eyebrow->setGeometry(14,10,160,15); title->setGeometry(14,25,168,22); eyebrow->show(); title->show();
    struct Def { Tool tool; const char* text; const char* tip; };
    const Def defs[] = {{Tool::Select,"↖  Selección","Seleccionar"},{Tool::Pan,"✥  Mover mapa","Mover lienzo"},{Tool::Coast,"▰  Tierra y costa","Pintar tierra"},{Tool::Eraser,"◌  Mar / borrar","Recortar tierra / pintar mar"},{Tool::River,"∿  Ríos","Río por nodos"},{Tool::Road,"━  Caminos","Camino por nodos"},{Tool::Border,"┄  Fronteras","Frontera por nodos"},{Tool::Region,"◇  Regiones","Región cerrada"},{Tool::Forest,"♣  Bosques","Pincel de bosque"},{Tool::Mountain,"△  Montañas","Pincel de cordillera"},{Tool::Settlement,"●  Asentamientos","Asentamiento"},{Tool::Stamp,"✦  Assets y sellos","Asset / sello"},{Tool::Label,"T  Etiquetas","Etiqueta cartográfica"},{Tool::Measure,"↔  Medir","Medir"}};
    for (const Def& def : defs) { auto* b = toolButton(QString::fromUtf8(def.text), tr(def.tip), toolRail_, QStringLiteral("pilinMapTool")); b->setCheckable(true); b->setAutoExclusive(true); b->setMinimumWidth(176); b->setMaximumWidth(176); if (def.tool == Tool::Select) b->setChecked(true); if (def.tool == Tool::Stamp) stampToolButton_ = b; connect(b,&QToolButton::clicked,this,[this,t=def.tool](){setActiveTool(t);}); toolButtons_.append(b); tools->addWidget(b); }

    topCommands_ = new QFrame(canvasHost_); topCommands_->setObjectName(QStringLiteral("pilinTopCommands")); auto* commands = new QHBoxLayout(topCommands_); commands->setContentsMargins(8,5,8,5); commands->setSpacing(3);
    undoButton_ = toolButton(QStringLiteral("↶"),tr("Deshacer"),topCommands_,QStringLiteral("pilinCommand")); redoButton_ = toolButton(QStringLiteral("↷"),tr("Rehacer"),topCommands_,QStringLiteral("pilinCommand"));
    layersButton_ = toolButton(tr("Capas"),tr("Capas"),topCommands_,QStringLiteral("pilinCommand")); assetsButton_ = toolButton(tr("Assets"),tr("Assets"),topCommands_,QStringLiteral("pilinCommand")); templateButton_ = toolButton(tr("Plantilla"),tr("Plantilla"),topCommands_,QStringLiteral("pilinCommand")); appearanceButton_ = toolButton(tr("Apariencia"),tr("Apariencia"),topCommands_,QStringLiteral("pilinCommand")); auto* fit = toolButton(tr("Encajar"),tr("Encajar"),topCommands_,QStringLiteral("pilinCommand")); auto* exportButton = toolButton(tr("Exportar"),tr("Exportar"),topCommands_,QStringLiteral("pilinCommand"));
    snapCheck_ = new QCheckBox(tr("Ajustar"),topCommands_); gridCheck_ = new QCheckBox(tr("Cuadrícula"),topCommands_);
    commands->addWidget(undoButton_); commands->addWidget(redoButton_); commands->addSpacing(4); commands->addWidget(layersButton_); commands->addWidget(assetsButton_); commands->addWidget(templateButton_); commands->addWidget(appearanceButton_); commands->addWidget(fit); commands->addStretch(); commands->addWidget(snapCheck_); commands->addWidget(gridCheck_); commands->addWidget(exportButton);

    auto makePopover = [this](const QString& titleText) { auto* frame = new QFrame(canvasHost_); frame->setObjectName(QStringLiteral("pilinPopover")); auto* layout = new QVBoxLayout(frame); layout->setContentsMargins(12,12,12,12); layout->setSpacing(7); auto* title = new QLabel(titleText,frame); title->setObjectName(QStringLiteral("pilinPopoverTitle")); layout->addWidget(title); return frame; };
    layersPopover_ = makePopover(tr("Capas")); { auto* layout = qobject_cast<QVBoxLayout*>(layersPopover_->layout()); layers_ = new QListWidget(layersPopover_); layers_->setMinimumHeight(190); layout->addWidget(layers_); auto* row = new QHBoxLayout; auto* add = toolButton("+",tr("Nueva capa"),layersPopover_,"pilinCommand"); auto* dup = toolButton("⧉",tr("Duplicar capa"),layersPopover_,"pilinCommand"); auto* del = toolButton("−",tr("Eliminar capa"),layersPopover_,"pilinCommand"); auto* up = toolButton("↑",tr("Subir"),layersPopover_,"pilinCommand"); auto* down = toolButton("↓",tr("Bajar"),layersPopover_,"pilinCommand"); row->addWidget(add);row->addWidget(dup);row->addWidget(del);row->addStretch();row->addWidget(up);row->addWidget(down);layout->addLayout(row); layerLocked_ = new QCheckBox(tr("Bloquear capa"),layersPopover_); layerOpacity_ = new QSlider(Qt::Horizontal,layersPopover_);layerOpacity_->setRange(0,100);layerOpacity_->setValue(100);layout->addWidget(layerLocked_);layout->addWidget(new QLabel(tr("Opacidad"),layersPopover_));layout->addWidget(layerOpacity_); connect(add,&QToolButton::clicked,this,&PilinReyEditor::addLayer);connect(dup,&QToolButton::clicked,this,&PilinReyEditor::duplicateLayer);connect(del,&QToolButton::clicked,this,&PilinReyEditor::removeLayer);connect(up,&QToolButton::clicked,this,[this](){moveLayer(-1);});connect(down,&QToolButton::clicked,this,[this](){moveLayer(1);}); }
    assetsPopover_ = makePopover(tr("Assets y sellos")); { auto* layout = qobject_cast<QVBoxLayout*>(assetsPopover_->layout()); assets_ = new QListWidget(assetsPopover_); assets_->setViewMode(QListView::IconMode); assets_->setResizeMode(QListView::Adjust); assets_->setMovement(QListView::Static); assets_->setGridSize(QSize(86,68)); assets_->setMinimumHeight(230); auto* import = new QPushButton(tr("Importar imagen…"),assetsPopover_); layout->addWidget(assets_); layout->addWidget(import); connect(import,&QPushButton::clicked,this,&PilinReyEditor::importAsset); }
    templatePopover_ = makePopover(tr("Plantilla de referencia")); { auto* layout = qobject_cast<QVBoxLayout*>(templatePopover_->layout()); auto* buttons = new QHBoxLayout; auto* load = new QPushButton(tr("Cargar…"),templatePopover_); auto* clear = new QPushButton(tr("Quitar"),templatePopover_); buttons->addWidget(load);buttons->addWidget(clear);layout->addLayout(buttons); templateOpacity_=new QSlider(Qt::Horizontal,templatePopover_);templateOpacity_->setRange(0,100);templateOpacity_->setValue(35); templateScale_=new QSlider(Qt::Horizontal,templatePopover_);templateScale_->setRange(10,300);templateScale_->setValue(100); templateRotation_=new QSlider(Qt::Horizontal,templatePopover_);templateRotation_->setRange(-180,180);layout->addWidget(new QLabel(tr("Opacidad"),templatePopover_));layout->addWidget(templateOpacity_);layout->addWidget(new QLabel(tr("Escala"),templatePopover_));layout->addWidget(templateScale_);layout->addWidget(new QLabel(tr("Rotación"),templatePopover_));layout->addWidget(templateRotation_);connect(load,&QPushButton::clicked,this,&PilinReyEditor::chooseTemplate);connect(clear,&QPushButton::clicked,this,&PilinReyEditor::clearTemplate); }
    appearancePopover_ = makePopover(tr("Apariencia")); { auto* layout = qobject_cast<QVBoxLayout*>(appearancePopover_->layout()); const QList<QPair<QString,QString>> entries{{"land",tr("Tierra")},{"sea",tr("Mar")},{"coast",tr("Costa")},{"river",tr("Ríos")},{"road",tr("Caminos")},{"border",tr("Fronteras")},{"forest",tr("Bosques")},{"mountain",tr("Montañas")},{"text",tr("Texto")}}; for (const auto& e:entries){auto* b=new QPushButton(e.second,appearancePopover_);connect(b,&QPushButton::clicked,this,[this,key=e.first](){setThemeColor(key);});layout->addWidget(b);} }

    auto* options = new QFrame(canvasHost_); options->setObjectName(QStringLiteral("pilinToolOptions")); auto* optionLayout = new QVBoxLayout(options); optionLayout->setContentsMargins(12,10,12,10); optionLayout->setSpacing(5); toolOptionsLabel_ = new QLabel(tr("Herramienta"),options); toolOptionsLabel_->setObjectName(QStringLiteral("pilinToolOptionsLabel")); optionLayout->addWidget(toolOptionsLabel_);
    auto addSlider=[&](const QString& label,int min,int max,int value,QSlider*& target){optionLayout->addWidget(new QLabel(label,options));target=new QSlider(Qt::Horizontal,options);target->setRange(min,max);target->setValue(value);target->setMinimumWidth(170);optionLayout->addWidget(target);}; addSlider(tr("Tamaño"),20,700,brushRadius_,brushRadiusSlider_); addSlider(tr("Densidad"),10,100,density_,densitySlider_); addSlider(tr("Trazo"),1,24,strokeWidth_,strokeWidthSlider_); addSlider(tr("Símbolo"),16,160,symbolSize_,symbolSizeSlider_); options->hide();

    selectionPopover_ = new QFrame(canvasHost_); selectionPopover_->setObjectName(QStringLiteral("pilinSelectionPopover")); auto* sl = new QVBoxLayout(selectionPopover_); sl->setContentsMargins(10,9,10,9); selectionLabel_=new QLabel(tr("Selección"),selectionPopover_);selectionLabel_->setObjectName(QStringLiteral("pilinPopoverTitle"));sl->addWidget(selectionLabel_);auto* actions=new QHBoxLayout;editObjectButton_=toolButton("✎",tr("Editar"),selectionPopover_,"pilinCommand");duplicateObjectButton_=toolButton("⧉",tr("Duplicar"),selectionPopover_,"pilinCommand");linkAtlasButton_=toolButton("⌁",tr("Enlazar Atlas"),selectionPopover_,"pilinCommand");deleteObjectButton_=toolButton("×",tr("Eliminar"),selectionPopover_,"pilinCommand");actions->addWidget(editObjectButton_);actions->addWidget(duplicateObjectButton_);actions->addWidget(linkAtlasButton_);actions->addWidget(deleteObjectButton_);sl->addLayout(actions);auto* transforms=new QHBoxLayout;rotateLeftButton_=toolButton("↺",tr("Rotar −15°"),selectionPopover_,"pilinCommand");rotateRightButton_=toolButton("↻",tr("Rotar +15°"),selectionPopover_,"pilinCommand");scaleDownButton_=toolButton("−",tr("Reducir"),selectionPopover_,"pilinCommand");scaleUpButton_=toolButton("+",tr("Aumentar"),selectionPopover_,"pilinCommand");transforms->addWidget(rotateLeftButton_);transforms->addWidget(rotateRightButton_);transforms->addWidget(scaleDownButton_);transforms->addWidget(scaleUpButton_);sl->addLayout(transforms);
    status_=new QLabel(tr("Pilín Rey · listo"),canvasHost_);status_->setObjectName(QStringLiteral("pilinStatus"));status_->setAttribute(Qt::WA_TransparentForMouseEvents,true);

    viewport_->onPath=[this](const QString& type,const QJsonArray& points){addPathObject(type,points);}; viewport_->onSettlement=[this](double x,double y){addSettlement(x,y);}; viewport_->onStamp=[this](double x,double y){addStamp(x,y);}; viewport_->onLabel=[this](double x,double y){addLabel(x,y);}; viewport_->onSelection=[this](const QStringList& ids){selectObjects(ids);}; viewport_->onMovePoint=[this](const QString& id,double x,double y){movePointObject(id,x,y);}; viewport_->onDeleteSelection=[this](){deleteSelectedObject();}; viewport_->onStatus=[this](const QString& text){status_->setText(text);};
    connect(brushRadiusSlider_,&QSlider::valueChanged,this,[this](int v){brushRadius_=v;viewport_->setBrushRadius(v);});connect(densitySlider_,&QSlider::valueChanged,this,[this](int v){density_=v;viewport_->setDensity(v);});connect(strokeWidthSlider_,&QSlider::valueChanged,this,[this](int v){strokeWidth_=v;viewport_->setStrokeWidth(v);});connect(symbolSizeSlider_,&QSlider::valueChanged,this,[this](int v){symbolSize_=v;viewport_->setSymbolSize(v);});
    auto toggle=[this](QFrame* panel){for(QFrame* f:{layersPopover_,assetsPopover_,templatePopover_,appearancePopover_})if(f!=panel)f->hide();panel->setVisible(!panel->isVisible());layoutFloatingPanels();};connect(layersButton_,&QToolButton::clicked,this,[this,toggle](){toggle(layersPopover_);});connect(assetsButton_,&QToolButton::clicked,this,[this,toggle](){toggle(assetsPopover_);});connect(templateButton_,&QToolButton::clicked,this,[this,toggle](){toggle(templatePopover_);});connect(appearanceButton_,&QToolButton::clicked,this,[this,toggle](){toggle(appearancePopover_);});connect(fit,&QToolButton::clicked,viewport_,&PilinReyViewport::fitDocument);connect(undoButton_,&QToolButton::clicked,this,&PilinReyEditor::undo);connect(redoButton_,&QToolButton::clicked,this,&PilinReyEditor::redo);connect(exportButton,&QToolButton::clicked,this,&PilinReyEditor::exportMap);connect(snapCheck_,&QCheckBox::toggled,viewport_,&PilinReyViewport::setSnap);connect(gridCheck_,&QCheckBox::toggled,viewport_,&PilinReyViewport::setGrid);
    connect(layerLocked_,&QCheckBox::toggled,this,[this](bool v){if(!refreshing_)setLayerLocked(v);});connect(layerOpacity_,&QSlider::valueChanged,this,[this](int v){if(!refreshing_)setLayerOpacity(v);});connect(layers_,&QListWidget::itemChanged,this,&PilinReyEditor::applyLayerItem);connect(layers_,&QListWidget::currentRowChanged,this,[this](int row){if(refreshing_||row<0)return;QJsonObject p=map_.value("pilinRey").toObject();QJsonArray ls=p.value("layers").toArray();if(row>=ls.size())return;p.insert("activeLayerId",ls.at(row).toObject().value("id").toString());map_.insert("pilinRey",p);refreshLayerControls();persistToArchive();});
    connect(assets_,&QListWidget::itemClicked,this,[this](QListWidgetItem* item){if(!item)return;selectedAssetKind_=item->data(Qt::UserRole).toString();selectedAssetData_=item->data(Qt::UserRole+1).toString();setActiveTool(Tool::Stamp);if(stampToolButton_)stampToolButton_->setChecked(true);assetsPopover_->hide();});
    connect(templateOpacity_,&QSlider::valueChanged,this,[this](int v){if(!refreshing_)mutateDocument([&](QJsonObject& p){auto t=p.value("template").toObject();t.insert("opacity",v/100.0);p.insert("template",t);});});connect(templateScale_,&QSlider::valueChanged,this,[this](int v){if(!refreshing_)mutateDocument([&](QJsonObject& p){auto t=p.value("template").toObject();t.insert("scale",v/100.0);p.insert("template",t);});});connect(templateRotation_,&QSlider::valueChanged,this,[this](int v){if(!refreshing_)mutateDocument([&](QJsonObject& p){auto t=p.value("template").toObject();t.insert("rotation",v);p.insert("template",t);});});
    connect(editObjectButton_,&QToolButton::clicked,this,&PilinReyEditor::editSelectedObject);connect(duplicateObjectButton_,&QToolButton::clicked,this,&PilinReyEditor::duplicateSelectedObject);connect(deleteObjectButton_,&QToolButton::clicked,this,&PilinReyEditor::deleteSelectedObject);connect(linkAtlasButton_,&QToolButton::clicked,this,&PilinReyEditor::linkSelectedToAtlas);connect(rotateLeftButton_,&QToolButton::clicked,this,[this](){transformSelection(1.0,-15);});connect(rotateRightButton_,&QToolButton::clicked,this,[this](){transformSelection(1.0,15);});connect(scaleDownButton_,&QToolButton::clicked,this,[this](){transformSelection(.9,0);});connect(scaleUpButton_,&QToolButton::clicked,this,[this](){transformSelection(1.1,0);});
    auto* undoShortcut=new QShortcut(QKeySequence::Undo,this);connect(undoShortcut,&QShortcut::activated,this,&PilinReyEditor::undo);auto* redoShortcut=new QShortcut(QKeySequence::Redo,this);connect(redoShortcut,&QShortcut::activated,this,&PilinReyEditor::redo);auto* copyShortcut=new QShortcut(QKeySequence::Copy,this);connect(copyShortcut,&QShortcut::activated,this,&PilinReyEditor::copySelectedObject);auto* pasteShortcut=new QShortcut(QKeySequence::Paste,this);connect(pasteShortcut,&QShortcut::activated,this,&PilinReyEditor::pasteCopiedObject);auto* duplicateShortcut=new QShortcut(QKeySequence(Qt::CTRL|Qt::Key_D),this);connect(duplicateShortcut,&QShortcut::activated,this,&PilinReyEditor::duplicateSelectedObject);

    for(QFrame* f:{layersPopover_,assetsPopover_,templatePopover_,appearancePopover_,selectionPopover_})f->hide();
    setStyleSheet(QStringLiteral(R"QSS(
#pilinReyEditor,#pilinCanvasHost,#pilinQtViewport{background:#14110d;}
#pilinToolPalette,#pilinTopCommands,#pilinPopover,#pilinSelectionPopover,#pilinToolOptions{background:rgba(16,17,17,244);color:#d8d2c7;border:1px solid #3a372f;border-radius:10px;}
#mapPaletteEyebrow{color:#bb8d57;font-size:7pt;font-weight:700;letter-spacing:1.3px;}#mapPaletteTitle{color:#f0eadf;font-family:'Georgia';font-size:12pt;font-weight:600;}
QToolButton#pilinMapTool,QToolButton#pilinCommand{background:transparent;color:#c7c1b7;border:0;border-radius:7px;padding:6px 9px;text-align:left;}QToolButton#pilinCommand{text-align:center;}
QToolButton#pilinMapTool:hover,QToolButton#pilinCommand:hover{background:#28251f;color:#fff8ea;}QToolButton#pilinMapTool:checked{background:#3a2f23;color:#edc58c;font-weight:700;}
#pilinPopoverTitle,#pilinToolOptionsLabel{color:#f0eadf;font-family:'Georgia';font-size:11pt;font-weight:600;}#pilinPopover QListWidget{background:#0c0e0f;color:#d9d5cd;border:1px solid #34332e;border-radius:8px;outline:0;}#pilinPopover QListWidget::item{padding:8px;border-radius:6px;}#pilinPopover QListWidget::item:selected{background:#342a20;color:#edc58c;}
#pilinPopover QPushButton{background:#191b1b;color:#d8d4cb;border:1px solid #383832;border-radius:7px;padding:7px 9px;}#pilinPopover QPushButton:hover{background:#262620;color:#fff8eb;}QCheckBox{color:#c6c1b8;spacing:7px;}QSlider::groove:horizontal{height:4px;background:#34342f;border-radius:2px;}QSlider::handle:horizontal{width:12px;margin:-4px 0;background:#ba8b55;border-radius:6px;}#pilinStatus{background:rgba(16,17,17,230);color:#c8c2b8;border:1px solid #38352f;border-radius:7px;padding:6px 9px;}
)QSS"));
}

void PilinReyEditor::resizeEvent(QResizeEvent* e){QWidget::resizeEvent(e);layoutFloatingPanels();}
void PilinReyEditor::showEvent(QShowEvent* e){QWidget::showEvent(e);layoutFloatingPanels();}
void PilinReyEditor::layoutFloatingPanels(){if(!canvasHost_)return;const int m=14;if(toolRail_){toolRail_->adjustSize();toolRail_->setFixedWidth(196);toolRail_->move(m,qMax(m,(canvasHost_->height()-toolRail_->height())/2));toolRail_->raise();}if(topCommands_){topCommands_->adjustSize();topCommands_->move(qMax(224,(canvasHost_->width()-topCommands_->width())/2),m);topCommands_->raise();}int y=64;for(QFrame* f:{layersPopover_,assetsPopover_,templatePopover_,appearancePopover_}){if(!f||!f->isVisible())continue;f->setMinimumWidth(285);f->setMaximumWidth(310);f->adjustSize();f->move(canvasHost_->width()-f->width()-m,y);f->raise();y=f->geometry().bottom()+10;}if(selectionPopover_&&selectionPopover_->isVisible()){selectionPopover_->adjustSize();selectionPopover_->move(canvasHost_->width()-selectionPopover_->width()-m,qMax(70,canvasHost_->height()-selectionPopover_->height()-24));selectionPopover_->raise();}if(QWidget* o=canvasHost_->findChild<QWidget*>("pilinToolOptions");o&&o->isVisible()){o->adjustSize();o->move(224,qMax(80,(canvasHost_->height()-o->height())/2));o->raise();}if(status_){status_->adjustSize();status_->move(m,canvasHost_->height()-status_->height()-m);status_->raise();}}

void PilinReyEditor::setMap(const QJsonObject& map){map_=map;ensurePilinDocument();undoStack_.clear();redoStack_.clear();selectedObjectIds_.clear();refreshLayers();refreshAssets();refreshSelectionControls();refreshViewport();QTimer::singleShot(0,viewport_,[this](){if(viewport_)viewport_->fitDocument();});}
void PilinReyEditor::ensurePilinDocument(){QJsonObject p=map_.value("pilinRey").toObject();p.insert("version",8);if(!p.contains("width"))p.insert("width",4096);if(!p.contains("height"))p.insert("height",2304);QJsonObject theme=p.value("theme").toObject();const QList<QPair<QString,QString>> defaults{{"land","#d7c798"},{"sea","#9fb7b6"},{"coast","#4a4033"},{"river","#24506d"},{"road","#795532"},{"border","#8c443b"},{"forest","#35563a"},{"mountain","#4e463d"},{"region","#a76d44"},{"symbol","#302820"},{"text","#29251f"},{"labelOutline","#eadfbe"}};for(const auto& e:defaults)if(!theme.contains(e.first))theme.insert(e.first,e.second);p.insert("theme",theme);QJsonObject t=p.value("template").toObject();if(!t.contains("dataUrl"))t.insert("dataUrl",map_.value("backgroundImageDataUrl").toString());if(!t.contains("opacity"))t.insert("opacity",.35);if(!t.contains("visible"))t.insert("visible",true);if(!t.contains("scale"))t.insert("scale",1.0);if(!t.contains("rotation"))t.insert("rotation",0.0);p.insert("template",t);QJsonArray layers=p.value("layers").toArray();if(layers.isEmpty()){for(const QString& name:{tr("Terreno"),tr("Hidrografía"),tr("Relieve y vegetación"),tr("Fronteras y regiones"),tr("Caminos"),tr("Asentamientos y assets"),tr("Etiquetas")})layers.append(QJsonObject{{"id",uid("layer")},{"name",name},{"visible",true},{"locked",false},{"opacity",1.0},{"objects",QJsonArray()}});p.insert("activeLayerId",layers.first().toObject().value("id").toString());}p.insert("layers",layers);if(!p.contains("assets"))p.insert("assets",QJsonArray());map_.insert("pilinRey",p);}
void PilinReyEditor::refreshLayers(){refreshing_=true;layers_->clear();QJsonObject p=map_.value("pilinRey").toObject();QString active=p.value("activeLayerId").toString();QJsonArray ls=p.value("layers").toArray();int current=0;for(int i=0;i<ls.size();++i){QJsonObject l=ls.at(i).toObject();auto* item=new QListWidgetItem(l.value("name").toString());item->setData(Qt::UserRole,l.value("id").toString());item->setFlags(item->flags()|Qt::ItemIsEditable|Qt::ItemIsUserCheckable);item->setCheckState(l.value("visible").toBool(true)?Qt::Checked:Qt::Unchecked);layers_->addItem(item);if(l.value("id").toString()==active)current=i;}if(layers_->count())layers_->setCurrentRow(current);refreshing_=false;refreshLayerControls();undoButton_->setEnabled(!undoStack_.isEmpty());redoButton_->setEnabled(!redoStack_.isEmpty());}
void PilinReyEditor::refreshLayerControls(){bool old=refreshing_;refreshing_=true;QString active=activeLayerId();for(const QJsonValue& v:map_.value("pilinRey").toObject().value("layers").toArray()){QJsonObject l=v.toObject();if(l.value("id").toString()!=active)continue;layerLocked_->setChecked(l.value("locked").toBool(false));layerOpacity_->setValue(qRound(l.value("opacity").toDouble(1.0)*100));break;}refreshing_=old;}
void PilinReyEditor::refreshAssets(){refreshing_=true;assets_->clear();const QList<QPair<QString,QString>> builtins{{"castle",tr("Castillo")},{"tower",tr("Torre")},{"temple",tr("Templo")},{"ruin",tr("Ruina")},{"ship",tr("Barco")},{"bridge",tr("Puente")},{"compass",tr("Brújula")},{"mill",tr("Molino")}};for(const auto& a:builtins){auto* item=new QListWidgetItem(QStringLiteral("✦\n")+a.second);item->setTextAlignment(Qt::AlignCenter);item->setData(Qt::UserRole,a.first);assets_->addItem(item);}for(const QJsonValue& v:map_.value("pilinRey").toObject().value("assets").toArray()){QJsonObject a=v.toObject();auto* item=new QListWidgetItem(QStringLiteral("▧\n")+a.value("name").toString(tr("Asset")));item->setTextAlignment(Qt::AlignCenter);item->setData(Qt::UserRole,"custom");item->setData(Qt::UserRole+1,a.value("dataUrl").toString());assets_->addItem(item);}refreshing_=false;}
void PilinReyEditor::refreshSelectionControls(){QJsonArray objects=selectedObjects();bool has=!objects.isEmpty();selectionPopover_->setVisible(has);editObjectButton_->setEnabled(objects.size()==1);duplicateObjectButton_->setEnabled(has);deleteObjectButton_->setEnabled(has);linkAtlasButton_->setEnabled(objects.size()==1&&objects.first().toObject().value("type").toString()=="settlement");selectionLabel_->setText(has?(objects.size()==1?tr("Selección · %1").arg(objects.first().toObject().value("label").toString(objects.first().toObject().value("text").toString(objects.first().toObject().value("type").toString()))):tr("%1 objetos seleccionados").arg(objects.size())):tr("Selección"));layoutFloatingPanels();}
void PilinReyEditor::refreshViewport(){if(viewport_){viewport_->setDocument(map_);viewport_->setSelection(selectedObjectIds_);}}
void PilinReyEditor::chooseTemplate(){QString path=QFileDialog::getOpenFileName(this,tr("Plantilla"),{},tr("Imágenes (*.png *.jpg *.jpeg *.webp)"));if(path.isEmpty())return;QString data=imageToDataUrl(path);if(data.isEmpty())return;mutateDocument([&](QJsonObject& p){QJsonObject t=p.value("template").toObject();t.insert("dataUrl",data);t.insert("visible",true);p.insert("template",t);});map_.insert("backgroundImageDataUrl",data);persistToArchive();}
void PilinReyEditor::clearTemplate(){mutateDocument([](QJsonObject& p){QJsonObject t=p.value("template").toObject();t.insert("dataUrl",QString());p.insert("template",t);});map_.remove("backgroundImageDataUrl");persistToArchive();}
void PilinReyEditor::rotateTemplate(double d){mutateDocument([&](QJsonObject& p){QJsonObject t=p.value("template").toObject();t.insert("rotation",t.value("rotation").toDouble()+d);p.insert("template",t);});}
void PilinReyEditor::scaleTemplate(double f){mutateDocument([&](QJsonObject& p){QJsonObject t=p.value("template").toObject();t.insert("scale",std::clamp(t.value("scale").toDouble(1.0)*f,.05,5.0));p.insert("template",t);});}
void PilinReyEditor::addLayer(){bool ok=false;QString name=QInputDialog::getText(this,tr("Nueva capa"),tr("Nombre:"),QLineEdit::Normal,tr("Nueva capa"),&ok).trimmed();if(!ok||name.isEmpty())return;mutateDocument([&](QJsonObject& p){QJsonArray ls=p.value("layers").toArray();QString id=uid("layer");ls.append(QJsonObject{{"id",id},{"name",name},{"visible",true},{"locked",false},{"opacity",1.0},{"objects",QJsonArray()}});p.insert("layers",ls);p.insert("activeLayerId",id);});}
void PilinReyEditor::duplicateLayer(){int row=layers_->currentRow();if(row<0)return;mutateDocument([&](QJsonObject& p){QJsonArray ls=p.value("layers").toArray();if(row>=ls.size())return;QJsonObject copy=ls.at(row).toObject();QString id=uid("layer");copy.insert("id",id);copy.insert("name",copy.value("name").toString()+tr(" copia"));QJsonArray os=copy.value("objects").toArray();for(int i=0;i<os.size();++i){QJsonObject o=os.at(i).toObject();o.insert("id",uid("mapobj"));os.replace(i,o);}copy.insert("objects",os);ls.insert(row+1,copy);p.insert("layers",ls);p.insert("activeLayerId",id);});}
void PilinReyEditor::removeLayer(){int row=layers_->currentRow();if(row<0)return;mutateDocument([&](QJsonObject& p){QJsonArray ls=p.value("layers").toArray();if(ls.size()<=1||row>=ls.size())return;ls.removeAt(row);p.insert("layers",ls);p.insert("activeLayerId",ls.at(qMin(row,ls.size()-1)).toObject().value("id").toString());});}
void PilinReyEditor::moveLayer(int delta){int row=layers_->currentRow(),target=row+delta;if(row<0)return;mutateDocument([&](QJsonObject& p){QJsonArray ls=p.value("layers").toArray();if(target<0||target>=ls.size())return;QJsonValue v=ls.at(row);ls.removeAt(row);ls.insert(target,v);p.insert("layers",ls);});}
void PilinReyEditor::setLayerLocked(bool locked){QString active=activeLayerId();mutateDocument([&](QJsonObject& p){QJsonArray ls=p.value("layers").toArray();for(int i=0;i<ls.size();++i){QJsonObject l=ls.at(i).toObject();if(l.value("id").toString()!=active)continue;l.insert("locked",locked);ls.replace(i,l);break;}p.insert("layers",ls);});}
void PilinReyEditor::setLayerOpacity(int value){QString active=activeLayerId();mutateDocument([&](QJsonObject& p){QJsonArray ls=p.value("layers").toArray();for(int i=0;i<ls.size();++i){QJsonObject l=ls.at(i).toObject();if(l.value("id").toString()!=active)continue;l.insert("opacity",value/100.0);ls.replace(i,l);break;}p.insert("layers",ls);});}
void PilinReyEditor::applyLayerItem(QListWidgetItem* item){if(refreshing_||!item)return;QString id=item->data(Qt::UserRole).toString(),name=item->text().trimmed();bool visible=item->checkState()==Qt::Checked;mutateDocument([&](QJsonObject& p){QJsonArray ls=p.value("layers").toArray();for(int i=0;i<ls.size();++i){QJsonObject l=ls.at(i).toObject();if(l.value("id").toString()!=id)continue;l.insert("name",name.isEmpty()?tr("Capa"):name);l.insert("visible",visible);ls.replace(i,l);break;}p.insert("layers",ls);});}
void PilinReyEditor::setActiveTool(Tool tool){viewport_->setTool(tool);if(QWidget* o=canvasHost_->findChild<QWidget*>("pilinToolOptions")){bool show=tool==Tool::Coast||tool==Tool::Eraser||tool==Tool::Forest||tool==Tool::Mountain||tool==Tool::River||tool==Tool::Road||tool==Tool::Border||tool==Tool::Region;o->setVisible(show);}toolOptionsLabel_->setText(tool==Tool::Forest?tr("Bosque"):tool==Tool::Mountain?tr("Montañas"):tool==Tool::River?tr("Río"):tool==Tool::Road?tr("Camino"):tool==Tool::Border?tr("Frontera"):tool==Tool::Coast?tr("Tierra y costa"):tool==Tool::Eraser?tr("Mar / borrar"):tr("Herramienta"));layoutFloatingPanels();}
QString PilinReyEditor::activeLayerId()const{return map_.value("pilinRey").toObject().value("activeLayerId").toString();}
void PilinReyEditor::addPathObject(const QString& type,const QJsonArray& points){if(type.isEmpty()||points.isEmpty())return;mutateDocument([&](QJsonObject& p){QJsonArray ls=p.value("layers").toArray();int target=layers_->currentRow();if(target<0||target>=ls.size())target=0;QJsonObject l=ls.at(target).toObject();if(l.value("locked").toBool(false))return;QJsonArray os=l.value("objects").toArray();QString id=uid("mapobj");QJsonObject o{{"id",id},{"type",type},{"points",points},{"width",strokeWidth_},{"density",density_},{"symbolSize",symbolSize_}};if(type=="region"){o.insert("closed",true);o.insert("fillOpacity",.15);}os.append(o);l.insert("objects",os);ls.replace(target,l);p.insert("layers",ls);selectedObjectIds_={id};});}
void PilinReyEditor::addSettlement(double x,double y){bool ok=false;QString name=QInputDialog::getText(this,tr("Asentamiento"),tr("Nombre:"),QLineEdit::Normal,tr("Poblado"),&ok).trimmed();if(!ok||name.isEmpty())return;mutateDocument([&](QJsonObject& p){QJsonArray ls=p.value("layers").toArray();int target=qMax(0,layers_->currentRow());if(target>=ls.size())target=ls.size()-1;QJsonObject l=ls.at(target).toObject();QJsonArray os=l.value("objects").toArray();QString id=uid("mapobj");os.append(QJsonObject{{"id",id},{"type","settlement"},{"label",name},{"x",x},{"y",y},{"scale",1.0}});l.insert("objects",os);ls.replace(target,l);p.insert("layers",ls);selectedObjectIds_={id};});}
void PilinReyEditor::addStamp(double x,double y){mutateDocument([&](QJsonObject& p){QJsonArray ls=p.value("layers").toArray();int target=qMax(0,layers_->currentRow());if(target>=ls.size())target=ls.size()-1;QJsonObject l=ls.at(target).toObject();QJsonArray os=l.value("objects").toArray();QString id=uid("mapobj");QJsonObject o{{"id",id},{"type","stamp"},{"assetKind",selectedAssetKind_},{"x",x},{"y",y},{"scale",1.0},{"rotation",0.0},{"size",symbolSize_*2.2},{"opacity",1.0}};if(!selectedAssetData_.isEmpty())o.insert("dataUrl",selectedAssetData_);os.append(o);l.insert("objects",os);ls.replace(target,l);p.insert("layers",ls);selectedObjectIds_={id};});}
void PilinReyEditor::addLabel(double x,double y){bool ok=false;QString text=QInputDialog::getText(this,tr("Etiqueta"),tr("Texto:"),QLineEdit::Normal,{},&ok).trimmed();if(!ok||text.isEmpty())return;mutateDocument([&](QJsonObject& p){QJsonArray ls=p.value("layers").toArray();int target=qMax(0,layers_->currentRow());if(target>=ls.size())target=ls.size()-1;QJsonObject l=ls.at(target).toObject();QJsonArray os=l.value("objects").toArray();QString id=uid("mapobj");os.append(QJsonObject{{"id",id},{"type","label"},{"text",text},{"x",x},{"y",y},{"fontSize",54},{"rotation",0.0},{"bold",false}});l.insert("objects",os);ls.replace(target,l);p.insert("layers",ls);selectedObjectIds_={id};});}
void PilinReyEditor::selectObjects(const QStringList& ids){selectedObjectIds_=ids;refreshSelectionControls();viewport_->setSelection(ids);}
QJsonObject PilinReyEditor::selectedObject()const{if(selectedObjectIds_.isEmpty())return{};QString id=selectedObjectIds_.first();for(const QJsonValue& lv:map_.value("pilinRey").toObject().value("layers").toArray())for(const QJsonValue& ov:lv.toObject().value("objects").toArray()){QJsonObject o=ov.toObject();if(o.value("id").toString()==id)return o;}return{};}
QJsonArray PilinReyEditor::selectedObjects()const{QJsonArray out;for(const QString& id:selectedObjectIds_)for(const QJsonValue& lv:map_.value("pilinRey").toObject().value("layers").toArray())for(const QJsonValue& ov:lv.toObject().value("objects").toArray()){QJsonObject o=ov.toObject();if(o.value("id").toString()==id){out.append(o);break;}}return out;}
void PilinReyEditor::movePointObject(const QString& id,double x,double y){mutateDocument([&](QJsonObject& p){QJsonArray ls=p.value("layers").toArray();for(int i=0;i<ls.size();++i){QJsonObject l=ls.at(i).toObject();QJsonArray os=l.value("objects").toArray();for(int j=0;j<os.size();++j){QJsonObject o=os.at(j).toObject();if(o.value("id").toString()!=id)continue;o.insert("x",x);o.insert("y",y);os.replace(j,o);l.insert("objects",os);ls.replace(i,l);p.insert("layers",ls);emit markerMoved(id,x,y);return;}}});}
void PilinReyEditor::movePathPoint(const QString&,int,double,double){}
void PilinReyEditor::editSelectedObject(){QJsonObject o=selectedObject();if(o.isEmpty()||selectedObjectIds_.size()!=1)return;QString id=selectedObjectIds_.first(),type=o.value("type").toString();bool ok=false;QString current=type=="label"?o.value("text").toString():o.value("label").toString();QString text=QInputDialog::getText(this,tr("Editar"),tr("Texto:"),QLineEdit::Normal,current,&ok).trimmed();if(!ok||text.isEmpty())return;mutateDocument([&](QJsonObject& p){QJsonArray ls=p.value("layers").toArray();for(int i=0;i<ls.size();++i){QJsonObject l=ls.at(i).toObject();QJsonArray os=l.value("objects").toArray();for(int j=0;j<os.size();++j){QJsonObject cur=os.at(j).toObject();if(cur.value("id").toString()!=id)continue;cur.insert(type=="label"?"text":"label",text);os.replace(j,cur);l.insert("objects",os);ls.replace(i,l);p.insert("layers",ls);return;}}});}
void PilinReyEditor::duplicateSelectedObject(){if(selectedObjectIds_.isEmpty())return;clipboardObjects_=selectedObjects();pasteCopiedObject();}
void PilinReyEditor::deleteSelectedObject(){if(selectedObjectIds_.isEmpty())return;mutateDocument([&](QJsonObject& p){QJsonArray ls=p.value("layers").toArray();for(int i=0;i<ls.size();++i){QJsonObject l=ls.at(i).toObject();QJsonArray os=l.value("objects").toArray();for(int j=os.size()-1;j>=0;--j)if(selectedObjectIds_.contains(os.at(j).toObject().value("id").toString()))os.removeAt(j);l.insert("objects",os);ls.replace(i,l);}p.insert("layers",ls);selectedObjectIds_.clear();});}
void PilinReyEditor::copySelectedObject(){clipboardObjects_=selectedObjects();}
void PilinReyEditor::pasteCopiedObject(){if(clipboardObjects_.isEmpty())return;mutateDocument([&](QJsonObject& p){QJsonArray ls=p.value("layers").toArray();int target=qMax(0,layers_->currentRow());if(target>=ls.size())target=0;QJsonObject l=ls.at(target).toObject();QJsonArray os=l.value("objects").toArray();QStringList ids;for(const QJsonValue& v:clipboardObjects_){QJsonObject o=v.toObject();QString id=uid("mapobj");o.insert("id",id);if(o.contains("x")){o.insert("x",o.value("x").toDouble()+80);o.insert("y",o.value("y").toDouble()+80);}else{QJsonArray ps=o.value("points").toArray();for(int i=0;i<ps.size();++i){QJsonObject pt=ps.at(i).toObject();pt.insert("x",pt.value("x").toDouble()+80);pt.insert("y",pt.value("y").toDouble()+80);ps.replace(i,pt);}o.insert("points",ps);}os.append(o);ids<<id;}l.insert("objects",os);ls.replace(target,l);p.insert("layers",ls);selectedObjectIds_=ids;});}
void PilinReyEditor::linkSelectedToAtlas(){QJsonObject o=selectedObject();if(o.value("type").toString()!="settlement")return;QWidget* cursor=parentWidget();WorldPage* world=nullptr;while(cursor){world=qobject_cast<WorldPage*>(cursor);if(world)break;cursor=cursor->parentWidget();}if(!world||!world->document_)return;QJsonArray atlas=world->document_->array("world");if(atlas.isEmpty()){QMessageBox::information(this,tr("Atlas"),tr("Crea primero una entrada en el Atlas."));return;}QStringList names,ids;for(const QJsonValue& v:atlas){QJsonObject e=v.toObject();names<<e.value("name").toString(tr("Entrada sin nombre"));ids<<e.value("id").toString();}bool ok=false;QString chosen=QInputDialog::getItem(this,tr("Enlazar con Atlas"),tr("Entrada:"),names,0,false,&ok);if(!ok)return;int idx=names.indexOf(chosen);if(idx<0)return;QString id=selectedObjectIds_.first();mutateDocument([&](QJsonObject& p){QJsonArray ls=p.value("layers").toArray();for(int i=0;i<ls.size();++i){QJsonObject l=ls.at(i).toObject();QJsonArray os=l.value("objects").toArray();for(int j=0;j<os.size();++j){QJsonObject cur=os.at(j).toObject();if(cur.value("id").toString()!=id)continue;cur.insert("atlasId",ids.at(idx));os.replace(j,cur);l.insert("objects",os);ls.replace(i,l);p.insert("layers",ls);return;}}});}
void PilinReyEditor::transformSelection(double sf,double rd){if(selectedObjectIds_.isEmpty())return;mutateDocument([&](QJsonObject& p){QJsonArray ls=p.value("layers").toArray();for(int i=0;i<ls.size();++i){QJsonObject l=ls.at(i).toObject();QJsonArray os=l.value("objects").toArray();for(int j=0;j<os.size();++j){QJsonObject o=os.at(j).toObject();if(!selectedObjectIds_.contains(o.value("id").toString()))continue;if(o.contains("scale"))o.insert("scale",std::clamp(o.value("scale").toDouble(1.0)*sf,.1,12.0));if(o.contains("rotation"))o.insert("rotation",o.value("rotation").toDouble()+rd);os.replace(j,o);}l.insert("objects",os);ls.replace(i,l);}p.insert("layers",ls);});}
void PilinReyEditor::importAsset(){QString path=QFileDialog::getOpenFileName(this,tr("Importar asset"),{},tr("Imágenes (*.png *.jpg *.jpeg *.webp)"));if(path.isEmpty())return;QString data=imageToDataUrl(path);if(data.isEmpty())return;mutateDocument([&](QJsonObject& p){QJsonArray as=p.value("assets").toArray();as.append(QJsonObject{{"id",uid("asset")},{"name",QFileInfo(path).completeBaseName()},{"dataUrl",data}});p.insert("assets",as);});}
void PilinReyEditor::setThemeColor(const QString& key){QJsonObject p=map_.value("pilinRey").toObject();QColor current(p.value("theme").toObject().value(key).toString());QColor color=QColorDialog::getColor(current.isValid()?current:Qt::white,this,tr("Color"));if(!color.isValid())return;mutateDocument([&](QJsonObject& target){QJsonObject t=target.value("theme").toObject();t.insert(key,color.name(QColor::HexRgb));target.insert("theme",t);});}
void PilinReyEditor::mutateDocument(const std::function<void(QJsonObject&)>& mutation){if(refreshing_)return;pushUndo();QJsonObject p=map_.value("pilinRey").toObject();mutation(p);map_.insert("pilinRey",p);refreshViewport();refreshLayers();refreshAssets();refreshSelectionControls();persistToArchive();emit mapEdited(map_);}
void PilinReyEditor::pushUndo(){undoStack_.append(map_);while(undoStack_.size()>64)undoStack_.removeFirst();redoStack_.clear();}
void PilinReyEditor::undo(){if(undoStack_.isEmpty())return;redoStack_.append(map_);map_=undoStack_.takeLast();selectedObjectIds_.clear();refreshLayers();refreshAssets();refreshSelectionControls();refreshViewport();persistToArchive();emit mapEdited(map_);}
void PilinReyEditor::redo(){if(redoStack_.isEmpty())return;undoStack_.append(map_);map_=redoStack_.takeLast();selectedObjectIds_.clear();refreshLayers();refreshAssets();refreshSelectionControls();refreshViewport();persistToArchive();emit mapEdited(map_);}
void PilinReyEditor::exportMap(){QJsonObject p=map_.value("pilinRey").toObject();QSize logical(qMax(1,p.value("width").toInt(4096)),qMax(1,p.value("height").toInt(2304)));MapExportDialog dialog(logical,this);if(dialog.exec()!=QDialog::Accepted)return;QString format=dialog.format(),ext=".png",filter=tr("PNG (*.png)");if(format=="svg"){ext=".svg";filter=tr("SVG (*.svg)");}else if(format=="pdf"){ext=".pdf";filter=tr("PDF (*.pdf)");}QString path=QFileDialog::getSaveFileName(this,tr("Exportar mapa"),map_.value("name").toString(tr("mapa"))+ext,filter);if(path.isEmpty())return;if(!path.endsWith(ext,Qt::CaseInsensitive))path+=ext;QString error;bool ok=format=="svg"?MapExporter::exportSvg(map_,path,dialog.outputSize(),&error):format=="pdf"?MapExporter::exportPdf(map_,path,dialog.outputSize(),&error):MapExporter::exportPng(map_,path,dialog.outputSize(),&error);if(!ok)QMessageBox::critical(this,tr("No se pudo exportar"),error);}
void PilinReyEditor::persistToArchive(){QWidget* cursor=parentWidget();WorldPage* world=nullptr;while(cursor){world=qobject_cast<WorldPage*>(cursor);if(world)break;cursor=cursor->parentWidget();}if(!world||!world->document_||!world->mapList_)return;int row=world->mapList_->currentRow();QJsonArray maps=world->document_->array("maps");if(row<0||row>=maps.size())return;QJsonObject stored=maps.at(row).toObject();stored.insert("pilinRey",map_.value("pilinRey"));if(map_.contains("backgroundImageDataUrl"))stored.insert("backgroundImageDataUrl",map_.value("backgroundImageDataUrl"));maps.replace(row,stored);world->document_->setArray("maps",maps);emit world->changed();}

} // namespace wbw
