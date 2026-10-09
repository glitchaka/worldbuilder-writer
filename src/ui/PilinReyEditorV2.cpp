#include "ui/PilinReyEditor.h"

#include "core/ArchiveDocument.h"
#include "ui/MapExportDialog.h"
#include "ui/MapExporter.h"
#include "ui/WorldPage.h"

#include <SDL3/SDL.h>

#include <QCheckBox>
#include <QColor>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEngine>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QShortcut>
#include <QSlider>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <vector>

namespace wbw {
namespace {

constexpr double kPi = 3.14159265358979323846;

QString uid(const QString& prefix) {
    return prefix + QLatin1Char('-') + QUuid::createUuid().toString(QUuid::Id128);
}

QJsonObject pointJson(double x, double y) {
    return QJsonObject{{QStringLiteral("x"), x}, {QStringLiteral("y"), y}};
}

QPointF jsonPoint(const QJsonValue& value) {
    const QJsonObject p = value.toObject();
    return QPointF(p.value(QStringLiteral("x")).toDouble(), p.value(QStringLiteral("y")).toDouble());
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

double sqr(double v) { return v * v; }

double distanceSquared(const QPointF& a, const QPointF& b) {
    return sqr(a.x() - b.x()) + sqr(a.y() - b.y());
}

double segmentDistanceSquared(const QPointF& p, const QPointF& a, const QPointF& b) {
    const QPointF ab = b - a;
    const double len2 = sqr(ab.x()) + sqr(ab.y());
    if (len2 < 1e-9) return distanceSquared(p, a);
    const QPointF ap = p - a;
    const double t = std::clamp((ap.x() * ab.x() + ap.y() * ab.y()) / len2, 0.0, 1.0);
    return distanceSquared(p, a + ab * t);
}

void rdpRecursive(const std::vector<QPointF>& input, int first, int last, double tolerance2, std::vector<bool>& keep) {
    if (last <= first + 1) return;
    double maxDistance = -1.0;
    int index = -1;
    for (int i = first + 1; i < last; ++i) {
        const double d = segmentDistanceSquared(input[static_cast<size_t>(i)], input[static_cast<size_t>(first)], input[static_cast<size_t>(last)]);
        if (d > maxDistance) { maxDistance = d; index = i; }
    }
    if (index >= 0 && maxDistance > tolerance2) {
        keep[static_cast<size_t>(index)] = true;
        rdpRecursive(input, first, index, tolerance2, keep);
        rdpRecursive(input, index, last, tolerance2, keep);
    }
}

QJsonArray simplifyStroke(const QJsonArray& input, double tolerance, bool closed) {
    if (input.size() < 5) return input;
    std::vector<QPointF> points;
    points.reserve(static_cast<size_t>(input.size()));
    for (const QJsonValue& value : input) points.push_back(jsonPoint(value));
    if (closed && points.size() > 3 && distanceSquared(points.front(), points.back()) < 1e-8) points.pop_back();
    if (points.size() < 4) return input;

    std::vector<bool> keep(points.size(), false);
    keep.front() = true;
    keep.back() = true;
    rdpRecursive(points, 0, static_cast<int>(points.size() - 1), tolerance * tolerance, keep);

    QJsonArray out;
    for (size_t i = 0; i < points.size(); ++i) {
        if (keep[i]) out.append(pointJson(points[i].x(), points[i].y()));
    }
    if (closed && out.size() >= 3) out.append(out.first());
    return out;
}

std::vector<QPointF> smoothPoints(const QJsonArray& points) {
    std::vector<QPointF> source;
    source.reserve(static_cast<size_t>(points.size()));
    for (const QJsonValue& value : points) source.push_back(jsonPoint(value));
    if (source.size() < 3) return source;

    const bool closed = distanceSquared(source.front(), source.back()) < 1e-8;
    if (closed && source.size() > 3) source.pop_back();
    if (source.size() < 3) return source;

    std::vector<QPointF> out;
    const size_t count = source.size();
    const size_t segments = closed ? count : count - 1;
    constexpr int samples = 8;
    out.reserve(segments * samples + 1);
    for (size_t i = 0; i < segments; ++i) {
        const size_t i1 = i;
        const size_t i2 = (i + 1) % count;
        const QPointF p0 = (!closed && i == 0) ? source[i1] : source[(i + count - 1) % count];
        const QPointF p1 = source[i1];
        const QPointF p2 = source[i2];
        const QPointF p3 = (!closed && i2 + 1 >= count) ? p2 : source[(i2 + 1) % count];
        for (int s = 0; s < samples; ++s) {
            const double t = static_cast<double>(s) / samples;
            const double t2 = t * t;
            const double t3 = t2 * t;
            out.emplace_back(
                0.5 * ((2.0 * p1.x()) + (-p0.x() + p2.x()) * t + (2.0 * p0.x() - 5.0 * p1.x() + 4.0 * p2.x() - p3.x()) * t2 + (-p0.x() + 3.0 * p1.x() - 3.0 * p2.x() + p3.x()) * t3),
                0.5 * ((2.0 * p1.y()) + (-p0.y() + p2.y()) * t + (2.0 * p0.y() - 5.0 * p1.y() + 4.0 * p2.y() - p3.y()) * t2 + (-p0.y() + 3.0 * p1.y() - 3.0 * p2.y() + p3.y()) * t3));
        }
    }
    out.push_back(closed ? out.front() : source.back());
    return out;
}

QColor paperColor() { return QColor(226, 216, 187); }
QColor landColor() { return QColor(199, 184, 146); }
QColor regionColor() { return QColor(191, 171, 128); }

QColor objectColor(const QString& type) {
    if (type == QStringLiteral("river")) return QColor(45, 101, 145);
    if (type == QStringLiteral("road")) return QColor(104, 72, 43);
    if (type == QStringLiteral("border")) return QColor(132, 65, 55);
    if (type == QStringLiteral("forest")) return QColor(51, 85, 50);
    if (type == QStringLiteral("mountain")) return QColor(73, 66, 59);
    if (type == QStringLiteral("region")) return QColor(115, 91, 55);
    return QColor(48, 44, 38);
}

QString toolType(PilinReyEditor::Tool tool) {
    switch (tool) {
        case PilinReyEditor::Tool::Coast: return QStringLiteral("coast");
        case PilinReyEditor::Tool::Region: return QStringLiteral("region");
        case PilinReyEditor::Tool::River: return QStringLiteral("river");
        case PilinReyEditor::Tool::Road: return QStringLiteral("road");
        case PilinReyEditor::Tool::Border: return QStringLiteral("border");
        case PilinReyEditor::Tool::Forest: return QStringLiteral("forest");
        case PilinReyEditor::Tool::Mountain: return QStringLiteral("mountain");
        default: return {};
    }
}

bool closesAutomatically(PilinReyEditor::Tool tool) {
    return tool == PilinReyEditor::Tool::Coast || tool == PilinReyEditor::Tool::Region;
}

int opacityByte(double opacity) {
    return qRound(std::clamp(opacity, 0.0, 1.0) * 255.0);
}

quint32 mixHash(quint32 x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

double unitNoise(quint32 seed) {
    return (mixHash(seed) & 0x00ffffffU) / static_cast<double>(0x01000000U);
}

QToolButton* iconButton(const QString& glyph, const QString& tooltip, QWidget* parent, const QString& objectName = QStringLiteral("pilinCommand")) {
    auto* button = new QToolButton(parent);
    button->setText(glyph);
    button->setToolTip(tooltip);
    button->setObjectName(objectName);
    button->setFixedSize(32, 32);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

} // namespace

class PilinReyViewport final : public QWidget {
public:
    using Tool = PilinReyEditor::Tool;

    std::function<void(const QString&, const QJsonArray&)> onPath;
    std::function<void(double, double)> onSettlement;
    std::function<void(double, double)> onLabel;
    std::function<void(const QString&)> onSelected;
    std::function<void(const QString&, double, double)> onMovePoint;
    std::function<void(const QString&, int, double, double)> onMovePathPoint;
    std::function<void()> onDeleteSelection;
    std::function<void(const QString&)> onStatus;

    explicit PilinReyViewport(QWidget* parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_NativeWindow, true);
        setAttribute(Qt::WA_PaintOnScreen, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setMinimumSize(640, 420);
    }

    ~PilinReyViewport() override {
        if (templateTexture_) SDL_DestroyTexture(templateTexture_);
        if (renderer_) SDL_DestroyRenderer(renderer_);
        if (window_) SDL_DestroyWindow(window_);
    }

    QPaintEngine* paintEngine() const override { return nullptr; }

    void setDocument(const QJsonObject& map) {
        map_ = map;
        templateDirty_ = true;
        if (!viewInitialized_) fitDocument();
        renderFrame();
    }

    void setTool(Tool tool) {
        tool_ = tool;
        drawing_ = false;
        currentPoints_ = QJsonArray();
        draggingObjectId_.clear();
        draggingPathId_.clear();
        draggingPathPointIndex_ = -1;
        measureActive_ = false;
        renderFrame();
    }

    Tool tool() const { return tool_; }
    int brushWidth() const { return brushWidth_; }
    int density() const { return density_; }
    int symbolSize() const { return symbolSize_; }
    int strokeWidth() const { return strokeWidth_; }

    void setBrushWidth(int value) { brushWidth_ = std::clamp(value, 30, 600); renderFrame(); }
    void setDensity(int value) { density_ = std::clamp(value, 10, 100); renderFrame(); }
    void setSymbolSize(int value) { symbolSize_ = std::clamp(value, 50, 180); renderFrame(); }
    void setStrokeWidth(int value) { strokeWidth_ = std::clamp(value, 1, 12); renderFrame(); }
    void setSnap(bool enabled) { snap_ = enabled; }

    void setSelection(const QString& id) { selectedObjectId_ = id; renderFrame(); }

    void fitDocument() {
        const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        const double docW = std::max(1.0, pilin.value(QStringLiteral("width")).toDouble(4096.0));
        const double docH = std::max(1.0, pilin.value(QStringLiteral("height")).toDouble(2304.0));
        if (width() < 80 || height() < 80) return;
        zoom_ = std::clamp(std::min((width() - 54.0) / docW, (height() - 54.0) / docH), 0.05, 5.0);
        pan_ = QPointF((width() - docW * zoom_) * 0.5, (height() - docH * zoom_) * 0.5);
        viewInitialized_ = true;
        renderFrame();
    }

protected:
    void showEvent(QShowEvent* event) override { QWidget::showEvent(event); ensureRenderer(); if (!viewInitialized_) fitDocument(); renderFrame(); }
    void resizeEvent(QResizeEvent* event) override { QWidget::resizeEvent(event); renderFrame(); }
    void paintEvent(QPaintEvent*) override { renderFrame(); }

    void wheelEvent(QWheelEvent* event) override {
        const QPointF before = screenToWorld(event->position());
        zoom_ = std::clamp(zoom_ * (event->angleDelta().y() > 0 ? 1.14 : 1.0 / 1.14), 0.05, 12.0);
        const QPointF after = screenToWorld(event->position());
        pan_ += QPointF((after.x() - before.x()) * zoom_, (after.y() - before.y()) * zoom_);
        event->accept();
        renderFrame();
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Escape) {
            drawing_ = false;
            currentPoints_ = QJsonArray();
            measureActive_ = false;
            draggingObjectId_.clear();
            draggingPathId_.clear();
            draggingPathPointIndex_ = -1;
            renderFrame();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Delete && onDeleteSelection) { onDeleteSelection(); event->accept(); return; }
        QWidget::keyPressEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override {
        setFocus(Qt::MouseFocusReason);
        lastMouse_ = event->position();
        if (event->button() == Qt::MiddleButton || tool_ == Tool::Pan) { panning_ = true; event->accept(); return; }
        if (event->button() != Qt::LeftButton) return;

        QPointF world = snapPoint(screenToWorld(event->position()));
        if (tool_ == Tool::Measure) {
            if (!measureActive_) { measureStart_ = world; measureEnd_ = world; measureActive_ = true; }
            else { measureEnd_ = world; measureActive_ = false; }
            renderFrame(); event->accept(); return;
        }
        if (tool_ == Tool::Eraser) {
            selectedObjectId_ = findObjectAt(world);
            if (onSelected) onSelected(selectedObjectId_);
            if (!selectedObjectId_.isEmpty() && onDeleteSelection) onDeleteSelection();
            event->accept(); return;
        }
        if (tool_ == Tool::Select) {
            selectedObjectId_ = findObjectAt(world);
            draggingObjectId_.clear(); draggingPathId_.clear(); draggingPathPointIndex_ = -1;
            if (!selectedObjectId_.isEmpty()) {
                const QJsonObject selected = objectById(selectedObjectId_);
                const QString type = selected.value(QStringLiteral("type")).toString();
                if (type == QStringLiteral("settlement") || type == QStringLiteral("label")) draggingObjectId_ = selectedObjectId_;
                else {
                    const int node = nearestNode(selected, world);
                    if (node >= 0) { draggingPathId_ = selectedObjectId_; draggingPathPointIndex_ = node; }
                }
            }
            if (onSelected) onSelected(selectedObjectId_);
            renderFrame(); event->accept(); return;
        }
        if (tool_ == Tool::Settlement) { if (onSettlement) onSettlement(world.x(), world.y()); event->accept(); return; }
        if (tool_ == Tool::Label) { if (onLabel) onLabel(world.x(), world.y()); event->accept(); return; }

        if (!toolType(tool_).isEmpty()) {
            drawing_ = true;
            currentPoints_ = QJsonArray{pointJson(world.x(), world.y())};
            event->accept();
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (panning_) { pan_ += event->position() - lastMouse_; lastMouse_ = event->position(); renderFrame(); return; }
        QPointF world = snapPoint(screenToWorld(event->position()));
        if (measureActive_) {
            measureEnd_ = world;
            const double d = std::sqrt(distanceSquared(measureStart_, measureEnd_));
            if (onStatus) onStatus(QStringLiteral("Medida %1 u · x %2 y %3 · %4%").arg(qRound(d)).arg(qRound(world.x())).arg(qRound(world.y())).arg(qRound(zoom_ * 100.0)));
            renderFrame(); return;
        }
        if (onStatus) onStatus(QStringLiteral("x %1  y %2  ·  %3%").arg(qRound(world.x())).arg(qRound(world.y())).arg(qRound(zoom_ * 100.0)));

        if (!draggingObjectId_.isEmpty() && (event->buttons() & Qt::LeftButton)) { updatePointObjectLocal(draggingObjectId_, world.x(), world.y()); renderFrame(); return; }
        if (!draggingPathId_.isEmpty() && draggingPathPointIndex_ >= 0 && (event->buttons() & Qt::LeftButton)) { updatePathPointLocal(draggingPathId_, draggingPathPointIndex_, world.x(), world.y()); renderFrame(); return; }
        if (!drawing_) return;

        const QPointF last = jsonPoint(currentPoints_.last());
        const double minDistance = std::max(3.0 / zoom_, 2.0);
        if (distanceSquared(last, world) < minDistance * minDistance) return;
        currentPoints_.append(pointJson(world.x(), world.y()));
        renderFrame();
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::MiddleButton || panning_) { panning_ = false; return; }
        if (event->button() != Qt::LeftButton) return;

        if (!draggingObjectId_.isEmpty()) {
            const QJsonObject object = objectById(draggingObjectId_);
            if (!object.isEmpty() && onMovePoint) onMovePoint(draggingObjectId_, object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble());
            draggingObjectId_.clear(); return;
        }
        if (!draggingPathId_.isEmpty() && draggingPathPointIndex_ >= 0) {
            const QJsonObject object = objectById(draggingPathId_);
            const QJsonArray points = object.value(QStringLiteral("points")).toArray();
            if (draggingPathPointIndex_ < points.size() && onMovePathPoint) {
                const QPointF p = jsonPoint(points.at(draggingPathPointIndex_));
                onMovePathPoint(draggingPathId_, draggingPathPointIndex_, p.x(), p.y());
            }
            draggingPathId_.clear(); draggingPathPointIndex_ = -1; return;
        }
        if (!drawing_) return;

        drawing_ = false;
        const QString type = toolType(tool_);
        const bool closed = closesAutomatically(tool_);
        if (closed && currentPoints_.size() >= 3) currentPoints_.append(currentPoints_.first());
        const double tolerance = std::max(2.0, 7.0 / std::max(zoom_, 0.05));
        QJsonArray simplified = simplifyStroke(currentPoints_, tolerance, closed);
        if (simplified.size() >= (closed ? 4 : 2) && onPath) onPath(type, simplified);
        currentPoints_ = QJsonArray();
        renderFrame();
    }

private:
    bool ensureRenderer() {
        if (renderer_) return true;
        if (!SDL_WasInit(SDL_INIT_VIDEO) && !SDL_Init(SDL_INIT_VIDEO)) {
            if (onStatus) onStatus(QStringLiteral("SDL: %1").arg(QString::fromUtf8(SDL_GetError())));
            return false;
        }
#ifdef Q_OS_WIN
        SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, reinterpret_cast<void*>(static_cast<quintptr>(winId())));
        window_ = SDL_CreateWindowWithProperties(props);
        SDL_DestroyProperties(props);
#else
        window_ = SDL_CreateWindow("Pilin Rey", width(), height(), SDL_WINDOW_RESIZABLE);
#endif
        if (!window_) return false;
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (!renderer_) return false;
        SDL_SetRenderVSync(renderer_, 1);
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
        return true;
    }

    QPointF screenToWorld(const QPointF& p) const { return QPointF((p.x() - pan_.x()) / zoom_, (p.y() - pan_.y()) / zoom_); }
    QPointF worldToScreen(const QPointF& p) const { return QPointF(p.x() * zoom_ + pan_.x(), p.y() * zoom_ + pan_.y()); }
    QPointF worldToScreen(double x, double y) const { return worldToScreen(QPointF(x, y)); }

    QPointF snapPoint(QPointF p) const {
        if (!snap_) return p;
        constexpr double grid = 25.0;
        p.setX(std::round(p.x() / grid) * grid);
        p.setY(std::round(p.y() / grid) * grid);
        return p;
    }

    QJsonObject objectById(const QString& id) const {
        const QJsonArray layers = map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray();
        for (const QJsonValue& lv : layers) for (const QJsonValue& ov : lv.toObject().value(QStringLiteral("objects")).toArray()) {
            const QJsonObject o = ov.toObject();
            if (o.value(QStringLiteral("id")).toString() == id) return o;
        }
        return {};
    }

    int nearestNode(const QJsonObject& object, const QPointF& world) const {
        const QJsonArray points = object.value(QStringLiteral("points")).toArray();
        const double tolerance2 = sqr(14.0 / std::max(zoom_, 0.05));
        int best = -1; double bestDistance = tolerance2;
        for (int i = 0; i < points.size(); ++i) {
            const double d = distanceSquared(world, jsonPoint(points.at(i)));
            if (d <= bestDistance) { best = i; bestDistance = d; }
        }
        return best;
    }

    QString findObjectAt(const QPointF& world) const {
        const double tolerance2 = sqr(18.0 / std::max(zoom_, 0.05));
        const QJsonArray layers = map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray();
        for (int li = layers.size() - 1; li >= 0; --li) {
            const QJsonObject layer = layers.at(li).toObject();
            if (!layer.value(QStringLiteral("visible")).toBool(true)) continue;
            const QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
            for (int oi = objects.size() - 1; oi >= 0; --oi) {
                const QJsonObject object = objects.at(oi).toObject();
                const QString type = object.value(QStringLiteral("type")).toString();
                if (type == QStringLiteral("settlement") || type == QStringLiteral("label")) {
                    const QPointF p(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble());
                    if (distanceSquared(world, p) <= tolerance2) return object.value(QStringLiteral("id")).toString();
                } else {
                    const QJsonArray pts = object.value(QStringLiteral("points")).toArray();
                    for (int i = 1; i < pts.size(); ++i) {
                        if (segmentDistanceSquared(world, jsonPoint(pts.at(i - 1)), jsonPoint(pts.at(i))) <= tolerance2) return object.value(QStringLiteral("id")).toString();
                    }
                }
            }
        }
        return {};
    }

    void updatePointObjectLocal(const QString& id, double x, double y) {
        QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();
        for (int i = 0; i < layers.size(); ++i) {
            QJsonObject layer = layers.at(i).toObject();
            if (layer.value(QStringLiteral("locked")).toBool(false)) continue;
            QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
            for (int j = 0; j < objects.size(); ++j) {
                QJsonObject object = objects.at(j).toObject();
                if (object.value(QStringLiteral("id")).toString() != id) continue;
                object.insert(QStringLiteral("x"), x); object.insert(QStringLiteral("y"), y);
                objects.replace(j, object); layer.insert(QStringLiteral("objects"), objects); layers.replace(i, layer);
                pilin.insert(QStringLiteral("layers"), layers); map_.insert(QStringLiteral("pilinRey"), pilin); return;
            }
        }
    }

    void updatePathPointLocal(const QString& id, int index, double x, double y) {
        QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();
        for (int i = 0; i < layers.size(); ++i) {
            QJsonObject layer = layers.at(i).toObject();
            if (layer.value(QStringLiteral("locked")).toBool(false)) continue;
            QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
            for (int j = 0; j < objects.size(); ++j) {
                QJsonObject object = objects.at(j).toObject();
                if (object.value(QStringLiteral("id")).toString() != id) continue;
                QJsonArray pts = object.value(QStringLiteral("points")).toArray();
                if (index < 0 || index >= pts.size()) return;
                pts.replace(index, pointJson(x, y));
                if (object.value(QStringLiteral("closed")).toBool(false) && pts.size() > 2) {
                    if (index == 0) pts.replace(pts.size() - 1, pointJson(x, y));
                    else if (index == pts.size() - 1) pts.replace(0, pointJson(x, y));
                }
                object.insert(QStringLiteral("points"), pts); objects.replace(j, object); layer.insert(QStringLiteral("objects"), objects); layers.replace(i, layer);
                pilin.insert(QStringLiteral("layers"), layers); map_.insert(QStringLiteral("pilinRey"), pilin); return;
            }
        }
    }

    void setColor(const QColor& c, int alpha = 255) { SDL_SetRenderDrawColor(renderer_, c.red(), c.green(), c.blue(), alpha); }

    void wideLine(const QPointF& aWorld, const QPointF& bWorld, const QColor& color, int alpha, double widthPx) {
        const QPointF a = worldToScreen(aWorld), b = worldToScreen(bWorld);
        const double dx = b.x() - a.x(), dy = b.y() - a.y();
        const double len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.001) return;
        const double nx = -dy / len, ny = dx / len;
        const int half = qMax(0, qRound(widthPx * 0.5));
        setColor(color, alpha);
        for (int o = -half; o <= half; ++o) {
            SDL_RenderLine(renderer_, static_cast<float>(a.x() + nx * o), static_cast<float>(a.y() + ny * o), static_cast<float>(b.x() + nx * o), static_cast<float>(b.y() + ny * o));
        }
    }

    void polyline(const std::vector<QPointF>& pts, const QColor& color, int alpha, double widthPx = 1.0, bool dashed = false) {
        for (size_t i = 1; i < pts.size(); ++i) {
            if (dashed && ((i / 3) % 2 == 1)) continue;
            wideLine(pts[i - 1], pts[i], color, alpha, widthPx);
        }
    }

    void fillPolygon(const std::vector<QPointF>& worldPoints, const QColor& color, int alpha) {
        if (worldPoints.size() < 4) return;
        std::vector<QPointF> pts; pts.reserve(worldPoints.size());
        for (const QPointF& p : worldPoints) pts.push_back(worldToScreen(p));
        double minY = pts.front().y(), maxY = minY;
        for (const QPointF& p : pts) { minY = std::min(minY, p.y()); maxY = std::max(maxY, p.y()); }
        setColor(color, alpha);
        for (int y = qMax(0, static_cast<int>(std::floor(minY))); y <= qMin(height() - 1, static_cast<int>(std::ceil(maxY))); ++y) {
            std::vector<double> xs;
            for (size_t i = 1; i < pts.size(); ++i) {
                const QPointF a = pts[i - 1], b = pts[i];
                if ((a.y() <= y && b.y() > y) || (b.y() <= y && a.y() > y)) xs.push_back(a.x() + (y - a.y()) * (b.x() - a.x()) / (b.y() - a.y()));
            }
            std::sort(xs.begin(), xs.end());
            for (size_t i = 1; i < xs.size(); i += 2) SDL_RenderLine(renderer_, static_cast<float>(xs[i - 1]), static_cast<float>(y), static_cast<float>(xs[i]), static_cast<float>(y));
        }
    }

    void circle(const QPointF& p, double radius, const QColor& color, int alpha, bool filled) {
        const QPointF s = worldToScreen(p);
        const int r = qMax(2, qRound(radius));
        setColor(color, alpha);
        if (filled) {
            for (int y = -r; y <= r; ++y) {
                const int x = qRound(std::sqrt(std::max(0.0, static_cast<double>(r * r - y * y))));
                SDL_RenderLine(renderer_, static_cast<float>(s.x() - x), static_cast<float>(s.y() + y), static_cast<float>(s.x() + x), static_cast<float>(s.y() + y));
            }
        } else {
            QPointF prev(s.x() + r, s.y());
            for (int i = 1; i <= 48; ++i) {
                const double a = 2.0 * kPi * i / 48.0;
                QPointF cur(s.x() + std::cos(a) * r, s.y() + std::sin(a) * r);
                SDL_RenderLine(renderer_, static_cast<float>(prev.x()), static_cast<float>(prev.y()), static_cast<float>(cur.x()), static_cast<float>(cur.y()));
                prev = cur;
            }
        }
    }

    struct Sample { QPointF p; QPointF tangent; };

    std::vector<Sample> samplePath(const std::vector<QPointF>& pts, double spacingWorld) const {
        std::vector<Sample> out;
        if (pts.size() < 2) return out;
        spacingWorld = std::max(2.0, spacingWorld);
        double carry = 0.0;
        for (size_t i = 1; i < pts.size(); ++i) {
            QPointF a = pts[i - 1], b = pts[i];
            QPointF d = b - a;
            const double len = std::sqrt(sqr(d.x()) + sqr(d.y()));
            if (len < 1e-6) continue;
            QPointF tangent(d.x() / len, d.y() / len);
            double pos = spacingWorld - carry;
            while (pos <= len) {
                out.push_back({a + d * (pos / len), tangent});
                pos += spacingWorld;
            }
            carry = std::max(0.0, len - (pos - spacingWorld));
        }
        return out;
    }

    void treeGlyph(const QPointF& p, double size, int alpha) {
        const QPointF s = worldToScreen(p);
        const double r = std::clamp(size * zoom_, 4.0, 16.0);
        setColor(QColor(46, 78, 45), alpha);
        SDL_RenderLine(renderer_, static_cast<float>(s.x()), static_cast<float>(s.y() - r), static_cast<float>(s.x() - r * .60), static_cast<float>(s.y() + r * .05));
        SDL_RenderLine(renderer_, static_cast<float>(s.x()), static_cast<float>(s.y() - r), static_cast<float>(s.x() + r * .60), static_cast<float>(s.y() + r * .05));
        SDL_RenderLine(renderer_, static_cast<float>(s.x() - r * .45), static_cast<float>(s.y() - r * .20), static_cast<float>(s.x() + r * .45), static_cast<float>(s.y() - r * .20));
        SDL_RenderLine(renderer_, static_cast<float>(s.x() - r * .34), static_cast<float>(s.y() + r * .05), static_cast<float>(s.x() + r * .34), static_cast<float>(s.y() + r * .05));
        setColor(QColor(91, 68, 43), alpha);
        SDL_RenderLine(renderer_, static_cast<float>(s.x()), static_cast<float>(s.y() + r * .05), static_cast<float>(s.x()), static_cast<float>(s.y() + r * .62));
    }

    void mountainGlyph(const QPointF& p, double size, int alpha) {
        const QPointF s = worldToScreen(p);
        const double r = std::clamp(size * zoom_, 5.0, 23.0);
        setColor(QColor(67, 61, 56), alpha);
        SDL_RenderLine(renderer_, static_cast<float>(s.x() - r), static_cast<float>(s.y() + r * .55), static_cast<float>(s.x()), static_cast<float>(s.y() - r));
        SDL_RenderLine(renderer_, static_cast<float>(s.x()), static_cast<float>(s.y() - r), static_cast<float>(s.x() + r), static_cast<float>(s.y() + r * .55));
        SDL_RenderLine(renderer_, static_cast<float>(s.x() - r * .38), static_cast<float>(s.y() - r * .10), static_cast<float>(s.x()), static_cast<float>(s.y() + r * .12));
        SDL_RenderLine(renderer_, static_cast<float>(s.x()), static_cast<float>(s.y() + r * .12), static_cast<float>(s.x() + r * .33), static_cast<float>(s.y() - r * .17));
        setColor(QColor(137, 120, 92), qRound(alpha * .7));
        SDL_RenderLine(renderer_, static_cast<float>(s.x()), static_cast<float>(s.y() - r), static_cast<float>(s.x() + r * .20), static_cast<float>(s.y() - r * .42));
    }

    void drawVegetationBrush(const QJsonObject& object, const std::vector<QPointF>& path, bool mountains, int alpha) {
        const double width = object.value(QStringLiteral("brushWidth")).toDouble(mountains ? 180.0 : 240.0);
        const int density = std::clamp(object.value(QStringLiteral("density")).toInt(70), 10, 100);
        const double sizeScale = object.value(QStringLiteral("symbolScale")).toDouble(1.0);
        const quint32 seed = static_cast<quint32>(object.value(QStringLiteral("seed")).toInt(1337));
        const double spacing = (mountains ? 85.0 : 62.0) * (1.25 - density / 130.0);
        const std::vector<Sample> samples = samplePath(path, spacing);
        const int lanes = std::clamp(qRound(width / (mountains ? 95.0 : 80.0)), 1, mountains ? 3 : 5);
        quint32 index = 0;
        for (const Sample& sample : samples) {
            QPointF normal(-sample.tangent.y(), sample.tangent.x());
            for (int lane = 0; lane < lanes; ++lane) {
                const quint32 h = seed + index * 1103515245U + static_cast<quint32>(lane * 31337);
                if (unitNoise(h + 1) * 100.0 > density) continue;
                const double laneCenter = lanes == 1 ? 0.0 : (-0.5 + lane / static_cast<double>(lanes - 1)) * width;
                const double jitter = (unitNoise(h + 2) - 0.5) * width / std::max(2, lanes);
                const double along = (unitNoise(h + 3) - 0.5) * spacing * .7;
                const QPointF p = sample.p + normal * (laneCenter + jitter) + sample.tangent * along;
                const double scale = (0.78 + unitNoise(h + 4) * .52) * sizeScale;
                if (mountains) mountainGlyph(p, 58.0 * scale, alpha);
                else treeGlyph(p, 44.0 * scale, alpha);
            }
            ++index;
        }
    }

    void drawCoastalHachures(const std::vector<QPointF>& path, int alpha) {
        if (path.size() < 8 || zoom_ < 0.18) return;
        QPointF centroid;
        for (const QPointF& p : path) centroid += p;
        centroid /= static_cast<double>(path.size());
        for (size_t i = 4; i + 1 < path.size(); i += 11) {
            const QPointF p = path[i];
            QPointF inward = centroid - p;
            const double len = std::sqrt(sqr(inward.x()) + sqr(inward.y()));
            if (len < 1e-6) continue;
            inward /= len;
            const QPointF a = p + inward * 18.0;
            const QPointF b = p + inward * 48.0;
            wideLine(a, b, QColor(111, 96, 69), qRound(alpha * .38), 1.0);
        }
    }

    void drawText(const QString& text, const QPointF& world, int alpha, bool emphasis) {
        if (text.trimmed().isEmpty()) return;
        const QPointF screen = worldToScreen(world);
        QFont font(QStringLiteral("Georgia"), qBound(9, qRound(13.0 * std::sqrt(zoom_ + 0.15)), 23));
        font.setBold(emphasis);
        QFontMetrics metrics(font);
        const QSize size = metrics.size(Qt::TextSingleLine, text) + QSize(12, 8);
        QImage image(size, QImage::Format_RGBA8888); image.fill(Qt::transparent);
        QPainter painter(&image); painter.setRenderHint(QPainter::TextAntialiasing, true); painter.setFont(font); painter.setPen(QColor(45, 41, 36)); painter.drawText(image.rect().adjusted(6, 4, -6, -4), Qt::AlignVCenter | Qt::AlignLeft, text); painter.end();
        SDL_Texture* texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, image.width(), image.height());
        if (!texture) return;
        SDL_UpdateTexture(texture, nullptr, image.constBits(), image.bytesPerLine()); SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND); SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(alpha));
        SDL_FRect dest{static_cast<float>(screen.x()), static_cast<float>(screen.y()), static_cast<float>(image.width()), static_cast<float>(image.height())};
        SDL_RenderTexture(renderer_, texture, nullptr, &dest); SDL_DestroyTexture(texture);
    }

    void drawPath(const QJsonObject& object, double opacity) {
        const QString type = object.value(QStringLiteral("type")).toString();
        const int alpha = opacityByte(opacity);
        const std::vector<QPointF> path = smoothPoints(object.value(QStringLiteral("points")).toArray());
        if (path.size() < 2) return;

        if (type == QStringLiteral("coast")) {
            if (path.size() >= 4) fillPolygon(path, landColor(), qRound(alpha * .88));
            polyline(path, QColor(44, 42, 37), alpha, zoom_ > .28 ? 2.4 : 1.4);
            polyline(path, QColor(111, 96, 69), qRound(alpha * .45), 1.0);
            drawCoastalHachures(path, alpha);
        } else if (type == QStringLiteral("region")) {
            if (path.size() >= 4) fillPolygon(path, regionColor(), qRound(alpha * .28));
            polyline(path, QColor(124, 92, 56), qRound(alpha * .9), 1.6, true);
        } else if (type == QStringLiteral("river")) {
            const double configured = object.value(QStringLiteral("strokeWidth")).toDouble(4.0);
            for (size_t i = 1; i < path.size(); ++i) {
                const double t = i / static_cast<double>(path.size() - 1);
                const double width = std::clamp((1.0 + t * configured) * zoom_ * 2.2, 1.2, 9.0);
                wideLine(path[i - 1], path[i], QColor(37, 82, 120), alpha, width + 1.3);
                wideLine(path[i - 1], path[i], QColor(96, 151, 190), qRound(alpha * .92), std::max(1.0, width - 1.2));
            }
        } else if (type == QStringLiteral("road")) {
            const double w = std::clamp(object.value(QStringLiteral("strokeWidth")).toDouble(3.0) * zoom_ * 2.0, 1.4, 7.0);
            polyline(path, QColor(75, 54, 37), qRound(alpha * .85), w + 1.2);
            polyline(path, QColor(181, 151, 102), alpha, std::max(1.0, w - 1.0));
        } else if (type == QStringLiteral("border")) {
            const double w = std::clamp(object.value(QStringLiteral("strokeWidth")).toDouble(2.0) * zoom_ * 1.8, 1.0, 5.0);
            polyline(path, QColor(132, 61, 52), alpha, w, true);
        } else if (type == QStringLiteral("forest")) {
            drawVegetationBrush(object, path, false, alpha);
        } else if (type == QStringLiteral("mountain")) {
            drawVegetationBrush(object, path, true, alpha);
        } else {
            polyline(path, objectColor(type), alpha, 1.0);
        }

        if (object.value(QStringLiteral("id")).toString() == selectedObjectId_) {
            const QJsonArray raw = object.value(QStringLiteral("points")).toArray();
            setColor(QColor(31, 116, 205), 255);
            for (int i = 0; i < raw.size(); ++i) {
                if (i == raw.size() - 1 && object.value(QStringLiteral("closed")).toBool(false)) continue;
                const QPointF p = worldToScreen(jsonPoint(raw.at(i)));
                SDL_FRect marker{static_cast<float>(p.x() - 4), static_cast<float>(p.y() - 4), 8.0f, 8.0f}; SDL_RenderFillRect(renderer_, &marker);
            }
        }
    }

    void drawPointObject(const QJsonObject& object, double opacity) {
        const QString type = object.value(QStringLiteral("type")).toString();
        const QPointF p(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble());
        const int alpha = opacityByte(opacity);
        if (type == QStringLiteral("label")) {
            drawText(object.value(QStringLiteral("text")).toString(), p, alpha, false);
        } else {
            const QString kind = object.value(QStringLiteral("kind")).toString();
            const double radius = std::clamp(5.0 * std::sqrt(zoom_ + .2), 3.0, 8.0);
            if (kind.contains(QStringLiteral("Capital"), Qt::CaseInsensitive)) {
                circle(p, radius + 4.0, QColor(47, 40, 34), alpha, false); circle(p, radius, QColor(47, 40, 34), alpha, true);
            } else if (kind.contains(QStringLiteral("Puerto"), Qt::CaseInsensitive)) {
                circle(p, radius + 1.0, QColor(47, 40, 34), alpha, false);
                const QPointF s = worldToScreen(p); setColor(QColor(47, 40, 34), alpha); SDL_RenderLine(renderer_, static_cast<float>(s.x()), static_cast<float>(s.y() - 8), static_cast<float>(s.x()), static_cast<float>(s.y() + 9)); SDL_RenderLine(renderer_, static_cast<float>(s.x() - 5), static_cast<float>(s.y() + 5), static_cast<float>(s.x() + 5), static_cast<float>(s.y() + 5));
            } else if (kind.contains(QStringLiteral("Fortaleza"), Qt::CaseInsensitive)) {
                const QPointF s = worldToScreen(p); setColor(QColor(47, 40, 34), alpha); SDL_FRect rect{static_cast<float>(s.x() - radius), static_cast<float>(s.y() - radius), static_cast<float>(radius * 2), static_cast<float>(radius * 2)}; SDL_RenderRect(renderer_, &rect);
            } else {
                circle(p, radius, QColor(47, 40, 34), alpha, true);
            }
            const QString label = object.value(QStringLiteral("label")).toString();
            if (!label.isEmpty() && zoom_ > .16) drawText(label, p + QPointF(18.0 / zoom_, -10.0 / zoom_), alpha, kind.contains(QStringLiteral("Capital"), Qt::CaseInsensitive));
        }
        if (object.value(QStringLiteral("id")).toString() == selectedObjectId_) {
            const QPointF s = worldToScreen(p); setColor(QColor(31, 116, 205), 255); SDL_FRect r{static_cast<float>(s.x() - 11), static_cast<float>(s.y() - 11), 22.0f, 22.0f}; SDL_RenderRect(renderer_, &r);
        }
    }

    void updateTemplateTexture() {
        if (!renderer_ || !templateDirty_) return;
        templateDirty_ = false;
        if (templateTexture_) { SDL_DestroyTexture(templateTexture_); templateTexture_ = nullptr; }
        const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        const QJsonObject templ = pilin.value(QStringLiteral("template")).toObject();
        QString dataUrl = templ.value(QStringLiteral("dataUrl")).toString();
        if (dataUrl.isEmpty()) dataUrl = map_.value(QStringLiteral("backgroundImageDataUrl")).toString();
        if (dataUrl.isEmpty()) return;
        QImage image; if (!image.loadFromData(dataUrlBytes(dataUrl))) return; image = image.convertToFormat(QImage::Format_RGBA8888);
        templateTexture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, image.width(), image.height());
        if (!templateTexture_) return;
        SDL_UpdateTexture(templateTexture_, nullptr, image.constBits(), image.bytesPerLine()); SDL_SetTextureBlendMode(templateTexture_, SDL_BLENDMODE_BLEND); SDL_SetTextureScaleMode(templateTexture_, SDL_SCALEMODE_LINEAR);
    }

    void renderFrame() {
        if (!isVisible() || !ensureRenderer()) return;
        setColor(paperColor(), 255); SDL_RenderClear(renderer_);
        const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        const double docW = pilin.value(QStringLiteral("width")).toDouble(4096.0), docH = pilin.value(QStringLiteral("height")).toDouble(2304.0);
        const QPointF tl = worldToScreen(0.0, 0.0), br = worldToScreen(docW, docH);
        setColor(QColor(96, 81, 61), 65); SDL_FRect boundary{static_cast<float>(tl.x()), static_cast<float>(tl.y()), static_cast<float>(br.x() - tl.x()), static_cast<float>(br.y() - tl.y())}; SDL_RenderRect(renderer_, &boundary);

        if (snap_ && zoom_ > .25) {
            setColor(QColor(108, 96, 74), 20);
            for (double x = 0; x <= docW; x += 100.0) { const QPointF a = worldToScreen(x, 0), b = worldToScreen(x, docH); SDL_RenderLine(renderer_, static_cast<float>(a.x()), static_cast<float>(a.y()), static_cast<float>(b.x()), static_cast<float>(b.y())); }
            for (double y = 0; y <= docH; y += 100.0) { const QPointF a = worldToScreen(0, y), b = worldToScreen(docW, y); SDL_RenderLine(renderer_, static_cast<float>(a.x()), static_cast<float>(a.y()), static_cast<float>(b.x()), static_cast<float>(b.y())); }
        }

        updateTemplateTexture();
        const QJsonObject templ = pilin.value(QStringLiteral("template")).toObject();
        if (templateTexture_ && templ.value(QStringLiteral("visible")).toBool(true)) {
            const QPointF p = worldToScreen(templ.value(QStringLiteral("x")).toDouble(), templ.value(QStringLiteral("y")).toDouble());
            const double scale = std::clamp(templ.value(QStringLiteral("scale")).toDouble(1.0), .05, 10.0);
            SDL_FRect dest{static_cast<float>(p.x()), static_cast<float>(p.y()), static_cast<float>(docW * zoom_ * scale), static_cast<float>(docH * zoom_ * scale)};
            SDL_SetTextureAlphaMod(templateTexture_, static_cast<Uint8>(opacityByte(templ.value(QStringLiteral("opacity")).toDouble(.35))));
            SDL_RenderTextureRotated(renderer_, templateTexture_, nullptr, &dest, templ.value(QStringLiteral("rotation")).toDouble(), nullptr, SDL_FLIP_NONE);
        }

        for (const QJsonValue& lv : pilin.value(QStringLiteral("layers")).toArray()) {
            const QJsonObject layer = lv.toObject();
            if (!layer.value(QStringLiteral("visible")).toBool(true)) continue;
            const double opacity = layer.value(QStringLiteral("opacity")).toDouble(1.0);
            for (const QJsonValue& ov : layer.value(QStringLiteral("objects")).toArray()) {
                const QJsonObject object = ov.toObject();
                const QString type = object.value(QStringLiteral("type")).toString();
                if (type == QStringLiteral("settlement") || type == QStringLiteral("label")) drawPointObject(object, opacity); else drawPath(object, opacity);
            }
        }

        if (drawing_ && currentPoints_.size() >= 2) {
            QJsonObject preview{{QStringLiteral("type"), toolType(tool_)}, {QStringLiteral("points"), currentPoints_}, {QStringLiteral("brushWidth"), brushWidth_}, {QStringLiteral("density"), density_}, {QStringLiteral("symbolScale"), symbolSize_ / 100.0}, {QStringLiteral("strokeWidth"), strokeWidth_}, {QStringLiteral("seed"), 1337}};
            if (closesAutomatically(tool_)) preview.insert(QStringLiteral("closed"), true);
            drawPath(preview, 1.0);
        }

        if (measureActive_) {
            const QPointF a = worldToScreen(measureStart_), b = worldToScreen(measureEnd_); setColor(QColor(31, 116, 205), 220); SDL_RenderLine(renderer_, static_cast<float>(a.x()), static_cast<float>(a.y()), static_cast<float>(b.x()), static_cast<float>(b.y()));
        }
        SDL_RenderPresent(renderer_);
    }

    QJsonObject map_;
    Tool tool_ = Tool::Select;
    bool panning_ = false, drawing_ = false, viewInitialized_ = false, snap_ = false, measureActive_ = false, templateDirty_ = true;
    QPointF measureStart_, measureEnd_, lastMouse_, pan_{24.0, 24.0};
    double zoom_ = .2;
    int brushWidth_ = 220, density_ = 72, symbolSize_ = 100, strokeWidth_ = 4;
    QJsonArray currentPoints_;
    QString selectedObjectId_, draggingObjectId_, draggingPathId_;
    int draggingPathPointIndex_ = -1;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* templateTexture_ = nullptr;
};

PilinReyEditor::PilinReyEditor(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("pilinReyEditor"));
    buildUi();
}

PilinReyEditor::~PilinReyEditor() = default;

void PilinReyEditor::buildUi() {
    auto* root = new QVBoxLayout(this); root->setContentsMargins(0, 0, 0, 0); root->setSpacing(0);
    canvasHost_ = new QWidget(this); canvasHost_->setObjectName(QStringLiteral("pilinCanvasHost"));
    auto* canvasLayout = new QVBoxLayout(canvasHost_); canvasLayout->setContentsMargins(0, 0, 0, 0);
    viewport_ = new PilinReyViewport(canvasHost_); canvasLayout->addWidget(viewport_); root->addWidget(canvasHost_, 1);

    toolRail_ = new QWidget(canvasHost_); toolRail_->setObjectName(QStringLiteral("pilinToolRail"));
    auto* rail = new QVBoxLayout(toolRail_); rail->setContentsMargins(4, 4, 4, 4); rail->setSpacing(2);
    struct ToolDef { Tool tool; const char* symbol; const char* label; };
    const ToolDef defs[] = {
        {Tool::Select,"↖","Seleccionar / nodos"},{Tool::Pan,"✥","Mover lienzo"},{Tool::Coast,"◒","Masa de tierra"},{Tool::Region,"◇","Región"},{Tool::River,"∿","Río"},{Tool::Road,"━","Camino"},{Tool::Border,"┄","Frontera"},{Tool::Forest,"♣","Bosque de área"},{Tool::Mountain,"△","Cordillera"},{Tool::Settlement,"●","Asentamiento"},{Tool::Label,"T","Etiqueta"},{Tool::Eraser,"⌫","Borrar objeto"},{Tool::Measure,"↔","Medir"}
    };
    for (const ToolDef& def : defs) {
        auto* b = iconButton(QString::fromUtf8(def.symbol), tr(def.label), toolRail_, QStringLiteral("pilinMapTool")); b->setCheckable(true); b->setAutoExclusive(true); if (def.tool == Tool::Select) b->setChecked(true);
        connect(b, &QToolButton::clicked, this, [this, tool = def.tool]() { setActiveTool(tool); }); toolButtons_.append(b); rail->addWidget(b);
    }

    topCommands_ = new QFrame(canvasHost_); topCommands_->setObjectName(QStringLiteral("pilinTopCommands"));
    auto* commands = new QHBoxLayout(topCommands_); commands->setContentsMargins(4,4,4,4); commands->setSpacing(2);
    layersButton_ = iconButton(QStringLiteral("▱"), tr("Capas"), topCommands_);
    templateButton_ = iconButton(QStringLiteral("▧"), tr("Plantilla"), topCommands_);
    auto* fit = iconButton(QStringLiteral("⌗"), tr("Encajar"), topCommands_);
    undoButton_ = iconButton(QStringLiteral("↶"), tr("Deshacer"), topCommands_);
    redoButton_ = iconButton(QStringLiteral("↷"), tr("Rehacer"), topCommands_);
    auto* exportButton = iconButton(QStringLiteral("⇩"), tr("Exportar"), topCommands_);
    snapCheck_ = new QCheckBox(tr("Ajustar"), topCommands_); snapCheck_->setChecked(false);
    for (QWidget* w : {static_cast<QWidget*>(layersButton_), static_cast<QWidget*>(templateButton_), static_cast<QWidget*>(fit), static_cast<QWidget*>(undoButton_), static_cast<QWidget*>(redoButton_), static_cast<QWidget*>(exportButton)}) commands->addWidget(w);
    commands->addSpacing(4); commands->addWidget(snapCheck_);

    auto* toolSettings = new QFrame(canvasHost_); toolSettings->setObjectName(QStringLiteral("pilinToolSettings")); toolSettings->setFixedWidth(210);
    auto* settingsLayout = new QVBoxLayout(toolSettings); settingsLayout->setContentsMargins(9,8,9,8); settingsLayout->setSpacing(4);
    auto* toolSettingsTitle = new QLabel(tr("Herramienta"), toolSettings); toolSettingsTitle->setObjectName(QStringLiteral("pilinPopoverTitle"));
    auto* widthLabel = new QLabel(tr("Anchura de pincel"), toolSettings); auto* widthSlider = new QSlider(Qt::Horizontal, toolSettings); widthSlider->setRange(30, 600); widthSlider->setValue(220);
    auto* densityLabel = new QLabel(tr("Densidad"), toolSettings); auto* densitySlider = new QSlider(Qt::Horizontal, toolSettings); densitySlider->setRange(10, 100); densitySlider->setValue(72);
    auto* sizeLabel = new QLabel(tr("Tamaño de símbolos"), toolSettings); auto* sizeSlider = new QSlider(Qt::Horizontal, toolSettings); sizeSlider->setRange(50, 180); sizeSlider->setValue(100);
    auto* strokeLabel = new QLabel(tr("Grosor de línea"), toolSettings); auto* strokeSlider = new QSlider(Qt::Horizontal, toolSettings); strokeSlider->setRange(1, 12); strokeSlider->setValue(4);
    settingsLayout->addWidget(toolSettingsTitle); settingsLayout->addWidget(widthLabel); settingsLayout->addWidget(widthSlider); settingsLayout->addWidget(densityLabel); settingsLayout->addWidget(densitySlider); settingsLayout->addWidget(sizeLabel); settingsLayout->addWidget(sizeSlider); settingsLayout->addWidget(strokeLabel); settingsLayout->addWidget(strokeSlider); toolSettings->hide();

    layersPopover_ = new QFrame(canvasHost_); layersPopover_->setObjectName(QStringLiteral("pilinPopover")); layersPopover_->setFixedWidth(280);
    auto* layerLayout = new QVBoxLayout(layersPopover_); layerLayout->setContentsMargins(10,10,10,10); layerLayout->setSpacing(6);
    auto* layerTitle = new QLabel(tr("Capas"), layersPopover_); layerTitle->setObjectName(QStringLiteral("pilinPopoverTitle"));
    layers_ = new QListWidget(layersPopover_); layers_->setMinimumHeight(190);
    auto* layerButtons = new QHBoxLayout; auto* add = iconButton(QStringLiteral("+"), tr("Nueva capa"), layersPopover_); auto* duplicate = iconButton(QStringLiteral("⧉"), tr("Duplicar capa"), layersPopover_); auto* remove = iconButton(QStringLiteral("−"), tr("Eliminar capa"), layersPopover_); auto* up = iconButton(QStringLiteral("↑"), tr("Subir"), layersPopover_); auto* down = iconButton(QStringLiteral("↓"), tr("Bajar"), layersPopover_);
    layerButtons->addWidget(add); layerButtons->addWidget(duplicate); layerButtons->addWidget(remove); layerButtons->addStretch(); layerButtons->addWidget(up); layerButtons->addWidget(down);
    layerLocked_ = new QCheckBox(tr("Bloquear capa"), layersPopover_); layerOpacity_ = new QSlider(Qt::Horizontal, layersPopover_); layerOpacity_->setRange(0,100); layerOpacity_->setValue(100);
    layerLayout->addWidget(layerTitle); layerLayout->addWidget(layers_); layerLayout->addLayout(layerButtons); layerLayout->addWidget(layerLocked_); layerLayout->addWidget(new QLabel(tr("Opacidad"), layersPopover_)); layerLayout->addWidget(layerOpacity_); layersPopover_->hide();

    templatePopover_ = new QFrame(canvasHost_); templatePopover_->setObjectName(QStringLiteral("pilinPopover")); templatePopover_->setFixedWidth(280);
    auto* templateLayout = new QVBoxLayout(templatePopover_); templateLayout->setContentsMargins(10,10,10,10); templateLayout->setSpacing(6);
    auto* templateTitle = new QLabel(tr("Plantilla"), templatePopover_); templateTitle->setObjectName(QStringLiteral("pilinPopoverTitle"));
    auto* templateButtons = new QHBoxLayout; auto* loadTemplate = iconButton(QStringLiteral("＋"), tr("Cargar"), templatePopover_); auto* clearTemplateButton = iconButton(QStringLiteral("×"), tr("Quitar"), templatePopover_); auto* rotateLeft = iconButton(QStringLiteral("↶"), tr("Girar izquierda"), templatePopover_); auto* rotateRight = iconButton(QStringLiteral("↷"), tr("Girar derecha"), templatePopover_); auto* scaleDown = iconButton(QStringLiteral("−"), tr("Reducir"), templatePopover_); auto* scaleUp = iconButton(QStringLiteral("+"), tr("Ampliar"), templatePopover_);
    templateButtons->addWidget(loadTemplate); templateButtons->addWidget(clearTemplateButton); templateButtons->addStretch(); templateButtons->addWidget(rotateLeft); templateButtons->addWidget(rotateRight); templateButtons->addWidget(scaleDown); templateButtons->addWidget(scaleUp);
    templateOpacity_ = new QSlider(Qt::Horizontal, templatePopover_); templateOpacity_->setRange(0,100); templateOpacity_->setValue(35);
    templateLayout->addWidget(templateTitle); templateLayout->addLayout(templateButtons); templateLayout->addWidget(new QLabel(tr("Opacidad"), templatePopover_)); templateLayout->addWidget(templateOpacity_); templatePopover_->hide();

    selectionPopover_ = new QFrame(canvasHost_); selectionPopover_->setObjectName(QStringLiteral("pilinSelectionPopover")); selectionPopover_->setFixedWidth(220);
    auto* selectionLayout = new QVBoxLayout(selectionPopover_); selectionLayout->setContentsMargins(9,8,9,8); selectionLayout->setSpacing(5);
    selectionLabel_ = new QLabel(tr("Selección"), selectionPopover_); selectionLabel_->setObjectName(QStringLiteral("pilinPopoverTitle"));
    auto* actionRow = new QHBoxLayout; editObjectButton_ = iconButton(QStringLiteral("✎"), tr("Editar"), selectionPopover_); duplicateObjectButton_ = iconButton(QStringLiteral("⧉"), tr("Duplicar"), selectionPopover_); linkAtlasButton_ = iconButton(QStringLiteral("⌁"), tr("Enlazar con Atlas"), selectionPopover_); deleteObjectButton_ = iconButton(QStringLiteral("×"), tr("Eliminar"), selectionPopover_);
    actionRow->addWidget(editObjectButton_); actionRow->addWidget(duplicateObjectButton_); actionRow->addWidget(linkAtlasButton_); actionRow->addStretch(); actionRow->addWidget(deleteObjectButton_); selectionLayout->addWidget(selectionLabel_); selectionLayout->addLayout(actionRow); selectionPopover_->hide();

    status_ = new QLabel(tr("Pilin Rey · listo"), canvasHost_); status_->setObjectName(QStringLiteral("pilinStatus")); status_->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    viewport_->onPath = [this](const QString& type, const QJsonArray& points){ addPathObject(type, points); };
    viewport_->onSettlement = [this](double x,double y){ addSettlement(x,y); };
    viewport_->onLabel = [this](double x,double y){ addLabel(x,y); };
    viewport_->onSelected = [this](const QString& id){ selectObject(id); };
    viewport_->onMovePoint = [this](const QString& id,double x,double y){ movePointObject(id,x,y); };
    viewport_->onMovePathPoint = [this](const QString& id,int i,double x,double y){ movePathPoint(id,i,x,y); };
    viewport_->onDeleteSelection = [this](){ deleteSelectedObject(); };
    viewport_->onStatus = [this](const QString& text){ status_->setText(text); };

    connect(widthSlider, &QSlider::valueChanged, viewport_, &PilinReyViewport::setBrushWidth);
    connect(densitySlider, &QSlider::valueChanged, viewport_, &PilinReyViewport::setDensity);
    connect(sizeSlider, &QSlider::valueChanged, viewport_, &PilinReyViewport::setSymbolSize);
    connect(strokeSlider, &QSlider::valueChanged, viewport_, &PilinReyViewport::setStrokeWidth);

    connect(layersButton_, &QToolButton::clicked, this, [this](){ templatePopover_->hide(); layersPopover_->setVisible(!layersPopover_->isVisible()); layoutFloatingPanels(); });
    connect(templateButton_, &QToolButton::clicked, this, [this](){ layersPopover_->hide(); templatePopover_->setVisible(!templatePopover_->isVisible()); layoutFloatingPanels(); });
    connect(fit, &QToolButton::clicked, viewport_, &PilinReyViewport::fitDocument); connect(undoButton_, &QToolButton::clicked, this, &PilinReyEditor::undo); connect(redoButton_, &QToolButton::clicked, this, &PilinReyEditor::redo); connect(exportButton, &QToolButton::clicked, this, &PilinReyEditor::exportMap); connect(snapCheck_, &QCheckBox::toggled, viewport_, &PilinReyViewport::setSnap);
    connect(loadTemplate, &QToolButton::clicked, this, &PilinReyEditor::chooseTemplate); connect(clearTemplateButton, &QToolButton::clicked, this, &PilinReyEditor::clearTemplate); connect(rotateLeft, &QToolButton::clicked, this, [this](){rotateTemplate(-15);}); connect(rotateRight, &QToolButton::clicked, this, [this](){rotateTemplate(15);}); connect(scaleDown, &QToolButton::clicked, this, [this](){scaleTemplate(.9);}); connect(scaleUp, &QToolButton::clicked, this, [this](){scaleTemplate(1.1);});
    connect(templateOpacity_, &QSlider::valueChanged, this, [this](int value){ if(refreshing_)return; mutateDocument([&](QJsonObject& pilin){QJsonObject t=pilin.value(QStringLiteral("template")).toObject();t.insert(QStringLiteral("opacity"),value/100.0);pilin.insert(QStringLiteral("template"),t);}); });
    connect(add,&QToolButton::clicked,this,&PilinReyEditor::addLayer); connect(duplicate,&QToolButton::clicked,this,&PilinReyEditor::duplicateLayer); connect(remove,&QToolButton::clicked,this,&PilinReyEditor::removeLayer); connect(up,&QToolButton::clicked,this,[this](){moveLayer(-1);}); connect(down,&QToolButton::clicked,this,[this](){moveLayer(1);});
    connect(layerLocked_,&QCheckBox::toggled,this,[this](bool v){if(!refreshing_)setLayerLocked(v);}); connect(layerOpacity_,&QSlider::valueChanged,this,[this](int v){if(!refreshing_)setLayerOpacity(v);});
    connect(layers_,&QListWidget::currentRowChanged,this,[this](int row){if(refreshing_||row<0)return;QJsonObject p=map_.value(QStringLiteral("pilinRey")).toObject();QJsonArray a=p.value(QStringLiteral("layers")).toArray();if(row>=a.size())return;p.insert(QStringLiteral("activeLayerId"),a.at(row).toObject().value(QStringLiteral("id")).toString());map_.insert(QStringLiteral("pilinRey"),p);refreshLayerControls();persistToArchive();});
    connect(layers_,&QListWidget::itemChanged,this,[this](QListWidgetItem* item){if(refreshing_)return;const QString id=item->data(Qt::UserRole).toString();mutateDocument([&](QJsonObject& p){QJsonArray a=p.value(QStringLiteral("layers")).toArray();for(int i=0;i<a.size();++i){QJsonObject l=a.at(i).toObject();if(l.value(QStringLiteral("id")).toString()!=id)continue;l.insert(QStringLiteral("name"),item->text());l.insert(QStringLiteral("visible"),item->checkState()==Qt::Checked);a.replace(i,l);break;}p.insert(QStringLiteral("layers"),a);});});
    connect(editObjectButton_,&QToolButton::clicked,this,&PilinReyEditor::editSelectedObject); connect(duplicateObjectButton_,&QToolButton::clicked,this,&PilinReyEditor::duplicateSelectedObject); connect(linkAtlasButton_,&QToolButton::clicked,this,&PilinReyEditor::linkSelectedToAtlas); connect(deleteObjectButton_,&QToolButton::clicked,this,&PilinReyEditor::deleteSelectedObject);

    auto* undoShortcut=new QShortcut(QKeySequence::Undo,this); auto* redoShortcut=new QShortcut(QKeySequence::Redo,this); auto* copyShortcut=new QShortcut(QKeySequence::Copy,this); auto* pasteShortcut=new QShortcut(QKeySequence::Paste,this); connect(undoShortcut,&QShortcut::activated,this,&PilinReyEditor::undo); connect(redoShortcut,&QShortcut::activated,this,&PilinReyEditor::redo); connect(copyShortcut,&QShortcut::activated,this,&PilinReyEditor::copySelectedObject); connect(pasteShortcut,&QShortcut::activated,this,&PilinReyEditor::pasteCopiedObject);

    setStyleSheet(QStringLiteral(
        "#pilinReyEditor,#pilinCanvasHost{background:transparent;}"
        "#pilinToolRail,#pilinTopCommands,#pilinPopover,#pilinSelectionPopover,#pilinToolSettings{background:palette(base);color:palette(text);border:1px solid palette(mid);border-radius:5px;}"
        "QToolButton#pilinMapTool,QToolButton#pilinCommand{background:transparent;color:palette(text);border:0;border-radius:4px;padding:0;font-size:12pt;}"
        "QToolButton#pilinMapTool:hover,QToolButton#pilinCommand:hover{background:palette(alternate-base);}"
        "QToolButton#pilinMapTool:checked{background:#1769c2;color:white;}"
        "#pilinPopoverTitle{font-weight:700;}"
        "#pilinStatus{background:rgba(20,24,29,.74);color:white;border-radius:4px;padding:4px 7px;}"
    ));

    refreshSelectionControls();
}

void PilinReyEditor::resizeEvent(QResizeEvent* e){QWidget::resizeEvent(e);layoutFloatingPanels();}
void PilinReyEditor::showEvent(QShowEvent* e){QWidget::showEvent(e);layoutFloatingPanels();}

void PilinReyEditor::layoutFloatingPanels(){
    if(!canvasHost_)return;const int m=10;
    if(toolRail_){toolRail_->adjustSize();toolRail_->move(m,m);toolRail_->raise();}
    if(topCommands_){topCommands_->adjustSize();topCommands_->move(qMax(m,canvasHost_->width()-topCommands_->width()-m),m);topCommands_->raise();}
    if(auto* settings=canvasHost_->findChild<QFrame*>(QStringLiteral("pilinToolSettings"));settings&&settings->isVisible()){settings->adjustSize();settings->move(m+(toolRail_?toolRail_->width():38)+8,m);settings->raise();}
    int y=m+(topCommands_?topCommands_->height():0)+8;for(QFrame* p:{layersPopover_,templatePopover_}){if(!p||!p->isVisible())continue;p->adjustSize();p->move(qMax(m,canvasHost_->width()-p->width()-m),y);p->raise();y+=p->height()+8;}
    if(selectionPopover_&&selectionPopover_->isVisible()){selectionPopover_->adjustSize();selectionPopover_->move(qMax(m,canvasHost_->width()-selectionPopover_->width()-m),qMax(m,canvasHost_->height()-selectionPopover_->height()-38));selectionPopover_->raise();}
    if(status_){status_->adjustSize();status_->move(m,qMax(m,canvasHost_->height()-status_->height()-m));status_->raise();}
}

void PilinReyEditor::setMap(const QJsonObject& map){map_=map;ensurePilinDocument();undoStack_.clear();redoStack_.clear();selectedObjectId_.clear();refreshing_=true;const QJsonObject p=map_.value(QStringLiteral("pilinRey")).toObject();templateOpacity_->setValue(qRound(p.value(QStringLiteral("template")).toObject().value(QStringLiteral("opacity")).toDouble(.35)*100));refreshing_=false;refreshLayers();refreshSelectionControls();refreshViewport();}

void PilinReyEditor::ensurePilinDocument(){QJsonObject p=map_.value(QStringLiteral("pilinRey")).toObject();if(!p.contains(QStringLiteral("version")))p.insert(QStringLiteral("version"),5);if(!p.contains(QStringLiteral("width")))p.insert(QStringLiteral("width"),4096);if(!p.contains(QStringLiteral("height")))p.insert(QStringLiteral("height"),2304);QJsonObject t=p.value(QStringLiteral("template")).toObject();if(!t.contains(QStringLiteral("dataUrl")))t.insert(QStringLiteral("dataUrl"),map_.value(QStringLiteral("backgroundImageDataUrl")).toString());if(!t.contains(QStringLiteral("opacity")))t.insert(QStringLiteral("opacity"),.35);if(!t.contains(QStringLiteral("visible")))t.insert(QStringLiteral("visible"),true);if(!t.contains(QStringLiteral("locked")))t.insert(QStringLiteral("locked"),true);if(!t.contains(QStringLiteral("x")))t.insert(QStringLiteral("x"),0.0);if(!t.contains(QStringLiteral("y")))t.insert(QStringLiteral("y"),0.0);if(!t.contains(QStringLiteral("scale")))t.insert(QStringLiteral("scale"),1.0);if(!t.contains(QStringLiteral("rotation")))t.insert(QStringLiteral("rotation"),0.0);p.insert(QStringLiteral("template"),t);QJsonArray layers=p.value(QStringLiteral("layers")).toArray();if(layers.isEmpty()){for(const QString& name:{tr("Costa y regiones"),tr("Hidrografía"),tr("Relieve"),tr("Vegetación"),tr("Fronteras"),tr("Caminos"),tr("Asentamientos"),tr("Etiquetas")})layers.append(QJsonObject{{QStringLiteral("id"),uid(QStringLiteral("layer"))},{QStringLiteral("name"),name},{QStringLiteral("visible"),true},{QStringLiteral("locked"),false},{QStringLiteral("opacity"),1.0},{QStringLiteral("objects"),QJsonArray()}});p.insert(QStringLiteral("activeLayerId"),layers.first().toObject().value(QStringLiteral("id")).toString());}p.insert(QStringLiteral("layers"),layers);map_.insert(QStringLiteral("pilinRey"),p);}

void PilinReyEditor::refreshLayers(){refreshing_=true;layers_->clear();const QJsonObject p=map_.value(QStringLiteral("pilinRey")).toObject();const QString active=p.value(QStringLiteral("activeLayerId")).toString();const QJsonArray a=p.value(QStringLiteral("layers")).toArray();int activeRow=0;for(int i=0;i<a.size();++i){const QJsonObject l=a.at(i).toObject();auto* item=new QListWidgetItem(l.value(QStringLiteral("name")).toString(tr("Capa")));item->setData(Qt::UserRole,l.value(QStringLiteral("id")).toString());item->setFlags(item->flags()|Qt::ItemIsUserCheckable|Qt::ItemIsEditable);item->setCheckState(l.value(QStringLiteral("visible")).toBool(true)?Qt::Checked:Qt::Unchecked);layers_->addItem(item);if(l.value(QStringLiteral("id")).toString()==active)activeRow=i;}if(layers_->count())layers_->setCurrentRow(activeRow);refreshing_=false;refreshLayerControls();undoButton_->setEnabled(!undoStack_.isEmpty());redoButton_->setEnabled(!redoStack_.isEmpty());}
void PilinReyEditor::refreshLayerControls(){const bool old=refreshing_;refreshing_=true;bool found=false;for(const QJsonValue& v:map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray()){const QJsonObject l=v.toObject();if(l.value(QStringLiteral("id")).toString()!=activeLayerId())continue;layerLocked_->setChecked(l.value(QStringLiteral("locked")).toBool(false));layerOpacity_->setValue(qRound(l.value(QStringLiteral("opacity")).toDouble(1)*100));found=true;break;}layerLocked_->setEnabled(found);layerOpacity_->setEnabled(found);refreshing_=old;}
void PilinReyEditor::refreshSelectionControls(){const QJsonObject o=selectedObject();const bool has=!o.isEmpty();selectionPopover_->setVisible(has);editObjectButton_->setEnabled(has);duplicateObjectButton_->setEnabled(has);deleteObjectButton_->setEnabled(has);linkAtlasButton_->setEnabled(has&&o.value(QStringLiteral("type")).toString()==QStringLiteral("settlement"));if(has){QString label=o.value(QStringLiteral("label")).toString();if(label.isEmpty())label=o.value(QStringLiteral("text")).toString();if(label.isEmpty())label=o.value(QStringLiteral("type")).toString();selectionLabel_->setText(tr("Selección · %1").arg(label));}layoutFloatingPanels();}
void PilinReyEditor::refreshViewport(){if(viewport_){viewport_->setDocument(map_);viewport_->setSelection(selectedObjectId_);}}

void PilinReyEditor::chooseTemplate(){const QString path=QFileDialog::getOpenFileName(this,tr("Plantilla"),QString(),tr("Imágenes (*.png *.jpg *.jpeg *.webp)"));if(path.isEmpty())return;const QString data=imageToDataUrl(path);if(data.isEmpty())return;mutateDocument([&](QJsonObject& p){QJsonObject t=p.value(QStringLiteral("template")).toObject();t.insert(QStringLiteral("dataUrl"),data);t.insert(QStringLiteral("visible"),true);p.insert(QStringLiteral("template"),t);});map_.insert(QStringLiteral("backgroundImageDataUrl"),data);persistToArchive();}
void PilinReyEditor::clearTemplate(){mutateDocument([](QJsonObject& p){QJsonObject t=p.value(QStringLiteral("template")).toObject();t.insert(QStringLiteral("dataUrl"),QString());p.insert(QStringLiteral("template"),t);});map_.remove(QStringLiteral("backgroundImageDataUrl"));persistToArchive();}
void PilinReyEditor::rotateTemplate(double d){mutateDocument([&](QJsonObject& p){QJsonObject t=p.value(QStringLiteral("template")).toObject();t.insert(QStringLiteral("rotation"),t.value(QStringLiteral("rotation")).toDouble()+d);p.insert(QStringLiteral("template"),t);});}
void PilinReyEditor::scaleTemplate(double f){mutateDocument([&](QJsonObject& p){QJsonObject t=p.value(QStringLiteral("template")).toObject();t.insert(QStringLiteral("scale"),std::clamp(t.value(QStringLiteral("scale")).toDouble(1)*f,.05,10.0));p.insert(QStringLiteral("template"),t);});}

void PilinReyEditor::addLayer(){bool ok=false;const QString name=QInputDialog::getText(this,tr("Nueva capa"),tr("Nombre:"),QLineEdit::Normal,tr("Nueva capa"),&ok).trimmed();if(!ok||name.isEmpty())return;mutateDocument([&](QJsonObject& p){QJsonArray a=p.value(QStringLiteral("layers")).toArray();const QString id=uid(QStringLiteral("layer"));a.append(QJsonObject{{QStringLiteral("id"),id},{QStringLiteral("name"),name},{QStringLiteral("visible"),true},{QStringLiteral("locked"),false},{QStringLiteral("opacity"),1.0},{QStringLiteral("objects"),QJsonArray()}});p.insert(QStringLiteral("layers"),a);p.insert(QStringLiteral("activeLayerId"),id);});}
void PilinReyEditor::duplicateLayer(){const int row=layers_->currentRow();if(row<0)return;mutateDocument([&](QJsonObject& p){QJsonArray a=p.value(QStringLiteral("layers")).toArray();if(row>=a.size())return;QJsonObject c=a.at(row).toObject();const QString id=uid(QStringLiteral("layer"));c.insert(QStringLiteral("id"),id);c.insert(QStringLiteral("name"),c.value(QStringLiteral("name")).toString()+tr(" copia"));QJsonArray objects=c.value(QStringLiteral("objects")).toArray();for(int i=0;i<objects.size();++i){QJsonObject o=objects.at(i).toObject();o.insert(QStringLiteral("id"),uid(QStringLiteral("mapobj")));objects.replace(i,o);}c.insert(QStringLiteral("objects"),objects);a.insert(row+1,c);p.insert(QStringLiteral("layers"),a);p.insert(QStringLiteral("activeLayerId"),id);});}
void PilinReyEditor::removeLayer(){const int row=layers_->currentRow();if(row<0)return;mutateDocument([&](QJsonObject& p){QJsonArray a=p.value(QStringLiteral("layers")).toArray();if(a.size()<=1||row>=a.size())return;a.removeAt(row);p.insert(QStringLiteral("layers"),a);p.insert(QStringLiteral("activeLayerId"),a.at(qMin(row,a.size()-1)).toObject().value(QStringLiteral("id")).toString());});selectedObjectId_.clear();}
void PilinReyEditor::moveLayer(int d){const int row=layers_->currentRow(),target=row+d;if(row<0)return;mutateDocument([&](QJsonObject& p){QJsonArray a=p.value(QStringLiteral("layers")).toArray();if(target<0||target>=a.size())return;QJsonValue v=a.at(row);a.removeAt(row);a.insert(target,v);p.insert(QStringLiteral("layers"),a);});if(target>=0&&target<layers_->count())layers_->setCurrentRow(target);}
void PilinReyEditor::setLayerLocked(bool v){const QString active=activeLayerId();mutateDocument([&](QJsonObject& p){QJsonArray a=p.value(QStringLiteral("layers")).toArray();for(int i=0;i<a.size();++i){QJsonObject l=a.at(i).toObject();if(l.value(QStringLiteral("id")).toString()!=active)continue;l.insert(QStringLiteral("locked"),v);a.replace(i,l);break;}p.insert(QStringLiteral("layers"),a);});}
void PilinReyEditor::setLayerOpacity(int v){const QString active=activeLayerId();mutateDocument([&](QJsonObject& p){QJsonArray a=p.value(QStringLiteral("layers")).toArray();for(int i=0;i<a.size();++i){QJsonObject l=a.at(i).toObject();if(l.value(QStringLiteral("id")).toString()!=active)continue;l.insert(QStringLiteral("opacity"),std::clamp(v/100.0,0.0,1.0));a.replace(i,l);break;}p.insert(QStringLiteral("layers"),a);});}

void PilinReyEditor::setActiveTool(Tool tool){viewport_->setTool(tool);if(auto* settings=canvasHost_->findChild<QFrame*>(QStringLiteral("pilinToolSettings"))){const bool show=tool==Tool::Forest||tool==Tool::Mountain||tool==Tool::River||tool==Tool::Road||tool==Tool::Border;settings->setVisible(show);layoutFloatingPanels();}}
QString PilinReyEditor::activeLayerId()const{return map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("activeLayerId")).toString();}

void PilinReyEditor::addPathObject(const QString& type,const QJsonArray& points){if(type.isEmpty()||points.size()<2)return;const QString active=activeLayerId();const int brush=viewport_->brushWidth(),density=viewport_->density(),size=viewport_->symbolSize(),stroke=viewport_->strokeWidth();const int seed=static_cast<int>(QRandomGenerator::global()->generate()&0x7fffffff);mutateDocument([&](QJsonObject& p){QJsonArray layers=p.value(QStringLiteral("layers")).toArray();for(int i=0;i<layers.size();++i){QJsonObject layer=layers.at(i).toObject();if(layer.value(QStringLiteral("id")).toString()!=active||layer.value(QStringLiteral("locked")).toBool(false))continue;QJsonArray objects=layer.value(QStringLiteral("objects")).toArray();const QString id=uid(QStringLiteral("mapobj"));QJsonObject o{{QStringLiteral("id"),id},{QStringLiteral("type"),type},{QStringLiteral("points"),points},{QStringLiteral("brushWidth"),brush},{QStringLiteral("density"),density},{QStringLiteral("symbolScale"),size/100.0},{QStringLiteral("strokeWidth"),stroke},{QStringLiteral("seed"),seed}};if(type==QStringLiteral("coast")||type==QStringLiteral("region"))o.insert(QStringLiteral("closed"),true);objects.append(o);layer.insert(QStringLiteral("objects"),objects);layers.replace(i,layer);p.insert(QStringLiteral("layers"),layers);selectedObjectId_=id;break;}});}

void PilinReyEditor::addSettlement(double x,double y){bool ok=false;QString label=QInputDialog::getText(this,tr("Asentamiento"),tr("Nombre:"),QLineEdit::Normal,tr("Poblado"),&ok).trimmed();if(!ok||label.isEmpty())return;const QStringList kinds{tr("Capital"),tr("Ciudad"),tr("Villa"),tr("Pueblo"),tr("Aldea"),tr("Puerto"),tr("Fortaleza"),tr("Ruina")};const QString kind=QInputDialog::getItem(this,tr("Asentamiento"),tr("Tipo:"),kinds,3,false,&ok);if(!ok)return;const QString active=activeLayerId();mutateDocument([&](QJsonObject& p){QJsonArray layers=p.value(QStringLiteral("layers")).toArray();for(int i=0;i<layers.size();++i){QJsonObject l=layers.at(i).toObject();if(l.value(QStringLiteral("id")).toString()!=active||l.value(QStringLiteral("locked")).toBool(false))continue;QJsonArray objects=l.value(QStringLiteral("objects")).toArray();const QString id=uid(QStringLiteral("mapobj"));objects.append(QJsonObject{{QStringLiteral("id"),id},{QStringLiteral("type"),QStringLiteral("settlement")},{QStringLiteral("kind"),kind},{QStringLiteral("label"),label},{QStringLiteral("x"),x},{QStringLiteral("y"),y}});l.insert(QStringLiteral("objects"),objects);layers.replace(i,l);p.insert(QStringLiteral("layers"),layers);selectedObjectId_=id;break;}});}
void PilinReyEditor::addLabel(double x,double y){bool ok=false;const QString text=QInputDialog::getText(this,tr("Etiqueta"),tr("Texto:"),QLineEdit::Normal,QString(),&ok).trimmed();if(!ok||text.isEmpty())return;const QString active=activeLayerId();mutateDocument([&](QJsonObject& p){QJsonArray layers=p.value(QStringLiteral("layers")).toArray();for(int i=0;i<layers.size();++i){QJsonObject l=layers.at(i).toObject();if(l.value(QStringLiteral("id")).toString()!=active||l.value(QStringLiteral("locked")).toBool(false))continue;QJsonArray objects=l.value(QStringLiteral("objects")).toArray();const QString id=uid(QStringLiteral("mapobj"));objects.append(QJsonObject{{QStringLiteral("id"),id},{QStringLiteral("type"),QStringLiteral("label")},{QStringLiteral("text"),text},{QStringLiteral("x"),x},{QStringLiteral("y"),y}});l.insert(QStringLiteral("objects"),objects);layers.replace(i,l);p.insert(QStringLiteral("layers"),layers);selectedObjectId_=id;break;}});}

void PilinReyEditor::selectObject(const QString& id){selectedObjectId_=id;refreshSelectionControls();if(viewport_)viewport_->setSelection(id);}
QJsonObject PilinReyEditor::selectedObject()const{if(selectedObjectId_.isEmpty())return{};for(const QJsonValue& lv:map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray())for(const QJsonValue& ov:lv.toObject().value(QStringLiteral("objects")).toArray()){const QJsonObject o=ov.toObject();if(o.value(QStringLiteral("id")).toString()==selectedObjectId_)return o;}return{};}
void PilinReyEditor::movePointObject(const QString& id,double x,double y){mutateDocument([&](QJsonObject& p){QJsonArray layers=p.value(QStringLiteral("layers")).toArray();for(int i=0;i<layers.size();++i){QJsonObject l=layers.at(i).toObject();if(l.value(QStringLiteral("locked")).toBool(false))continue;QJsonArray objects=l.value(QStringLiteral("objects")).toArray();for(int j=0;j<objects.size();++j){QJsonObject o=objects.at(j).toObject();if(o.value(QStringLiteral("id")).toString()!=id)continue;o.insert(QStringLiteral("x"),x);o.insert(QStringLiteral("y"),y);objects.replace(j,o);l.insert(QStringLiteral("objects"),objects);layers.replace(i,l);p.insert(QStringLiteral("layers"),layers);emit markerMoved(id,x,y);return;}}});}
void PilinReyEditor::movePathPoint(const QString& id,int idx,double x,double y){mutateDocument([&](QJsonObject& p){QJsonArray layers=p.value(QStringLiteral("layers")).toArray();for(int i=0;i<layers.size();++i){QJsonObject l=layers.at(i).toObject();if(l.value(QStringLiteral("locked")).toBool(false))continue;QJsonArray objects=l.value(QStringLiteral("objects")).toArray();for(int j=0;j<objects.size();++j){QJsonObject o=objects.at(j).toObject();if(o.value(QStringLiteral("id")).toString()!=id)continue;QJsonArray pts=o.value(QStringLiteral("points")).toArray();if(idx<0||idx>=pts.size())return;pts.replace(idx,pointJson(x,y));if(o.value(QStringLiteral("closed")).toBool(false)&&pts.size()>2){if(idx==0)pts.replace(pts.size()-1,pointJson(x,y));else if(idx==pts.size()-1)pts.replace(0,pointJson(x,y));}o.insert(QStringLiteral("points"),pts);objects.replace(j,o);l.insert(QStringLiteral("objects"),objects);layers.replace(i,l);p.insert(QStringLiteral("layers"),layers);return;}}});}

void PilinReyEditor::editSelectedObject(){const QJsonObject selected=selectedObject();if(selected.isEmpty())return;const QString id=selectedObjectId_,type=selected.value(QStringLiteral("type")).toString();if(type==QStringLiteral("settlement")||type==QStringLiteral("label")){bool ok=false;QString text=type==QStringLiteral("label")?selected.value(QStringLiteral("text")).toString():selected.value(QStringLiteral("label")).toString();text=QInputDialog::getText(this,tr("Editar"),tr("Texto:"),QLineEdit::Normal,text,&ok).trimmed();if(!ok||text.isEmpty())return;mutateDocument([&](QJsonObject& p){QJsonArray layers=p.value(QStringLiteral("layers")).toArray();for(int i=0;i<layers.size();++i){QJsonObject l=layers.at(i).toObject();QJsonArray objects=l.value(QStringLiteral("objects")).toArray();for(int j=0;j<objects.size();++j){QJsonObject o=objects.at(j).toObject();if(o.value(QStringLiteral("id")).toString()!=id)continue;o.insert(type==QStringLiteral("label")?QStringLiteral("text"):QStringLiteral("label"),text);objects.replace(j,o);l.insert(QStringLiteral("objects"),objects);layers.replace(i,l);p.insert(QStringLiteral("layers"),layers);return;}}});return;}bool ok=false;int width=selected.value(QStringLiteral("brushWidth")).toInt(220);if(type==QStringLiteral("forest")||type==QStringLiteral("mountain")){width=QInputDialog::getInt(this,tr("Anchura"),tr("Anchura del área:"),width,30,600,10,&ok);if(!ok)return;}int stroke=selected.value(QStringLiteral("strokeWidth")).toInt(4);if(type==QStringLiteral("river")||type==QStringLiteral("road")||type==QStringLiteral("border")){stroke=QInputDialog::getInt(this,tr("Grosor"),tr("Grosor:"),stroke,1,12,1,&ok);if(!ok)return;}mutateDocument([&](QJsonObject& p){QJsonArray layers=p.value(QStringLiteral("layers")).toArray();for(int i=0;i<layers.size();++i){QJsonObject l=layers.at(i).toObject();QJsonArray objects=l.value(QStringLiteral("objects")).toArray();for(int j=0;j<objects.size();++j){QJsonObject o=objects.at(j).toObject();if(o.value(QStringLiteral("id")).toString()!=id)continue;o.insert(QStringLiteral("brushWidth"),width);o.insert(QStringLiteral("strokeWidth"),stroke);objects.replace(j,o);l.insert(QStringLiteral("objects"),objects);layers.replace(i,l);p.insert(QStringLiteral("layers"),layers);return;}}});}

void PilinReyEditor::duplicateSelectedObject(){const QJsonObject selected=selectedObject();if(selected.isEmpty())return;copiedObject_=selected;pasteCopiedObject();}
void PilinReyEditor::copySelectedObject(){copiedObject_=selectedObject();}
void PilinReyEditor::pasteCopiedObject(){if(copiedObject_.isEmpty())return;const QString active=activeLayerId();const QJsonObject original=copiedObject_;mutateDocument([&](QJsonObject& p){QJsonArray layers=p.value(QStringLiteral("layers")).toArray();for(int i=0;i<layers.size();++i){QJsonObject l=layers.at(i).toObject();if(l.value(QStringLiteral("id")).toString()!=active||l.value(QStringLiteral("locked")).toBool(false))continue;QJsonObject c=original;const QString id=uid(QStringLiteral("mapobj"));c.insert(QStringLiteral("id"),id);if(c.contains(QStringLiteral("x"))){c.insert(QStringLiteral("x"),c.value(QStringLiteral("x")).toDouble()+50);c.insert(QStringLiteral("y"),c.value(QStringLiteral("y")).toDouble()+50);}else{QJsonArray pts=c.value(QStringLiteral("points")).toArray();for(int j=0;j<pts.size();++j){QPointF q=jsonPoint(pts.at(j));pts.replace(j,pointJson(q.x()+50,q.y()+50));}c.insert(QStringLiteral("points"),pts);}QJsonArray objects=l.value(QStringLiteral("objects")).toArray();objects.append(c);l.insert(QStringLiteral("objects"),objects);layers.replace(i,l);p.insert(QStringLiteral("layers"),layers);selectedObjectId_=id;copiedObject_=c;return;}});}
void PilinReyEditor::deleteSelectedObject(){if(selectedObjectId_.isEmpty())return;const QString id=selectedObjectId_;mutateDocument([&](QJsonObject& p){QJsonArray layers=p.value(QStringLiteral("layers")).toArray();for(int i=0;i<layers.size();++i){QJsonObject l=layers.at(i).toObject();if(l.value(QStringLiteral("locked")).toBool(false))continue;QJsonArray objects=l.value(QStringLiteral("objects")).toArray();for(int j=objects.size()-1;j>=0;--j)if(objects.at(j).toObject().value(QStringLiteral("id")).toString()==id){objects.removeAt(j);l.insert(QStringLiteral("objects"),objects);layers.replace(i,l);p.insert(QStringLiteral("layers"),layers);return;}}});selectedObjectId_.clear();refreshSelectionControls();refreshViewport();}

void PilinReyEditor::linkSelectedToAtlas(){const QJsonObject selected=selectedObject();if(selected.value(QStringLiteral("type")).toString()!=QStringLiteral("settlement"))return;QWidget* cursor=parentWidget();WorldPage* world=nullptr;while(cursor){world=qobject_cast<WorldPage*>(cursor);if(world)break;cursor=cursor->parentWidget();}if(!world||!world->document_)return;const QJsonArray entries=world->document_->array(QStringLiteral("world"));if(entries.isEmpty()){QMessageBox::information(this,tr("Atlas vacío"),tr("Crea primero una entrada en Mundo → Atlas."));return;}QStringList options,ids;options<<tr("— Sin enlace —");ids<<QString();for(const QJsonValue& v:entries){const QJsonObject e=v.toObject();const QString name=e.value(QStringLiteral("name")).toString(tr("Sin nombre"));const QString kind=e.value(QStringLiteral("kind")).toString();options<<(kind.isEmpty()?name:QStringLiteral("%1 · %2").arg(kind,name));ids<<e.value(QStringLiteral("id")).toString();}bool ok=false;const QString choice=QInputDialog::getItem(this,tr("Enlazar con Atlas"),tr("Entrada:"),options,0,false,&ok);if(!ok)return;const int idx=options.indexOf(choice);if(idx<0)return;const QString atlasId=ids.at(idx),selectedId=selectedObjectId_;mutateDocument([&](QJsonObject& p){QJsonArray layers=p.value(QStringLiteral("layers")).toArray();for(int i=0;i<layers.size();++i){QJsonObject l=layers.at(i).toObject();QJsonArray objects=l.value(QStringLiteral("objects")).toArray();for(int j=0;j<objects.size();++j){QJsonObject o=objects.at(j).toObject();if(o.value(QStringLiteral("id")).toString()!=selectedId)continue;if(atlasId.isEmpty()){o.remove(QStringLiteral("atlasId"));o.remove(QStringLiteral("atlasName"));}else{o.insert(QStringLiteral("atlasId"),atlasId);o.insert(QStringLiteral("atlasName"),entries.at(idx-1).toObject().value(QStringLiteral("name")).toString());}objects.replace(j,o);l.insert(QStringLiteral("objects"),objects);layers.replace(i,l);p.insert(QStringLiteral("layers"),layers);return;}}});}

void PilinReyEditor::pushUndo(){undoStack_.append(map_);while(undoStack_.size()>60)undoStack_.removeFirst();redoStack_.clear();}
void PilinReyEditor::mutateDocument(const std::function<void(QJsonObject&)>& mutation){if(refreshing_)return;pushUndo();QJsonObject p=map_.value(QStringLiteral("pilinRey")).toObject();mutation(p);map_.insert(QStringLiteral("pilinRey"),p);refreshViewport();refreshLayers();refreshSelectionControls();persistToArchive();emit mapEdited(map_);}
void PilinReyEditor::undo(){if(undoStack_.isEmpty())return;redoStack_.append(map_);map_=undoStack_.takeLast();selectedObjectId_.clear();refreshLayers();refreshSelectionControls();refreshViewport();persistToArchive();emit mapEdited(map_);}
void PilinReyEditor::redo(){if(redoStack_.isEmpty())return;undoStack_.append(map_);map_=redoStack_.takeLast();selectedObjectId_.clear();refreshLayers();refreshSelectionControls();refreshViewport();persistToArchive();emit mapEdited(map_);}

void PilinReyEditor::exportMap(){const QJsonObject p=map_.value(QStringLiteral("pilinRey")).toObject();const QSize size(qMax(1,p.value(QStringLiteral("width")).toInt(4096)),qMax(1,p.value(QStringLiteral("height")).toInt(2304)));MapExportDialog dialog(size,this);if(dialog.exec()!=QDialog::Accepted)return;const QString format=dialog.format();QString ext=QStringLiteral(".png"),filter=tr("PNG (*.png)");if(format==QStringLiteral("svg")){ext=QStringLiteral(".svg");filter=tr("SVG (*.svg)");}else if(format==QStringLiteral("pdf")){ext=QStringLiteral(".pdf");filter=tr("PDF (*.pdf)");}QString base=map_.value(QStringLiteral("name")).toString().trimmed();if(base.isEmpty())base=tr("mapa");QString path=QFileDialog::getSaveFileName(this,tr("Exportar mapa"),base+ext,filter);if(path.isEmpty())return;if(!path.endsWith(ext,Qt::CaseInsensitive))path+=ext;QString error;bool ok=false;if(format==QStringLiteral("svg"))ok=MapExporter::exportSvg(map_,path,dialog.outputSize(),&error);else if(format==QStringLiteral("pdf"))ok=MapExporter::exportPdf(map_,path,dialog.outputSize(),&error);else ok=MapExporter::exportPng(map_,path,dialog.outputSize(),&error);if(!ok)QMessageBox::critical(this,tr("No se pudo exportar"),error.isEmpty()?tr("La exportación falló."):error);else status_->setText(tr("Exportado · %1").arg(path));}

void PilinReyEditor::persistToArchive(){QWidget* cursor=parentWidget();WorldPage* world=nullptr;while(cursor){world=qobject_cast<WorldPage*>(cursor);if(world)break;cursor=cursor->parentWidget();}if(!world||!world->document_||!world->mapList_)return;const int row=world->mapList_->currentRow();QJsonArray maps=world->document_->array(QStringLiteral("maps"));if(row<0||row>=maps.size())return;QJsonObject stored=maps.at(row).toObject();stored.insert(QStringLiteral("pilinRey"),map_.value(QStringLiteral("pilinRey")));if(map_.contains(QStringLiteral("backgroundImageDataUrl")))stored.insert(QStringLiteral("backgroundImageDataUrl"),map_.value(QStringLiteral("backgroundImageDataUrl")));else stored.remove(QStringLiteral("backgroundImageDataUrl"));maps.replace(row,stored);world->document_->setArray(QStringLiteral("maps"),maps);emit world->changed();}

} // namespace wbw
