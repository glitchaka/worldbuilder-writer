#include "ui/PilinReyEditor.h"

#include "core/ArchiveDocument.h"
#include "ui/MapExportDialog.h"
#include "ui/MapExporter.h"
#include "ui/WorldPage.h"

#include <SDL3/SDL.h>

#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEngine>
#include <QPushButton>
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
#include <vector>

namespace wbw {
namespace {

constexpr double kTau = 6.2831853071795864769;

QString uid(const QString& prefix) {
    return prefix + QLatin1Char('-') + QUuid::createUuid().toString(QUuid::Id128);
}

QJsonObject pointJson(double x, double y, double radius = 0.0) {
    QJsonObject p{{QStringLiteral("x"), x}, {QStringLiteral("y"), y}};
    if (radius > 0.0) p.insert(QStringLiteral("radius"), radius);
    return p;
}

QPointF jsonPoint(const QJsonValue& value) {
    const QJsonObject p = value.toObject();
    return QPointF(p.value(QStringLiteral("x")).toDouble(), p.value(QStringLiteral("y")).toDouble());
}

double jsonRadius(const QJsonValue& value, double fallback) {
    return value.toObject().value(QStringLiteral("radius")).toDouble(fallback);
}

double dist2(const QPointF& a, const QPointF& b) {
    const double dx = a.x() - b.x();
    const double dy = a.y() - b.y();
    return dx * dx + dy * dy;
}

double segmentDistance2(const QPointF& p, const QPointF& a, const QPointF& b) {
    const QPointF ab = b - a;
    const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
    if (len2 < 1e-9) return dist2(p, a);
    const QPointF ap = p - a;
    const double t = std::clamp((ap.x() * ab.x() + ap.y() * ab.y()) / len2, 0.0, 1.0);
    return dist2(p, a + ab * t);
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

std::vector<QPointF> smoothPath(const QJsonArray& source) {
    std::vector<QPointF> points;
    points.reserve(source.size());
    for (const QJsonValue& v : source) points.push_back(jsonPoint(v));
    if (points.size() < 3) return points;
    const bool closed = dist2(points.front(), points.back()) < 1e-8;
    if (closed && points.size() > 3) points.pop_back();
    const size_t n = points.size();
    std::vector<QPointF> out;
    const size_t segments = closed ? n : n - 1;
    constexpr int samples = 10;
    out.reserve(segments * samples + 1);
    for (size_t i = 0; i < segments; ++i) {
        const size_t j = (i + 1) % n;
        const QPointF p0 = (!closed && i == 0) ? points[i] : points[(i + n - 1) % n];
        const QPointF p1 = points[i];
        const QPointF p2 = points[j];
        const QPointF p3 = (!closed && j + 1 >= n) ? p2 : points[(j + 1) % n];
        for (int s = 0; s < samples; ++s) {
            const double t = static_cast<double>(s) / samples;
            const double t2 = t * t;
            const double t3 = t2 * t;
            out.emplace_back(
                0.5 * ((2.0 * p1.x()) + (-p0.x() + p2.x()) * t + (2.0 * p0.x() - 5.0 * p1.x() + 4.0 * p2.x() - p3.x()) * t2 + (-p0.x() + 3.0 * p1.x() - 3.0 * p2.x() + p3.x()) * t3),
                0.5 * ((2.0 * p1.y()) + (-p0.y() + p2.y()) * t + (2.0 * p0.y() - 5.0 * p1.y() + 4.0 * p2.y() - p3.y()) * t2 + (-p0.y() + 3.0 * p1.y() - 3.0 * p2.y() + p3.y()) * t3));
        }
    }
    out.push_back(closed ? out.front() : points.back());
    return out;
}

QToolButton* toolButton(const QString& glyph, const QString& tip, QWidget* parent, const QString& name) {
    auto* b = new QToolButton(parent);
    b->setText(glyph);
    b->setToolTip(tip);
    b->setObjectName(name);
    b->setFixedSize(34, 34);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}

QColor themeColor(const QJsonObject& pilin, const QString& key, const QColor& fallback) {
    const QColor parsed(pilin.value(QStringLiteral("theme")).toObject().value(key).toString());
    return parsed.isValid() ? parsed : fallback;
}

QString pathType(PilinReyEditor::Tool tool) {
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

bool isBrushTool(PilinReyEditor::Tool tool) {
    return tool == PilinReyEditor::Tool::Coast || tool == PilinReyEditor::Tool::Eraser ||
           tool == PilinReyEditor::Tool::Forest || tool == PilinReyEditor::Tool::Mountain;
}

bool isNodeTool(PilinReyEditor::Tool tool) {
    return tool == PilinReyEditor::Tool::Region || tool == PilinReyEditor::Tool::River ||
           tool == PilinReyEditor::Tool::Road || tool == PilinReyEditor::Tool::Border;
}

quint32 hashText(const QString& text) {
    quint32 h = 2166136261u;
    const QByteArray bytes = text.toUtf8();
    for (const char c : bytes) { h ^= static_cast<unsigned char>(c); h *= 16777619u; }
    return h;
}

QPointF rotatePoint(const QPointF& p, const QPointF& origin, double degrees, double scale) {
    const double a = degrees * 3.14159265358979323846 / 180.0;
    const double cs = std::cos(a), sn = std::sin(a);
    const QPointF q = (p - origin) * scale;
    return origin + QPointF(q.x() * cs - q.y() * sn, q.x() * sn + q.y() * cs);
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
        setAttribute(Qt::WA_NativeWindow, true);
        setAttribute(Qt::WA_PaintOnScreen, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setMinimumSize(720, 460);
    }

    ~PilinReyViewport() override {
        clearStampTextures();
        destroyTexture(templateTexture_);
        destroyTexture(terrainTexture_);
        destroyTexture(coastTexture_);
        destroyTexture(regionTexture_);
        if (renderer_) SDL_DestroyRenderer(renderer_);
        if (window_) SDL_DestroyWindow(window_);
    }

    QPaintEngine* paintEngine() const override { return nullptr; }

    void setDocument(const QJsonObject& map) {
        map_ = map;
        surfaceDirty_ = true;
        templateDirty_ = true;
        clearStampTextures();
        if (!viewInitialized_) fitDocument();
        renderFrame();
    }

    void setTool(Tool tool) {
        tool_ = tool;
        brushStroke_ = QJsonArray{};
        nodePath_ = QJsonArray{};
        drawingBrush_ = false;
        draggingObjectId_.clear();
        draggingPathId_.clear();
        draggingPathPointIndex_ = -1;
        measureActive_ = false;
        renderFrame();
    }

    void setSelection(const QStringList& ids) { selectedObjectIds_ = ids; renderFrame(); }
    void setSnap(bool enabled) { snap_ = enabled; }
    void setGrid(bool enabled) { grid_ = enabled; renderFrame(); }
    void setBrushRadius(int value) { brushRadius_ = std::clamp(value, 30, 700); renderFrame(); }
    void setDensity(int value) { density_ = std::clamp(value, 10, 100); renderFrame(); }
    void setStrokeWidth(int value) { strokeWidth_ = std::clamp(value, 1, 24); renderFrame(); }
    void setSymbolSize(int value) { symbolSize_ = std::clamp(value, 16, 160); renderFrame(); }

    void fitDocument() {
        const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        const double docW = std::max(1.0, pilin.value(QStringLiteral("width")).toDouble(4096));
        const double docH = std::max(1.0, pilin.value(QStringLiteral("height")).toDouble(2304));
        if (width() < 100 || height() < 100) return;
        zoom_ = std::clamp(std::min((width() - 60.0) / docW, (height() - 60.0) / docH), 0.04, 5.0);
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
        zoom_ = std::clamp(zoom_ * (event->angleDelta().y() > 0 ? 1.12 : 1.0 / 1.12), 0.035, 14.0);
        const QPointF after = screenToWorld(event->position());
        pan_ += QPointF((after.x() - before.x()) * zoom_, (after.y() - before.y()) * zoom_);
        renderFrame();
        event->accept();
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Escape) {
            brushStroke_ = QJsonArray{};
            nodePath_ = QJsonArray{};
            drawingBrush_ = false;
            measureActive_ = false;
            renderFrame();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Backspace && !nodePath_.isEmpty()) {
            nodePath_.removeLast();
            renderFrame();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Delete && onDeleteSelection) {
            onDeleteSelection();
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override {
        setFocus(Qt::MouseFocusReason);
        lastMouse_ = event->position();
        if (event->button() == Qt::MiddleButton || tool_ == Tool::Pan) {
            panning_ = true;
            event->accept();
            return;
        }
        if (event->button() == Qt::RightButton && isNodeTool(tool_)) {
            finishNodePath();
            event->accept();
            return;
        }
        if (event->button() != Qt::LeftButton) return;

        const QPointF world = snapPoint(screenToWorld(event->position()));
        cursorWorld_ = world;

        if (tool_ == Tool::Measure) {
            if (!measureActive_) { measureStart_ = world; measureEnd_ = world; measureActive_ = true; }
            else { measureEnd_ = world; measureActive_ = false; }
            renderFrame();
            return;
        }

        if (tool_ == Tool::Select) {
            const QString hit = findObjectAt(world);
            const bool extend = event->modifiers().testFlag(Qt::ShiftModifier) || event->modifiers().testFlag(Qt::ControlModifier);
            if (!extend) selectedObjectIds_.clear();
            if (!hit.isEmpty()) {
                if (extend && selectedObjectIds_.contains(hit)) selectedObjectIds_.removeAll(hit);
                else if (!selectedObjectIds_.contains(hit)) selectedObjectIds_.append(hit);
            }
            draggingObjectId_.clear();
            draggingPathId_.clear();
            draggingPathPointIndex_ = -1;
            if (!hit.isEmpty() && selectedObjectIds_.contains(hit)) {
                const QJsonObject object = objectById(hit);
                if (object.contains(QStringLiteral("x"))) draggingObjectId_ = hit;
                else {
                    const int node = nearestNode(object, world);
                    if (node >= 0) { draggingPathId_ = hit; draggingPathPointIndex_ = node; }
                }
            }
            if (onSelection) onSelection(selectedObjectIds_);
            renderFrame();
            return;
        }

        if (tool_ == Tool::Settlement) { if (onSettlement) onSettlement(world.x(), world.y()); return; }
        if (tool_ == Tool::Stamp) { if (onStamp) onStamp(world.x(), world.y()); return; }
        if (tool_ == Tool::Label) { if (onLabel) onLabel(world.x(), world.y()); return; }

        if (isBrushTool(tool_)) {
            drawingBrush_ = true;
            brushStroke_ = QJsonArray{pointJson(world.x(), world.y(), brushRadius_)};
            renderFrame();
            return;
        }

        if (isNodeTool(tool_)) {
            nodePath_.append(pointJson(world.x(), world.y()));
            renderFrame();
        }
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && isNodeTool(tool_)) {
            const QPointF world = snapPoint(screenToWorld(event->position()));
            if (nodePath_.isEmpty() || dist2(jsonPoint(nodePath_.last()), world) > 1.0) nodePath_.append(pointJson(world.x(), world.y()));
            finishNodePath();
            event->accept();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (panning_) {
            pan_ += event->position() - lastMouse_;
            lastMouse_ = event->position();
            renderFrame();
            return;
        }
        const QPointF world = snapPoint(screenToWorld(event->position()));
        cursorWorld_ = world;
        if (onStatus) onStatus(QStringLiteral("x %1  y %2  ·  %3%").arg(qRound(world.x())).arg(qRound(world.y())).arg(qRound(zoom_ * 100)));

        if (measureActive_) {
            measureEnd_ = world;
            if (onStatus) onStatus(QStringLiteral("Medida %1 u · %2%").arg(qRound(std::sqrt(dist2(measureStart_, measureEnd_)))).arg(qRound(zoom_ * 100)));
            renderFrame();
            return;
        }

        if (!draggingObjectId_.isEmpty() && (event->buttons() & Qt::LeftButton)) {
            updatePointObjectLocal(draggingObjectId_, world.x(), world.y());
            renderFrame();
            return;
        }
        if (!draggingPathId_.isEmpty() && draggingPathPointIndex_ >= 0 && (event->buttons() & Qt::LeftButton)) {
            updatePathPointLocal(draggingPathId_, draggingPathPointIndex_, world.x(), world.y());
            renderFrame();
            return;
        }

        if (drawingBrush_ && (event->buttons() & Qt::LeftButton)) {
            if (brushStroke_.isEmpty() || dist2(jsonPoint(brushStroke_.last()), world) > std::pow(std::max(brushRadius_ * 0.14, 10.0), 2.0))
                brushStroke_.append(pointJson(world.x(), world.y(), brushRadius_));
            renderFrame();
            return;
        }
        renderFrame();
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::MiddleButton || panning_) { panning_ = false; return; }
        if (event->button() != Qt::LeftButton) return;

        if (!draggingObjectId_.isEmpty()) {
            const QJsonObject object = objectById(draggingObjectId_);
            if (onMovePoint) onMovePoint(draggingObjectId_, object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble());
            draggingObjectId_.clear();
            return;
        }
        if (!draggingPathId_.isEmpty() && draggingPathPointIndex_ >= 0) {
            const QJsonObject object = objectById(draggingPathId_);
            const QJsonArray pts = object.value(QStringLiteral("points")).toArray();
            if (draggingPathPointIndex_ < pts.size() && onMovePathPoint) {
                const QPointF p = jsonPoint(pts.at(draggingPathPointIndex_));
                onMovePathPoint(draggingPathId_, draggingPathPointIndex_, p.x(), p.y());
            }
            draggingPathId_.clear();
            draggingPathPointIndex_ = -1;
            return;
        }
        if (drawingBrush_) {
            drawingBrush_ = false;
            if (!brushStroke_.isEmpty() && onPath) onPath(pathType(tool_), brushStroke_);
            brushStroke_ = QJsonArray{};
            return;
        }
    }

private:
    void destroyTexture(SDL_Texture*& texture) { if (texture) { SDL_DestroyTexture(texture); texture = nullptr; } }
    void clearStampTextures() { for (SDL_Texture* texture : stampTextures_) if (texture) SDL_DestroyTexture(texture); stampTextures_.clear(); }

    bool ensureRenderer() {
        if (renderer_) return true;
        if (!SDL_WasInit(SDL_INIT_VIDEO) && !SDL_Init(SDL_INIT_VIDEO)) return false;
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

    QPointF snapPoint(QPointF p) const {
        if (!snap_) return p;
        constexpr double step = 20.0;
        p.setX(std::round(p.x() / step) * step);
        p.setY(std::round(p.y() / step) * step);
        return p;
    }

    void finishNodePath() {
        if (nodePath_.isEmpty()) return;
        QJsonArray points = nodePath_;
        nodePath_ = QJsonArray{};
        const QString type = pathType(tool_);
        if (type == QStringLiteral("region") && points.size() >= 3) points.append(points.first());
        const int minimum = type == QStringLiteral("region") ? 4 : 2;
        if (points.size() >= minimum && onPath) onPath(type, points);
        renderFrame();
    }

    QJsonObject objectById(const QString& id) const {
        for (const QJsonValue& lv : map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray()) {
            for (const QJsonValue& ov : lv.toObject().value(QStringLiteral("objects")).toArray()) {
                const QJsonObject object = ov.toObject();
                if (object.value(QStringLiteral("id")).toString() == id) return object;
            }
        }
        return {};
    }

    int nearestNode(const QJsonObject& object, const QPointF& world) const {
        const QJsonArray pts = object.value(QStringLiteral("points")).toArray();
        const double tolerance2 = std::pow(15.0 / std::max(zoom_, 0.04), 2.0);
        int best = -1;
        double bestD = tolerance2;
        for (int i = 0; i < pts.size(); ++i) {
            const double d = dist2(jsonPoint(pts.at(i)), world);
            if (d <= bestD) { best = i; bestD = d; }
        }
        return best;
    }

    QString findObjectAt(const QPointF& world) const {
        const double tol = 18.0 / std::max(zoom_, 0.04);
        const QJsonArray layers = map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray();
        for (int li = layers.size() - 1; li >= 0; --li) {
            const QJsonObject layer = layers.at(li).toObject();
            if (!layer.value(QStringLiteral("visible")).toBool(true) || layer.value(QStringLiteral("locked")).toBool(false)) continue;
            const QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
            for (int oi = objects.size() - 1; oi >= 0; --oi) {
                const QJsonObject object = objects.at(oi).toObject();
                const QString type = object.value(QStringLiteral("type")).toString();
                if (object.contains(QStringLiteral("x"))) {
                    const double size = object.value(QStringLiteral("scale")).toDouble(1.0) * 120.0;
                    const double r = std::max(tol, size * .55);
                    if (dist2(QPointF(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble()), world) <= r * r)
                        return object.value(QStringLiteral("id")).toString();
                    continue;
                }
                const QJsonArray pts = object.value(QStringLiteral("points")).toArray();
                if (pts.isEmpty()) continue;
                if (type == QStringLiteral("land") || type == QStringLiteral("sea") || type == QStringLiteral("forestArea") || type == QStringLiteral("mountainArea")) {
                    for (const QJsonValue& pv : pts) {
                        const double radius = jsonRadius(pv, brushRadius_);
                        if (dist2(jsonPoint(pv), world) <= radius * radius) return object.value(QStringLiteral("id")).toString();
                    }
                    continue;
                }
                for (int i = 1; i < pts.size(); ++i) {
                    if (segmentDistance2(world, jsonPoint(pts.at(i - 1)), jsonPoint(pts.at(i))) <= tol * tol)
                        return object.value(QStringLiteral("id")).toString();
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
            QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
            for (int j = 0; j < objects.size(); ++j) {
                QJsonObject object = objects.at(j).toObject();
                if (object.value(QStringLiteral("id")).toString() != id) continue;
                QJsonArray pts = object.value(QStringLiteral("points")).toArray();
                if (index < 0 || index >= pts.size()) return;
                QJsonObject p = pts.at(index).toObject(); p.insert(QStringLiteral("x"), x); p.insert(QStringLiteral("y"), y); pts.replace(index, p);
                if (object.value(QStringLiteral("closed")).toBool(false) && pts.size() > 2) {
                    if (index == 0) pts.replace(pts.size() - 1, p);
                    else if (index == pts.size() - 1) pts.replace(0, p);
                }
                object.insert(QStringLiteral("points"), pts); objects.replace(j, object); layer.insert(QStringLiteral("objects"), objects); layers.replace(i, layer);
                pilin.insert(QStringLiteral("layers"), layers); map_.insert(QStringLiteral("pilinRey"), pilin); surfaceDirty_ = true; return;
            }
        }
    }

    void updateTemplateTexture() {
        if (!renderer_ || !templateDirty_) return;
        templateDirty_ = false;
        destroyTexture(templateTexture_);
        const QJsonObject templ = map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("template")).toObject();
        QString data = templ.value(QStringLiteral("dataUrl")).toString();
        if (data.isEmpty()) data = map_.value(QStringLiteral("backgroundImageDataUrl")).toString();
        if (data.isEmpty()) return;
        QImage image;
        if (!image.loadFromData(dataUrlBytes(data))) return;
        image = image.convertToFormat(QImage::Format_RGBA8888);
        templateTexture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, image.width(), image.height());
        if (!templateTexture_) return;
        SDL_UpdateTexture(templateTexture_, nullptr, image.constBits(), image.bytesPerLine());
        SDL_SetTextureBlendMode(templateTexture_, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(templateTexture_, SDL_SCALEMODE_LINEAR);
    }

    void uploadImageTexture(const QImage& source, SDL_Texture*& texture) {
        destroyTexture(texture);
        QImage image = source.convertToFormat(QImage::Format_RGBA8888);
        texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, image.width(), image.height());
        if (!texture) return;
        SDL_UpdateTexture(texture, nullptr, image.constBits(), image.bytesPerLine());
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    }

    void updateSurfaceTextures() {
        if (!renderer_ || !surfaceDirty_) return;
        surfaceDirty_ = false;
        const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        const double docW = pilin.value(QStringLiteral("width")).toDouble(4096);
        const double docH = pilin.value(QStringLiteral("height")).toDouble(2304);
        const int w = 1536;
        const int h = qMax(384, qRound(w * docH / docW));
        const double sx = w / docW;
        const double sy = h / docH;

        QImage mask(w, h, QImage::Format_ARGB32_Premultiplied);
        mask.fill(Qt::transparent);
        QImage regions(w, h, QImage::Format_ARGB32_Premultiplied);
        regions.fill(Qt::transparent);
        QPainter terrainPainter(&mask);
        QPainter regionPainter(&regions);
        terrainPainter.setRenderHint(QPainter::Antialiasing, true);
        regionPainter.setRenderHint(QPainter::Antialiasing, true);
        for (const QJsonValue& lv : pilin.value(QStringLiteral("layers")).toArray()) {
            const QJsonObject layer = lv.toObject();
            if (!layer.value(QStringLiteral("visible")).toBool(true)) continue;
            const double opacity = std::clamp(layer.value(QStringLiteral("opacity")).toDouble(1.0), 0.0, 1.0);
            for (const QJsonValue& ov : layer.value(QStringLiteral("objects")).toArray()) {
                const QJsonObject object = ov.toObject();
                const QString type = object.value(QStringLiteral("type")).toString();
                const QJsonArray pts = object.value(QStringLiteral("points")).toArray();
                if (type == QStringLiteral("land") || type == QStringLiteral("sea")) {
                    if (pts.isEmpty()) continue;
                    terrainPainter.setCompositionMode(type == QStringLiteral("land") ? QPainter::CompositionMode_SourceOver : QPainter::CompositionMode_Clear);
                    terrainPainter.setOpacity(opacity);
                    for (int i = 0; i < pts.size(); ++i) {
                        const QPointF p = jsonPoint(pts.at(i));
                        const double radius = jsonRadius(pts.at(i), 180.0);
                        terrainPainter.setPen(QPen(Qt::white, radius * 2.0 * sx, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                        if (i == 0) terrainPainter.drawPoint(QPointF(p.x() * sx, p.y() * sy));
                        else {
                            const QPointF a = jsonPoint(pts.at(i - 1));
                            terrainPainter.drawLine(QPointF(a.x() * sx, a.y() * sy), QPointF(p.x() * sx, p.y() * sy));
                        }
                    }
                    terrainPainter.setOpacity(1.0);
                } else if (type == QStringLiteral("region") && pts.size() >= 4) {
                    QPainterPath path;
                    const QPointF first = jsonPoint(pts.first());
                    path.moveTo(first.x() * sx, first.y() * sy);
                    for (int i = 1; i < pts.size(); ++i) { const QPointF p = jsonPoint(pts.at(i)); path.lineTo(p.x() * sx, p.y() * sy); }
                    path.closeSubpath();
                    QColor fill = themeColor(pilin, QStringLiteral("region"), QColor(176, 117, 72));
                    fill.setAlphaF(std::clamp(object.value(QStringLiteral("fillOpacity")).toDouble(.18) * opacity, 0.0, 1.0));
                    regionPainter.fillPath(path, fill);
                }
            }
        }
        terrainPainter.end();
        regionPainter.end();

        const QColor landColor = themeColor(pilin, QStringLiteral("land"), QColor(216, 201, 158));
        const QColor coastColor = themeColor(pilin, QStringLiteral("coast"), QColor(65, 59, 48));
        QImage land(w, h, QImage::Format_ARGB32_Premultiplied); land.fill(Qt::transparent);
        QImage coast(w, h, QImage::Format_ARGB32_Premultiplied); coast.fill(Qt::transparent);
        const QRgb landRgb = qRgba(landColor.red(), landColor.green(), landColor.blue(), 255);
        const QRgb coastRgb = qRgba(coastColor.red(), coastColor.green(), coastColor.blue(), 240);
        for (int y = 1; y < h - 1; ++y) {
            const QRgb* src = reinterpret_cast<const QRgb*>(mask.constScanLine(y));
            QRgb* dst = reinterpret_cast<QRgb*>(land.scanLine(y));
            QRgb* edge = reinterpret_cast<QRgb*>(coast.scanLine(y));
            const QRgb* up = reinterpret_cast<const QRgb*>(mask.constScanLine(y - 1));
            const QRgb* down = reinterpret_cast<const QRgb*>(mask.constScanLine(y + 1));
            for (int x = 1; x < w - 1; ++x) {
                if (qAlpha(src[x]) < 100) continue;
                dst[x] = landRgb;
                if (qAlpha(up[x]) < 100 || qAlpha(down[x]) < 100 || qAlpha(src[x - 1]) < 100 || qAlpha(src[x + 1]) < 100) edge[x] = coastRgb;
            }
        }
        uploadImageTexture(land, terrainTexture_);
        uploadImageTexture(coast, coastTexture_);
        uploadImageTexture(regions, regionTexture_);
    }

    void setColor(const QColor& color, int alpha = 255) {
        const int a = qBound(0, qRound(alpha * currentLayerAlpha_), 255);
        SDL_SetRenderDrawColor(renderer_, color.red(), color.green(), color.blue(), a);
    }

    void drawPolyline(const std::vector<QPointF>& points, const QColor& color, int width = 1, bool dashed = false) {
        if (points.size() < 2) return;
        setColor(color);
        for (size_t i = 1; i < points.size(); ++i) {
            if (dashed && ((i / 4) % 2)) continue;
            const QPointF a = worldToScreen(points[i - 1]);
            const QPointF b = worldToScreen(points[i]);
            for (int offset = -(width / 2); offset <= width / 2; ++offset)
                SDL_RenderLine(renderer_, static_cast<float>(a.x()), static_cast<float>(a.y() + offset), static_cast<float>(b.x()), static_cast<float>(b.y() + offset));
        }
    }

    void drawVariableRiver(const std::vector<QPointF>& points, const QJsonObject& object) {
        if (points.size() < 2) return;
        const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        const QColor dark = themeColor(pilin, QStringLiteral("river"), QColor(37, 79, 109));
        const QColor light = dark.lighter(155);
        const double base = object.value(QStringLiteral("width")).toDouble(strokeWidth_);
        for (size_t i = 1; i < points.size(); ++i) {
            const double t = static_cast<double>(i) / std::max<size_t>(1, points.size() - 1);
            const int outer = qMax(1, qRound((base * (.35 + .85 * t)) * zoom_));
            drawPolyline({points[i - 1], points[i]}, dark, outer + 2);
            drawPolyline({points[i - 1], points[i]}, light, qMax(1, outer / 2));
        }
    }

    void drawForestArea(const QJsonObject& object) {
        const QJsonArray pts = object.value(QStringLiteral("points")).toArray();
        if (pts.isEmpty()) return;
        const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        const QColor color = themeColor(pilin, QStringLiteral("forest"), QColor(55, 86, 53));
        const int density = object.value(QStringLiteral("density")).toInt(density_);
        const double symbol = object.value(QStringLiteral("symbolSize")).toDouble(symbolSize_);
        QRandomGenerator rng(hashText(object.value(QStringLiteral("id")).toString()));
        const int count = qMax(24, pts.size() * density / 2);
        setColor(color, 235);
        for (int i = 0; i < count; ++i) {
            const int index = rng.bounded(pts.size());
            const QPointF center = jsonPoint(pts.at(index));
            const double radius = jsonRadius(pts.at(index), 180.0);
            const double angle = rng.generateDouble() * kTau;
            const double rr = std::sqrt(rng.generateDouble()) * radius;
            const QPointF p = worldToScreen(center + QPointF(std::cos(angle) * rr, std::sin(angle) * rr));
            const float s = static_cast<float>(std::clamp(symbol * zoom_ * (0.7 + rng.generateDouble() * .65), 3.0, 28.0));
            SDL_RenderLine(renderer_, p.x(), p.y() - s, p.x() - s * .62f, p.y() + s * .35f);
            SDL_RenderLine(renderer_, p.x(), p.y() - s, p.x() + s * .62f, p.y() + s * .35f);
            SDL_RenderLine(renderer_, p.x(), p.y() - s * .46f, p.x() - s * .48f, p.y() + s * .02f);
            SDL_RenderLine(renderer_, p.x(), p.y() - s * .46f, p.x() + s * .48f, p.y() + s * .02f);
            SDL_RenderLine(renderer_, p.x(), p.y() + s * .35f, p.x(), p.y() + s * .78f);
        }
    }

    void drawMountainArea(const QJsonObject& object) {
        const QJsonArray pts = object.value(QStringLiteral("points")).toArray();
        if (pts.isEmpty()) return;
        const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        const QColor color = themeColor(pilin, QStringLiteral("mountain"), QColor(77, 68, 59));
        const int density = object.value(QStringLiteral("density")).toInt(density_);
        const double symbol = object.value(QStringLiteral("symbolSize")).toDouble(symbolSize_ * 1.15);
        QRandomGenerator rng(hashText(object.value(QStringLiteral("id")).toString()) ^ 0x8f34a1u);
        const int count = qMax(18, pts.size() * density / 3);
        setColor(color, 242);
        for (int i = 0; i < count; ++i) {
            const int index = rng.bounded(pts.size());
            const QPointF center = jsonPoint(pts.at(index));
            const double radius = jsonRadius(pts.at(index), 220.0);
            const double angle = rng.generateDouble() * kTau;
            const double rr = std::sqrt(rng.generateDouble()) * radius;
            const QPointF p = worldToScreen(center + QPointF(std::cos(angle) * rr, std::sin(angle) * rr));
            const float s = static_cast<float>(std::clamp(symbol * zoom_ * (0.66 + rng.generateDouble() * .75), 4.0, 38.0));
            SDL_RenderLine(renderer_, p.x() - s, p.y() + s * .56f, p.x(), p.y() - s);
            SDL_RenderLine(renderer_, p.x(), p.y() - s, p.x() + s, p.y() + s * .56f);
            SDL_RenderLine(renderer_, p.x() - s * .31f, p.y() - s * .05f, p.x(), p.y() + s * .2f);
            SDL_RenderLine(renderer_, p.x(), p.y() + s * .2f, p.x() + s * .28f, p.y() - s * .13f);
        }
    }

    SDL_Texture* textureForStamp(const QJsonObject& object) {
        const QString id = object.value(QStringLiteral("id")).toString();
        if (stampTextures_.contains(id)) return stampTextures_.value(id);
        const QString data = object.value(QStringLiteral("dataUrl")).toString();
        if (data.isEmpty()) return nullptr;
        QImage image;
        if (!image.loadFromData(dataUrlBytes(data))) return nullptr;
        image = image.convertToFormat(QImage::Format_RGBA8888);
        SDL_Texture* texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, image.width(), image.height());
        if (!texture) return nullptr;
        SDL_UpdateTexture(texture, nullptr, image.constBits(), image.bytesPerLine());
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
        stampTextures_.insert(id, texture);
        return texture;
    }

    void drawBuiltinStamp(const QString& kind, const QPointF& p, float s) {
        const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        setColor(themeColor(pilin, QStringLiteral("symbol"), QColor(47, 42, 36)), 245);
        if (kind == QStringLiteral("castle")) {
            SDL_FRect body{p.x() - s, p.y() - s * .35f, s * 2, s * 1.35f}; SDL_RenderRect(renderer_, &body);
            for (int i = -1; i <= 1; ++i) { SDL_FRect tower{p.x() + i * s * .75f - s * .23f, p.y() - s, s * .46f, s * .7f}; SDL_RenderRect(renderer_, &tower); }
        } else if (kind == QStringLiteral("tower")) {
            SDL_FRect body{p.x() - s * .45f, p.y() - s, s * .9f, s * 1.8f}; SDL_RenderRect(renderer_, &body); SDL_RenderLine(renderer_, p.x() - s * .7f, p.y() - s, p.x() + s * .7f, p.y() - s);
        } else if (kind == QStringLiteral("temple")) {
            SDL_RenderLine(renderer_, p.x() - s, p.y() - s * .35f, p.x(), p.y() - s); SDL_RenderLine(renderer_, p.x(), p.y() - s, p.x() + s, p.y() - s * .35f);
            for (int i = -1; i <= 1; ++i) SDL_RenderLine(renderer_, p.x() + i * s * .55f, p.y() - s * .3f, p.x() + i * s * .55f, p.y() + s * .75f);
            SDL_RenderLine(renderer_, p.x() - s, p.y() + s * .75f, p.x() + s, p.y() + s * .75f);
        } else if (kind == QStringLiteral("ruin")) {
            SDL_RenderLine(renderer_, p.x() - s, p.y() + s * .7f, p.x() - s * .7f, p.y() - s * .6f); SDL_RenderLine(renderer_, p.x() - s * .7f, p.y() - s * .6f, p.x() - s * .15f, p.y() - s * .15f); SDL_RenderLine(renderer_, p.x() - s * .15f, p.y() - s * .15f, p.x() + s * .25f, p.y() - s * .75f); SDL_RenderLine(renderer_, p.x() + s * .25f, p.y() - s * .75f, p.x() + s, p.y() + s * .7f);
        } else if (kind == QStringLiteral("ship")) {
            SDL_RenderLine(renderer_, p.x() - s, p.y() + s * .35f, p.x() + s, p.y() + s * .35f); SDL_RenderLine(renderer_, p.x() - s, p.y() + s * .35f, p.x() - s * .6f, p.y() + s * .8f); SDL_RenderLine(renderer_, p.x() - s * .6f, p.y() + s * .8f, p.x() + s * .65f, p.y() + s * .8f); SDL_RenderLine(renderer_, p.x(), p.y() - s, p.x(), p.y() + s * .35f); SDL_RenderLine(renderer_, p.x(), p.y() - s * .8f, p.x() + s * .6f, p.y() - s * .05f); SDL_RenderLine(renderer_, p.x() + s * .6f, p.y() - s * .05f, p.x(), p.y() - s * .05f);
        } else if (kind == QStringLiteral("bridge")) {
            SDL_RenderLine(renderer_, p.x() - s, p.y() + s * .55f, p.x() + s, p.y() + s * .55f); SDL_RenderLine(renderer_, p.x() - s, p.y() + s * .55f, p.x() - s * .65f, p.y() - s * .25f); SDL_RenderLine(renderer_, p.x() - s * .65f, p.y() - s * .25f, p.x() + s * .65f, p.y() - s * .25f); SDL_RenderLine(renderer_, p.x() + s * .65f, p.y() - s * .25f, p.x() + s, p.y() + s * .55f);
        } else if (kind == QStringLiteral("compass")) {
            SDL_RenderLine(renderer_, p.x(), p.y() - s, p.x(), p.y() + s); SDL_RenderLine(renderer_, p.x() - s, p.y(), p.x() + s, p.y()); SDL_RenderLine(renderer_, p.x() - s * .7f, p.y() - s * .7f, p.x() + s * .7f, p.y() + s * .7f); SDL_RenderLine(renderer_, p.x() + s * .7f, p.y() - s * .7f, p.x() - s * .7f, p.y() + s * .7f);
        } else if (kind == QStringLiteral("mill")) {
            SDL_RenderLine(renderer_, p.x(), p.y() - s * .2f, p.x(), p.y() + s); SDL_FRect body{p.x() - s * .35f, p.y() + s * .2f, s * .7f, s * .8f}; SDL_RenderRect(renderer_, &body); SDL_RenderLine(renderer_, p.x() - s, p.y() - s, p.x() + s, p.y() + s * .6f); SDL_RenderLine(renderer_, p.x() + s, p.y() - s, p.x() - s, p.y() + s * .6f);
        } else {
            SDL_FRect rect{p.x() - s * .55f, p.y() - s * .55f, s * 1.1f, s * 1.1f}; SDL_RenderRect(renderer_, &rect);
        }
    }

    void drawStamp(const QJsonObject& object) {
        const QPointF p = worldToScreen(QPointF(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble()));
        const double scale = std::clamp(object.value(QStringLiteral("scale")).toDouble(1.0), .08, 12.0);
        const double rotation = object.value(QStringLiteral("rotation")).toDouble();
        const double worldSize = object.value(QStringLiteral("size")).toDouble(150.0) * scale;
        const float screenSize = static_cast<float>(std::clamp(worldSize * zoom_, 6.0, 360.0));
        if (SDL_Texture* texture = textureForStamp(object)) {
            int tw = 1, th = 1; SDL_GetTextureSize(texture, reinterpret_cast<float*>(&tw), reinterpret_cast<float*>(&th));
            const float aspect = 1.0f;
            SDL_FRect dst{static_cast<float>(p.x() - screenSize * .5f), static_cast<float>(p.y() - screenSize * .5f / aspect), screenSize, screenSize / aspect};
            SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(255 * currentLayerAlpha_ * std::clamp(object.value(QStringLiteral("opacity")).toDouble(1.0), 0.0, 1.0)));
            SDL_RenderTextureRotated(renderer_, texture, nullptr, &dst, rotation, nullptr, SDL_FLIP_NONE);
        } else {
            drawBuiltinStamp(object.value(QStringLiteral("assetKind")).toString(QStringLiteral("castle")), p, screenSize * .5f);
        }
        if (selectedObjectIds_.contains(object.value(QStringLiteral("id")).toString())) {
            setColor(QColor(38, 119, 209)); SDL_FRect box{p.x() - screenSize * .58f, p.y() - screenSize * .58f, screenSize * 1.16f, screenSize * 1.16f}; SDL_RenderRect(renderer_, &box);
        }
    }

    void drawText(const QJsonObject& object, const QString& text, const QPointF& world, bool fallbackBold = false) {
        if (text.trimmed().isEmpty()) return;
        const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        const int worldFont = object.value(QStringLiteral("fontSize")).toInt(54);
        const int pixel = qBound(9, qRound(worldFont * zoom_), 180);
        QFont font(object.value(QStringLiteral("fontFamily")).toString(QStringLiteral("Georgia")), pixel);
        font.setPixelSize(pixel);
        font.setBold(object.value(QStringLiteral("bold")).toBool(fallbackBold));
        font.setItalic(object.value(QStringLiteral("italic")).toBool(false));
        QFontMetrics metrics(font);
        const QSize size = metrics.size(Qt::TextSingleLine, text) + QSize(18, 16);
        QImage image(size, QImage::Format_RGBA8888); image.fill(Qt::transparent);
        QPainter painter(&image); painter.setRenderHint(QPainter::Antialiasing, true); painter.setRenderHint(QPainter::TextAntialiasing, true);
        QPainterPath path; path.addText(9, 8 + metrics.ascent(), font, text);
        QColor fill(object.value(QStringLiteral("color")).toString()); if (!fill.isValid()) fill = themeColor(pilin, QStringLiteral("text"), QColor(40, 37, 32));
        QColor outline = themeColor(pilin, QStringLiteral("labelOutline"), QColor(238, 226, 195)); outline.setAlpha(210);
        painter.setPen(QPen(outline, qMax(1.0, pixel * .08))); painter.setBrush(fill); painter.drawPath(path); painter.end();
        SDL_Texture* texture = nullptr; uploadImageTexture(image, texture); if (!texture) return;
        SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(255 * currentLayerAlpha_));
        const QPointF p = worldToScreen(world);
        SDL_FRect dst{static_cast<float>(p.x()), static_cast<float>(p.y()), static_cast<float>(image.width()), static_cast<float>(image.height())};
        SDL_RenderTextureRotated(renderer_, texture, nullptr, &dst, object.value(QStringLiteral("rotation")).toDouble(), nullptr, SDL_FLIP_NONE);
        SDL_DestroyTexture(texture);
    }

    void drawSettlement(const QJsonObject& object) {
        const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        const QPointF p = worldToScreen(QPointF(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble()));
        const double scale = std::clamp(object.value(QStringLiteral("scale")).toDouble(1.0), .25, 6.0);
        const float r = static_cast<float>(std::clamp(10.0 * scale * std::sqrt(zoom_ + .18), 4.0, 24.0));
        setColor(themeColor(pilin, QStringLiteral("symbol"), QColor(48, 40, 34)));
        const QString kind = object.value(QStringLiteral("kind")).toString();
        SDL_FRect rect{static_cast<float>(p.x() - r), static_cast<float>(p.y() - r), r * 2, r * 2};
        if (kind.contains(QStringLiteral("Capital"), Qt::CaseInsensitive)) { SDL_RenderFillRect(renderer_, &rect); SDL_FRect outer{rect.x - 4, rect.y - 4, rect.w + 8, rect.h + 8}; SDL_RenderRect(renderer_, &outer); }
        else if (kind.contains(QStringLiteral("Puerto"), Qt::CaseInsensitive)) { SDL_RenderRect(renderer_, &rect); SDL_RenderLine(renderer_, p.x(), p.y() - r * 1.6f, p.x(), p.y() + r * 1.6f); }
        else if (kind.contains(QStringLiteral("Fortaleza"), Qt::CaseInsensitive)) { SDL_RenderRect(renderer_, &rect); SDL_RenderLine(renderer_, rect.x, rect.y, rect.x + rect.w, rect.y + rect.h); SDL_RenderLine(renderer_, rect.x + rect.w, rect.y, rect.x, rect.y + rect.h); }
        else SDL_RenderFillRect(renderer_, &rect);
        QJsonObject labelStyle = object; if (!labelStyle.contains(QStringLiteral("fontSize"))) labelStyle.insert(QStringLiteral("fontSize"), kind.contains(QStringLiteral("Capital"), Qt::CaseInsensitive) ? 58 : 46);
        drawText(labelStyle, object.value(QStringLiteral("label")).toString(), QPointF(object.value(QStringLiteral("x")).toDouble() + 26 / zoom_, object.value(QStringLiteral("y")).toDouble() - 12 / zoom_), kind.contains(QStringLiteral("Capital"), Qt::CaseInsensitive));
        if (selectedObjectIds_.contains(object.value(QStringLiteral("id")).toString())) { setColor(QColor(38, 119, 209)); SDL_FRect box{p.x() - r - 5, p.y() - r - 5, r * 2 + 10, r * 2 + 10}; SDL_RenderRect(renderer_, &box); }
    }

    void drawObject(const QJsonObject& object) {
        const QString type = object.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("land") || type == QStringLiteral("sea")) return;
        if (type == QStringLiteral("forestArea")) { drawForestArea(object); return; }
        if (type == QStringLiteral("mountainArea")) { drawMountainArea(object); return; }
        if (type == QStringLiteral("settlement")) { drawSettlement(object); return; }
        if (type == QStringLiteral("stamp")) { drawStamp(object); return; }
        if (type == QStringLiteral("label")) { drawText(object, object.value(QStringLiteral("text")).toString(), QPointF(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble())); return; }

        const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        const std::vector<QPointF> path = smoothPath(object.value(QStringLiteral("points")).toArray());
        if (path.size() < 2) return;
        const int width = object.value(QStringLiteral("width")).toInt(strokeWidth_);
        if (type == QStringLiteral("river")) {
            drawVariableRiver(path, object);
        } else if (type == QStringLiteral("road")) {
            const QColor road = themeColor(pilin, QStringLiteral("road"), QColor(112, 78, 46));
            drawPolyline(path, road.darker(145), qMax(2, qRound((width + 3) * zoom_)));
            drawPolyline(path, road.lighter(150), qMax(1, qRound(width * zoom_)));
        } else if (type == QStringLiteral("border")) {
            drawPolyline(path, themeColor(pilin, QStringLiteral("border"), QColor(145, 63, 55)), qMax(1, qRound(width * zoom_)), true);
        } else if (type == QStringLiteral("region")) {
            drawPolyline(path, themeColor(pilin, QStringLiteral("region"), QColor(113, 91, 57)), qMax(1, qRound(width * .6 * zoom_)), true);
        }

        if (selectedObjectIds_.contains(object.value(QStringLiteral("id")).toString())) {
            setColor(QColor(38, 119, 209));
            const QJsonArray pts = object.value(QStringLiteral("points")).toArray();
            for (int i = 0; i < pts.size(); ++i) {
                if (i == pts.size() - 1 && object.value(QStringLiteral("closed")).toBool(false)) continue;
                const QPointF p = worldToScreen(jsonPoint(pts.at(i)));
                SDL_FRect marker{static_cast<float>(p.x() - 4), static_cast<float>(p.y() - 4), 8, 8}; SDL_RenderFillRect(renderer_, &marker);
            }
        }
    }

    void drawGrid(const SDL_FRect& page, double docW, double docH) {
        if (!grid_) return;
        setColor(QColor(40, 55, 62), 55);
        constexpr double step = 100.0;
        for (double x = 0; x <= docW; x += step) { const QPointF a = worldToScreen(QPointF(x, 0)), b = worldToScreen(QPointF(x, docH)); SDL_RenderLine(renderer_, a.x(), a.y(), b.x(), b.y()); }
        for (double y = 0; y <= docH; y += step) { const QPointF a = worldToScreen(QPointF(0, y)), b = worldToScreen(QPointF(docW, y)); SDL_RenderLine(renderer_, a.x(), a.y(), b.x(), b.y()); }
        Q_UNUSED(page);
    }

    void drawBrushPreview() {
        if (!isBrushTool(tool_)) return;
        const QPointF center = worldToScreen(cursorWorld_);
        const float radius = static_cast<float>(brushRadius_ * zoom_);
        if (radius < 2) return;
        setColor(tool_ == Tool::Eraser ? QColor(80, 130, 160) : QColor(38, 119, 209), 190);
        QPointF prev(center.x() + radius, center.y());
        for (int i = 1; i <= 48; ++i) {
            const double a = kTau * i / 48.0;
            const QPointF next(center.x() + std::cos(a) * radius, center.y() + std::sin(a) * radius);
            SDL_RenderLine(renderer_, prev.x(), prev.y(), next.x(), next.y()); prev = next;
        }
    }

    void renderFrame() {
        if (!isVisible() || !ensureRenderer()) return;
        currentLayerAlpha_ = 1.0;
        SDL_SetRenderDrawColor(renderer_, 38, 40, 44, 255); SDL_RenderClear(renderer_);

        const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        const double docW = pilin.value(QStringLiteral("width")).toDouble(4096);
        const double docH = pilin.value(QStringLiteral("height")).toDouble(2304);
        const QPointF tl = worldToScreen(QPointF(0, 0));
        const QPointF br = worldToScreen(QPointF(docW, docH));
        SDL_FRect page{static_cast<float>(tl.x()), static_cast<float>(tl.y()), static_cast<float>(br.x() - tl.x()), static_cast<float>(br.y() - tl.y())};
        const QColor sea = themeColor(pilin, QStringLiteral("sea"), QColor(177, 194, 198));
        setColor(sea); SDL_RenderFillRect(renderer_, &page); setColor(QColor(25, 28, 30), 180); SDL_RenderRect(renderer_, &page);
        drawGrid(page, docW, docH);

        updateSurfaceTextures();
        if (terrainTexture_) SDL_RenderTexture(renderer_, terrainTexture_, nullptr, &page);
        if (coastTexture_) SDL_RenderTexture(renderer_, coastTexture_, nullptr, &page);
        if (regionTexture_) SDL_RenderTexture(renderer_, regionTexture_, nullptr, &page);

        updateTemplateTexture();
        const QJsonObject templ = pilin.value(QStringLiteral("template")).toObject();
        if (templateTexture_ && templ.value(QStringLiteral("visible")).toBool(true)) {
            const double scale = std::clamp(templ.value(QStringLiteral("scale")).toDouble(1.0), .05, 5.0);
            const float w = static_cast<float>(page.w * scale), h = static_cast<float>(page.h * scale);
            const float x = static_cast<float>(page.x + templ.value(QStringLiteral("x")).toDouble() * zoom_ + (page.w - w) * .5);
            const float y = static_cast<float>(page.y + templ.value(QStringLiteral("y")).toDouble() * zoom_ + (page.h - h) * .5);
            SDL_FRect dst{x, y, w, h};
            SDL_SetTextureAlphaMod(templateTexture_, static_cast<Uint8>(std::clamp(templ.value(QStringLiteral("opacity")).toDouble(.35), 0.0, 1.0) * 255));
            SDL_RenderTextureRotated(renderer_, templateTexture_, nullptr, &dst, templ.value(QStringLiteral("rotation")).toDouble(), nullptr, SDL_FLIP_NONE);
        }

        for (const QJsonValue& lv : pilin.value(QStringLiteral("layers")).toArray()) {
            const QJsonObject layer = lv.toObject();
            if (!layer.value(QStringLiteral("visible")).toBool(true)) continue;
            currentLayerAlpha_ = std::clamp(layer.value(QStringLiteral("opacity")).toDouble(1.0), 0.0, 1.0);
            for (const QJsonValue& ov : layer.value(QStringLiteral("objects")).toArray()) drawObject(ov.toObject());
        }
        currentLayerAlpha_ = 1.0;

        if (!nodePath_.isEmpty()) {
            std::vector<QPointF> preview; for (const QJsonValue& v : nodePath_) preview.push_back(jsonPoint(v)); preview.push_back(cursorWorld_);
            drawPolyline(preview, QColor(38, 119, 209), 2);
        }
        if (drawingBrush_ && !brushStroke_.isEmpty()) {
            QJsonObject preview{{QStringLiteral("id"), QStringLiteral("preview")}, {QStringLiteral("type"), pathType(tool_)}, {QStringLiteral("points"), brushStroke_}, {QStringLiteral("density"), density_}, {QStringLiteral("symbolSize"), symbolSize_}};
            if (tool_ == Tool::Forest) drawForestArea(preview); else if (tool_ == Tool::Mountain) drawMountainArea(preview);
        }
        if (measureActive_) { const QPointF a = worldToScreen(measureStart_), b = worldToScreen(measureEnd_); setColor(QColor(38, 119, 209)); SDL_RenderLine(renderer_, a.x(), a.y(), b.x(), b.y()); }
        drawBrushPreview();
        SDL_RenderPresent(renderer_);
    }

    QJsonObject map_;
    Tool tool_ = Tool::Select;
    bool panning_ = false;
    bool drawingBrush_ = false;
    bool viewInitialized_ = false;
    bool snap_ = false;
    bool grid_ = false;
    bool measureActive_ = false;
    int brushRadius_ = 220;
    int density_ = 58;
    int strokeWidth_ = 5;
    int symbolSize_ = 64;
    double currentLayerAlpha_ = 1.0;
    QPointF pan_{40, 40};
    QPointF lastMouse_;
    QPointF cursorWorld_;
    QPointF measureStart_;
    QPointF measureEnd_;
    double zoom_ = .2;
    QJsonArray brushStroke_;
    QJsonArray nodePath_;
    QStringList selectedObjectIds_;
    QString draggingObjectId_;
    QString draggingPathId_;
    int draggingPathPointIndex_ = -1;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* terrainTexture_ = nullptr;
    SDL_Texture* coastTexture_ = nullptr;
    SDL_Texture* regionTexture_ = nullptr;
    SDL_Texture* templateTexture_ = nullptr;
    QHash<QString, SDL_Texture*> stampTextures_;
    bool surfaceDirty_ = true;
    bool templateDirty_ = true;
};

PilinReyEditor::PilinReyEditor(QWidget* parent) : QWidget(parent) { buildUi(); }
PilinReyEditor::~PilinReyEditor() = default;

void PilinReyEditor::buildUi() {
    setObjectName(QStringLiteral("pilinReyEditor"));
    auto* root = new QVBoxLayout(this); root->setContentsMargins(0, 0, 0, 0); root->setSpacing(0);
    canvasHost_ = new QWidget(this); canvasHost_->setObjectName(QStringLiteral("pilinCanvasHost"));
    auto* canvasLayout = new QVBoxLayout(canvasHost_); canvasLayout->setContentsMargins(0, 0, 0, 0);
    viewport_ = new PilinReyViewport(canvasHost_); canvasLayout->addWidget(viewport_); root->addWidget(canvasHost_, 1);

    toolRail_ = new QFrame(canvasHost_); toolRail_->setObjectName(QStringLiteral("pilinToolPalette"));
    auto* tools = new QGridLayout(toolRail_); tools->setContentsMargins(5, 5, 5, 5); tools->setHorizontalSpacing(2); tools->setVerticalSpacing(2);
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
    int row = 0, col = 0;
    for (const ToolDef& def : defs) {
        auto* button = toolButton(QString::fromUtf8(def.glyph), tr(def.tip), toolRail_, QStringLiteral("pilinMapTool")); button->setCheckable(true); button->setAutoExclusive(true);
        if (def.tool == Tool::Select) button->setChecked(true); if (def.tool == Tool::Stamp) stampToolButton_ = button;
        connect(button, &QToolButton::clicked, this, [this, tool = def.tool]() { setActiveTool(tool); }); toolButtons_.append(button); tools->addWidget(button, row, col); if (++col == 2) { col = 0; ++row; }
    }

    auto* options = new QWidget(toolRail_); auto* optionLayout = new QVBoxLayout(options); optionLayout->setContentsMargins(3, 6, 3, 2); optionLayout->setSpacing(2);
    toolOptionsLabel_ = new QLabel(tr("Herramienta"), options); toolOptionsLabel_->setObjectName(QStringLiteral("pilinToolOptionsLabel"));
    auto addSlider = [&](const QString& name, int min, int max, int value, QSlider*& target) { auto* label = new QLabel(name, options); label->setObjectName(QStringLiteral("pilinTinyLabel")); target = new QSlider(Qt::Horizontal, options); target->setRange(min, max); target->setValue(value); target->setFixedWidth(76); optionLayout->addWidget(label); optionLayout->addWidget(target); };
    optionLayout->addWidget(toolOptionsLabel_);
    addSlider(tr("Tamaño"), 30, 700, brushRadius_, brushRadiusSlider_);
    addSlider(tr("Densidad"), 10, 100, density_, densitySlider_);
    addSlider(tr("Trazo"), 1, 24, strokeWidth_, strokeWidthSlider_);
    addSlider(tr("Símbolo"), 16, 160, symbolSize_, symbolSizeSlider_);
    tools->addWidget(options, row + 1, 0, 1, 2);
    connect(brushRadiusSlider_, &QSlider::valueChanged, this, [this](int v) { brushRadius_ = v; viewport_->setBrushRadius(v); });
    connect(densitySlider_, &QSlider::valueChanged, this, [this](int v) { density_ = v; viewport_->setDensity(v); });
    connect(strokeWidthSlider_, &QSlider::valueChanged, this, [this](int v) { strokeWidth_ = v; viewport_->setStrokeWidth(v); });
    connect(symbolSizeSlider_, &QSlider::valueChanged, this, [this](int v) { symbolSize_ = v; viewport_->setSymbolSize(v); });

    topCommands_ = new QFrame(canvasHost_); topCommands_->setObjectName(QStringLiteral("pilinTopCommands"));
    auto* commands = new QHBoxLayout(topCommands_); commands->setContentsMargins(5, 4, 5, 4); commands->setSpacing(2);
    layersButton_ = toolButton(QStringLiteral("▱"), tr("Capas"), topCommands_, QStringLiteral("pilinCommand"));
    assetsButton_ = toolButton(QStringLiteral("✦"), tr("Assets"), topCommands_, QStringLiteral("pilinCommand"));
    templateButton_ = toolButton(QStringLiteral("▧"), tr("Plantilla"), topCommands_, QStringLiteral("pilinCommand"));
    appearanceButton_ = toolButton(QStringLiteral("◐"), tr("Apariencia"), topCommands_, QStringLiteral("pilinCommand"));
    auto* fitButton = toolButton(QStringLiteral("⌗"), tr("Encajar"), topCommands_, QStringLiteral("pilinCommand"));
    undoButton_ = toolButton(QStringLiteral("↶"), tr("Deshacer"), topCommands_, QStringLiteral("pilinCommand")); redoButton_ = toolButton(QStringLiteral("↷"), tr("Rehacer"), topCommands_, QStringLiteral("pilinCommand"));
    auto* exportButton = toolButton(QStringLiteral("⇩"), tr("Exportar"), topCommands_, QStringLiteral("pilinCommand"));
    snapCheck_ = new QCheckBox(tr("Ajustar"), topCommands_); gridCheck_ = new QCheckBox(tr("Cuadrícula"), topCommands_);
    for (QWidget* w : {static_cast<QWidget*>(layersButton_), assetsButton_, templateButton_, appearanceButton_, fitButton, undoButton_, redoButton_, exportButton}) commands->addWidget(w);
    commands->addWidget(snapCheck_); commands->addWidget(gridCheck_);

    layersPopover_ = new QFrame(canvasHost_); layersPopover_->setObjectName(QStringLiteral("pilinPopover")); layersPopover_->setFixedWidth(290);
    auto* layerLayout = new QVBoxLayout(layersPopover_); layerLayout->setContentsMargins(10, 10, 10, 10); layerLayout->setSpacing(6);
    auto* layerTitle = new QLabel(tr("Capas"), layersPopover_); layerTitle->setObjectName(QStringLiteral("pilinPopoverTitle")); layers_ = new QListWidget(layersPopover_); layers_->setMinimumHeight(190);
    auto* layerButtons = new QHBoxLayout; auto* addLayerButton = toolButton(QStringLiteral("+"), tr("Nueva capa"), layersPopover_, QStringLiteral("pilinCommand")); auto* duplicateLayerButton = toolButton(QStringLiteral("⧉"), tr("Duplicar capa"), layersPopover_, QStringLiteral("pilinCommand")); auto* removeLayerButton = toolButton(QStringLiteral("−"), tr("Eliminar capa"), layersPopover_, QStringLiteral("pilinCommand")); auto* upLayerButton = toolButton(QStringLiteral("↑"), tr("Subir"), layersPopover_, QStringLiteral("pilinCommand")); auto* downLayerButton = toolButton(QStringLiteral("↓"), tr("Bajar"), layersPopover_, QStringLiteral("pilinCommand"));
    layerButtons->addWidget(addLayerButton); layerButtons->addWidget(duplicateLayerButton); layerButtons->addWidget(removeLayerButton); layerButtons->addStretch(); layerButtons->addWidget(upLayerButton); layerButtons->addWidget(downLayerButton);
    layerLocked_ = new QCheckBox(tr("Bloquear"), layersPopover_); layerOpacity_ = new QSlider(Qt::Horizontal, layersPopover_); layerOpacity_->setRange(0, 100); layerOpacity_->setValue(100);
    layerLayout->addWidget(layerTitle); layerLayout->addWidget(layers_); layerLayout->addLayout(layerButtons); layerLayout->addWidget(layerLocked_); layerLayout->addWidget(new QLabel(tr("Opacidad"), layersPopover_)); layerLayout->addWidget(layerOpacity_); layersPopover_->hide();

    assetsPopover_ = new QFrame(canvasHost_); assetsPopover_->setObjectName(QStringLiteral("pilinPopover")); assetsPopover_->setFixedWidth(260);
    auto* assetLayout = new QVBoxLayout(assetsPopover_); assetLayout->setContentsMargins(10, 10, 10, 10); assetLayout->setSpacing(6); auto* assetTitle = new QLabel(tr("Assets cartográficos"), assetsPopover_); assetTitle->setObjectName(QStringLiteral("pilinPopoverTitle"));
    assets_ = new QListWidget(assetsPopover_); assets_->setViewMode(QListView::IconMode); assets_->setResizeMode(QListView::Adjust); assets_->setMovement(QListView::Static); assets_->setGridSize(QSize(74, 56)); assets_->setMinimumHeight(220);
    auto* importAssetButton = new QPushButton(tr("Importar imagen…"), assetsPopover_); assetLayout->addWidget(assetTitle); assetLayout->addWidget(assets_); assetLayout->addWidget(importAssetButton); assetsPopover_->hide();

    templatePopover_ = new QFrame(canvasHost_); templatePopover_->setObjectName(QStringLiteral("pilinPopover")); templatePopover_->setFixedWidth(270);
    auto* templateLayout = new QVBoxLayout(templatePopover_); templateLayout->setContentsMargins(10, 10, 10, 10); templateLayout->setSpacing(6); auto* templateTitle = new QLabel(tr("Plantilla de referencia"), templatePopover_); templateTitle->setObjectName(QStringLiteral("pilinPopoverTitle"));
    auto* templateRow = new QHBoxLayout; auto* loadTemplate = toolButton(QStringLiteral("＋"), tr("Cargar"), templatePopover_, QStringLiteral("pilinCommand")); auto* clearTemplateButton = toolButton(QStringLiteral("×"), tr("Quitar"), templatePopover_, QStringLiteral("pilinCommand")); templateRow->addWidget(loadTemplate); templateRow->addWidget(clearTemplateButton); templateRow->addStretch();
    templateOpacity_ = new QSlider(Qt::Horizontal, templatePopover_); templateOpacity_->setRange(0, 100); templateOpacity_->setValue(35); templateScale_ = new QSlider(Qt::Horizontal, templatePopover_); templateScale_->setRange(10, 300); templateScale_->setValue(100); templateRotation_ = new QSlider(Qt::Horizontal, templatePopover_); templateRotation_->setRange(-180, 180); templateRotation_->setValue(0);
    templateLayout->addWidget(templateTitle); templateLayout->addLayout(templateRow); templateLayout->addWidget(new QLabel(tr("Opacidad"), templatePopover_)); templateLayout->addWidget(templateOpacity_); templateLayout->addWidget(new QLabel(tr("Escala"), templatePopover_)); templateLayout->addWidget(templateScale_); templateLayout->addWidget(new QLabel(tr("Rotación"), templatePopover_)); templateLayout->addWidget(templateRotation_); templatePopover_->hide();

    appearancePopover_ = new QFrame(canvasHost_); appearancePopover_->setObjectName(QStringLiteral("pilinPopover")); appearancePopover_->setFixedWidth(220); auto* appearanceLayout = new QVBoxLayout(appearancePopover_); appearanceLayout->setContentsMargins(10, 10, 10, 10); appearanceLayout->setSpacing(4); auto* appearanceTitle = new QLabel(tr("Apariencia"), appearancePopover_); appearanceTitle->setObjectName(QStringLiteral("pilinPopoverTitle")); appearanceLayout->addWidget(appearanceTitle);
    const QList<QPair<QString, QString>> colors{{QStringLiteral("land"), tr("Tierra")}, {QStringLiteral("sea"), tr("Mar")}, {QStringLiteral("coast"), tr("Costa")}, {QStringLiteral("river"), tr("Ríos")}, {QStringLiteral("road"), tr("Caminos")}, {QStringLiteral("border"), tr("Fronteras")}, {QStringLiteral("forest"), tr("Bosques")}, {QStringLiteral("mountain"), tr("Montañas")}, {QStringLiteral("text"), tr("Texto")}};
    for (const auto& entry : colors) { auto* b = new QPushButton(entry.second, appearancePopover_); connect(b, &QPushButton::clicked, this, [this, key = entry.first]() { setThemeColor(key); }); appearanceLayout->addWidget(b); } appearancePopover_->hide();

    selectionPopover_ = new QFrame(canvasHost_); selectionPopover_->setObjectName(QStringLiteral("pilinSelectionPopover")); selectionPopover_->setFixedWidth(250); auto* selectionLayout = new QVBoxLayout(selectionPopover_); selectionLayout->setContentsMargins(9, 8, 9, 8); selectionLayout->setSpacing(5); selectionLabel_ = new QLabel(tr("Selección"), selectionPopover_); selectionLabel_->setObjectName(QStringLiteral("pilinPopoverTitle"));
    auto* selectionActions = new QHBoxLayout; editObjectButton_ = toolButton(QStringLiteral("✎"), tr("Editar"), selectionPopover_, QStringLiteral("pilinCommand")); duplicateObjectButton_ = toolButton(QStringLiteral("⧉"), tr("Duplicar"), selectionPopover_, QStringLiteral("pilinCommand")); linkAtlasButton_ = toolButton(QStringLiteral("⌁"), tr("Enlazar Atlas"), selectionPopover_, QStringLiteral("pilinCommand")); deleteObjectButton_ = toolButton(QStringLiteral("×"), tr("Eliminar"), selectionPopover_, QStringLiteral("pilinCommand")); selectionActions->addWidget(editObjectButton_); selectionActions->addWidget(duplicateObjectButton_); selectionActions->addWidget(linkAtlasButton_); selectionActions->addStretch(); selectionActions->addWidget(deleteObjectButton_);
    auto* transforms = new QHBoxLayout; rotateLeftButton_ = toolButton(QStringLiteral("↺"), tr("Rotar −15°"), selectionPopover_, QStringLiteral("pilinCommand")); rotateRightButton_ = toolButton(QStringLiteral("↻"), tr("Rotar +15°"), selectionPopover_, QStringLiteral("pilinCommand")); scaleDownButton_ = toolButton(QStringLiteral("−"), tr("Reducir 10%"), selectionPopover_, QStringLiteral("pilinCommand")); scaleUpButton_ = toolButton(QStringLiteral("+"), tr("Aumentar 10%"), selectionPopover_, QStringLiteral("pilinCommand")); transforms->addWidget(rotateLeftButton_); transforms->addWidget(rotateRightButton_); transforms->addStretch(); transforms->addWidget(scaleDownButton_); transforms->addWidget(scaleUpButton_);
    selectionLayout->addWidget(selectionLabel_); selectionLayout->addLayout(selectionActions); selectionLayout->addLayout(transforms); selectionPopover_->hide();

    status_ = new QLabel(tr("Pilín Rey · listo"), canvasHost_); status_->setObjectName(QStringLiteral("pilinStatus")); status_->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    viewport_->onPath = [this](const QString& type, const QJsonArray& points) { addPathObject(type, points); }; viewport_->onSettlement = [this](double x, double y) { addSettlement(x, y); }; viewport_->onStamp = [this](double x, double y) { addStamp(x, y); }; viewport_->onLabel = [this](double x, double y) { addLabel(x, y); }; viewport_->onSelection = [this](const QStringList& ids) { selectObjects(ids); }; viewport_->onMovePoint = [this](const QString& id, double x, double y) { movePointObject(id, x, y); }; viewport_->onMovePathPoint = [this](const QString& id, int index, double x, double y) { movePathPoint(id, index, x, y); }; viewport_->onDeleteSelection = [this]() { deleteSelectedObject(); }; viewport_->onStatus = [this](const QString& text) { status_->setText(text); };

    auto closeOthers = [this](QFrame* keep) { for (QFrame* p : {layersPopover_, assetsPopover_, templatePopover_, appearancePopover_}) if (p && p != keep) p->hide(); };
    connect(layersButton_, &QToolButton::clicked, this, [this, closeOthers]() mutable { closeOthers(layersPopover_); layersPopover_->setVisible(!layersPopover_->isVisible()); layoutFloatingPanels(); });
    connect(assetsButton_, &QToolButton::clicked, this, [this, closeOthers]() mutable { closeOthers(assetsPopover_); assetsPopover_->setVisible(!assetsPopover_->isVisible()); layoutFloatingPanels(); });
    connect(templateButton_, &QToolButton::clicked, this, [this, closeOthers]() mutable { closeOthers(templatePopover_); templatePopover_->setVisible(!templatePopover_->isVisible()); layoutFloatingPanels(); });
    connect(appearanceButton_, &QToolButton::clicked, this, [this, closeOthers]() mutable { closeOthers(appearancePopover_); appearancePopover_->setVisible(!appearancePopover_->isVisible()); layoutFloatingPanels(); });
    connect(fitButton, &QToolButton::clicked, viewport_, &PilinReyViewport::fitDocument); connect(undoButton_, &QToolButton::clicked, this, &PilinReyEditor::undo); connect(redoButton_, &QToolButton::clicked, this, &PilinReyEditor::redo); connect(exportButton, &QToolButton::clicked, this, &PilinReyEditor::exportMap); connect(snapCheck_, &QCheckBox::toggled, viewport_, &PilinReyViewport::setSnap); connect(gridCheck_, &QCheckBox::toggled, viewport_, &PilinReyViewport::setGrid);
    connect(loadTemplate, &QToolButton::clicked, this, &PilinReyEditor::chooseTemplate); connect(clearTemplateButton, &QToolButton::clicked, this, &PilinReyEditor::clearTemplate);
    connect(templateOpacity_, &QSlider::valueChanged, this, [this](int value) { if (!refreshing_) mutateDocument([&](QJsonObject& p) { QJsonObject t = p.value(QStringLiteral("template")).toObject(); t.insert(QStringLiteral("opacity"), value / 100.0); p.insert(QStringLiteral("template"), t); }); });
    connect(templateScale_, &QSlider::valueChanged, this, [this](int value) { if (!refreshing_) mutateDocument([&](QJsonObject& p) { QJsonObject t = p.value(QStringLiteral("template")).toObject(); t.insert(QStringLiteral("scale"), value / 100.0); p.insert(QStringLiteral("template"), t); }); });
    connect(templateRotation_, &QSlider::valueChanged, this, [this](int value) { if (!refreshing_) mutateDocument([&](QJsonObject& p) { QJsonObject t = p.value(QStringLiteral("template")).toObject(); t.insert(QStringLiteral("rotation"), value); p.insert(QStringLiteral("template"), t); }); });
    connect(addLayerButton, &QToolButton::clicked, this, &PilinReyEditor::addLayer); connect(duplicateLayerButton, &QToolButton::clicked, this, &PilinReyEditor::duplicateLayer); connect(removeLayerButton, &QToolButton::clicked, this, &PilinReyEditor::removeLayer); connect(upLayerButton, &QToolButton::clicked, this, [this]() { moveLayer(-1); }); connect(downLayerButton, &QToolButton::clicked, this, [this]() { moveLayer(1); }); connect(layerLocked_, &QCheckBox::toggled, this, [this](bool checked) { if (!refreshing_) setLayerLocked(checked); }); connect(layerOpacity_, &QSlider::valueChanged, this, [this](int value) { if (!refreshing_) setLayerOpacity(value); });
    connect(layers_, &QListWidget::currentRowChanged, this, [this](int row) { if (refreshing_ || row < 0) return; QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject(); const QJsonArray a = p.value(QStringLiteral("layers")).toArray(); if (row >= a.size()) return; p.insert(QStringLiteral("activeLayerId"), a.at(row).toObject().value(QStringLiteral("id")).toString()); map_.insert(QStringLiteral("pilinRey"), p); refreshLayerControls(); persistToArchive(); });
    connect(layers_, &QListWidget::itemChanged, this, &PilinReyEditor::applyLayerItem);
    connect(importAssetButton, &QPushButton::clicked, this, &PilinReyEditor::importAsset); connect(assets_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) { if (!item) return; selectedAssetKind_ = item->data(Qt::UserRole).toString(); selectedAssetData_ = item->data(Qt::UserRole + 1).toString(); setActiveTool(Tool::Stamp); if (stampToolButton_) stampToolButton_->setChecked(true); assetsPopover_->hide(); });
    connect(editObjectButton_, &QToolButton::clicked, this, &PilinReyEditor::editSelectedObject); connect(duplicateObjectButton_, &QToolButton::clicked, this, &PilinReyEditor::duplicateSelectedObject); connect(linkAtlasButton_, &QToolButton::clicked, this, &PilinReyEditor::linkSelectedToAtlas); connect(deleteObjectButton_, &QToolButton::clicked, this, &PilinReyEditor::deleteSelectedObject); connect(rotateLeftButton_, &QToolButton::clicked, this, [this]() { transformSelection(1.0, -15.0); }); connect(rotateRightButton_, &QToolButton::clicked, this, [this]() { transformSelection(1.0, 15.0); }); connect(scaleDownButton_, &QToolButton::clicked, this, [this]() { transformSelection(.9, 0.0); }); connect(scaleUpButton_, &QToolButton::clicked, this, [this]() { transformSelection(1.1, 0.0); });

    auto* undoShortcut = new QShortcut(QKeySequence::Undo, this); connect(undoShortcut, &QShortcut::activated, this, &PilinReyEditor::undo); auto* redoShortcut = new QShortcut(QKeySequence::Redo, this); connect(redoShortcut, &QShortcut::activated, this, &PilinReyEditor::redo); auto* copyShortcut = new QShortcut(QKeySequence::Copy, this); connect(copyShortcut, &QShortcut::activated, this, &PilinReyEditor::copySelectedObject); auto* pasteShortcut = new QShortcut(QKeySequence::Paste, this); connect(pasteShortcut, &QShortcut::activated, this, &PilinReyEditor::pasteCopiedObject); auto* dupShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_D), this); connect(dupShortcut, &QShortcut::activated, this, &PilinReyEditor::duplicateSelectedObject);

    setStyleSheet(QStringLiteral(
        "#pilinReyEditor,#pilinCanvasHost{background:#26282c;}"
        "#pilinToolPalette,#pilinTopCommands,#pilinPopover,#pilinSelectionPopover{background:palette(base);color:palette(text);border:1px solid palette(mid);border-radius:6px;}"
        "QToolButton#pilinMapTool,QToolButton#pilinCommand{background:transparent;color:palette(text);border:0;border-radius:4px;font-size:12pt;}"
        "QToolButton#pilinMapTool:hover,QToolButton#pilinCommand:hover{background:palette(alternate-base);}"
        "QToolButton#pilinMapTool:checked{background:#1769c2;color:white;}"
        "#pilinPopoverTitle{font-weight:700;} #pilinTinyLabel{font-size:8pt;color:palette(mid-text);} #pilinToolOptionsLabel{font-size:8pt;font-weight:700;}"
        "#pilinStatus{background:rgba(18,21,24,.82);color:white;border-radius:4px;padding:4px 7px;}"
    ));
}

void PilinReyEditor::resizeEvent(QResizeEvent* event) { QWidget::resizeEvent(event); layoutFloatingPanels(); }
void PilinReyEditor::showEvent(QShowEvent* event) { QWidget::showEvent(event); layoutFloatingPanels(); }

void PilinReyEditor::layoutFloatingPanels() {
    if (!canvasHost_) return;
    const int m = 12;
    if (toolRail_) { toolRail_->adjustSize(); toolRail_->move(m, m); toolRail_->raise(); }
    if (topCommands_) { topCommands_->adjustSize(); topCommands_->move(qMax(m, canvasHost_->width() - topCommands_->width() - m), m); topCommands_->raise(); }
    int y = m + (topCommands_ ? topCommands_->height() : 0) + 8;
    for (QFrame* panel : {layersPopover_, assetsPopover_, templatePopover_, appearancePopover_}) if (panel && panel->isVisible()) { panel->adjustSize(); panel->move(qMax(m, canvasHost_->width() - panel->width() - m), y); panel->raise(); y += panel->height() + 8; }
    if (selectionPopover_ && selectionPopover_->isVisible()) { selectionPopover_->adjustSize(); selectionPopover_->move(qMax(m, canvasHost_->width() - selectionPopover_->width() - m), qMax(m, canvasHost_->height() - selectionPopover_->height() - 44)); selectionPopover_->raise(); }
    if (status_) { status_->adjustSize(); status_->move(m, qMax(m, canvasHost_->height() - status_->height() - m)); status_->raise(); }
}

void PilinReyEditor::setMap(const QJsonObject& map) {
    map_ = map; ensurePilinDocument(); undoStack_.clear(); redoStack_.clear(); selectedObjectIds_.clear();
    const QJsonObject templ = map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("template")).toObject(); refreshing_ = true; templateOpacity_->setValue(qRound(templ.value(QStringLiteral("opacity")).toDouble(.35) * 100)); templateScale_->setValue(qRound(templ.value(QStringLiteral("scale")).toDouble(1.0) * 100)); templateRotation_->setValue(qRound(templ.value(QStringLiteral("rotation")).toDouble())); refreshing_ = false;
    refreshLayers(); refreshAssets(); refreshSelectionControls(); refreshViewport();
}

void PilinReyEditor::ensurePilinDocument() {
    QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject(); p.insert(QStringLiteral("version"), 6);
    if (!p.contains(QStringLiteral("width"))) p.insert(QStringLiteral("width"), 4096); if (!p.contains(QStringLiteral("height"))) p.insert(QStringLiteral("height"), 2304);
    QJsonObject theme = p.value(QStringLiteral("theme")).toObject(); const QList<QPair<QString, QString>> defaults{{QStringLiteral("land"), QStringLiteral("#d8c99e")}, {QStringLiteral("sea"), QStringLiteral("#b1c2c6")}, {QStringLiteral("coast"), QStringLiteral("#413b30")}, {QStringLiteral("river"), QStringLiteral("#25506f")}, {QStringLiteral("road"), QStringLiteral("#70502f")}, {QStringLiteral("border"), QStringLiteral("#913f37")}, {QStringLiteral("forest"), QStringLiteral("#375635")}, {QStringLiteral("mountain"), QStringLiteral("#4d443b")}, {QStringLiteral("region"), QStringLiteral("#b07548")}, {QStringLiteral("symbol"), QStringLiteral("#302822")}, {QStringLiteral("text"), QStringLiteral("#282520")}, {QStringLiteral("labelOutline"), QStringLiteral("#eee2c3")}}; for (const auto& d : defaults) if (!theme.contains(d.first)) theme.insert(d.first, d.second); p.insert(QStringLiteral("theme"), theme);
    QJsonObject templ = p.value(QStringLiteral("template")).toObject(); if (!templ.contains(QStringLiteral("dataUrl"))) templ.insert(QStringLiteral("dataUrl"), map_.value(QStringLiteral("backgroundImageDataUrl")).toString()); if (!templ.contains(QStringLiteral("opacity"))) templ.insert(QStringLiteral("opacity"), .35); if (!templ.contains(QStringLiteral("visible"))) templ.insert(QStringLiteral("visible"), true); if (!templ.contains(QStringLiteral("scale"))) templ.insert(QStringLiteral("scale"), 1.0); if (!templ.contains(QStringLiteral("rotation"))) templ.insert(QStringLiteral("rotation"), 0.0); if (!templ.contains(QStringLiteral("x"))) templ.insert(QStringLiteral("x"), 0.0); if (!templ.contains(QStringLiteral("y"))) templ.insert(QStringLiteral("y"), 0.0); p.insert(QStringLiteral("template"), templ);
    QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); if (layers.isEmpty()) { const QStringList names{tr("Terreno"), tr("Hidrografía"), tr("Relieve y vegetación"), tr("Fronteras y regiones"), tr("Caminos"), tr("Asentamientos y assets"), tr("Etiquetas")}; for (const QString& name : names) layers.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("layer"))}, {QStringLiteral("name"), name}, {QStringLiteral("visible"), true}, {QStringLiteral("locked"), false}, {QStringLiteral("opacity"), 1.0}, {QStringLiteral("objects"), QJsonArray()}}); p.insert(QStringLiteral("activeLayerId"), layers.first().toObject().value(QStringLiteral("id")).toString()); }
    p.insert(QStringLiteral("layers"), layers); if (!p.contains(QStringLiteral("activeLayerId")) && !layers.isEmpty()) p.insert(QStringLiteral("activeLayerId"), layers.first().toObject().value(QStringLiteral("id")).toString()); if (!p.contains(QStringLiteral("assets"))) p.insert(QStringLiteral("assets"), QJsonArray()); map_.insert(QStringLiteral("pilinRey"), p);
}

void PilinReyEditor::refreshLayers() {
    refreshing_ = true; layers_->clear(); const QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject(); const QString active = p.value(QStringLiteral("activeLayerId")).toString(); const QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); int current = 0;
    for (int i = 0; i < layers.size(); ++i) { const QJsonObject layer = layers.at(i).toObject(); auto* item = new QListWidgetItem(layer.value(QStringLiteral("name")).toString(tr("Capa"))); item->setData(Qt::UserRole, layer.value(QStringLiteral("id")).toString()); item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable); item->setCheckState(layer.value(QStringLiteral("visible")).toBool(true) ? Qt::Checked : Qt::Unchecked); layers_->addItem(item); if (layer.value(QStringLiteral("id")).toString() == active) current = i; }
    if (layers_->count()) layers_->setCurrentRow(current); refreshing_ = false; refreshLayerControls(); undoButton_->setEnabled(!undoStack_.isEmpty()); redoButton_->setEnabled(!redoStack_.isEmpty());
}

void PilinReyEditor::refreshLayerControls() { const bool old = refreshing_; refreshing_ = true; const QString active = activeLayerId(); bool found = false; for (const QJsonValue& v : map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray()) { const QJsonObject layer = v.toObject(); if (layer.value(QStringLiteral("id")).toString() != active) continue; layerLocked_->setChecked(layer.value(QStringLiteral("locked")).toBool(false)); layerOpacity_->setValue(qRound(layer.value(QStringLiteral("opacity")).toDouble(1) * 100)); found = true; break; } layerLocked_->setEnabled(found); layerOpacity_->setEnabled(found); refreshing_ = old; }

void PilinReyEditor::refreshAssets() {
    refreshing_ = true; assets_->clear(); const QList<QPair<QString, QString>> builtins{{QStringLiteral("castle"), tr("Castillo")}, {QStringLiteral("tower"), tr("Torre")}, {QStringLiteral("temple"), tr("Templo")}, {QStringLiteral("ruin"), tr("Ruina")}, {QStringLiteral("ship"), tr("Barco")}, {QStringLiteral("bridge"), tr("Puente")}, {QStringLiteral("compass"), tr("Brújula")}, {QStringLiteral("mill"), tr("Molino")}};
    for (const auto& a : builtins) { auto* item = new QListWidgetItem(QStringLiteral("✦\n") + a.second); item->setTextAlignment(Qt::AlignCenter); item->setData(Qt::UserRole, a.first); assets_->addItem(item); }
    for (const QJsonValue& v : map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("assets")).toArray()) { const QJsonObject a = v.toObject(); auto* item = new QListWidgetItem(QStringLiteral("▧\n") + a.value(QStringLiteral("name")).toString(tr("Asset"))); item->setTextAlignment(Qt::AlignCenter); item->setData(Qt::UserRole, QStringLiteral("custom")); item->setData(Qt::UserRole + 1, a.value(QStringLiteral("dataUrl")).toString()); assets_->addItem(item); }
    refreshing_ = false;
}

void PilinReyEditor::refreshSelectionControls() {
    const QJsonArray objects = selectedObjects(); const bool has = !objects.isEmpty(); selectionPopover_->setVisible(has); editObjectButton_->setEnabled(objects.size() == 1); duplicateObjectButton_->setEnabled(has); deleteObjectButton_->setEnabled(has); linkAtlasButton_->setEnabled(objects.size() == 1 && objects.first().toObject().value(QStringLiteral("type")).toString() == QStringLiteral("settlement"));
    selectionLabel_->setText(has ? (objects.size() == 1 ? tr("Selección · %1").arg(objects.first().toObject().value(QStringLiteral("label")).toString(objects.first().toObject().value(QStringLiteral("text")).toString(objects.first().toObject().value(QStringLiteral("type")).toString()))) : tr("%1 objetos seleccionados").arg(objects.size())) : tr("Selección")); layoutFloatingPanels();
}

void PilinReyEditor::refreshViewport() { if (viewport_) { viewport_->setDocument(map_); viewport_->setSelection(selectedObjectIds_); } }
void PilinReyEditor::chooseTemplate() { const QString path = QFileDialog::getOpenFileName(this, tr("Plantilla"), QString(), tr("Imágenes (*.png *.jpg *.jpeg *.webp)")); if (path.isEmpty()) return; const QString data = imageToDataUrl(path); if (data.isEmpty()) return; mutateDocument([&](QJsonObject& p) { QJsonObject t = p.value(QStringLiteral("template")).toObject(); t.insert(QStringLiteral("dataUrl"), data); t.insert(QStringLiteral("visible"), true); p.insert(QStringLiteral("template"), t); }); map_.insert(QStringLiteral("backgroundImageDataUrl"), data); persistToArchive(); }
void PilinReyEditor::clearTemplate() { mutateDocument([](QJsonObject& p) { QJsonObject t = p.value(QStringLiteral("template")).toObject(); t.insert(QStringLiteral("dataUrl"), QString()); p.insert(QStringLiteral("template"), t); }); map_.remove(QStringLiteral("backgroundImageDataUrl")); persistToArchive(); }
void PilinReyEditor::rotateTemplate(double delta) { mutateDocument([&](QJsonObject& p) { QJsonObject t = p.value(QStringLiteral("template")).toObject(); t.insert(QStringLiteral("rotation"), t.value(QStringLiteral("rotation")).toDouble() + delta); p.insert(QStringLiteral("template"), t); }); }
void PilinReyEditor::scaleTemplate(double factor) { mutateDocument([&](QJsonObject& p) { QJsonObject t = p.value(QStringLiteral("template")).toObject(); t.insert(QStringLiteral("scale"), std::clamp(t.value(QStringLiteral("scale")).toDouble(1.0) * factor, .05, 5.0)); p.insert(QStringLiteral("template"), t); }); }

void PilinReyEditor::addLayer() { bool ok = false; const QString name = QInputDialog::getText(this, tr("Nueva capa"), tr("Nombre:"), QLineEdit::Normal, tr("Nueva capa"), &ok).trimmed(); if (!ok || name.isEmpty()) return; mutateDocument([&](QJsonObject& p) { QJsonArray a = p.value(QStringLiteral("layers")).toArray(); const QString id = uid(QStringLiteral("layer")); a.append(QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("name"), name}, {QStringLiteral("visible"), true}, {QStringLiteral("locked"), false}, {QStringLiteral("opacity"), 1.0}, {QStringLiteral("objects"), QJsonArray()}}); p.insert(QStringLiteral("layers"), a); p.insert(QStringLiteral("activeLayerId"), id); }); }
void PilinReyEditor::duplicateLayer() { const int row = layers_->currentRow(); if (row < 0) return; mutateDocument([&](QJsonObject& p) { QJsonArray a = p.value(QStringLiteral("layers")).toArray(); if (row >= a.size()) return; QJsonObject copy = a.at(row).toObject(); const QString id = uid(QStringLiteral("layer")); copy.insert(QStringLiteral("id"), id); copy.insert(QStringLiteral("name"), copy.value(QStringLiteral("name")).toString() + tr(" copia")); QJsonArray objects = copy.value(QStringLiteral("objects")).toArray(); for (int i = 0; i < objects.size(); ++i) { QJsonObject o = objects.at(i).toObject(); o.insert(QStringLiteral("id"), uid(QStringLiteral("mapobj"))); objects.replace(i, o); } copy.insert(QStringLiteral("objects"), objects); a.insert(row + 1, copy); p.insert(QStringLiteral("layers"), a); p.insert(QStringLiteral("activeLayerId"), id); }); }
void PilinReyEditor::removeLayer() { const int row = layers_->currentRow(); if (row < 0) return; mutateDocument([&](QJsonObject& p) { QJsonArray a = p.value(QStringLiteral("layers")).toArray(); if (a.size() <= 1 || row >= a.size()) return; a.removeAt(row); p.insert(QStringLiteral("layers"), a); p.insert(QStringLiteral("activeLayerId"), a.at(qMin(row, a.size() - 1)).toObject().value(QStringLiteral("id")).toString()); }); }
void PilinReyEditor::moveLayer(int delta) { const int row = layers_->currentRow(), target = row + delta; if (row < 0) return; mutateDocument([&](QJsonObject& p) { QJsonArray a = p.value(QStringLiteral("layers")).toArray(); if (target < 0 || target >= a.size()) return; const QJsonValue v = a.at(row); a.removeAt(row); a.insert(target, v); p.insert(QStringLiteral("layers"), a); }); }
void PilinReyEditor::setLayerLocked(bool locked) { const QString active = activeLayerId(); mutateDocument([&](QJsonObject& p) { QJsonArray a = p.value(QStringLiteral("layers")).toArray(); for (int i = 0; i < a.size(); ++i) { QJsonObject l = a.at(i).toObject(); if (l.value(QStringLiteral("id")).toString() != active) continue; l.insert(QStringLiteral("locked"), locked); a.replace(i, l); break; } p.insert(QStringLiteral("layers"), a); }); }
void PilinReyEditor::setLayerOpacity(int value) { const QString active = activeLayerId(); mutateDocument([&](QJsonObject& p) { QJsonArray a = p.value(QStringLiteral("layers")).toArray(); for (int i = 0; i < a.size(); ++i) { QJsonObject l = a.at(i).toObject(); if (l.value(QStringLiteral("id")).toString() != active) continue; l.insert(QStringLiteral("opacity"), value / 100.0); a.replace(i, l); break; } p.insert(QStringLiteral("layers"), a); }); }
void PilinReyEditor::applyLayerItem(QListWidgetItem* item) { if (refreshing_ || !item) return; const QString id = item->data(Qt::UserRole).toString(); const QString name = item->text().trimmed(); const bool visible = item->checkState() == Qt::Checked; mutateDocument([&](QJsonObject& p) { QJsonArray a = p.value(QStringLiteral("layers")).toArray(); for (int i = 0; i < a.size(); ++i) { QJsonObject l = a.at(i).toObject(); if (l.value(QStringLiteral("id")).toString() != id) continue; l.insert(QStringLiteral("name"), name.isEmpty() ? tr("Capa") : name); l.insert(QStringLiteral("visible"), visible); a.replace(i, l); break; } p.insert(QStringLiteral("layers"), a); }); }
void PilinReyEditor::setActiveTool(Tool tool) { viewport_->setTool(tool); toolOptionsLabel_->setText(tool == Tool::Forest ? tr("Bosque") : tool == Tool::Mountain ? tr("Montaña") : tool == Tool::River ? tr("Río") : tool == Tool::Road ? tr("Camino") : tool == Tool::Border ? tr("Frontera") : tool == Tool::Coast ? tr("Tierra") : tool == Tool::Eraser ? tr("Mar") : tr("Herramienta")); }
QString PilinReyEditor::activeLayerId() const { return map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("activeLayerId")).toString(); }

void PilinReyEditor::addPathObject(const QString& type, const QJsonArray& points) {
    if (type.isEmpty() || points.isEmpty()) return;
    mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); int target = layers_->currentRow(); if (target < 0 || target >= layers.size()) target = 0; QJsonObject layer = layers.at(target).toObject(); if (layer.value(QStringLiteral("locked")).toBool(false)) return; QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); const QString id = uid(QStringLiteral("mapobj")); QJsonObject object{{QStringLiteral("id"), id}, {QStringLiteral("type"), type}, {QStringLiteral("points"), points}, {QStringLiteral("width"), strokeWidth_}, {QStringLiteral("density"), density_}, {QStringLiteral("symbolSize"), symbolSize_}}; if (type == QStringLiteral("region")) { object.insert(QStringLiteral("closed"), true); object.insert(QStringLiteral("fillOpacity"), .18); } objects.append(object); layer.insert(QStringLiteral("objects"), objects); layers.replace(target, layer); p.insert(QStringLiteral("layers"), layers); selectedObjectIds_ = {id}; });
}

void PilinReyEditor::addSettlement(double x, double y) { bool ok = false; const QString name = QInputDialog::getText(this, tr("Asentamiento"), tr("Nombre:"), QLineEdit::Normal, tr("Poblado"), &ok).trimmed(); if (!ok || name.isEmpty()) return; const QStringList kinds{tr("Capital"), tr("Ciudad"), tr("Villa"), tr("Pueblo"), tr("Aldea"), tr("Puerto"), tr("Fortaleza"), tr("Ruina")}; const QString kind = QInputDialog::getItem(this, tr("Asentamiento"), tr("Tipo:"), kinds, 3, false, &ok); if (!ok) return; mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); int target = layers_->currentRow(); if (target < 0 || target >= layers.size()) target = layers.size() - 2; QJsonObject layer = layers.at(target).toObject(); if (layer.value(QStringLiteral("locked")).toBool(false)) return; QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); const QString id = uid(QStringLiteral("mapobj")); objects.append(QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("type"), QStringLiteral("settlement")}, {QStringLiteral("kind"), kind}, {QStringLiteral("label"), name}, {QStringLiteral("x"), x}, {QStringLiteral("y"), y}, {QStringLiteral("scale"), 1.0}}); layer.insert(QStringLiteral("objects"), objects); layers.replace(target, layer); p.insert(QStringLiteral("layers"), layers); selectedObjectIds_ = {id}; }); }

void PilinReyEditor::addStamp(double x, double y) { mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); int target = layers_->currentRow(); if (target < 0 || target >= layers.size()) target = layers.size() - 2; QJsonObject layer = layers.at(target).toObject(); if (layer.value(QStringLiteral("locked")).toBool(false)) return; QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); const QString id = uid(QStringLiteral("mapobj")); QJsonObject stamp{{QStringLiteral("id"), id}, {QStringLiteral("type"), QStringLiteral("stamp")}, {QStringLiteral("assetKind"), selectedAssetKind_}, {QStringLiteral("x"), x}, {QStringLiteral("y"), y}, {QStringLiteral("scale"), 1.0}, {QStringLiteral("rotation"), 0.0}, {QStringLiteral("size"), symbolSize_ * 2.2}, {QStringLiteral("opacity"), 1.0}}; if (!selectedAssetData_.isEmpty()) stamp.insert(QStringLiteral("dataUrl"), selectedAssetData_); objects.append(stamp); layer.insert(QStringLiteral("objects"), objects); layers.replace(target, layer); p.insert(QStringLiteral("layers"), layers); selectedObjectIds_ = {id}; }); }

void PilinReyEditor::addLabel(double x, double y) { bool ok = false; const QString text = QInputDialog::getText(this, tr("Etiqueta"), tr("Texto:"), QLineEdit::Normal, QString(), &ok).trimmed(); if (!ok || text.isEmpty()) return; const int size = QInputDialog::getInt(this, tr("Etiqueta"), tr("Tamaño:"), 54, 18, 220, 2, &ok); if (!ok) return; mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); int target = layers_->currentRow(); if (target < 0 || target >= layers.size()) target = layers.size() - 1; QJsonObject layer = layers.at(target).toObject(); if (layer.value(QStringLiteral("locked")).toBool(false)) return; QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); const QString id = uid(QStringLiteral("mapobj")); objects.append(QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("type"), QStringLiteral("label")}, {QStringLiteral("text"), text}, {QStringLiteral("x"), x}, {QStringLiteral("y"), y}, {QStringLiteral("fontSize"), size}, {QStringLiteral("rotation"), 0.0}, {QStringLiteral("bold"), false}}); layer.insert(QStringLiteral("objects"), objects); layers.replace(target, layer); p.insert(QStringLiteral("layers"), layers); selectedObjectIds_ = {id}; }); }

void PilinReyEditor::selectObjects(const QStringList& ids) { selectedObjectIds_ = ids; refreshSelectionControls(); if (viewport_) viewport_->setSelection(ids); }
QJsonObject PilinReyEditor::selectedObject() const { if (selectedObjectIds_.isEmpty()) return {}; const QString id = selectedObjectIds_.first(); for (const QJsonValue& lv : map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray()) for (const QJsonValue& ov : lv.toObject().value(QStringLiteral("objects")).toArray()) { const QJsonObject o = ov.toObject(); if (o.value(QStringLiteral("id")).toString() == id) return o; } return {}; }
QJsonArray PilinReyEditor::selectedObjects() const { QJsonArray out; for (const QString& id : selectedObjectIds_) for (const QJsonValue& lv : map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray()) for (const QJsonValue& ov : lv.toObject().value(QStringLiteral("objects")).toArray()) { const QJsonObject o = ov.toObject(); if (o.value(QStringLiteral("id")).toString() == id) { out.append(o); break; } } return out; }

void PilinReyEditor::movePointObject(const QString& id, double x, double y) { mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); for (int i = 0; i < layers.size(); ++i) { QJsonObject l = layers.at(i).toObject(); QJsonArray objects = l.value(QStringLiteral("objects")).toArray(); for (int j = 0; j < objects.size(); ++j) { QJsonObject o = objects.at(j).toObject(); if (o.value(QStringLiteral("id")).toString() != id) continue; o.insert(QStringLiteral("x"), x); o.insert(QStringLiteral("y"), y); objects.replace(j, o); l.insert(QStringLiteral("objects"), objects); layers.replace(i, l); p.insert(QStringLiteral("layers"), layers); emit markerMoved(id, x, y); return; } } }); }
void PilinReyEditor::movePathPoint(const QString& id, int index, double x, double y) { mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); for (int i = 0; i < layers.size(); ++i) { QJsonObject l = layers.at(i).toObject(); QJsonArray objects = l.value(QStringLiteral("objects")).toArray(); for (int j = 0; j < objects.size(); ++j) { QJsonObject o = objects.at(j).toObject(); if (o.value(QStringLiteral("id")).toString() != id) continue; QJsonArray pts = o.value(QStringLiteral("points")).toArray(); if (index < 0 || index >= pts.size()) return; QJsonObject point = pts.at(index).toObject(); point.insert(QStringLiteral("x"), x); point.insert(QStringLiteral("y"), y); pts.replace(index, point); if (o.value(QStringLiteral("closed")).toBool(false) && pts.size() > 2) { if (index == 0) pts.replace(pts.size() - 1, point); else if (index == pts.size() - 1) pts.replace(0, point); } o.insert(QStringLiteral("points"), pts); objects.replace(j, o); l.insert(QStringLiteral("objects"), objects); layers.replace(i, l); p.insert(QStringLiteral("layers"), layers); return; } } }); }

void PilinReyEditor::editSelectedObject() {
    QJsonObject selected = selectedObject(); if (selected.isEmpty() || selectedObjectIds_.size() != 1) return; const QString id = selectedObjectIds_.first(); const QString type = selected.value(QStringLiteral("type")).toString(); bool ok = false;
    if (type == QStringLiteral("label")) { const QString text = QInputDialog::getText(this, tr("Editar etiqueta"), tr("Texto:"), QLineEdit::Normal, selected.value(QStringLiteral("text")).toString(), &ok).trimmed(); if (!ok || text.isEmpty()) return; const int size = QInputDialog::getInt(this, tr("Editar etiqueta"), tr("Tamaño:"), selected.value(QStringLiteral("fontSize")).toInt(54), 18, 220, 2, &ok); if (!ok) return; const double rotation = QInputDialog::getDouble(this, tr("Editar etiqueta"), tr("Rotación:"), selected.value(QStringLiteral("rotation")).toDouble(), -180, 180, 1, &ok); if (!ok) return; mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); for (int i=0;i<layers.size();++i){QJsonObject l=layers.at(i).toObject();QJsonArray os=l.value(QStringLiteral("objects")).toArray();for(int j=0;j<os.size();++j){QJsonObject o=os.at(j).toObject();if(o.value(QStringLiteral("id")).toString()!=id)continue;o.insert(QStringLiteral("text"),text);o.insert(QStringLiteral("fontSize"),size);o.insert(QStringLiteral("rotation"),rotation);os.replace(j,o);l.insert(QStringLiteral("objects"),os);layers.replace(i,l);p.insert(QStringLiteral("layers"),layers);return;}} }); return; }
    if (type == QStringLiteral("settlement")) { const QString text = QInputDialog::getText(this, tr("Editar asentamiento"), tr("Nombre:"), QLineEdit::Normal, selected.value(QStringLiteral("label")).toString(), &ok).trimmed(); if (!ok || text.isEmpty()) return; const QStringList kinds{tr("Capital"),tr("Ciudad"),tr("Villa"),tr("Pueblo"),tr("Aldea"),tr("Puerto"),tr("Fortaleza"),tr("Ruina")}; const QString kind=QInputDialog::getItem(this,tr("Editar asentamiento"),tr("Tipo:"),kinds,qMax(0,kinds.indexOf(selected.value(QStringLiteral("kind")).toString())),false,&ok);if(!ok)return; mutateDocument([&](QJsonObject& p){QJsonArray layers=p.value(QStringLiteral("layers")).toArray();for(int i=0;i<layers.size();++i){QJsonObject l=layers.at(i).toObject();QJsonArray os=l.value(QStringLiteral("objects")).toArray();for(int j=0;j<os.size();++j){QJsonObject o=os.at(j).toObject();if(o.value(QStringLiteral("id")).toString()!=id)continue;o.insert(QStringLiteral("label"),text);o.insert(QStringLiteral("kind"),kind);os.replace(j,o);l.insert(QStringLiteral("objects"),os);layers.replace(i,l);p.insert(QStringLiteral("layers"),layers);return;}}}); return; }
    if (type == QStringLiteral("stamp")) { const int scale = QInputDialog::getInt(this, tr("Editar asset"), tr("Escala %:"), qRound(selected.value(QStringLiteral("scale")).toDouble(1.0)*100), 10, 800, 5, &ok); if(!ok)return; const double rot=QInputDialog::getDouble(this,tr("Editar asset"),tr("Rotación:"),selected.value(QStringLiteral("rotation")).toDouble(),-180,180,1,&ok);if(!ok)return; mutateDocument([&](QJsonObject& p){QJsonArray layers=p.value(QStringLiteral("layers")).toArray();for(int i=0;i<layers.size();++i){QJsonObject l=layers.at(i).toObject();QJsonArray os=l.value(QStringLiteral("objects")).toArray();for(int j=0;j<os.size();++j){QJsonObject o=os.at(j).toObject();if(o.value(QStringLiteral("id")).toString()!=id)continue;o.insert(QStringLiteral("scale"),scale/100.0);o.insert(QStringLiteral("rotation"),rot);os.replace(j,o);l.insert(QStringLiteral("objects"),os);layers.replace(i,l);p.insert(QStringLiteral("layers"),layers);return;}}}); return; }
    if (type == QStringLiteral("river") || type == QStringLiteral("road") || type == QStringLiteral("border") || type == QStringLiteral("region")) { const int width=QInputDialog::getInt(this,tr("Editar trazado"),tr("Grosor:"),selected.value(QStringLiteral("width")).toInt(5),1,40,1,&ok);if(!ok)return; mutateDocument([&](QJsonObject& p){QJsonArray layers=p.value(QStringLiteral("layers")).toArray();for(int i=0;i<layers.size();++i){QJsonObject l=layers.at(i).toObject();QJsonArray os=l.value(QStringLiteral("objects")).toArray();for(int j=0;j<os.size();++j){QJsonObject o=os.at(j).toObject();if(o.value(QStringLiteral("id")).toString()!=id)continue;o.insert(QStringLiteral("width"),width);os.replace(j,o);l.insert(QStringLiteral("objects"),os);layers.replace(i,l);p.insert(QStringLiteral("layers"),layers);return;}}}); return; }
    if (type == QStringLiteral("forestArea") || type == QStringLiteral("mountainArea")) { const int density=QInputDialog::getInt(this,tr("Editar pincel"),tr("Densidad:"),selected.value(QStringLiteral("density")).toInt(58),10,100,1,&ok);if(!ok)return; const int size=QInputDialog::getInt(this,tr("Editar pincel"),tr("Tamaño de símbolo:"),selected.value(QStringLiteral("symbolSize")).toInt(64),16,180,2,&ok);if(!ok)return; mutateDocument([&](QJsonObject& p){QJsonArray layers=p.value(QStringLiteral("layers")).toArray();for(int i=0;i<layers.size();++i){QJsonObject l=layers.at(i).toObject();QJsonArray os=l.value(QStringLiteral("objects")).toArray();for(int j=0;j<os.size();++j){QJsonObject o=os.at(j).toObject();if(o.value(QStringLiteral("id")).toString()!=id)continue;o.insert(QStringLiteral("density"),density);o.insert(QStringLiteral("symbolSize"),size);os.replace(j,o);l.insert(QStringLiteral("objects"),os);layers.replace(i,l);p.insert(QStringLiteral("layers"),layers);return;}}}); }
}

void PilinReyEditor::copySelectedObject() { clipboardObjects_ = selectedObjects(); }
void PilinReyEditor::duplicateSelectedObject() { copySelectedObject(); pasteCopiedObject(); }
void PilinReyEditor::pasteCopiedObject() { if (clipboardObjects_.isEmpty()) return; mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); int target = layers_->currentRow(); if (target < 0 || target >= layers.size()) target = 0; QJsonObject layer = layers.at(target).toObject(); if (layer.value(QStringLiteral("locked")).toBool(false)) return; QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); QStringList newIds; for (const QJsonValue& v : clipboardObjects_) { QJsonObject copy = v.toObject(); const QString id = uid(QStringLiteral("mapobj")); copy.insert(QStringLiteral("id"), id); if (copy.contains(QStringLiteral("x"))) { copy.insert(QStringLiteral("x"), copy.value(QStringLiteral("x")).toDouble()+60); copy.insert(QStringLiteral("y"), copy.value(QStringLiteral("y")).toDouble()+60); } else { QJsonArray pts=copy.value(QStringLiteral("points")).toArray(); for(int i=0;i<pts.size();++i){QJsonObject q=pts.at(i).toObject();q.insert(QStringLiteral("x"),q.value(QStringLiteral("x")).toDouble()+60);q.insert(QStringLiteral("y"),q.value(QStringLiteral("y")).toDouble()+60);pts.replace(i,q);} copy.insert(QStringLiteral("points"),pts); } objects.append(copy); newIds.append(id); } layer.insert(QStringLiteral("objects"),objects); layers.replace(target,layer); p.insert(QStringLiteral("layers"),layers); selectedObjectIds_=newIds; clipboardObjects_=selectedObjects(); }); }
void PilinReyEditor::deleteSelectedObject() { if (selectedObjectIds_.isEmpty()) return; const QStringList ids=selectedObjectIds_; mutateDocument([&](QJsonObject& p){QJsonArray layers=p.value(QStringLiteral("layers")).toArray();for(int i=0;i<layers.size();++i){QJsonObject l=layers.at(i).toObject();if(l.value(QStringLiteral("locked")).toBool(false))continue;QJsonArray os=l.value(QStringLiteral("objects")).toArray();for(int j=os.size()-1;j>=0;--j)if(ids.contains(os.at(j).toObject().value(QStringLiteral("id")).toString()))os.removeAt(j);l.insert(QStringLiteral("objects"),os);layers.replace(i,l);}p.insert(QStringLiteral("layers"),layers);}); selectedObjectIds_.clear(); refreshSelectionControls(); }

void PilinReyEditor::transformSelection(double scaleFactor, double rotationDegrees) { if(selectedObjectIds_.isEmpty())return; const QStringList ids=selectedObjectIds_; mutateDocument([&](QJsonObject& p){QJsonArray layers=p.value(QStringLiteral("layers")).toArray(); QPointF center; int count=0; for(const QJsonValue& lv:layers){for(const QJsonValue& ov:lv.toObject().value(QStringLiteral("objects")).toArray()){const QJsonObject o=ov.toObject();if(!ids.contains(o.value(QStringLiteral("id")).toString()))continue;if(o.contains(QStringLiteral("x"))){center+=QPointF(o.value(QStringLiteral("x")).toDouble(),o.value(QStringLiteral("y")).toDouble());++count;}else{for(const QJsonValue& pv:o.value(QStringLiteral("points")).toArray()){center+=jsonPoint(pv);++count;}}}} if(!count)return; center/=count; for(int i=0;i<layers.size();++i){QJsonObject l=layers.at(i).toObject();if(l.value(QStringLiteral("locked")).toBool(false))continue;QJsonArray os=l.value(QStringLiteral("objects")).toArray();for(int j=0;j<os.size();++j){QJsonObject o=os.at(j).toObject();if(!ids.contains(o.value(QStringLiteral("id")).toString()))continue;if(o.contains(QStringLiteral("x"))){QPointF q=rotatePoint(QPointF(o.value(QStringLiteral("x")).toDouble(),o.value(QStringLiteral("y")).toDouble()),center,rotationDegrees,scaleFactor);o.insert(QStringLiteral("x"),q.x());o.insert(QStringLiteral("y"),q.y());o.insert(QStringLiteral("scale"),std::clamp(o.value(QStringLiteral("scale")).toDouble(1.0)*scaleFactor,.08,12.0));o.insert(QStringLiteral("rotation"),o.value(QStringLiteral("rotation")).toDouble()+rotationDegrees);}else{QJsonArray pts=o.value(QStringLiteral("points")).toArray();for(int k=0;k<pts.size();++k){QJsonObject po=pts.at(k).toObject();QPointF q=rotatePoint(jsonPoint(po),center,rotationDegrees,scaleFactor);po.insert(QStringLiteral("x"),q.x());po.insert(QStringLiteral("y"),q.y());if(po.contains(QStringLiteral("radius")))po.insert(QStringLiteral("radius"),po.value(QStringLiteral("radius")).toDouble()*scaleFactor);pts.replace(k,po);}o.insert(QStringLiteral("points"),pts);}os.replace(j,o);}l.insert(QStringLiteral("objects"),os);layers.replace(i,l);}p.insert(QStringLiteral("layers"),layers);}); }

void PilinReyEditor::linkSelectedToAtlas() { const QJsonObject selected=selectedObject(); if(selected.value(QStringLiteral("type")).toString()!=QStringLiteral("settlement"))return; QWidget* cursor=parentWidget();WorldPage* world=nullptr;while(cursor){world=qobject_cast<WorldPage*>(cursor);if(world)break;cursor=cursor->parentWidget();}if(!world||!world->document_)return;const QJsonArray entries=world->document_->array(QStringLiteral("world"));if(entries.isEmpty()){QMessageBox::information(this,tr("Atlas vacío"),tr("Crea primero una entrada en Mundo → Atlas."));return;}QStringList options,ids;options<<tr("— Sin enlace —");ids<<QString();for(const QJsonValue& v:entries){const QJsonObject e=v.toObject();options<<e.value(QStringLiteral("name")).toString(tr("Sin nombre"));ids<<e.value(QStringLiteral("id")).toString();}bool ok=false;const QString choice=QInputDialog::getItem(this,tr("Enlazar Atlas"),tr("Entrada:"),options,0,false,&ok);if(!ok)return;const int index=options.indexOf(choice);if(index<0)return;const QString atlasId=ids.at(index),objectId=selectedObjectIds_.first();mutateDocument([&](QJsonObject& p){QJsonArray layers=p.value(QStringLiteral("layers")).toArray();for(int i=0;i<layers.size();++i){QJsonObject l=layers.at(i).toObject();QJsonArray os=l.value(QStringLiteral("objects")).toArray();for(int j=0;j<os.size();++j){QJsonObject o=os.at(j).toObject();if(o.value(QStringLiteral("id")).toString()!=objectId)continue;if(atlasId.isEmpty())o.remove(QStringLiteral("atlasId"));else o.insert(QStringLiteral("atlasId"),atlasId);os.replace(j,o);l.insert(QStringLiteral("objects"),os);layers.replace(i,l);p.insert(QStringLiteral("layers"),layers);return;}}}); }

void PilinReyEditor::importAsset() { const QString path=QFileDialog::getOpenFileName(this,tr("Importar asset"),QString(),tr("Imágenes (*.png *.jpg *.jpeg *.webp)"));if(path.isEmpty())return;const QString data=imageToDataUrl(path);if(data.isEmpty())return;const QString name=QFileInfo(path).completeBaseName();mutateDocument([&](QJsonObject& p){QJsonArray a=p.value(QStringLiteral("assets")).toArray();a.append(QJsonObject{{QStringLiteral("id"),uid(QStringLiteral("asset"))},{QStringLiteral("name"),name},{QStringLiteral("dataUrl"),data}});p.insert(QStringLiteral("assets"),a);});refreshAssets(); }
void PilinReyEditor::setThemeColor(const QString& key) { QJsonObject p=map_.value(QStringLiteral("pilinRey")).toObject();QJsonObject theme=p.value(QStringLiteral("theme")).toObject();QColor current(theme.value(key).toString());QColor chosen=QColorDialog::getColor(current.isValid()?current:Qt::white,this,tr("Color"));if(!chosen.isValid())return;mutateDocument([&](QJsonObject& doc){QJsonObject t=doc.value(QStringLiteral("theme")).toObject();t.insert(key,chosen.name(QColor::HexRgb));doc.insert(QStringLiteral("theme"),t);}); }

void PilinReyEditor::pushUndo() { undoStack_.append(map_); while(undoStack_.size()>100)undoStack_.removeFirst(); redoStack_.clear(); }
void PilinReyEditor::mutateDocument(const std::function<void(QJsonObject&)>& mutation) { if(refreshing_)return; pushUndo();QJsonObject p=map_.value(QStringLiteral("pilinRey")).toObject();mutation(p);map_.insert(QStringLiteral("pilinRey"),p);refreshViewport();refreshLayers();refreshAssets();refreshSelectionControls();persistToArchive();emit mapEdited(map_); }
void PilinReyEditor::undo() { if(undoStack_.isEmpty())return;redoStack_.append(map_);map_=undoStack_.takeLast();selectedObjectIds_.clear();refreshLayers();refreshAssets();refreshSelectionControls();refreshViewport();persistToArchive();emit mapEdited(map_); }
void PilinReyEditor::redo() { if(redoStack_.isEmpty())return;undoStack_.append(map_);map_=redoStack_.takeLast();selectedObjectIds_.clear();refreshLayers();refreshAssets();refreshSelectionControls();refreshViewport();persistToArchive();emit mapEdited(map_); }

void PilinReyEditor::exportMap() { const QJsonObject p=map_.value(QStringLiteral("pilinRey")).toObject();const QSize logical(qMax(1,p.value(QStringLiteral("width")).toInt(4096)),qMax(1,p.value(QStringLiteral("height")).toInt(2304)));MapExportDialog dialog(logical,this);if(dialog.exec()!=QDialog::Accepted)return;const QString format=dialog.format();QString extension=QStringLiteral(".png"),filter=tr("PNG (*.png)");if(format==QStringLiteral("svg")){extension=QStringLiteral(".svg");filter=tr("SVG (*.svg)");}else if(format==QStringLiteral("pdf")){extension=QStringLiteral(".pdf");filter=tr("PDF (*.pdf)");}QString path=QFileDialog::getSaveFileName(this,tr("Exportar mapa"),map_.value(QStringLiteral("name")).toString(tr("mapa"))+extension,filter);if(path.isEmpty())return;if(!path.endsWith(extension,Qt::CaseInsensitive))path+=extension;QString error;const bool ok=format==QStringLiteral("svg")?MapExporter::exportSvg(map_,path,dialog.outputSize(),&error):format==QStringLiteral("pdf")?MapExporter::exportPdf(map_,path,dialog.outputSize(),&error):MapExporter::exportPng(map_,path,dialog.outputSize(),&error);if(!ok)QMessageBox::critical(this,tr("No se pudo exportar"),error); }
void PilinReyEditor::persistToArchive() { QWidget* cursor=parentWidget();WorldPage* world=nullptr;while(cursor){world=qobject_cast<WorldPage*>(cursor);if(world)break;cursor=cursor->parentWidget();}if(!world||!world->document_||!world->mapList_)return;const int row=world->mapList_->currentRow();QJsonArray maps=world->document_->array(QStringLiteral("maps"));if(row<0||row>=maps.size())return;QJsonObject stored=maps.at(row).toObject();stored.insert(QStringLiteral("pilinRey"),map_.value(QStringLiteral("pilinRey")));if(map_.contains(QStringLiteral("backgroundImageDataUrl")))stored.insert(QStringLiteral("backgroundImageDataUrl"),map_.value(QStringLiteral("backgroundImageDataUrl")));maps.replace(row,stored);world->document_->setArray(QStringLiteral("maps"),maps);emit world->changed(); }

} // namespace wbw
