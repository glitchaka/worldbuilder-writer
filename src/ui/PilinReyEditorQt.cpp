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
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLineF>
#include <QListView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QShortcut>
#include <QShowEvent>
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
constexpr double kTau = kPi * 2.0;

QString uid(const QString& prefix) {
    return prefix + QLatin1Char('-') + QUuid::createUuid().toString(QUuid::Id128);
}

QPointF jsonPoint(const QJsonValue& value) {
    const QJsonObject point = value.toObject();
    return QPointF(point.value(QStringLiteral("x")).toDouble(), point.value(QStringLiteral("y")).toDouble());
}

QJsonObject pointJson(const QPointF& point, double radius = 0.0) {
    QJsonObject result{{QStringLiteral("x"), point.x()}, {QStringLiteral("y"), point.y()}};
    if (radius > 0.0) result.insert(QStringLiteral("radius"), radius);
    return result;
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
    button->setMinimumSize(32, 30);
    return button;
}

QPainterPath smoothPath(const QJsonArray& points, const std::function<QPointF(const QPointF&)>& transform) {
    QPainterPath path;
    if (points.isEmpty()) return path;
    const QPointF first = transform(jsonPoint(points.first()));
    path.moveTo(first);
    if (points.size() == 1) return path;
    for (int i = 1; i < points.size(); ++i) {
        const QPointF previous = transform(jsonPoint(points.at(i - 1)));
        const QPointF current = transform(jsonPoint(points.at(i)));
        const QPointF mid = (previous + current) * 0.5;
        if (i == 1) path.lineTo(mid);
        else path.quadTo(previous, mid);
        if (i == points.size() - 1) path.quadTo(mid, current);
    }
    return path;
}

double segmentDistance(const QPointF& p, const QPointF& a, const QPointF& b) {
    const QPointF ab = b - a;
    const double length2 = ab.x() * ab.x() + ab.y() * ab.y();
    if (length2 < 0.0001) return QLineF(p, a).length();
    const QPointF ap = p - a;
    const double t = std::clamp((ap.x() * ab.x() + ap.y() * ab.y()) / length2, 0.0, 1.0);
    return QLineF(p, a + ab * t).length();
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
    std::function<void(const QString&, int, double, double)> onMovePathPoint;
    std::function<void()> onDeleteSelection;
    std::function<void(const QString&)> onStatus;

    explicit PilinReyViewport(QWidget* parent = nullptr) : QWidget(parent) {
        setObjectName(QStringLiteral("pilinQtViewport"));
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setMinimumSize(320, 220);
        setAutoFillBackground(false);
    }

    void setDocument(const QJsonObject& map) {
        map_ = map;
        templateImage_ = QImage();
        const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        const QString data = pilin.value(QStringLiteral("template")).toObject().value(QStringLiteral("dataUrl")).toString();
        if (!data.isEmpty()) templateImage_.loadFromData(dataUrlBytes(data));
        if (!viewInitialized_) fitDocument();
        update();
    }

    void setTool(Tool tool) {
        tool_ = tool;
        brushStroke_ = QJsonArray();
        nodePath_ = QJsonArray();
        drawing_ = false;
        draggingPointId_.clear();
        draggingPathId_.clear();
        draggingPathIndex_ = -1;
        setCursor(tool_ == Tool::Pan ? Qt::OpenHandCursor : Qt::CrossCursor);
        if (tool_ == Tool::Select) setCursor(Qt::ArrowCursor);
        update();
    }

    void setSelection(const QStringList& ids) { selectedIds_ = ids; update(); }
    void setSnap(bool enabled) { snap_ = enabled; update(); }
    void setGrid(bool enabled) { grid_ = enabled; update(); }
    void setBrushRadius(int value) { brushRadius_ = qBound(30, value, 700); update(); }
    void setDensity(int value) { density_ = qBound(10, value, 100); update(); }
    void setStrokeWidth(int value) { strokeWidth_ = qBound(1, value, 24); update(); }
    void setSymbolSize(int value) { symbolSize_ = qBound(16, value, 160); update(); }

    void fitDocument() {
        const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        const double docW = std::max(1.0, pilin.value(QStringLiteral("width")).toDouble(4096));
        const double docH = std::max(1.0, pilin.value(QStringLiteral("height")).toDouble(2304));
        const double margin = 46.0;
        if (width() < 100 || height() < 100) return;
        zoom_ = std::clamp(std::min((width() - margin * 2.0) / docW, (height() - margin * 2.0) / docH), 0.03, 8.0);
        pan_ = QPointF((width() - docW * zoom_) * 0.5, (height() - docH * zoom_) * 0.5);
        viewInitialized_ = true;
        update();
    }

protected:
    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        if (autoFit_) fitDocument();
    }

    void wheelEvent(QWheelEvent* event) override {
        autoFit_ = false;
        const QPointF cursor = event->position();
        const QPointF before = screenToWorld(cursor);
        const double factor = event->angleDelta().y() >= 0 ? 1.12 : (1.0 / 1.12);
        zoom_ = std::clamp(zoom_ * factor, 0.025, 14.0);
        const QPointF after = screenToWorld(cursor);
        pan_ += QPointF((after.x() - before.x()) * zoom_, (after.y() - before.y()) * zoom_);
        update();
        event->accept();
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Delete && onDeleteSelection) {
            onDeleteSelection();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Escape) {
            brushStroke_ = QJsonArray();
            nodePath_ = QJsonArray();
            drawing_ = false;
            update();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Backspace && !nodePath_.isEmpty()) {
            nodePath_.removeLast();
            update();
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override {
        setFocus(Qt::MouseFocusReason);
        lastMouse_ = event->position();
        cursorWorld_ = snapPoint(screenToWorld(event->position()));

        if (event->button() == Qt::MiddleButton || tool_ == Tool::Pan) {
            panning_ = true;
            autoFit_ = false;
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
        if (event->button() == Qt::RightButton && nodeTool(tool_)) {
            finishNodePath();
            event->accept();
            return;
        }
        if (event->button() != Qt::LeftButton) return;

        if (tool_ == Tool::Select) {
            QString id;
            int pathIndex = -1;
            findObject(cursorWorld_, &id, &pathIndex);
            const bool extend = event->modifiers().testFlag(Qt::ShiftModifier) || event->modifiers().testFlag(Qt::ControlModifier);
            if (!extend) selectedIds_.clear();
            if (!id.isEmpty()) {
                if (extend && selectedIds_.contains(id)) selectedIds_.removeAll(id);
                else if (!selectedIds_.contains(id)) selectedIds_.append(id);
                const QJsonObject object = objectById(id);
                if (object.contains(QStringLiteral("x"))) {
                    draggingPointId_ = id;
                    dragStartWorld_ = cursorWorld_;
                    dragOriginalPoint_ = QPointF(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble());
                } else if (pathIndex >= 0) {
                    draggingPathId_ = id;
                    draggingPathIndex_ = pathIndex;
                    dragStartWorld_ = cursorWorld_;
                    dragOriginalPoint_ = jsonPoint(object.value(QStringLiteral("points")).toArray().at(pathIndex));
                }
            }
            if (onSelection) onSelection(selectedIds_);
            update();
            return;
        }

        if (brushTool(tool_)) {
            drawing_ = true;
            brushStroke_ = QJsonArray{pointJson(cursorWorld_, brushRadius_)};
            update();
            return;
        }

        if (nodeTool(tool_)) {
            nodePath_.append(pointJson(cursorWorld_));
            if (onStatus) onStatus(QObject::tr("Clic: añade nodo · clic derecho: termina"));
            update();
            return;
        }

        if (tool_ == Tool::Settlement && onSettlement) onSettlement(cursorWorld_.x(), cursorWorld_.y());
        else if (tool_ == Tool::Stamp && onStamp) onStamp(cursorWorld_.x(), cursorWorld_.y());
        else if (tool_ == Tool::Label && onLabel) onLabel(cursorWorld_.x(), cursorWorld_.y());
        else if (tool_ == Tool::Measure) {
            if (!measuring_) { measureA_ = cursorWorld_; measureB_ = cursorWorld_; measuring_ = true; }
            else { measureB_ = cursorWorld_; measuring_ = false; }
            update();
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        cursorWorld_ = snapPoint(screenToWorld(event->position()));
        if (panning_) {
            const QPointF delta = event->position() - lastMouse_;
            pan_ += delta;
            lastMouse_ = event->position();
            update();
            return;
        }
        if (drawing_) {
            if (brushStroke_.isEmpty() || QLineF(jsonPoint(brushStroke_.last()), cursorWorld_).length() > std::max(8.0, brushRadius_ * 0.12))
                brushStroke_.append(pointJson(cursorWorld_, brushRadius_));
            update();
            return;
        }
        if (measuring_) { measureB_ = cursorWorld_; update(); }
        update();
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (panning_) {
            panning_ = false;
            setCursor(tool_ == Tool::Pan ? Qt::OpenHandCursor : Qt::ArrowCursor);
            event->accept();
            return;
        }
        if (event->button() != Qt::LeftButton) return;
        if (drawing_) {
            drawing_ = false;
            if (!brushStroke_.isEmpty() && onPath) onPath(typeForTool(tool_), brushStroke_);
            brushStroke_ = QJsonArray();
            update();
            return;
        }
        if (!draggingPointId_.isEmpty()) {
            const QPointF delta = cursorWorld_ - dragStartWorld_;
            const QPointF target = dragOriginalPoint_ + delta;
            if (onMovePoint) onMovePoint(draggingPointId_, target.x(), target.y());
            draggingPointId_.clear();
            return;
        }
        if (!draggingPathId_.isEmpty() && draggingPathIndex_ >= 0) {
            const QPointF delta = cursorWorld_ - dragStartWorld_;
            const QPointF target = dragOriginalPoint_ + delta;
            if (onMovePathPoint) onMovePathPoint(draggingPathId_, draggingPathIndex_, target.x(), target.y());
            draggingPathId_.clear();
            draggingPathIndex_ = -1;
        }
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && nodeTool(tool_)) {
            cursorWorld_ = snapPoint(screenToWorld(event->position()));
            nodePath_.append(pointJson(cursorWorld_));
            finishNodePath();
            event->accept();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }

    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), QColor(QStringLiteral("#15130f")));

        const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        const double docW = pilin.value(QStringLiteral("width")).toDouble(4096);
        const double docH = pilin.value(QStringLiteral("height")).toDouble(2304);
        const QRectF page(worldToScreen(QPointF(0, 0)), worldToScreen(QPointF(docW, docH)));

        painter.save();
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 80));
        painter.drawRoundedRect(page.translated(8, 10), 5, 5);
        painter.restore();

        const QColor sea = themeColor(pilin, QStringLiteral("sea"), QColor(QStringLiteral("#b7c5c2")));
        painter.fillRect(page, sea);

        painter.save();
        painter.setClipRect(page);
        painter.setPen(QPen(QColor(72, 61, 48, 15), 1));
        for (int y = static_cast<int>(page.top()); y < page.bottom(); y += 17) painter.drawLine(QPointF(page.left(), y), QPointF(page.right(), y));
        painter.setPen(QPen(QColor(255, 255, 255, 12), 1));
        for (int x = static_cast<int>(page.left()); x < page.right(); x += 23) painter.drawLine(QPointF(x, page.top()), QPointF(x, page.bottom()));
        painter.restore();

        drawTemplate(painter, page, pilin);
        if (grid_) drawGrid(painter, page, docW, docH);

        const QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();
        for (const QJsonValue& layerValue : layers) {
            const QJsonObject layer = layerValue.toObject();
            if (!layer.value(QStringLiteral("visible")).toBool(true)) continue;
            painter.save();
            painter.setOpacity(std::clamp(layer.value(QStringLiteral("opacity")).toDouble(1.0), 0.0, 1.0));
            for (const QJsonValue& objectValue : layer.value(QStringLiteral("objects")).toArray()) drawObject(painter, objectValue.toObject(), pilin);
            painter.restore();
        }

        drawPreview(painter, pilin);
        painter.setPen(QPen(themeColor(pilin, QStringLiteral("coast"), QColor(QStringLiteral("#4a4135"))), 1.2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(page);
        painter.save();
        painter.setPen(QPen(QColor(80, 69, 54, 90), 1));
        const qreal inset = 7.0;
        painter.drawRect(page.adjusted(inset, inset, -inset, -inset));
        painter.restore();
    }

private:
    QPointF worldToScreen(const QPointF& world) const { return QPointF(world.x() * zoom_ + pan_.x(), world.y() * zoom_ + pan_.y()); }
    QPointF screenToWorld(const QPointF& screen) const { return QPointF((screen.x() - pan_.x()) / zoom_, (screen.y() - pan_.y()) / zoom_); }

    QPointF snapPoint(const QPointF& point) const {
        if (!snap_) return point;
        constexpr double step = 25.0;
        return QPointF(std::round(point.x() / step) * step, std::round(point.y() / step) * step);
    }

    void finishNodePath() {
        if (nodePath_.size() >= 2 && onPath) {
            QJsonArray finished = nodePath_;
            if (tool_ == Tool::Region && finished.size() >= 3) finished.append(finished.first());
            onPath(typeForTool(tool_), finished);
        }
        nodePath_ = QJsonArray();
        update();
    }

    QJsonObject objectById(const QString& id) const {
        const QJsonArray layers = map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray();
        for (const QJsonValue& layerValue : layers) {
            for (const QJsonValue& objectValue : layerValue.toObject().value(QStringLiteral("objects")).toArray()) {
                const QJsonObject object = objectValue.toObject();
                if (object.value(QStringLiteral("id")).toString() == id) return object;
            }
        }
        return {};
    }

    void findObject(const QPointF& world, QString* id, int* pathIndex) const {
        if (id) id->clear();
        if (pathIndex) *pathIndex = -1;
        const double tolerance = 18.0 / std::max(zoom_, 0.03);
        const QJsonArray layers = map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray();
        for (int li = layers.size() - 1; li >= 0; --li) {
            const QJsonObject layer = layers.at(li).toObject();
            if (!layer.value(QStringLiteral("visible")).toBool(true) || layer.value(QStringLiteral("locked")).toBool(false)) continue;
            const QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
            for (int oi = objects.size() - 1; oi >= 0; --oi) {
                const QJsonObject object = objects.at(oi).toObject();
                const QString objectId = object.value(QStringLiteral("id")).toString();
                if (object.contains(QStringLiteral("x"))) {
                    const QPointF center(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble());
                    const double radius = std::max(tolerance, object.value(QStringLiteral("size")).toDouble(90.0) * object.value(QStringLiteral("scale")).toDouble(1.0) * 0.55);
                    if (QLineF(center, world).length() <= radius) { if (id) *id = objectId; return; }
                    continue;
                }
                const QJsonArray points = object.value(QStringLiteral("points")).toArray();
                for (int pi = 0; pi < points.size(); ++pi) {
                    if (QLineF(jsonPoint(points.at(pi)), world).length() <= tolerance) {
                        if (id) *id = objectId;
                        if (pathIndex) *pathIndex = pi;
                        return;
                    }
                }
                for (int pi = 1; pi < points.size(); ++pi) {
                    if (segmentDistance(world, jsonPoint(points.at(pi - 1)), jsonPoint(points.at(pi))) <= tolerance) {
                        if (id) *id = objectId;
                        return;
                    }
                }
            }
        }
    }

    void drawTemplate(QPainter& painter, const QRectF& page, const QJsonObject& pilin) {
        if (templateImage_.isNull()) return;
        const QJsonObject templ = pilin.value(QStringLiteral("template")).toObject();
        if (!templ.value(QStringLiteral("visible")).toBool(true)) return;
        const double scale = std::clamp(templ.value(QStringLiteral("scale")).toDouble(1.0), 0.05, 5.0);
        const QSizeF size(page.width() * scale, page.height() * scale);
        const QPointF center = page.center() + QPointF(templ.value(QStringLiteral("x")).toDouble() * zoom_, templ.value(QStringLiteral("y")).toDouble() * zoom_);
        const QRectF target(center.x() - size.width() / 2.0, center.y() - size.height() / 2.0, size.width(), size.height());
        painter.save();
        painter.setOpacity(std::clamp(templ.value(QStringLiteral("opacity")).toDouble(0.35), 0.0, 1.0));
        painter.translate(target.center());
        painter.rotate(templ.value(QStringLiteral("rotation")).toDouble());
        painter.translate(-target.center());
        painter.drawImage(target, templateImage_);
        painter.restore();
    }

    void drawGrid(QPainter& painter, const QRectF& page, double docW, double docH) {
        painter.save();
        painter.setClipRect(page);
        painter.setPen(QPen(QColor(58, 68, 65, 55), 1));
        constexpr double step = 100.0;
        for (double x = 0; x <= docW; x += step) painter.drawLine(worldToScreen(QPointF(x, 0)), worldToScreen(QPointF(x, docH)));
        for (double y = 0; y <= docH; y += step) painter.drawLine(worldToScreen(QPointF(0, y)), worldToScreen(QPointF(docW, y)));
        painter.restore();
    }

    void drawObject(QPainter& painter, const QJsonObject& object, const QJsonObject& pilin) {
        const QString type = object.value(QStringLiteral("type")).toString();
        const bool selected = selectedIds_.contains(object.value(QStringLiteral("id")).toString());

        if (type == QStringLiteral("land") || type == QStringLiteral("sea")) {
            const QJsonArray points = object.value(QStringLiteral("points")).toArray();
            if (points.isEmpty()) return;
            const QColor fill = type == QStringLiteral("land")
                ? themeColor(pilin, QStringLiteral("land"), QColor(QStringLiteral("#d8c99e")))
                : themeColor(pilin, QStringLiteral("sea"), QColor(QStringLiteral("#b7c5c2")));
            const QColor coast = themeColor(pilin, QStringLiteral("coast"), QColor(QStringLiteral("#4a4135")));
            for (int i = 0; i < points.size(); ++i) {
                const QPointF point = worldToScreen(jsonPoint(points.at(i)));
                const double radius = pointRadius(points.at(i), brushRadius_) * zoom_;
                if (i == 0) {
                    painter.setPen(QPen(coast, qMax(2.0, radius * 2.0 + 3.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                    painter.drawPoint(point);
                    painter.setPen(QPen(fill, qMax(1.0, radius * 2.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                    painter.drawPoint(point);
                } else {
                    const QPointF previous = worldToScreen(jsonPoint(points.at(i - 1)));
                    painter.setPen(QPen(coast, qMax(2.0, radius * 2.0 + 3.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                    painter.drawLine(previous, point);
                    painter.setPen(QPen(fill, qMax(1.0, radius * 2.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                    painter.drawLine(previous, point);
                }
            }
            return;
        }

        if (type == QStringLiteral("forestArea") || type == QStringLiteral("mountainArea")) {
            const QJsonArray points = object.value(QStringLiteral("points")).toArray();
            if (points.isEmpty()) return;
            const QColor ink = type == QStringLiteral("forestArea")
                ? themeColor(pilin, QStringLiteral("forest"), QColor(QStringLiteral("#375635")))
                : themeColor(pilin, QStringLiteral("mountain"), QColor(QStringLiteral("#4d443b")));
            painter.save();
            painter.setPen(QPen(ink, 1.2));
            painter.setBrush(Qt::NoBrush);
            const int repeats = qMax(1, object.value(QStringLiteral("density")).toInt(density_) / 18);
            for (int i = 0; i < points.size(); ++i) {
                const QPointF center = jsonPoint(points.at(i));
                const double radius = pointRadius(points.at(i), brushRadius_);
                for (int j = 0; j < repeats; ++j) {
                    const double angle = (j * 2.3999632297 + i * 0.77);
                    const double rr = radius * (0.18 + 0.72 * ((j + 1.0) / (repeats + 1.0)));
                    const QPointF p = worldToScreen(center + QPointF(std::cos(angle) * rr, std::sin(angle) * rr));
                    const double size = std::clamp(object.value(QStringLiteral("symbolSize")).toDouble(symbolSize_) * zoom_ * 0.34, 5.0, 24.0);
                    if (type == QStringLiteral("forestArea")) {
                        QPainterPath tree;
                        tree.moveTo(p.x(), p.y() - size);
                        tree.lineTo(p.x() - size * .58, p.y() + size * .2);
                        tree.lineTo(p.x() + size * .58, p.y() + size * .2);
                        tree.closeSubpath();
                        painter.drawPath(tree);
                        painter.drawLine(QPointF(p.x(), p.y() + size * .18), QPointF(p.x(), p.y() + size * .7));
                    } else {
                        QPainterPath mountain;
                        mountain.moveTo(p.x() - size, p.y() + size * .55);
                        mountain.lineTo(p.x(), p.y() - size);
                        mountain.lineTo(p.x() + size, p.y() + size * .55);
                        painter.drawPath(mountain);
                        painter.drawLine(QPointF(p.x() - size * .26, p.y() - size * .1), QPointF(p.x(), p.y() - size));
                        painter.drawLine(QPointF(p.x(), p.y() - size), QPointF(p.x() + size * .25, p.y() - size * .05));
                    }
                }
            }
            painter.restore();
            return;
        }

        if (type == QStringLiteral("settlement")) {
            const QPointF p = worldToScreen(QPointF(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble()));
            const QColor ink = themeColor(pilin, QStringLiteral("symbol"), QColor(QStringLiteral("#302822")));
            const double size = std::clamp(object.value(QStringLiteral("scale")).toDouble(1.0) * symbolSize_ * zoom_ * .52, 5.0, 22.0);
            painter.save();
            painter.setPen(QPen(ink, 1.5));
            painter.setBrush(QColor(239, 226, 195, 220));
            painter.drawEllipse(p, size * .45, size * .45);
            painter.drawEllipse(p, size * .16, size * .16);
            QFont font(QStringLiteral("Georgia")); font.setPointSizeF(qBound(7.0, 10.0 * zoom_ + 6.0, 12.5));
            painter.setFont(font);
            painter.drawText(QPointF(p.x() + size * .72, p.y() + 4), object.value(QStringLiteral("label")).toString());
            painter.restore();
            if (selected) drawSelection(painter, p, size * 1.4);
            return;
        }

        if (type == QStringLiteral("stamp")) {
            const QPointF p = worldToScreen(QPointF(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble()));
            const double size = std::clamp(object.value(QStringLiteral("size")).toDouble(symbolSize_ * 2.2) * object.value(QStringLiteral("scale")).toDouble(1.0) * zoom_, 10.0, 96.0);
            const QString data = object.value(QStringLiteral("dataUrl")).toString();
            painter.save();
            painter.translate(p);
            painter.rotate(object.value(QStringLiteral("rotation")).toDouble());
            painter.setOpacity(std::clamp(object.value(QStringLiteral("opacity")).toDouble(1.0), 0.0, 1.0));
            if (!data.isEmpty()) {
                QImage image; image.loadFromData(dataUrlBytes(data));
                if (!image.isNull()) painter.drawImage(QRectF(-size / 2, -size / 2, size, size), image);
                else drawBuiltinStamp(painter, object.value(QStringLiteral("assetKind")).toString(), size);
            } else drawBuiltinStamp(painter, object.value(QStringLiteral("assetKind")).toString(), size);
            painter.restore();
            if (selected) drawSelection(painter, p, size * .62);
            return;
        }

        if (type == QStringLiteral("label")) {
            const QPointF p = worldToScreen(QPointF(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble()));
            painter.save();
            painter.translate(p);
            painter.rotate(object.value(QStringLiteral("rotation")).toDouble());
            QFont font(QStringLiteral("Georgia"));
            font.setPointSizeF(std::clamp(object.value(QStringLiteral("fontSize")).toDouble(54.0) * zoom_ * .55, 8.0, 34.0));
            font.setBold(object.value(QStringLiteral("bold")).toBool(false));
            painter.setFont(font);
            const QString text = object.value(QStringLiteral("text")).toString();
            const QColor outline = themeColor(pilin, QStringLiteral("labelOutline"), QColor(QStringLiteral("#eee2c3")));
            const QColor ink = themeColor(pilin, QStringLiteral("text"), QColor(QStringLiteral("#282520")));
            QPainterPath glyphs; glyphs.addText(QPointF(0, 0), font, text);
            painter.setPen(QPen(outline, 3.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(ink);
            painter.drawPath(glyphs);
            painter.setPen(QPen(ink, .7));
            painter.drawPath(glyphs);
            painter.restore();
            if (selected) drawSelection(painter, p, 26);
            return;
        }

        const QJsonArray points = object.value(QStringLiteral("points")).toArray();
        if (points.size() < 2) return;
        QPainterPath path = smoothPath(points, [this](const QPointF& p) { return worldToScreen(p); });
        const int width = object.value(QStringLiteral("width")).toInt(strokeWidth_);
        painter.save();
        painter.setBrush(Qt::NoBrush);
        if (type == QStringLiteral("river")) {
            const QColor river = themeColor(pilin, QStringLiteral("river"), QColor(QStringLiteral("#25506f")));
            painter.setPen(QPen(river.darker(145), qMax(2.0, width * zoom_ + 2.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
            painter.setPen(QPen(river.lighter(150), qMax(1.0, width * zoom_ * .55), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
        } else if (type == QStringLiteral("road")) {
            const QColor road = themeColor(pilin, QStringLiteral("road"), QColor(QStringLiteral("#70502f")));
            painter.setPen(QPen(road.darker(150), qMax(2.0, width * zoom_ + 2.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
            painter.setPen(QPen(road.lighter(155), qMax(1.0, width * zoom_ * .55), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
        } else if (type == QStringLiteral("border")) {
            painter.setPen(QPen(themeColor(pilin, QStringLiteral("border"), QColor(QStringLiteral("#913f37"))), qMax(1.0, width * zoom_), Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
        } else if (type == QStringLiteral("region")) {
            QColor region = themeColor(pilin, QStringLiteral("region"), QColor(QStringLiteral("#b07548")));
            QColor fill = region; fill.setAlphaF(std::clamp(object.value(QStringLiteral("fillOpacity")).toDouble(.18), 0.0, .7));
            painter.setPen(QPen(region.darker(130), qMax(1.0, width * zoom_ * .7), Qt::DashLine));
            painter.setBrush(fill);
            painter.drawPath(path);
        }
        painter.restore();

        if (selected) {
            painter.save();
            painter.setPen(QPen(QColor(QStringLiteral("#c18b52")), 1.5));
            painter.setBrush(QColor(193, 139, 82, 210));
            for (const QJsonValue& point : points) {
                const QPointF p = worldToScreen(jsonPoint(point));
                painter.drawRect(QRectF(p.x() - 3.5, p.y() - 3.5, 7, 7));
            }
            painter.restore();
        }
    }

    void drawBuiltinStamp(QPainter& painter, const QString& kind, double size) {
        const QColor ink(QStringLiteral("#332b23"));
        painter.setPen(QPen(ink, qMax(1.2, size * .035), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(QColor(230, 215, 177, 170));
        const double h = size * .62;
        if (kind == QStringLiteral("ship")) {
            painter.drawArc(QRectF(-size * .42, -size * .1, size * .84, size * .44), 205 * 16, 130 * 16);
            painter.drawLine(QPointF(0, -h * .75), QPointF(0, h * .38));
            QPainterPath sail; sail.moveTo(0, -h * .68); sail.lineTo(size * .34, -h * .18); sail.lineTo(0, -h * .08); sail.closeSubpath(); painter.drawPath(sail);
            return;
        }
        if (kind == QStringLiteral("bridge")) {
            painter.drawArc(QRectF(-size * .45, -size * .1, size * .9, size * .55), 0, 180 * 16);
            painter.drawLine(QPointF(-size * .46, -size * .1), QPointF(size * .46, -size * .1));
            return;
        }
        if (kind == QStringLiteral("compass")) {
            painter.drawEllipse(QPointF(0, 0), size * .4, size * .4);
            for (int i = 0; i < 8; ++i) {
                const double a = i * kPi / 4.0;
                painter.drawLine(QPointF(std::cos(a) * size * .08, std::sin(a) * size * .08), QPointF(std::cos(a) * size * .48, std::sin(a) * size * .48));
            }
            return;
        }
        if (kind == QStringLiteral("ruin")) {
            painter.drawRect(QRectF(-size * .34, -size * .18, size * .24, size * .48));
            painter.drawRect(QRectF(size * .04, -size * .04, size * .25, size * .34));
            painter.drawLine(QPointF(-size * .46, size * .31), QPointF(size * .44, size * .31));
            return;
        }
        if (kind == QStringLiteral("temple")) {
            QPainterPath roof; roof.moveTo(-size * .42, -size * .14); roof.lineTo(0, -size * .43); roof.lineTo(size * .42, -size * .14); roof.closeSubpath(); painter.drawPath(roof);
            for (int i = -2; i <= 2; ++i) painter.drawLine(QPointF(i * size * .14, -size * .12), QPointF(i * size * .14, size * .3));
            painter.drawLine(QPointF(-size * .48, size * .31), QPointF(size * .48, size * .31));
            return;
        }
        painter.drawRect(QRectF(-size * .32, -size * .18, size * .64, size * .5));
        painter.drawRect(QRectF(-size * .46, -size * .34, size * .22, size * .66));
        painter.drawRect(QRectF(size * .24, -size * .34, size * .22, size * .66));
        painter.drawLine(QPointF(-size * .46, -size * .34), QPointF(-size * .35, -size * .48));
        painter.drawLine(QPointF(-size * .24, -size * .34), QPointF(-size * .35, -size * .48));
        painter.drawLine(QPointF(size * .24, -size * .34), QPointF(size * .35, -size * .48));
        painter.drawLine(QPointF(size * .46, -size * .34), QPointF(size * .35, -size * .48));
    }

    void drawSelection(QPainter& painter, const QPointF& center, double radius) {
        painter.save();
        painter.setPen(QPen(QColor(QStringLiteral("#c18b52")), 1.4, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(center, radius, radius);
        painter.restore();
    }

    void drawPreview(QPainter& painter, const QJsonObject& pilin) {
        if (!nodePath_.isEmpty()) {
            QJsonArray preview = nodePath_;
            preview.append(pointJson(cursorWorld_));
            QPainterPath path = smoothPath(preview, [this](const QPointF& p) { return worldToScreen(p); });
            painter.save();
            painter.setPen(QPen(QColor(QStringLiteral("#c18b52")), 2, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
            painter.restore();
        }
        if (drawing_ && !brushStroke_.isEmpty()) {
            QJsonObject object{{QStringLiteral("id"), QStringLiteral("preview")}, {QStringLiteral("type"), typeForTool(tool_)}, {QStringLiteral("points"), brushStroke_}, {QStringLiteral("density"), density_}, {QStringLiteral("symbolSize"), symbolSize_}};
            drawObject(painter, object, pilin);
        }
        if (brushTool(tool_)) {
            const QPointF center = worldToScreen(cursorWorld_);
            painter.save();
            painter.setPen(QPen(QColor(193, 139, 82, 180), 1.3, Qt::DashLine));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(center, brushRadius_ * zoom_, brushRadius_ * zoom_);
            painter.restore();
        }
        if (measuring_) {
            painter.save();
            const QPointF a = worldToScreen(measureA_);
            const QPointF b = worldToScreen(measureB_);
            painter.setPen(QPen(QColor(QStringLiteral("#c18b52")), 1.6, Qt::DashLine));
            painter.drawLine(a, b);
            const double distance = QLineF(measureA_, measureB_).length();
            painter.drawText((a + b) * .5 + QPointF(8, -8), QObject::tr("%1 u").arg(qRound(distance)));
            painter.restore();
        }
    }

    QJsonObject map_;
    QImage templateImage_;
    Tool tool_ = Tool::Select;
    QStringList selectedIds_;
    QJsonArray brushStroke_;
    QJsonArray nodePath_;
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
    QPointF pan_{40, 40};
    QPointF cursorWorld_;
    QPointF lastMouse_;
    QPointF measureA_;
    QPointF measureB_;
    QString draggingPointId_;
    QString draggingPathId_;
    int draggingPathIndex_ = -1;
    QPointF dragStartWorld_;
    QPointF dragOriginalPoint_;
};

PilinReyEditor::PilinReyEditor(QWidget* parent) : QWidget(parent) { buildUi(); }
PilinReyEditor::~PilinReyEditor() = default;

void PilinReyEditor::buildUi() {
    setObjectName(QStringLiteral("pilinReyEditor"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    canvasHost_ = new QWidget(this);
    canvasHost_->setObjectName(QStringLiteral("pilinCanvasHost"));
    auto* canvasLayout = new QVBoxLayout(canvasHost_);
    canvasLayout->setContentsMargins(0, 0, 0, 0);
    viewport_ = new PilinReyViewport(canvasHost_);
    canvasLayout->addWidget(viewport_);
    root->addWidget(canvasHost_, 1);

    toolRail_ = new QFrame(canvasHost_);
    toolRail_->setObjectName(QStringLiteral("pilinToolPalette"));
    auto* tools = new QGridLayout(toolRail_);
    tools->setContentsMargins(8, 8, 8, 8);
    tools->setHorizontalSpacing(3);
    tools->setVerticalSpacing(3);

    struct ToolDef { Tool tool; const char* glyph; const char* tip; };
    const ToolDef defs[] = {
        {Tool::Select, "↖", "Seleccionar · Shift suma selección"}, {Tool::Pan, "✥", "Mover lienzo"},
        {Tool::Coast, "▰", "Pintar tierra"}, {Tool::Eraser, "◌", "Recortar tierra / pintar mar"},
        {Tool::River, "∿", "Río por nodos"}, {Tool::Road, "━", "Camino por nodos"},
        {Tool::Border, "┄", "Frontera por nodos"}, {Tool::Region, "◇", "Región cerrada"},
        {Tool::Forest, "♣", "Pincel de bosque"}, {Tool::Mountain, "△", "Pincel de cordillera"},
        {Tool::Settlement, "●", "Asentamiento"}, {Tool::Stamp, "✦", "Asset / sello"},
        {Tool::Label, "T", "Etiqueta cartográfica"}, {Tool::Measure, "↔", "Medir"}
    };
    int row = 0;
    for (const ToolDef& def : defs) {
        auto* button = toolButton(QString::fromUtf8(def.glyph), tr(def.tip), toolRail_, QStringLiteral("pilinMapTool"));
        button->setCheckable(true);
        button->setAutoExclusive(true);
        if (def.tool == Tool::Select) button->setChecked(true);
        if (def.tool == Tool::Stamp) stampToolButton_ = button;
        connect(button, &QToolButton::clicked, this, [this, tool = def.tool]() { setActiveTool(tool); });
        toolButtons_.append(button);
        tools->addWidget(button, row++, 0);
    }

    auto* options = new QFrame(canvasHost_);
    options->setObjectName(QStringLiteral("pilinToolOptions"));
    auto* optionLayout = new QVBoxLayout(options);
    optionLayout->setContentsMargins(10, 9, 10, 9);
    optionLayout->setSpacing(4);
    toolOptionsLabel_ = new QLabel(tr("Herramienta"), options);
    toolOptionsLabel_->setObjectName(QStringLiteral("pilinToolOptionsLabel"));
    optionLayout->addWidget(toolOptionsLabel_);
    auto addSlider = [&](const QString& title, int min, int max, int value, QSlider*& target) {
        auto* label = new QLabel(title, options);
        label->setObjectName(QStringLiteral("pilinTinyLabel"));
        target = new QSlider(Qt::Horizontal, options);
        target->setRange(min, max);
        target->setValue(value);
        target->setMinimumWidth(150);
        optionLayout->addWidget(label);
        optionLayout->addWidget(target);
    };
    addSlider(tr("Tamaño"), 30, 700, brushRadius_, brushRadiusSlider_);
    addSlider(tr("Densidad"), 10, 100, density_, densitySlider_);
    addSlider(tr("Trazo"), 1, 24, strokeWidth_, strokeWidthSlider_);
    addSlider(tr("Símbolo"), 16, 160, symbolSize_, symbolSizeSlider_);
    options->hide();

    topCommands_ = new QFrame(canvasHost_);
    topCommands_->setObjectName(QStringLiteral("pilinTopCommands"));
    auto* commands = new QHBoxLayout(topCommands_);
    commands->setContentsMargins(6, 4, 6, 4);
    commands->setSpacing(3);
    layersButton_ = toolButton(QStringLiteral("Capas"), tr("Capas"), topCommands_, QStringLiteral("pilinCommand"));
    assetsButton_ = toolButton(QStringLiteral("Assets"), tr("Assets"), topCommands_, QStringLiteral("pilinCommand"));
    templateButton_ = toolButton(QStringLiteral("Plantilla"), tr("Plantilla"), topCommands_, QStringLiteral("pilinCommand"));
    appearanceButton_ = toolButton(QStringLiteral("Apariencia"), tr("Apariencia"), topCommands_, QStringLiteral("pilinCommand"));
    auto* fitButton = toolButton(QStringLiteral("Encajar"), tr("Encajar"), topCommands_, QStringLiteral("pilinCommand"));
    undoButton_ = toolButton(QStringLiteral("↶"), tr("Deshacer"), topCommands_, QStringLiteral("pilinCommand"));
    redoButton_ = toolButton(QStringLiteral("↷"), tr("Rehacer"), topCommands_, QStringLiteral("pilinCommand"));
    auto* exportButton = toolButton(QStringLiteral("Exportar"), tr("Exportar"), topCommands_, QStringLiteral("pilinCommand"));
    snapCheck_ = new QCheckBox(tr("Ajustar"), topCommands_);
    gridCheck_ = new QCheckBox(tr("Cuadrícula"), topCommands_);
    commands->addWidget(layersButton_);
    commands->addWidget(assetsButton_);
    commands->addWidget(templateButton_);
    commands->addWidget(appearanceButton_);
    commands->addWidget(fitButton);
    commands->addWidget(undoButton_);
    commands->addWidget(redoButton_);
    commands->addWidget(exportButton);
    commands->addWidget(snapCheck_);
    commands->addWidget(gridCheck_);

    layersPopover_ = new QFrame(canvasHost_);
    layersPopover_->setObjectName(QStringLiteral("pilinPopover"));
    auto* layerLayout = new QVBoxLayout(layersPopover_);
    layerLayout->setContentsMargins(10, 10, 10, 10);
    layerLayout->setSpacing(6);
    auto* layerTitle = new QLabel(tr("Capas"), layersPopover_);
    layerTitle->setObjectName(QStringLiteral("pilinPopoverTitle"));
    layers_ = new QListWidget(layersPopover_);
    layers_->setMinimumHeight(190);
    auto* layerButtons = new QHBoxLayout;
    auto* addLayerButton = toolButton(QStringLiteral("+"), tr("Nueva capa"), layersPopover_, QStringLiteral("pilinCommand"));
    auto* duplicateLayerButton = toolButton(QStringLiteral("⧉"), tr("Duplicar capa"), layersPopover_, QStringLiteral("pilinCommand"));
    auto* removeLayerButton = toolButton(QStringLiteral("−"), tr("Eliminar capa"), layersPopover_, QStringLiteral("pilinCommand"));
    auto* upLayerButton = toolButton(QStringLiteral("↑"), tr("Subir"), layersPopover_, QStringLiteral("pilinCommand"));
    auto* downLayerButton = toolButton(QStringLiteral("↓"), tr("Bajar"), layersPopover_, QStringLiteral("pilinCommand"));
    layerButtons->addWidget(addLayerButton); layerButtons->addWidget(duplicateLayerButton); layerButtons->addWidget(removeLayerButton); layerButtons->addStretch(); layerButtons->addWidget(upLayerButton); layerButtons->addWidget(downLayerButton);
    layerLocked_ = new QCheckBox(tr("Bloquear"), layersPopover_);
    layerOpacity_ = new QSlider(Qt::Horizontal, layersPopover_); layerOpacity_->setRange(0, 100); layerOpacity_->setValue(100);
    layerLayout->addWidget(layerTitle); layerLayout->addWidget(layers_); layerLayout->addLayout(layerButtons); layerLayout->addWidget(layerLocked_); layerLayout->addWidget(new QLabel(tr("Opacidad"), layersPopover_)); layerLayout->addWidget(layerOpacity_);

    assetsPopover_ = new QFrame(canvasHost_);
    assetsPopover_->setObjectName(QStringLiteral("pilinPopover"));
    auto* assetLayout = new QVBoxLayout(assetsPopover_);
    assetLayout->setContentsMargins(10, 10, 10, 10);
    assetLayout->setSpacing(6);
    auto* assetTitle = new QLabel(tr("Assets y sellos"), assetsPopover_);
    assetTitle->setObjectName(QStringLiteral("pilinPopoverTitle"));
    assets_ = new QListWidget(assetsPopover_);
    assets_->setViewMode(QListView::IconMode);
    assets_->setResizeMode(QListView::Adjust);
    assets_->setMovement(QListView::Static);
    assets_->setGridSize(QSize(78, 62));
    assets_->setMinimumHeight(220);
    auto* importAssetButton = new QPushButton(tr("Importar imagen…"), assetsPopover_);
    assetLayout->addWidget(assetTitle); assetLayout->addWidget(assets_); assetLayout->addWidget(importAssetButton);

    templatePopover_ = new QFrame(canvasHost_);
    templatePopover_->setObjectName(QStringLiteral("pilinPopover"));
    auto* templateLayout = new QVBoxLayout(templatePopover_);
    templateLayout->setContentsMargins(10, 10, 10, 10);
    templateLayout->setSpacing(6);
    auto* templateTitle = new QLabel(tr("Plantilla"), templatePopover_);
    templateTitle->setObjectName(QStringLiteral("pilinPopoverTitle"));
    auto* templateButtons = new QHBoxLayout;
    auto* loadTemplate = new QPushButton(tr("Cargar…"), templatePopover_);
    auto* clearTemplateButton = new QPushButton(tr("Quitar"), templatePopover_);
    templateButtons->addWidget(loadTemplate); templateButtons->addWidget(clearTemplateButton);
    templateOpacity_ = new QSlider(Qt::Horizontal, templatePopover_); templateOpacity_->setRange(0, 100); templateOpacity_->setValue(35);
    templateScale_ = new QSlider(Qt::Horizontal, templatePopover_); templateScale_->setRange(10, 300); templateScale_->setValue(100);
    templateRotation_ = new QSlider(Qt::Horizontal, templatePopover_); templateRotation_->setRange(-180, 180); templateRotation_->setValue(0);
    templateLayout->addWidget(templateTitle); templateLayout->addLayout(templateButtons); templateLayout->addWidget(new QLabel(tr("Opacidad"), templatePopover_)); templateLayout->addWidget(templateOpacity_); templateLayout->addWidget(new QLabel(tr("Escala"), templatePopover_)); templateLayout->addWidget(templateScale_); templateLayout->addWidget(new QLabel(tr("Rotación"), templatePopover_)); templateLayout->addWidget(templateRotation_);

    appearancePopover_ = new QFrame(canvasHost_);
    appearancePopover_->setObjectName(QStringLiteral("pilinPopover"));
    auto* appearanceLayout = new QVBoxLayout(appearancePopover_);
    appearanceLayout->setContentsMargins(10, 10, 10, 10);
    appearanceLayout->setSpacing(4);
    auto* appearanceTitle = new QLabel(tr("Apariencia"), appearancePopover_);
    appearanceTitle->setObjectName(QStringLiteral("pilinPopoverTitle"));
    appearanceLayout->addWidget(appearanceTitle);
    const QList<QPair<QString, QString>> colors{{QStringLiteral("land"), tr("Tierra")}, {QStringLiteral("sea"), tr("Mar")}, {QStringLiteral("coast"), tr("Costa")}, {QStringLiteral("river"), tr("Ríos")}, {QStringLiteral("road"), tr("Caminos")}, {QStringLiteral("border"), tr("Fronteras")}, {QStringLiteral("forest"), tr("Bosques")}, {QStringLiteral("mountain"), tr("Montañas")}, {QStringLiteral("text"), tr("Texto")}};
    for (const auto& entry : colors) {
        auto* button = new QPushButton(entry.second, appearancePopover_);
        connect(button, &QPushButton::clicked, this, [this, key = entry.first]() { setThemeColor(key); });
        appearanceLayout->addWidget(button);
    }

    selectionPopover_ = new QFrame(canvasHost_);
    selectionPopover_->setObjectName(QStringLiteral("pilinSelectionPopover"));
    auto* selectionLayout = new QVBoxLayout(selectionPopover_);
    selectionLayout->setContentsMargins(9, 8, 9, 8);
    selectionLayout->setSpacing(5);
    selectionLabel_ = new QLabel(tr("Selección"), selectionPopover_);
    selectionLabel_->setObjectName(QStringLiteral("pilinPopoverTitle"));
    auto* selectionActions = new QHBoxLayout;
    editObjectButton_ = toolButton(QStringLiteral("✎"), tr("Editar"), selectionPopover_, QStringLiteral("pilinCommand"));
    duplicateObjectButton_ = toolButton(QStringLiteral("⧉"), tr("Duplicar"), selectionPopover_, QStringLiteral("pilinCommand"));
    linkAtlasButton_ = toolButton(QStringLiteral("⌁"), tr("Enlazar Atlas"), selectionPopover_, QStringLiteral("pilinCommand"));
    deleteObjectButton_ = toolButton(QStringLiteral("×"), tr("Eliminar"), selectionPopover_, QStringLiteral("pilinCommand"));
    selectionActions->addWidget(editObjectButton_); selectionActions->addWidget(duplicateObjectButton_); selectionActions->addWidget(linkAtlasButton_); selectionActions->addStretch(); selectionActions->addWidget(deleteObjectButton_);
    auto* transforms = new QHBoxLayout;
    rotateLeftButton_ = toolButton(QStringLiteral("↺"), tr("Rotar −15°"), selectionPopover_, QStringLiteral("pilinCommand"));
    rotateRightButton_ = toolButton(QStringLiteral("↻"), tr("Rotar +15°"), selectionPopover_, QStringLiteral("pilinCommand"));
    scaleDownButton_ = toolButton(QStringLiteral("−"), tr("Reducir 10%"), selectionPopover_, QStringLiteral("pilinCommand"));
    scaleUpButton_ = toolButton(QStringLiteral("+"), tr("Aumentar 10%"), selectionPopover_, QStringLiteral("pilinCommand"));
    transforms->addWidget(rotateLeftButton_); transforms->addWidget(rotateRightButton_); transforms->addStretch(); transforms->addWidget(scaleDownButton_); transforms->addWidget(scaleUpButton_);
    selectionLayout->addWidget(selectionLabel_); selectionLayout->addLayout(selectionActions); selectionLayout->addLayout(transforms);

    status_ = new QLabel(tr("Pilín Rey · listo"), canvasHost_);
    status_->setObjectName(QStringLiteral("pilinStatus"));
    status_->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    for (QFrame* panel : {layersPopover_, assetsPopover_, templatePopover_, appearancePopover_, selectionPopover_}) panel->hide();

    viewport_->onPath = [this](const QString& type, const QJsonArray& points) { addPathObject(type, points); };
    viewport_->onSettlement = [this](double x, double y) { addSettlement(x, y); };
    viewport_->onStamp = [this](double x, double y) { addStamp(x, y); };
    viewport_->onLabel = [this](double x, double y) { addLabel(x, y); };
    viewport_->onSelection = [this](const QStringList& ids) { selectObjects(ids); };
    viewport_->onMovePoint = [this](const QString& id, double x, double y) { movePointObject(id, x, y); };
    viewport_->onMovePathPoint = [this](const QString& id, int index, double x, double y) { movePathPoint(id, index, x, y); };
    viewport_->onDeleteSelection = [this]() { deleteSelectedObject(); };
    viewport_->onStatus = [this](const QString& text) { if (status_) status_->setText(text); };

    connect(brushRadiusSlider_, &QSlider::valueChanged, this, [this](int value) { brushRadius_ = value; viewport_->setBrushRadius(value); });
    connect(densitySlider_, &QSlider::valueChanged, this, [this](int value) { density_ = value; viewport_->setDensity(value); });
    connect(strokeWidthSlider_, &QSlider::valueChanged, this, [this](int value) { strokeWidth_ = value; viewport_->setStrokeWidth(value); });
    connect(symbolSizeSlider_, &QSlider::valueChanged, this, [this](int value) { symbolSize_ = value; viewport_->setSymbolSize(value); });

    auto togglePopover = [this](QFrame* panel) {
        for (QFrame* candidate : {layersPopover_, assetsPopover_, templatePopover_, appearancePopover_}) if (candidate != panel) candidate->hide();
        panel->setVisible(!panel->isVisible());
        layoutFloatingPanels();
    };
    connect(layersButton_, &QToolButton::clicked, this, [togglePopover, this]() { togglePopover(layersPopover_); });
    connect(assetsButton_, &QToolButton::clicked, this, [togglePopover, this]() { togglePopover(assetsPopover_); });
    connect(templateButton_, &QToolButton::clicked, this, [togglePopover, this]() { togglePopover(templatePopover_); });
    connect(appearanceButton_, &QToolButton::clicked, this, [togglePopover, this]() { togglePopover(appearancePopover_); });
    connect(fitButton, &QToolButton::clicked, viewport_, &PilinReyViewport::fitDocument);
    connect(undoButton_, &QToolButton::clicked, this, &PilinReyEditor::undo);
    connect(redoButton_, &QToolButton::clicked, this, &PilinReyEditor::redo);
    connect(exportButton, &QToolButton::clicked, this, &PilinReyEditor::exportMap);
    connect(snapCheck_, &QCheckBox::toggled, viewport_, &PilinReyViewport::setSnap);
    connect(gridCheck_, &QCheckBox::toggled, viewport_, &PilinReyViewport::setGrid);
    connect(loadTemplate, &QPushButton::clicked, this, &PilinReyEditor::chooseTemplate);
    connect(clearTemplateButton, &QPushButton::clicked, this, &PilinReyEditor::clearTemplate);

    connect(templateOpacity_, &QSlider::valueChanged, this, [this](int value) { if (!refreshing_) mutateDocument([&](QJsonObject& p) { QJsonObject t = p.value(QStringLiteral("template")).toObject(); t.insert(QStringLiteral("opacity"), value / 100.0); p.insert(QStringLiteral("template"), t); }); });
    connect(templateScale_, &QSlider::valueChanged, this, [this](int value) { if (!refreshing_) mutateDocument([&](QJsonObject& p) { QJsonObject t = p.value(QStringLiteral("template")).toObject(); t.insert(QStringLiteral("scale"), value / 100.0); p.insert(QStringLiteral("template"), t); }); });
    connect(templateRotation_, &QSlider::valueChanged, this, [this](int value) { if (!refreshing_) mutateDocument([&](QJsonObject& p) { QJsonObject t = p.value(QStringLiteral("template")).toObject(); t.insert(QStringLiteral("rotation"), value); p.insert(QStringLiteral("template"), t); }); });

    connect(addLayerButton, &QToolButton::clicked, this, &PilinReyEditor::addLayer);
    connect(duplicateLayerButton, &QToolButton::clicked, this, &PilinReyEditor::duplicateLayer);
    connect(removeLayerButton, &QToolButton::clicked, this, &PilinReyEditor::removeLayer);
    connect(upLayerButton, &QToolButton::clicked, this, [this]() { moveLayer(-1); });
    connect(downLayerButton, &QToolButton::clicked, this, [this]() { moveLayer(1); });
    connect(layerLocked_, &QCheckBox::toggled, this, [this](bool checked) { if (!refreshing_) setLayerLocked(checked); });
    connect(layerOpacity_, &QSlider::valueChanged, this, [this](int value) { if (!refreshing_) setLayerOpacity(value); });
    connect(layers_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (refreshing_ || row < 0) return;
        QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject();
        const QJsonArray layers = p.value(QStringLiteral("layers")).toArray();
        if (row >= layers.size()) return;
        p.insert(QStringLiteral("activeLayerId"), layers.at(row).toObject().value(QStringLiteral("id")).toString());
        map_.insert(QStringLiteral("pilinRey"), p);
        refreshLayerControls();
        persistToArchive();
    });
    connect(layers_, &QListWidget::itemChanged, this, &PilinReyEditor::applyLayerItem);

    connect(importAssetButton, &QPushButton::clicked, this, &PilinReyEditor::importAsset);
    connect(assets_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (!item) return;
        selectedAssetKind_ = item->data(Qt::UserRole).toString();
        selectedAssetData_ = item->data(Qt::UserRole + 1).toString();
        setActiveTool(Tool::Stamp);
        if (stampToolButton_) stampToolButton_->setChecked(true);
        assetsPopover_->hide();
    });

    connect(editObjectButton_, &QToolButton::clicked, this, &PilinReyEditor::editSelectedObject);
    connect(duplicateObjectButton_, &QToolButton::clicked, this, &PilinReyEditor::duplicateSelectedObject);
    connect(linkAtlasButton_, &QToolButton::clicked, this, &PilinReyEditor::linkSelectedToAtlas);
    connect(deleteObjectButton_, &QToolButton::clicked, this, &PilinReyEditor::deleteSelectedObject);
    connect(rotateLeftButton_, &QToolButton::clicked, this, [this]() { transformSelection(1.0, -15.0); });
    connect(rotateRightButton_, &QToolButton::clicked, this, [this]() { transformSelection(1.0, 15.0); });
    connect(scaleDownButton_, &QToolButton::clicked, this, [this]() { transformSelection(.9, 0.0); });
    connect(scaleUpButton_, &QToolButton::clicked, this, [this]() { transformSelection(1.1, 0.0); });

    auto* undoShortcut = new QShortcut(QKeySequence::Undo, this); connect(undoShortcut, &QShortcut::activated, this, &PilinReyEditor::undo);
    auto* redoShortcut = new QShortcut(QKeySequence::Redo, this); connect(redoShortcut, &QShortcut::activated, this, &PilinReyEditor::redo);
    auto* copyShortcut = new QShortcut(QKeySequence::Copy, this); connect(copyShortcut, &QShortcut::activated, this, &PilinReyEditor::copySelectedObject);
    auto* pasteShortcut = new QShortcut(QKeySequence::Paste, this); connect(pasteShortcut, &QShortcut::activated, this, &PilinReyEditor::pasteCopiedObject);
    auto* duplicateShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_D), this); connect(duplicateShortcut, &QShortcut::activated, this, &PilinReyEditor::duplicateSelectedObject);

    setStyleSheet(QStringLiteral(
        "#pilinReyEditor,#pilinCanvasHost,#pilinQtViewport{background:#15130f;}"
        "#pilinToolPalette,#pilinTopCommands,#pilinPopover,#pilinSelectionPopover,#pilinToolOptions{background:#111212;color:#d8d2c7;border:1px solid #37352f;border-radius:9px;}"
        "QToolButton#pilinMapTool,QToolButton#pilinCommand{background:transparent;color:#c8c3b9;border:0;border-radius:6px;padding:5px 8px;}"
        "QToolButton#pilinMapTool:hover,QToolButton#pilinCommand:hover{background:#24231f;color:#fff7e8;}"
        "QToolButton#pilinMapTool:checked{background:#3a2f23;color:#e8c18a;}"
        "#pilinPopoverTitle,#pilinToolOptionsLabel{color:#efe9de;font-weight:700;}"
        "#pilinTinyLabel{font-size:8pt;color:#8a8982;}"
        "#pilinStatus{background:rgba(17,18,18,220);color:#c8c3b9;border:1px solid #37352f;border-radius:6px;padding:5px 8px;}"
    ));
}

void PilinReyEditor::resizeEvent(QResizeEvent* event) { QWidget::resizeEvent(event); layoutFloatingPanels(); }
void PilinReyEditor::showEvent(QShowEvent* event) { QWidget::showEvent(event); layoutFloatingPanels(); }

void PilinReyEditor::layoutFloatingPanels() {
    if (!canvasHost_) return;
    const int margin = 14;
    if (toolRail_) {
        toolRail_->adjustSize();
        toolRail_->move(margin, qMax(margin, (canvasHost_->height() - toolRail_->height()) / 2));
        toolRail_->raise();
    }
    if (topCommands_) {
        topCommands_->adjustSize();
        topCommands_->move(qMax(margin + 200, (canvasHost_->width() - topCommands_->width()) / 2), margin);
        topCommands_->raise();
    }
    int y = margin + (topCommands_ ? topCommands_->height() : 0) + 10;
    for (QFrame* panel : {layersPopover_, assetsPopover_, templatePopover_, appearancePopover_}) {
        if (!panel || !panel->isVisible()) continue;
        panel->adjustSize();
        panel->move(qMax(margin, canvasHost_->width() - panel->width() - margin), y);
        panel->raise();
        y = panel->geometry().bottom() + 10;
    }
    if (selectionPopover_ && selectionPopover_->isVisible()) {
        selectionPopover_->adjustSize();
        selectionPopover_->move(qMax(margin, canvasHost_->width() - selectionPopover_->width() - margin), qMax(70, canvasHost_->height() - selectionPopover_->height() - 42));
        selectionPopover_->raise();
    }
    if (QWidget* options = canvasHost_->findChild<QWidget*>(QStringLiteral("pilinToolOptions")); options && options->isVisible()) {
        options->adjustSize();
        options->move(margin + (toolRail_ ? toolRail_->width() : 0) + 10, qMax(70, (canvasHost_->height() - options->height()) / 2));
        options->raise();
    }
    if (status_) {
        status_->adjustSize();
        status_->move(margin, qMax(margin, canvasHost_->height() - status_->height() - margin));
        status_->raise();
    }
}

void PilinReyEditor::setMap(const QJsonObject& map) {
    map_ = map;
    ensurePilinDocument();
    undoStack_.clear();
    redoStack_.clear();
    selectedObjectIds_.clear();
    const QJsonObject templ = map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("template")).toObject();
    refreshing_ = true;
    templateOpacity_->setValue(qRound(templ.value(QStringLiteral("opacity")).toDouble(.35) * 100));
    templateScale_->setValue(qRound(templ.value(QStringLiteral("scale")).toDouble(1.0) * 100));
    templateRotation_->setValue(qRound(templ.value(QStringLiteral("rotation")).toDouble()));
    refreshing_ = false;
    refreshLayers();
    refreshAssets();
    refreshSelectionControls();
    refreshViewport();
    QTimer::singleShot(0, viewport_, [this]() { if (viewport_) viewport_->fitDocument(); });
}

void PilinReyEditor::ensurePilinDocument() {
    QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject();
    p.insert(QStringLiteral("version"), 7);
    if (!p.contains(QStringLiteral("width"))) p.insert(QStringLiteral("width"), 4096);
    if (!p.contains(QStringLiteral("height"))) p.insert(QStringLiteral("height"), 2304);

    QJsonObject theme = p.value(QStringLiteral("theme")).toObject();
    const QList<QPair<QString, QString>> defaults{
        {QStringLiteral("land"), QStringLiteral("#d8c99e")}, {QStringLiteral("sea"), QStringLiteral("#b7c5c2")},
        {QStringLiteral("coast"), QStringLiteral("#4a4135")}, {QStringLiteral("river"), QStringLiteral("#25506f")},
        {QStringLiteral("road"), QStringLiteral("#70502f")}, {QStringLiteral("border"), QStringLiteral("#913f37")},
        {QStringLiteral("forest"), QStringLiteral("#375635")}, {QStringLiteral("mountain"), QStringLiteral("#4d443b")},
        {QStringLiteral("region"), QStringLiteral("#b07548")}, {QStringLiteral("symbol"), QStringLiteral("#302822")},
        {QStringLiteral("text"), QStringLiteral("#282520")}, {QStringLiteral("labelOutline"), QStringLiteral("#eee2c3")}
    };
    for (const auto& entry : defaults) if (!theme.contains(entry.first)) theme.insert(entry.first, entry.second);
    p.insert(QStringLiteral("theme"), theme);

    QJsonObject templ = p.value(QStringLiteral("template")).toObject();
    if (!templ.contains(QStringLiteral("dataUrl"))) templ.insert(QStringLiteral("dataUrl"), map_.value(QStringLiteral("backgroundImageDataUrl")).toString());
    if (!templ.contains(QStringLiteral("opacity"))) templ.insert(QStringLiteral("opacity"), .35);
    if (!templ.contains(QStringLiteral("visible"))) templ.insert(QStringLiteral("visible"), true);
    if (!templ.contains(QStringLiteral("scale"))) templ.insert(QStringLiteral("scale"), 1.0);
    if (!templ.contains(QStringLiteral("rotation"))) templ.insert(QStringLiteral("rotation"), 0.0);
    if (!templ.contains(QStringLiteral("x"))) templ.insert(QStringLiteral("x"), 0.0);
    if (!templ.contains(QStringLiteral("y"))) templ.insert(QStringLiteral("y"), 0.0);
    p.insert(QStringLiteral("template"), templ);

    QJsonArray layers = p.value(QStringLiteral("layers")).toArray();
    if (layers.isEmpty()) {
        const QStringList names{tr("Terreno"), tr("Hidrografía"), tr("Relieve y vegetación"), tr("Fronteras y regiones"), tr("Caminos"), tr("Asentamientos y assets"), tr("Etiquetas")};
        for (const QString& name : names) layers.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("layer"))}, {QStringLiteral("name"), name}, {QStringLiteral("visible"), true}, {QStringLiteral("locked"), false}, {QStringLiteral("opacity"), 1.0}, {QStringLiteral("objects"), QJsonArray()}});
        p.insert(QStringLiteral("activeLayerId"), layers.first().toObject().value(QStringLiteral("id")).toString());
    }
    p.insert(QStringLiteral("layers"), layers);
    if (!p.contains(QStringLiteral("activeLayerId")) && !layers.isEmpty()) p.insert(QStringLiteral("activeLayerId"), layers.first().toObject().value(QStringLiteral("id")).toString());
    if (!p.contains(QStringLiteral("assets"))) p.insert(QStringLiteral("assets"), QJsonArray());
    map_.insert(QStringLiteral("pilinRey"), p);
}

void PilinReyEditor::refreshLayers() {
    refreshing_ = true;
    layers_->clear();
    const QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject();
    const QString active = p.value(QStringLiteral("activeLayerId")).toString();
    const QJsonArray layers = p.value(QStringLiteral("layers")).toArray();
    int current = 0;
    for (int i = 0; i < layers.size(); ++i) {
        const QJsonObject layer = layers.at(i).toObject();
        auto* item = new QListWidgetItem(layer.value(QStringLiteral("name")).toString(tr("Capa")));
        item->setData(Qt::UserRole, layer.value(QStringLiteral("id")).toString());
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
        item->setCheckState(layer.value(QStringLiteral("visible")).toBool(true) ? Qt::Checked : Qt::Unchecked);
        layers_->addItem(item);
        if (layer.value(QStringLiteral("id")).toString() == active) current = i;
    }
    if (layers_->count()) layers_->setCurrentRow(current);
    refreshing_ = false;
    refreshLayerControls();
    undoButton_->setEnabled(!undoStack_.isEmpty());
    redoButton_->setEnabled(!redoStack_.isEmpty());
}

void PilinReyEditor::refreshLayerControls() {
    const bool previous = refreshing_;
    refreshing_ = true;
    const QString active = activeLayerId();
    bool found = false;
    for (const QJsonValue& value : map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray()) {
        const QJsonObject layer = value.toObject();
        if (layer.value(QStringLiteral("id")).toString() != active) continue;
        layerLocked_->setChecked(layer.value(QStringLiteral("locked")).toBool(false));
        layerOpacity_->setValue(qRound(layer.value(QStringLiteral("opacity")).toDouble(1.0) * 100));
        found = true;
        break;
    }
    layerLocked_->setEnabled(found);
    layerOpacity_->setEnabled(found);
    refreshing_ = previous;
}

void PilinReyEditor::refreshAssets() {
    refreshing_ = true;
    assets_->clear();
    const QList<QPair<QString, QString>> builtins{{QStringLiteral("castle"), tr("Castillo")}, {QStringLiteral("tower"), tr("Torre")}, {QStringLiteral("temple"), tr("Templo")}, {QStringLiteral("ruin"), tr("Ruina")}, {QStringLiteral("ship"), tr("Barco")}, {QStringLiteral("bridge"), tr("Puente")}, {QStringLiteral("compass"), tr("Brújula")}, {QStringLiteral("mill"), tr("Molino")}};
    for (const auto& asset : builtins) {
        auto* item = new QListWidgetItem(QStringLiteral("✦\n") + asset.second);
        item->setTextAlignment(Qt::AlignCenter);
        item->setData(Qt::UserRole, asset.first);
        assets_->addItem(item);
    }
    for (const QJsonValue& value : map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("assets")).toArray()) {
        const QJsonObject asset = value.toObject();
        auto* item = new QListWidgetItem(QStringLiteral("▧\n") + asset.value(QStringLiteral("name")).toString(tr("Asset")));
        item->setTextAlignment(Qt::AlignCenter);
        item->setData(Qt::UserRole, QStringLiteral("custom"));
        item->setData(Qt::UserRole + 1, asset.value(QStringLiteral("dataUrl")).toString());
        assets_->addItem(item);
    }
    refreshing_ = false;
}

void PilinReyEditor::refreshSelectionControls() {
    const QJsonArray objects = selectedObjects();
    const bool has = !objects.isEmpty();
    selectionPopover_->setVisible(has);
    editObjectButton_->setEnabled(objects.size() == 1);
    duplicateObjectButton_->setEnabled(has);
    deleteObjectButton_->setEnabled(has);
    linkAtlasButton_->setEnabled(objects.size() == 1 && objects.first().toObject().value(QStringLiteral("type")).toString() == QStringLiteral("settlement"));
    if (!has) selectionLabel_->setText(tr("Selección"));
    else if (objects.size() == 1) {
        const QJsonObject object = objects.first().toObject();
        const QString label = object.value(QStringLiteral("label")).toString(object.value(QStringLiteral("text")).toString(object.value(QStringLiteral("type")).toString()));
        selectionLabel_->setText(tr("Selección · %1").arg(label));
    } else selectionLabel_->setText(tr("%1 objetos seleccionados").arg(objects.size()));
    layoutFloatingPanels();
}

void PilinReyEditor::refreshViewport() { if (viewport_) { viewport_->setDocument(map_); viewport_->setSelection(selectedObjectIds_); } }

void PilinReyEditor::chooseTemplate() {
    const QString path = QFileDialog::getOpenFileName(this, tr("Plantilla"), QString(), tr("Imágenes (*.png *.jpg *.jpeg *.webp)"));
    if (path.isEmpty()) return;
    const QString data = imageToDataUrl(path);
    if (data.isEmpty()) return;
    mutateDocument([&](QJsonObject& p) { QJsonObject t = p.value(QStringLiteral("template")).toObject(); t.insert(QStringLiteral("dataUrl"), data); t.insert(QStringLiteral("visible"), true); p.insert(QStringLiteral("template"), t); });
    map_.insert(QStringLiteral("backgroundImageDataUrl"), data);
    persistToArchive();
}

void PilinReyEditor::clearTemplate() {
    mutateDocument([](QJsonObject& p) { QJsonObject t = p.value(QStringLiteral("template")).toObject(); t.insert(QStringLiteral("dataUrl"), QString()); p.insert(QStringLiteral("template"), t); });
    map_.remove(QStringLiteral("backgroundImageDataUrl"));
    persistToArchive();
}

void PilinReyEditor::rotateTemplate(double delta) { mutateDocument([&](QJsonObject& p) { QJsonObject t = p.value(QStringLiteral("template")).toObject(); t.insert(QStringLiteral("rotation"), t.value(QStringLiteral("rotation")).toDouble() + delta); p.insert(QStringLiteral("template"), t); }); }
void PilinReyEditor::scaleTemplate(double factor) { mutateDocument([&](QJsonObject& p) { QJsonObject t = p.value(QStringLiteral("template")).toObject(); t.insert(QStringLiteral("scale"), std::clamp(t.value(QStringLiteral("scale")).toDouble(1.0) * factor, .05, 5.0)); p.insert(QStringLiteral("template"), t); }); }

void PilinReyEditor::addLayer() {
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Nueva capa"), tr("Nombre:"), QLineEdit::Normal, tr("Nueva capa"), &ok).trimmed();
    if (!ok || name.isEmpty()) return;
    mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); const QString id = uid(QStringLiteral("layer")); layers.append(QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("name"), name}, {QStringLiteral("visible"), true}, {QStringLiteral("locked"), false}, {QStringLiteral("opacity"), 1.0}, {QStringLiteral("objects"), QJsonArray()}}); p.insert(QStringLiteral("layers"), layers); p.insert(QStringLiteral("activeLayerId"), id); });
}

void PilinReyEditor::duplicateLayer() {
    const int row = layers_->currentRow();
    if (row < 0) return;
    mutateDocument([&](QJsonObject& p) {
        QJsonArray layers = p.value(QStringLiteral("layers")).toArray();
        if (row >= layers.size()) return;
        QJsonObject copy = layers.at(row).toObject();
        const QString id = uid(QStringLiteral("layer"));
        copy.insert(QStringLiteral("id"), id);
        copy.insert(QStringLiteral("name"), copy.value(QStringLiteral("name")).toString() + tr(" copia"));
        QJsonArray objects = copy.value(QStringLiteral("objects")).toArray();
        for (int i = 0; i < objects.size(); ++i) { QJsonObject object = objects.at(i).toObject(); object.insert(QStringLiteral("id"), uid(QStringLiteral("mapobj"))); objects.replace(i, object); }
        copy.insert(QStringLiteral("objects"), objects);
        layers.insert(row + 1, copy);
        p.insert(QStringLiteral("layers"), layers);
        p.insert(QStringLiteral("activeLayerId"), id);
    });
}

void PilinReyEditor::removeLayer() {
    const int row = layers_->currentRow();
    if (row < 0) return;
    mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); if (layers.size() <= 1 || row >= layers.size()) return; layers.removeAt(row); p.insert(QStringLiteral("layers"), layers); p.insert(QStringLiteral("activeLayerId"), layers.at(qMin(row, layers.size() - 1)).toObject().value(QStringLiteral("id")).toString()); });
}

void PilinReyEditor::moveLayer(int delta) {
    const int row = layers_->currentRow();
    const int target = row + delta;
    if (row < 0) return;
    mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); if (target < 0 || target >= layers.size()) return; const QJsonValue value = layers.at(row); layers.removeAt(row); layers.insert(target, value); p.insert(QStringLiteral("layers"), layers); });
}

void PilinReyEditor::setLayerLocked(bool locked) {
    const QString active = activeLayerId();
    mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); for (int i = 0; i < layers.size(); ++i) { QJsonObject layer = layers.at(i).toObject(); if (layer.value(QStringLiteral("id")).toString() != active) continue; layer.insert(QStringLiteral("locked"), locked); layers.replace(i, layer); break; } p.insert(QStringLiteral("layers"), layers); });
}

void PilinReyEditor::setLayerOpacity(int value) {
    const QString active = activeLayerId();
    mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); for (int i = 0; i < layers.size(); ++i) { QJsonObject layer = layers.at(i).toObject(); if (layer.value(QStringLiteral("id")).toString() != active) continue; layer.insert(QStringLiteral("opacity"), value / 100.0); layers.replace(i, layer); break; } p.insert(QStringLiteral("layers"), layers); });
}

void PilinReyEditor::applyLayerItem(QListWidgetItem* item) {
    if (refreshing_ || !item) return;
    const QString id = item->data(Qt::UserRole).toString();
    const QString name = item->text().trimmed();
    const bool visible = item->checkState() == Qt::Checked;
    mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); for (int i = 0; i < layers.size(); ++i) { QJsonObject layer = layers.at(i).toObject(); if (layer.value(QStringLiteral("id")).toString() != id) continue; layer.insert(QStringLiteral("name"), name.isEmpty() ? tr("Capa") : name); layer.insert(QStringLiteral("visible"), visible); layers.replace(i, layer); break; } p.insert(QStringLiteral("layers"), layers); });
}

void PilinReyEditor::setActiveTool(Tool tool) {
    viewport_->setTool(tool);
    if (QWidget* options = canvasHost_->findChild<QWidget*>(QStringLiteral("pilinToolOptions"))) {
        const bool show = tool == Tool::Coast || tool == Tool::Eraser || tool == Tool::Forest || tool == Tool::Mountain || tool == Tool::River || tool == Tool::Road || tool == Tool::Border || tool == Tool::Region;
        options->setVisible(show);
    }
    toolOptionsLabel_->setText(tool == Tool::Forest ? tr("Bosque") : tool == Tool::Mountain ? tr("Montaña") : tool == Tool::River ? tr("Río") : tool == Tool::Road ? tr("Camino") : tool == Tool::Border ? tr("Frontera") : tool == Tool::Coast ? tr("Tierra") : tool == Tool::Eraser ? tr("Mar") : tr("Herramienta"));
    layoutFloatingPanels();
}

QString PilinReyEditor::activeLayerId() const { return map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("activeLayerId")).toString(); }

void PilinReyEditor::addPathObject(const QString& type, const QJsonArray& points) {
    if (type.isEmpty() || points.isEmpty()) return;
    mutateDocument([&](QJsonObject& p) {
        QJsonArray layers = p.value(QStringLiteral("layers")).toArray();
        int target = layers_->currentRow();
        if (target < 0 || target >= layers.size()) target = 0;
        QJsonObject layer = layers.at(target).toObject();
        if (layer.value(QStringLiteral("locked")).toBool(false)) return;
        QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
        const QString id = uid(QStringLiteral("mapobj"));
        QJsonObject object{{QStringLiteral("id"), id}, {QStringLiteral("type"), type}, {QStringLiteral("points"), points}, {QStringLiteral("width"), strokeWidth_}, {QStringLiteral("density"), density_}, {QStringLiteral("symbolSize"), symbolSize_}};
        if (type == QStringLiteral("region")) { object.insert(QStringLiteral("closed"), true); object.insert(QStringLiteral("fillOpacity"), .18); }
        objects.append(object);
        layer.insert(QStringLiteral("objects"), objects);
        layers.replace(target, layer);
        p.insert(QStringLiteral("layers"), layers);
        selectedObjectIds_ = {id};
    });
}

void PilinReyEditor::addSettlement(double x, double y) {
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Asentamiento"), tr("Nombre:"), QLineEdit::Normal, tr("Poblado"), &ok).trimmed();
    if (!ok || name.isEmpty()) return;
    const QStringList kinds{tr("Capital"), tr("Ciudad"), tr("Villa"), tr("Pueblo"), tr("Aldea"), tr("Puerto"), tr("Fortaleza"), tr("Ruina")};
    const QString kind = QInputDialog::getItem(this, tr("Asentamiento"), tr("Tipo:"), kinds, 3, false, &ok);
    if (!ok) return;
    mutateDocument([&](QJsonObject& p) {
        QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); int target = layers_->currentRow(); if (target < 0 || target >= layers.size()) target = qMax(0, layers.size() - 2);
        QJsonObject layer = layers.at(target).toObject(); if (layer.value(QStringLiteral("locked")).toBool(false)) return;
        QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); const QString id = uid(QStringLiteral("mapobj"));
        objects.append(QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("type"), QStringLiteral("settlement")}, {QStringLiteral("kind"), kind}, {QStringLiteral("label"), name}, {QStringLiteral("x"), x}, {QStringLiteral("y"), y}, {QStringLiteral("scale"), 1.0}});
        layer.insert(QStringLiteral("objects"), objects); layers.replace(target, layer); p.insert(QStringLiteral("layers"), layers); selectedObjectIds_ = {id};
    });
}

void PilinReyEditor::addStamp(double x, double y) {
    mutateDocument([&](QJsonObject& p) {
        QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); int target = layers_->currentRow(); if (target < 0 || target >= layers.size()) target = qMax(0, layers.size() - 2);
        QJsonObject layer = layers.at(target).toObject(); if (layer.value(QStringLiteral("locked")).toBool(false)) return;
        QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); const QString id = uid(QStringLiteral("mapobj"));
        QJsonObject stamp{{QStringLiteral("id"), id}, {QStringLiteral("type"), QStringLiteral("stamp")}, {QStringLiteral("assetKind"), selectedAssetKind_}, {QStringLiteral("x"), x}, {QStringLiteral("y"), y}, {QStringLiteral("scale"), 1.0}, {QStringLiteral("rotation"), 0.0}, {QStringLiteral("size"), symbolSize_ * 2.2}, {QStringLiteral("opacity"), 1.0}};
        if (!selectedAssetData_.isEmpty()) stamp.insert(QStringLiteral("dataUrl"), selectedAssetData_);
        objects.append(stamp); layer.insert(QStringLiteral("objects"), objects); layers.replace(target, layer); p.insert(QStringLiteral("layers"), layers); selectedObjectIds_ = {id};
    });
}

void PilinReyEditor::addLabel(double x, double y) {
    bool ok = false;
    const QString text = QInputDialog::getText(this, tr("Etiqueta"), tr("Texto:"), QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || text.isEmpty()) return;
    const int size = QInputDialog::getInt(this, tr("Etiqueta"), tr("Tamaño:"), 54, 18, 220, 2, &ok);
    if (!ok) return;
    mutateDocument([&](QJsonObject& p) {
        QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); int target = layers_->currentRow(); if (target < 0 || target >= layers.size()) target = layers.size() - 1;
        QJsonObject layer = layers.at(target).toObject(); if (layer.value(QStringLiteral("locked")).toBool(false)) return;
        QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); const QString id = uid(QStringLiteral("mapobj"));
        objects.append(QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("type"), QStringLiteral("label")}, {QStringLiteral("text"), text}, {QStringLiteral("x"), x}, {QStringLiteral("y"), y}, {QStringLiteral("fontSize"), size}, {QStringLiteral("rotation"), 0.0}, {QStringLiteral("bold"), false}});
        layer.insert(QStringLiteral("objects"), objects); layers.replace(target, layer); p.insert(QStringLiteral("layers"), layers); selectedObjectIds_ = {id};
    });
}

void PilinReyEditor::selectObjects(const QStringList& ids) { selectedObjectIds_ = ids; refreshSelectionControls(); if (viewport_) viewport_->setSelection(ids); }

QJsonObject PilinReyEditor::selectedObject() const {
    if (selectedObjectIds_.isEmpty()) return {};
    const QString id = selectedObjectIds_.first();
    for (const QJsonValue& layerValue : map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray())
        for (const QJsonValue& objectValue : layerValue.toObject().value(QStringLiteral("objects")).toArray()) {
            const QJsonObject object = objectValue.toObject();
            if (object.value(QStringLiteral("id")).toString() == id) return object;
        }
    return {};
}

QJsonArray PilinReyEditor::selectedObjects() const {
    QJsonArray result;
    for (const QString& id : selectedObjectIds_)
        for (const QJsonValue& layerValue : map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray())
            for (const QJsonValue& objectValue : layerValue.toObject().value(QStringLiteral("objects")).toArray()) {
                const QJsonObject object = objectValue.toObject();
                if (object.value(QStringLiteral("id")).toString() == id) { result.append(object); break; }
            }
    return result;
}

void PilinReyEditor::movePointObject(const QString& id, double x, double y) {
    mutateDocument([&](QJsonObject& p) {
        QJsonArray layers = p.value(QStringLiteral("layers")).toArray();
        for (int i = 0; i < layers.size(); ++i) {
            QJsonObject layer = layers.at(i).toObject(); QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
            for (int j = 0; j < objects.size(); ++j) {
                QJsonObject object = objects.at(j).toObject(); if (object.value(QStringLiteral("id")).toString() != id) continue;
                object.insert(QStringLiteral("x"), x); object.insert(QStringLiteral("y"), y); objects.replace(j, object); layer.insert(QStringLiteral("objects"), objects); layers.replace(i, layer); p.insert(QStringLiteral("layers"), layers); emit markerMoved(id, x, y); return;
            }
        }
    });
}

void PilinReyEditor::movePathPoint(const QString& id, int index, double x, double y) {
    mutateDocument([&](QJsonObject& p) {
        QJsonArray layers = p.value(QStringLiteral("layers")).toArray();
        for (int i = 0; i < layers.size(); ++i) {
            QJsonObject layer = layers.at(i).toObject(); QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
            for (int j = 0; j < objects.size(); ++j) {
                QJsonObject object = objects.at(j).toObject(); if (object.value(QStringLiteral("id")).toString() != id) continue;
                QJsonArray points = object.value(QStringLiteral("points")).toArray(); if (index < 0 || index >= points.size()) return;
                QJsonObject point = points.at(index).toObject(); point.insert(QStringLiteral("x"), x); point.insert(QStringLiteral("y"), y); points.replace(index, point);
                if (object.value(QStringLiteral("closed")).toBool(false) && points.size() > 2) { if (index == 0) points.replace(points.size() - 1, point); else if (index == points.size() - 1) points.replace(0, point); }
                object.insert(QStringLiteral("points"), points); objects.replace(j, object); layer.insert(QStringLiteral("objects"), objects); layers.replace(i, layer); p.insert(QStringLiteral("layers"), layers); return;
            }
        }
    });
}

void PilinReyEditor::editSelectedObject() {
    QJsonObject object = selectedObject();
    if (object.isEmpty() || selectedObjectIds_.size() != 1) return;
    const QString id = selectedObjectIds_.first();
    const QString type = object.value(QStringLiteral("type")).toString();
    bool ok = false;
    if (type == QStringLiteral("label")) {
        const QString text = QInputDialog::getText(this, tr("Editar etiqueta"), tr("Texto:"), QLineEdit::Normal, object.value(QStringLiteral("text")).toString(), &ok).trimmed();
        if (!ok || text.isEmpty()) return;
        mutateDocument([&](QJsonObject& p) {
            QJsonArray layers = p.value(QStringLiteral("layers")).toArray();
            for (int i = 0; i < layers.size(); ++i) { QJsonObject layer = layers.at(i).toObject(); QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); for (int j = 0; j < objects.size(); ++j) { QJsonObject current = objects.at(j).toObject(); if (current.value(QStringLiteral("id")).toString() != id) continue; current.insert(QStringLiteral("text"), text); objects.replace(j, current); layer.insert(QStringLiteral("objects"), objects); layers.replace(i, layer); p.insert(QStringLiteral("layers"), layers); return; } }
        });
        return;
    }
    if (type == QStringLiteral("settlement")) {
        const QString text = QInputDialog::getText(this, tr("Editar asentamiento"), tr("Nombre:"), QLineEdit::Normal, object.value(QStringLiteral("label")).toString(), &ok).trimmed();
        if (!ok || text.isEmpty()) return;
        mutateDocument([&](QJsonObject& p) {
            QJsonArray layers = p.value(QStringLiteral("layers")).toArray();
            for (int i = 0; i < layers.size(); ++i) { QJsonObject layer = layers.at(i).toObject(); QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); for (int j = 0; j < objects.size(); ++j) { QJsonObject current = objects.at(j).toObject(); if (current.value(QStringLiteral("id")).toString() != id) continue; current.insert(QStringLiteral("label"), text); objects.replace(j, current); layer.insert(QStringLiteral("objects"), objects); layers.replace(i, layer); p.insert(QStringLiteral("layers"), layers); return; } }
        });
    }
}

void PilinReyEditor::duplicateSelectedObject() {
    if (selectedObjectIds_.isEmpty()) return;
    mutateDocument([&](QJsonObject& p) {
        QJsonArray layers = p.value(QStringLiteral("layers")).toArray();
        QStringList newIds;
        for (int i = 0; i < layers.size(); ++i) {
            QJsonObject layer = layers.at(i).toObject(); QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); const int originalCount = objects.size();
            for (int j = 0; j < originalCount; ++j) {
                QJsonObject object = objects.at(j).toObject(); if (!selectedObjectIds_.contains(object.value(QStringLiteral("id")).toString())) continue;
                const QString id = uid(QStringLiteral("mapobj")); object.insert(QStringLiteral("id"), id);
                if (object.contains(QStringLiteral("x"))) { object.insert(QStringLiteral("x"), object.value(QStringLiteral("x")).toDouble() + 70); object.insert(QStringLiteral("y"), object.value(QStringLiteral("y")).toDouble() + 70); }
                else { QJsonArray points = object.value(QStringLiteral("points")).toArray(); for (int k = 0; k < points.size(); ++k) { QJsonObject point = points.at(k).toObject(); point.insert(QStringLiteral("x"), point.value(QStringLiteral("x")).toDouble() + 70); point.insert(QStringLiteral("y"), point.value(QStringLiteral("y")).toDouble() + 70); points.replace(k, point); } object.insert(QStringLiteral("points"), points); }
                objects.append(object); newIds.append(id);
            }
            layer.insert(QStringLiteral("objects"), objects); layers.replace(i, layer);
        }
        p.insert(QStringLiteral("layers"), layers); selectedObjectIds_ = newIds;
    });
}

void PilinReyEditor::deleteSelectedObject() {
    if (selectedObjectIds_.isEmpty()) return;
    mutateDocument([&](QJsonObject& p) {
        QJsonArray layers = p.value(QStringLiteral("layers")).toArray();
        for (int i = 0; i < layers.size(); ++i) {
            QJsonObject layer = layers.at(i).toObject(); QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
            for (int j = objects.size() - 1; j >= 0; --j) if (selectedObjectIds_.contains(objects.at(j).toObject().value(QStringLiteral("id")).toString())) objects.removeAt(j);
            layer.insert(QStringLiteral("objects"), objects); layers.replace(i, layer);
        }
        p.insert(QStringLiteral("layers"), layers); selectedObjectIds_.clear();
    });
}

void PilinReyEditor::copySelectedObject() { clipboardObjects_ = selectedObjects(); }

void PilinReyEditor::pasteCopiedObject() {
    if (clipboardObjects_.isEmpty()) return;
    mutateDocument([&](QJsonObject& p) {
        QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); int target = layers_->currentRow(); if (target < 0 || target >= layers.size()) target = 0;
        QJsonObject layer = layers.at(target).toObject(); QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); QStringList newIds;
        for (const QJsonValue& value : clipboardObjects_) {
            QJsonObject object = value.toObject(); const QString id = uid(QStringLiteral("mapobj")); object.insert(QStringLiteral("id"), id);
            if (object.contains(QStringLiteral("x"))) { object.insert(QStringLiteral("x"), object.value(QStringLiteral("x")).toDouble() + 80); object.insert(QStringLiteral("y"), object.value(QStringLiteral("y")).toDouble() + 80); }
            else { QJsonArray points = object.value(QStringLiteral("points")).toArray(); for (int i = 0; i < points.size(); ++i) { QJsonObject point = points.at(i).toObject(); point.insert(QStringLiteral("x"), point.value(QStringLiteral("x")).toDouble() + 80); point.insert(QStringLiteral("y"), point.value(QStringLiteral("y")).toDouble() + 80); points.replace(i, point); } object.insert(QStringLiteral("points"), points); }
            objects.append(object); newIds.append(id);
        }
        layer.insert(QStringLiteral("objects"), objects); layers.replace(target, layer); p.insert(QStringLiteral("layers"), layers); selectedObjectIds_ = newIds;
    });
}

void PilinReyEditor::linkSelectedToAtlas() {
    QJsonObject object = selectedObject();
    if (object.isEmpty() || object.value(QStringLiteral("type")).toString() != QStringLiteral("settlement")) return;
    QWidget* cursor = parentWidget(); WorldPage* world = nullptr;
    while (cursor) { world = qobject_cast<WorldPage*>(cursor); if (world) break; cursor = cursor->parentWidget(); }
    if (!world || !world->document_) return;
    const QJsonArray atlas = world->document_->array(QStringLiteral("world"));
    QStringList names; QStringList ids;
    for (const QJsonValue& value : atlas) { const QJsonObject entry = value.toObject(); names.append(entry.value(QStringLiteral("name")).toString(tr("Entrada sin nombre"))); ids.append(entry.value(QStringLiteral("id")).toString()); }
    if (names.isEmpty()) { QMessageBox::information(this, tr("Atlas"), tr("Crea primero una entrada en el Atlas.")); return; }
    bool ok = false; const QString chosen = QInputDialog::getItem(this, tr("Enlazar con Atlas"), tr("Entrada:"), names, 0, false, &ok); if (!ok) return;
    const int chosenIndex = names.indexOf(chosen); if (chosenIndex < 0) return; const QString selectedId = selectedObjectIds_.first();
    mutateDocument([&](QJsonObject& p) {
        QJsonArray layers = p.value(QStringLiteral("layers")).toArray();
        for (int i = 0; i < layers.size(); ++i) { QJsonObject layer = layers.at(i).toObject(); QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); for (int j = 0; j < objects.size(); ++j) { QJsonObject current = objects.at(j).toObject(); if (current.value(QStringLiteral("id")).toString() != selectedId) continue; current.insert(QStringLiteral("atlasId"), ids.at(chosenIndex)); objects.replace(j, current); layer.insert(QStringLiteral("objects"), objects); layers.replace(i, layer); p.insert(QStringLiteral("layers"), layers); return; } }
    });
}

void PilinReyEditor::transformSelection(double scaleFactor, double rotationDegrees) {
    if (selectedObjectIds_.isEmpty()) return;
    mutateDocument([&](QJsonObject& p) {
        QJsonArray layers = p.value(QStringLiteral("layers")).toArray();
        for (int i = 0; i < layers.size(); ++i) {
            QJsonObject layer = layers.at(i).toObject(); QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
            for (int j = 0; j < objects.size(); ++j) {
                QJsonObject object = objects.at(j).toObject(); if (!selectedObjectIds_.contains(object.value(QStringLiteral("id")).toString())) continue;
                if (object.contains(QStringLiteral("scale"))) object.insert(QStringLiteral("scale"), std::clamp(object.value(QStringLiteral("scale")).toDouble(1.0) * scaleFactor, .1, 12.0));
                if (object.contains(QStringLiteral("rotation"))) object.insert(QStringLiteral("rotation"), object.value(QStringLiteral("rotation")).toDouble() + rotationDegrees);
                objects.replace(j, object);
            }
            layer.insert(QStringLiteral("objects"), objects); layers.replace(i, layer);
        }
        p.insert(QStringLiteral("layers"), layers);
    });
}

void PilinReyEditor::importAsset() {
    const QString path = QFileDialog::getOpenFileName(this, tr("Importar asset"), QString(), tr("Imágenes (*.png *.jpg *.jpeg *.webp)"));
    if (path.isEmpty()) return;
    const QString data = imageToDataUrl(path);
    if (data.isEmpty()) return;
    mutateDocument([&](QJsonObject& p) { QJsonArray assets = p.value(QStringLiteral("assets")).toArray(); assets.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("asset"))}, {QStringLiteral("name"), QFileInfo(path).completeBaseName()}, {QStringLiteral("dataUrl"), data}}); p.insert(QStringLiteral("assets"), assets); });
}

void PilinReyEditor::setThemeColor(const QString& key) {
    QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject(); QJsonObject theme = p.value(QStringLiteral("theme")).toObject();
    const QColor current(theme.value(key).toString());
    const QColor color = QColorDialog::getColor(current.isValid() ? current : Qt::white, this, tr("Color"));
    if (!color.isValid()) return;
    mutateDocument([&](QJsonObject& target) { QJsonObject updated = target.value(QStringLiteral("theme")).toObject(); updated.insert(key, color.name(QColor::HexRgb)); target.insert(QStringLiteral("theme"), updated); });
}

void PilinReyEditor::mutateDocument(const std::function<void(QJsonObject&)>& mutation) {
    if (refreshing_) return;
    pushUndo();
    QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
    mutation(pilin);
    map_.insert(QStringLiteral("pilinRey"), pilin);
    refreshViewport();
    refreshLayers();
    refreshAssets();
    refreshSelectionControls();
    persistToArchive();
    emit mapEdited(map_);
}

void PilinReyEditor::pushUndo() {
    undoStack_.append(map_);
    while (undoStack_.size() > 64) undoStack_.removeFirst();
    redoStack_.clear();
}

void PilinReyEditor::undo() {
    if (undoStack_.isEmpty()) return;
    redoStack_.append(map_);
    map_ = undoStack_.takeLast();
    selectedObjectIds_.clear();
    refreshLayers(); refreshAssets(); refreshSelectionControls(); refreshViewport(); persistToArchive(); emit mapEdited(map_);
}

void PilinReyEditor::redo() {
    if (redoStack_.isEmpty()) return;
    undoStack_.append(map_);
    map_ = redoStack_.takeLast();
    selectedObjectIds_.clear();
    refreshLayers(); refreshAssets(); refreshSelectionControls(); refreshViewport(); persistToArchive(); emit mapEdited(map_);
}

void PilinReyEditor::exportMap() {
    const QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject();
    const QSize logical(qMax(1, p.value(QStringLiteral("width")).toInt(4096)), qMax(1, p.value(QStringLiteral("height")).toInt(2304)));
    MapExportDialog dialog(logical, this);
    if (dialog.exec() != QDialog::Accepted) return;
    const QString format = dialog.format();
    QString extension = QStringLiteral(".png"); QString filter = tr("PNG (*.png)");
    if (format == QStringLiteral("svg")) { extension = QStringLiteral(".svg"); filter = tr("SVG (*.svg)"); }
    else if (format == QStringLiteral("pdf")) { extension = QStringLiteral(".pdf"); filter = tr("PDF (*.pdf)"); }
    QString path = QFileDialog::getSaveFileName(this, tr("Exportar mapa"), map_.value(QStringLiteral("name")).toString(tr("mapa")) + extension, filter);
    if (path.isEmpty()) return;
    if (!path.endsWith(extension, Qt::CaseInsensitive)) path += extension;
    QString error;
    const bool ok = format == QStringLiteral("svg") ? MapExporter::exportSvg(map_, path, dialog.outputSize(), &error) : format == QStringLiteral("pdf") ? MapExporter::exportPdf(map_, path, dialog.outputSize(), &error) : MapExporter::exportPng(map_, path, dialog.outputSize(), &error);
    if (!ok) QMessageBox::critical(this, tr("No se pudo exportar"), error);
}

void PilinReyEditor::persistToArchive() {
    QWidget* cursor = parentWidget(); WorldPage* world = nullptr;
    while (cursor) { world = qobject_cast<WorldPage*>(cursor); if (world) break; cursor = cursor->parentWidget(); }
    if (!world || !world->document_ || !world->mapList_) return;
    const int row = world->mapList_->currentRow();
    QJsonArray maps = world->document_->array(QStringLiteral("maps"));
    if (row < 0 || row >= maps.size()) return;
    QJsonObject stored = maps.at(row).toObject();
    stored.insert(QStringLiteral("pilinRey"), map_.value(QStringLiteral("pilinRey")));
    if (map_.contains(QStringLiteral("backgroundImageDataUrl"))) stored.insert(QStringLiteral("backgroundImageDataUrl"), map_.value(QStringLiteral("backgroundImageDataUrl")));
    maps.replace(row, stored);
    world->document_->setArray(QStringLiteral("maps"), maps);
    emit world->changed();
}

} // namespace wbw
