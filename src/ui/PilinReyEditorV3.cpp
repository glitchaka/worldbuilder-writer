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
#include <QGridLayout>
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
#include <QPushButton>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QShortcut>
#include <QSlider>
#include <QSpinBox>
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

QColor seaColor() { return QColor(177, 194, 198); }
QColor paperEdge() { return QColor(46, 49, 53); }
QColor landColor() { return QColor(216, 201, 158); }
QColor coastColor() { return QColor(65, 59, 48); }
QColor forestColor() { return QColor(55, 86, 53); }
QColor mountainColor() { return QColor(77, 68, 59); }

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
        setMinimumSize(720, 460);
    }

    ~PilinReyViewport() override {
        destroyTexture(templateTexture_);
        destroyTexture(terrainTexture_);
        destroyTexture(coastTexture_);
        if (renderer_) SDL_DestroyRenderer(renderer_);
        if (window_) SDL_DestroyWindow(window_);
    }

    QPaintEngine* paintEngine() const override { return nullptr; }

    void setDocument(const QJsonObject& map) {
        map_ = map;
        terrainDirty_ = true;
        templateDirty_ = true;
        if (!viewInitialized_) fitDocument();
        renderFrame();
    }

    void setTool(Tool tool) {
        tool_ = tool;
        brushStroke_.clear();
        nodePath_.clear();
        drawingBrush_ = false;
        draggingObjectId_.clear();
        draggingPathId_.clear();
        draggingPathPointIndex_ = -1;
        measureActive_ = false;
        renderFrame();
    }

    void setSelection(const QString& id) { selectedObjectId_ = id; renderFrame(); }
    void setSnap(bool enabled) { snap_ = enabled; }
    void setBrushRadius(int value) { brushRadius_ = std::clamp(value, 30, 700); renderFrame(); }
    void setDensity(int value) { density_ = std::clamp(value, 10, 100); renderFrame(); }
    void setStrokeWidth(int value) { strokeWidth_ = std::clamp(value, 1, 14); renderFrame(); }

    void fitDocument() {
        const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        const double docW = std::max(1.0, pilin.value(QStringLiteral("width")).toDouble(4096));
        const double docH = std::max(1.0, pilin.value(QStringLiteral("height")).toDouble(2304));
        if (width() < 100 || height() < 100) return;
        zoom_ = std::clamp(std::min((width() - 80.0) / docW, (height() - 80.0) / docH), 0.04, 5.0);
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
        zoom_ = std::clamp(zoom_ * (event->angleDelta().y() > 0 ? 1.12 : 1.0 / 1.12), 0.04, 12.0);
        const QPointF after = screenToWorld(event->position());
        pan_ += QPointF((after.x() - before.x()) * zoom_, (after.y() - before.y()) * zoom_);
        renderFrame();
        event->accept();
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Escape) {
            brushStroke_.clear();
            nodePath_.clear();
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
            selectedObjectId_ = findObjectAt(world);
            draggingObjectId_.clear();
            draggingPathId_.clear();
            draggingPathPointIndex_ = -1;
            if (!selectedObjectId_.isEmpty()) {
                const QJsonObject object = objectById(selectedObjectId_);
                if (object.contains(QStringLiteral("x"))) draggingObjectId_ = selectedObjectId_;
                else {
                    const int node = nearestNode(object, world);
                    if (node >= 0) { draggingPathId_ = selectedObjectId_; draggingPathPointIndex_ = node; }
                }
            }
            if (onSelected) onSelected(selectedObjectId_);
            renderFrame();
            return;
        }

        if (tool_ == Tool::Settlement) { if (onSettlement) onSettlement(world.x(), world.y()); return; }
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
            if (brushStroke_.isEmpty() || dist2(jsonPoint(brushStroke_.last()), world) > std::pow(std::max(brushRadius_ * 0.18, 12.0), 2.0)) {
                brushStroke_.append(pointJson(world.x(), world.y(), brushRadius_));
            }
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
            brushStroke_.clear();
            return;
        }
    }

private:
    void destroyTexture(SDL_Texture*& texture) { if (texture) { SDL_DestroyTexture(texture); texture = nullptr; } }

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
        nodePath_.clear();
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
        const double tolerance2 = std::pow(14.0 / std::max(zoom_, 0.04), 2.0);
        int best = -1;
        double bestD = tolerance2;
        for (int i = 0; i < pts.size(); ++i) {
            const double d = dist2(jsonPoint(pts.at(i)), world);
            if (d <= bestD) { best = i; bestD = d; }
        }
        return best;
    }

    QString findObjectAt(const QPointF& world) const {
        const double tol = 16.0 / std::max(zoom_, 0.04);
        const QJsonArray layers = map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray();
        for (int li = layers.size() - 1; li >= 0; --li) {
            const QJsonObject layer = layers.at(li).toObject();
            if (!layer.value(QStringLiteral("visible")).toBool(true)) continue;
            const QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
            for (int oi = objects.size() - 1; oi >= 0; --oi) {
                const QJsonObject object = objects.at(oi).toObject();
                const QString type = object.value(QStringLiteral("type")).toString();
                if (object.contains(QStringLiteral("x"))) {
                    if (dist2(QPointF(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble()), world) <= tol * tol)
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
                pilin.insert(QStringLiteral("layers"), layers); map_.insert(QStringLiteral("pilinRey"), pilin); terrainDirty_ = true; return;
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

    void updateTerrainTextures() {
        if (!renderer_ || !terrainDirty_) return;
        terrainDirty_ = false;
        const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        const double docW = pilin.value(QStringLiteral("width")).toDouble(4096);
        const double docH = pilin.value(QStringLiteral("height")).toDouble(2304);
        const int w = 1024;
        const int h = qMax(256, qRound(w * docH / docW));
        const double sx = w / docW;
        const double sy = h / docH;

        QImage mask(w, h, QImage::Format_ARGB32_Premultiplied);
        mask.fill(Qt::transparent);
        QPainter painter(&mask);
        painter.setRenderHint(QPainter::Antialiasing, true);
        for (const QJsonValue& lv : pilin.value(QStringLiteral("layers")).toArray()) {
            const QJsonObject layer = lv.toObject();
            if (!layer.value(QStringLiteral("visible")).toBool(true)) continue;
            for (const QJsonValue& ov : layer.value(QStringLiteral("objects")).toArray()) {
                const QJsonObject object = ov.toObject();
                const QString type = object.value(QStringLiteral("type")).toString();
                if (type != QStringLiteral("land") && type != QStringLiteral("sea")) continue;
                const QJsonArray pts = object.value(QStringLiteral("points")).toArray();
                if (pts.isEmpty()) continue;
                painter.setCompositionMode(type == QStringLiteral("land") ? QPainter::CompositionMode_SourceOver : QPainter::CompositionMode_Clear);
                for (int i = 0; i < pts.size(); ++i) {
                    const QPointF p = jsonPoint(pts.at(i));
                    const double radius = jsonRadius(pts.at(i), 180.0);
                    painter.setPen(QPen(Qt::white, radius * 2.0 * sx, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                    if (i == 0) painter.drawPoint(QPointF(p.x() * sx, p.y() * sy));
                    else {
                        const QPointF a = jsonPoint(pts.at(i - 1));
                        painter.drawLine(QPointF(a.x() * sx, a.y() * sy), QPointF(p.x() * sx, p.y() * sy));
                    }
                }
            }
        }
        painter.end();

        QImage land(w, h, QImage::Format_ARGB32_Premultiplied);
        land.fill(Qt::transparent);
        QImage coast(w, h, QImage::Format_ARGB32_Premultiplied);
        coast.fill(Qt::transparent);
        const QRgb landRgb = qRgba(landColor().red(), landColor().green(), landColor().blue(), 255);
        const QRgb coastRgb = qRgba(coastColor().red(), coastColor().green(), coastColor().blue(), 235);
        for (int y = 1; y < h - 1; ++y) {
            const QRgb* src = reinterpret_cast<const QRgb*>(mask.constScanLine(y));
            QRgb* dst = reinterpret_cast<QRgb*>(land.scanLine(y));
            QRgb* edge = reinterpret_cast<QRgb*>(coast.scanLine(y));
            for (int x = 1; x < w - 1; ++x) {
                if (qAlpha(src[x]) < 100) continue;
                dst[x] = landRgb;
                const bool boundary = qAlpha(reinterpret_cast<const QRgb*>(mask.constScanLine(y - 1))[x]) < 100 ||
                                      qAlpha(reinterpret_cast<const QRgb*>(mask.constScanLine(y + 1))[x]) < 100 ||
                                      qAlpha(src[x - 1]) < 100 || qAlpha(src[x + 1]) < 100;
                if (boundary) edge[x] = coastRgb;
            }
        }
        uploadImageTexture(land, terrainTexture_);
        uploadImageTexture(coast, coastTexture_);
    }

    void setColor(const QColor& color, int alpha = 255) {
        SDL_SetRenderDrawColor(renderer_, color.red(), color.green(), color.blue(), alpha);
    }

    void drawPolyline(const std::vector<QPointF>& points, const QColor& color, int width = 1, bool dashed = false) {
        if (points.size() < 2) return;
        setColor(color);
        for (size_t i = 1; i < points.size(); ++i) {
            if (dashed && ((i / 4) % 2)) continue;
            const QPointF a = worldToScreen(points[i - 1]);
            const QPointF b = worldToScreen(points[i]);
            for (int offset = -(width / 2); offset <= width / 2; ++offset) {
                SDL_RenderLine(renderer_, static_cast<float>(a.x()), static_cast<float>(a.y() + offset), static_cast<float>(b.x()), static_cast<float>(b.y() + offset));
            }
        }
    }

    void drawForestArea(const QJsonObject& object) {
        const QJsonArray pts = object.value(QStringLiteral("points")).toArray();
        if (pts.isEmpty()) return;
        QRandomGenerator rng(hashText(object.value(QStringLiteral("id")).toString()));
        const int count = qMax(18, pts.size() * density_ / 3);
        setColor(forestColor(), 230);
        for (int i = 0; i < count; ++i) {
            const int index = rng.bounded(pts.size());
            const QPointF center = jsonPoint(pts.at(index));
            const double radius = jsonRadius(pts.at(index), 180.0);
            const double angle = rng.generateDouble() * 6.28318530718;
            const double rr = std::sqrt(rng.generateDouble()) * radius;
            const QPointF p = worldToScreen(center + QPointF(std::cos(angle) * rr, std::sin(angle) * rr));
            const float s = static_cast<float>(std::clamp(8.0 * zoom_ * (0.75 + rng.generateDouble() * 0.6), 3.0, 12.0));
            SDL_RenderLine(renderer_, p.x(), p.y() - s, p.x() - s * .65f, p.y() + s * .35f);
            SDL_RenderLine(renderer_, p.x(), p.y() - s, p.x() + s * .65f, p.y() + s * .35f);
            SDL_RenderLine(renderer_, p.x(), p.y() - s * .45f, p.x() - s * .52f, p.y() + s * .05f);
            SDL_RenderLine(renderer_, p.x(), p.y() - s * .45f, p.x() + s * .52f, p.y() + s * .05f);
            SDL_RenderLine(renderer_, p.x(), p.y() + s * .35f, p.x(), p.y() + s * .72f);
        }
    }

    void drawMountainArea(const QJsonObject& object) {
        const QJsonArray pts = object.value(QStringLiteral("points")).toArray();
        if (pts.isEmpty()) return;
        QRandomGenerator rng(hashText(object.value(QStringLiteral("id")).toString()) ^ 0x8f34a1u);
        const int count = qMax(14, pts.size() * density_ / 4);
        setColor(mountainColor(), 240);
        for (int i = 0; i < count; ++i) {
            const int index = rng.bounded(pts.size());
            const QPointF center = jsonPoint(pts.at(index));
            const double radius = jsonRadius(pts.at(index), 220.0);
            const double angle = rng.generateDouble() * 6.28318530718;
            const double rr = std::sqrt(rng.generateDouble()) * radius;
            const QPointF p = worldToScreen(center + QPointF(std::cos(angle) * rr, std::sin(angle) * rr));
            const float s = static_cast<float>(std::clamp(13.0 * zoom_ * (0.7 + rng.generateDouble() * 0.8), 4.0, 18.0));
            SDL_RenderLine(renderer_, p.x() - s, p.y() + s * .55f, p.x(), p.y() - s);
            SDL_RenderLine(renderer_, p.x(), p.y() - s, p.x() + s, p.y() + s * .55f);
            SDL_RenderLine(renderer_, p.x() - s * .32f, p.y() - s * .05f, p.x(), p.y() + s * .18f);
            SDL_RenderLine(renderer_, p.x(), p.y() + s * .18f, p.x() + s * .28f, p.y() - s * .12f);
        }
    }

    void drawText(const QString& text, const QPointF& world, bool bold = false) {
        if (text.trimmed().isEmpty()) return;
        QFont font(QStringLiteral("Georgia"), qBound(9, qRound(13.0 * std::sqrt(zoom_ + .15)), 22));
        font.setBold(bold);
        QFontMetrics metrics(font);
        const QSize size = metrics.size(Qt::TextSingleLine, text) + QSize(12, 8);
        QImage image(size, QImage::Format_RGBA8888);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setFont(font); painter.setPen(QColor(40, 37, 32));
        painter.drawText(image.rect().adjusted(6, 4, -6, -4), Qt::AlignLeft | Qt::AlignVCenter, text);
        painter.end();
        SDL_Texture* texture = nullptr;
        uploadImageTexture(image, texture);
        if (!texture) return;
        const QPointF p = worldToScreen(world);
        SDL_FRect dst{static_cast<float>(p.x()), static_cast<float>(p.y()), static_cast<float>(image.width()), static_cast<float>(image.height())};
        SDL_RenderTexture(renderer_, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }

    void drawObject(const QJsonObject& object) {
        const QString type = object.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("land") || type == QStringLiteral("sea")) return;
        if (type == QStringLiteral("forestArea")) { drawForestArea(object); return; }
        if (type == QStringLiteral("mountainArea")) { drawMountainArea(object); return; }
        if (type == QStringLiteral("settlement")) {
            const QPointF p = worldToScreen(QPointF(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble()));
            const QString kind = object.value(QStringLiteral("kind")).toString();
            const float r = static_cast<float>(std::clamp(6.0 * std::sqrt(zoom_ + .1), 3.5, 8.0));
            setColor(QColor(48, 40, 34));
            SDL_FRect rect{static_cast<float>(p.x() - r), static_cast<float>(p.y() - r), r * 2, r * 2};
            if (kind.contains(QStringLiteral("Capital"), Qt::CaseInsensitive)) { SDL_RenderFillRect(renderer_, &rect); SDL_FRect outer{rect.x - 3, rect.y - 3, rect.w + 6, rect.h + 6}; SDL_RenderRect(renderer_, &outer); }
            else if (kind.contains(QStringLiteral("Puerto"), Qt::CaseInsensitive)) SDL_RenderRect(renderer_, &rect);
            else SDL_RenderFillRect(renderer_, &rect);
            drawText(object.value(QStringLiteral("label")).toString(), QPointF(object.value(QStringLiteral("x")).toDouble() + 18 / zoom_, object.value(QStringLiteral("y")).toDouble() - 10 / zoom_), false);
            return;
        }
        if (type == QStringLiteral("label")) { drawText(object.value(QStringLiteral("text")).toString(), QPointF(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble()), false); return; }

        const std::vector<QPointF> path = smoothPath(object.value(QStringLiteral("points")).toArray());
        if (path.size() < 2) return;
        if (type == QStringLiteral("river")) {
            drawPolyline(path, QColor(37, 79, 109), qMax(2, qRound(strokeWidth_ * zoom_ + 1)));
            drawPolyline(path, QColor(111, 160, 183), qMax(1, qRound(strokeWidth_ * zoom_ * .45)));
        } else if (type == QStringLiteral("road")) {
            drawPolyline(path, QColor(72, 55, 40), qMax(2, qRound((strokeWidth_ + 2) * zoom_)));
            drawPolyline(path, QColor(176, 146, 99), qMax(1, qRound(strokeWidth_ * zoom_)));
        } else if (type == QStringLiteral("border")) {
            drawPolyline(path, QColor(126, 65, 55), qMax(1, qRound(strokeWidth_ * zoom_)), true);
        } else if (type == QStringLiteral("region")) {
            drawPolyline(path, QColor(119, 88, 55), 2, true);
        }

        if (object.value(QStringLiteral("id")).toString() == selectedObjectId_) {
            setColor(QColor(38, 119, 209));
            const QJsonArray pts = object.value(QStringLiteral("points")).toArray();
            for (int i = 0; i < pts.size(); ++i) {
                if (i == pts.size() - 1 && object.value(QStringLiteral("closed")).toBool(false)) continue;
                const QPointF p = worldToScreen(jsonPoint(pts.at(i)));
                SDL_FRect marker{static_cast<float>(p.x() - 4), static_cast<float>(p.y() - 4), 8, 8};
                SDL_RenderFillRect(renderer_, &marker);
            }
        }
    }

    void drawBrushPreview() {
        if (!isBrushTool(tool_)) return;
        const QPointF center = worldToScreen(cursorWorld_);
        const float radius = static_cast<float>(brushRadius_ * zoom_);
        if (radius < 2) return;
        const QColor color = tool_ == Tool::Eraser ? QColor(80, 130, 160) : QColor(38, 119, 209);
        setColor(color, 180);
        constexpr int segments = 48;
        QPointF prev(center.x() + radius, center.y());
        for (int i = 1; i <= segments; ++i) {
            const double a = 6.28318530718 * i / segments;
            QPointF next(center.x() + std::cos(a) * radius, center.y() + std::sin(a) * radius);
            SDL_RenderLine(renderer_, prev.x(), prev.y(), next.x(), next.y());
            prev = next;
        }
    }

    void renderFrame() {
        if (!isVisible() || !ensureRenderer()) return;
        setColor(QColor(43, 46, 50));
        SDL_RenderClear(renderer_);

        const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        const double docW = pilin.value(QStringLiteral("width")).toDouble(4096);
        const double docH = pilin.value(QStringLiteral("height")).toDouble(2304);
        const QPointF tl = worldToScreen(QPointF(0, 0));
        const QPointF br = worldToScreen(QPointF(docW, docH));
        SDL_FRect page{static_cast<float>(tl.x()), static_cast<float>(tl.y()), static_cast<float>(br.x() - tl.x()), static_cast<float>(br.y() - tl.y())};
        setColor(seaColor()); SDL_RenderFillRect(renderer_, &page);
        setColor(paperEdge(), 180); SDL_RenderRect(renderer_, &page);

        updateTerrainTextures();
        if (terrainTexture_) SDL_RenderTexture(renderer_, terrainTexture_, nullptr, &page);
        if (coastTexture_) SDL_RenderTexture(renderer_, coastTexture_, nullptr, &page);

        updateTemplateTexture();
        const QJsonObject templ = pilin.value(QStringLiteral("template")).toObject();
        if (templateTexture_ && templ.value(QStringLiteral("visible")).toBool(true)) {
            SDL_SetTextureAlphaMod(templateTexture_, static_cast<Uint8>(std::clamp(templ.value(QStringLiteral("opacity")).toDouble(.35), 0.0, 1.0) * 255));
            SDL_RenderTexture(renderer_, templateTexture_, nullptr, &page);
        }

        for (const QJsonValue& lv : pilin.value(QStringLiteral("layers")).toArray()) {
            const QJsonObject layer = lv.toObject();
            if (!layer.value(QStringLiteral("visible")).toBool(true)) continue;
            for (const QJsonValue& ov : layer.value(QStringLiteral("objects")).toArray()) drawObject(ov.toObject());
        }

        if (!nodePath_.isEmpty()) {
            std::vector<QPointF> preview;
            for (const QJsonValue& v : nodePath_) preview.push_back(jsonPoint(v));
            preview.push_back(cursorWorld_);
            drawPolyline(preview, QColor(38, 119, 209), 2);
        }
        if (drawingBrush_ && !brushStroke_.isEmpty()) {
            QJsonObject preview{{QStringLiteral("id"), QStringLiteral("preview")}, {QStringLiteral("type"), pathType(tool_)}, {QStringLiteral("points"), brushStroke_}};
            if (tool_ == Tool::Forest) drawForestArea(preview);
            else if (tool_ == Tool::Mountain) drawMountainArea(preview);
        }
        if (measureActive_) {
            const QPointF a = worldToScreen(measureStart_); const QPointF b = worldToScreen(measureEnd_);
            setColor(QColor(38, 119, 209)); SDL_RenderLine(renderer_, a.x(), a.y(), b.x(), b.y());
        }
        drawBrushPreview();
        SDL_RenderPresent(renderer_);
    }

    QJsonObject map_;
    Tool tool_ = Tool::Select;
    bool panning_ = false;
    bool drawingBrush_ = false;
    bool viewInitialized_ = false;
    bool snap_ = false;
    bool measureActive_ = false;
    int brushRadius_ = 220;
    int density_ = 58;
    int strokeWidth_ = 5;
    QPointF pan_{40, 40};
    QPointF lastMouse_;
    QPointF cursorWorld_;
    QPointF measureStart_;
    QPointF measureEnd_;
    double zoom_ = .2;
    QJsonArray brushStroke_;
    QJsonArray nodePath_;
    QString selectedObjectId_;
    QString draggingObjectId_;
    QString draggingPathId_;
    int draggingPathPointIndex_ = -1;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* terrainTexture_ = nullptr;
    SDL_Texture* coastTexture_ = nullptr;
    SDL_Texture* templateTexture_ = nullptr;
    bool terrainDirty_ = true;
    bool templateDirty_ = true;
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
    tools->setContentsMargins(5, 5, 5, 5);
    tools->setHorizontalSpacing(2);
    tools->setVerticalSpacing(2);

    struct ToolDef { Tool tool; const char* glyph; const char* tip; };
    const ToolDef defs[] = {
        {Tool::Select, "↖", "Seleccionar / editar nodos"}, {Tool::Pan, "✥", "Mover lienzo"},
        {Tool::Coast, "▰", "Añadir tierra"}, {Tool::Eraser, "◌", "Recortar tierra / mar"},
        {Tool::River, "∿", "Río por nodos"}, {Tool::Road, "━", "Camino por nodos"},
        {Tool::Border, "┄", "Frontera por nodos"}, {Tool::Region, "◇", "Región política"},
        {Tool::Forest, "♣", "Pintar bosque"}, {Tool::Mountain, "△", "Pintar cordillera"},
        {Tool::Settlement, "●", "Asentamiento"}, {Tool::Label, "T", "Etiqueta"},
        {Tool::Measure, "↔", "Medir"}
    };
    int row = 0, col = 0;
    for (const ToolDef& def : defs) {
        auto* button = toolButton(QString::fromUtf8(def.glyph), tr(def.tip), toolRail_, QStringLiteral("pilinMapTool"));
        button->setCheckable(true); button->setAutoExclusive(true);
        if (def.tool == Tool::Select) button->setChecked(true);
        connect(button, &QToolButton::clicked, this, [this, tool = def.tool]() { setActiveTool(tool); });
        toolButtons_.append(button);
        tools->addWidget(button, row, col);
        if (++col == 2) { col = 0; ++row; }
    }

    auto* brushPanel = new QWidget(toolRail_);
    auto* brushLayout = new QVBoxLayout(brushPanel);
    brushLayout->setContentsMargins(3, 5, 3, 2);
    brushLayout->setSpacing(3);
    auto* brushTitle = new QLabel(tr("Tamaño"), brushPanel);
    auto* brushSlider = new QSlider(Qt::Horizontal, brushPanel);
    brushSlider->setRange(30, 700); brushSlider->setValue(220); brushSlider->setFixedWidth(72);
    brushLayout->addWidget(brushTitle); brushLayout->addWidget(brushSlider);
    tools->addWidget(brushPanel, row + 1, 0, 1, 2);
    connect(brushSlider, &QSlider::valueChanged, viewport_, &PilinReyViewport::setBrushRadius);

    topCommands_ = new QFrame(canvasHost_);
    topCommands_->setObjectName(QStringLiteral("pilinTopCommands"));
    auto* commands = new QHBoxLayout(topCommands_);
    commands->setContentsMargins(5, 4, 5, 4); commands->setSpacing(2);
    layersButton_ = toolButton(QStringLiteral("▱"), tr("Capas"), topCommands_, QStringLiteral("pilinCommand"));
    templateButton_ = toolButton(QStringLiteral("▧"), tr("Plantilla"), topCommands_, QStringLiteral("pilinCommand"));
    auto* generateButton = toolButton(QStringLiteral("✦"), tr("Generar terreno"), topCommands_, QStringLiteral("pilinCommand"));
    auto* fitButton = toolButton(QStringLiteral("⌗"), tr("Encajar"), topCommands_, QStringLiteral("pilinCommand"));
    undoButton_ = toolButton(QStringLiteral("↶"), tr("Deshacer"), topCommands_, QStringLiteral("pilinCommand"));
    redoButton_ = toolButton(QStringLiteral("↷"), tr("Rehacer"), topCommands_, QStringLiteral("pilinCommand"));
    auto* exportButton = toolButton(QStringLiteral("⇩"), tr("Exportar"), topCommands_, QStringLiteral("pilinCommand"));
    snapCheck_ = new QCheckBox(tr("Ajustar"), topCommands_);
    commands->addWidget(layersButton_); commands->addWidget(templateButton_); commands->addWidget(generateButton); commands->addWidget(fitButton);
    commands->addSpacing(6); commands->addWidget(undoButton_); commands->addWidget(redoButton_); commands->addWidget(exportButton); commands->addWidget(snapCheck_);

    layersPopover_ = new QFrame(canvasHost_);
    layersPopover_->setObjectName(QStringLiteral("pilinPopover")); layersPopover_->setFixedWidth(280);
    auto* layerLayout = new QVBoxLayout(layersPopover_); layerLayout->setContentsMargins(10, 10, 10, 10); layerLayout->setSpacing(6);
    auto* layerTitle = new QLabel(tr("Capas"), layersPopover_); layerTitle->setObjectName(QStringLiteral("pilinPopoverTitle"));
    layers_ = new QListWidget(layersPopover_); layers_->setMinimumHeight(180);
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
    layersPopover_->hide();

    templatePopover_ = new QFrame(canvasHost_);
    templatePopover_->setObjectName(QStringLiteral("pilinPopover")); templatePopover_->setFixedWidth(260);
    auto* templateLayout = new QVBoxLayout(templatePopover_); templateLayout->setContentsMargins(10, 10, 10, 10); templateLayout->setSpacing(6);
    auto* templateTitle = new QLabel(tr("Plantilla"), templatePopover_); templateTitle->setObjectName(QStringLiteral("pilinPopoverTitle"));
    auto* templateRow = new QHBoxLayout;
    auto* loadTemplate = toolButton(QStringLiteral("＋"), tr("Cargar"), templatePopover_, QStringLiteral("pilinCommand"));
    auto* clearTemplate = toolButton(QStringLiteral("×"), tr("Quitar"), templatePopover_, QStringLiteral("pilinCommand"));
    templateRow->addWidget(loadTemplate); templateRow->addWidget(clearTemplate); templateRow->addStretch();
    templateOpacity_ = new QSlider(Qt::Horizontal, templatePopover_); templateOpacity_->setRange(0, 100); templateOpacity_->setValue(35);
    templateLayout->addWidget(templateTitle); templateLayout->addLayout(templateRow); templateLayout->addWidget(new QLabel(tr("Opacidad"), templatePopover_)); templateLayout->addWidget(templateOpacity_);
    templatePopover_->hide();

    selectionPopover_ = new QFrame(canvasHost_);
    selectionPopover_->setObjectName(QStringLiteral("pilinSelectionPopover")); selectionPopover_->setFixedWidth(220);
    auto* selectionLayout = new QVBoxLayout(selectionPopover_); selectionLayout->setContentsMargins(9, 8, 9, 8); selectionLayout->setSpacing(5);
    selectionLabel_ = new QLabel(tr("Selección"), selectionPopover_); selectionLabel_->setObjectName(QStringLiteral("pilinPopoverTitle"));
    auto* selectionActions = new QHBoxLayout;
    editObjectButton_ = toolButton(QStringLiteral("✎"), tr("Editar"), selectionPopover_, QStringLiteral("pilinCommand"));
    duplicateObjectButton_ = toolButton(QStringLiteral("⧉"), tr("Duplicar"), selectionPopover_, QStringLiteral("pilinCommand"));
    linkAtlasButton_ = toolButton(QStringLiteral("⌁"), tr("Enlazar Atlas"), selectionPopover_, QStringLiteral("pilinCommand"));
    deleteObjectButton_ = toolButton(QStringLiteral("×"), tr("Eliminar"), selectionPopover_, QStringLiteral("pilinCommand"));
    selectionActions->addWidget(editObjectButton_); selectionActions->addWidget(duplicateObjectButton_); selectionActions->addWidget(linkAtlasButton_); selectionActions->addStretch(); selectionActions->addWidget(deleteObjectButton_);
    selectionLayout->addWidget(selectionLabel_); selectionLayout->addLayout(selectionActions); selectionPopover_->hide();

    status_ = new QLabel(tr("Pilín Rey · listo"), canvasHost_); status_->setObjectName(QStringLiteral("pilinStatus")); status_->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    viewport_->onPath = [this](const QString& type, const QJsonArray& points) { addPathObject(type, points); };
    viewport_->onSettlement = [this](double x, double y) { addSettlement(x, y); };
    viewport_->onLabel = [this](double x, double y) { addLabel(x, y); };
    viewport_->onSelected = [this](const QString& id) { selectObject(id); };
    viewport_->onMovePoint = [this](const QString& id, double x, double y) { movePointObject(id, x, y); };
    viewport_->onMovePathPoint = [this](const QString& id, int index, double x, double y) { movePathPoint(id, index, x, y); };
    viewport_->onDeleteSelection = [this]() { deleteSelectedObject(); };
    viewport_->onStatus = [this](const QString& text) { status_->setText(text); };

    connect(layersButton_, &QToolButton::clicked, this, [this]() { templatePopover_->hide(); layersPopover_->setVisible(!layersPopover_->isVisible()); layoutFloatingPanels(); });
    connect(templateButton_, &QToolButton::clicked, this, [this]() { layersPopover_->hide(); templatePopover_->setVisible(!templatePopover_->isVisible()); layoutFloatingPanels(); });
    connect(fitButton, &QToolButton::clicked, viewport_, &PilinReyViewport::fitDocument);
    connect(undoButton_, &QToolButton::clicked, this, &PilinReyEditor::undo);
    connect(redoButton_, &QToolButton::clicked, this, &PilinReyEditor::redo);
    connect(exportButton, &QToolButton::clicked, this, &PilinReyEditor::exportMap);
    connect(snapCheck_, &QCheckBox::toggled, viewport_, &PilinReyViewport::setSnap);
    connect(loadTemplate, &QToolButton::clicked, this, &PilinReyEditor::chooseTemplate);
    connect(clearTemplate, &QToolButton::clicked, this, &PilinReyEditor::clearTemplate);
    connect(templateOpacity_, &QSlider::valueChanged, this, [this](int value) { if (!refreshing_) mutateDocument([&](QJsonObject& p) { QJsonObject t = p.value(QStringLiteral("template")).toObject(); t.insert(QStringLiteral("opacity"), value / 100.0); p.insert(QStringLiteral("template"), t); }); });
    connect(addLayerButton, &QToolButton::clicked, this, &PilinReyEditor::addLayer);
    connect(duplicateLayerButton, &QToolButton::clicked, this, &PilinReyEditor::duplicateLayer);
    connect(removeLayerButton, &QToolButton::clicked, this, &PilinReyEditor::removeLayer);
    connect(upLayerButton, &QToolButton::clicked, this, [this]() { moveLayer(-1); });
    connect(downLayerButton, &QToolButton::clicked, this, [this]() { moveLayer(1); });
    connect(layerLocked_, &QCheckBox::toggled, this, [this](bool checked) { if (!refreshing_) setLayerLocked(checked); });
    connect(layerOpacity_, &QSlider::valueChanged, this, [this](int value) { if (!refreshing_) setLayerOpacity(value); });
    connect(layers_, &QListWidget::currentRowChanged, this, [this](int row) { if (refreshing_ || row < 0) return; QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject(); const QJsonArray a = p.value(QStringLiteral("layers")).toArray(); if (row >= a.size()) return; p.insert(QStringLiteral("activeLayerId"), a.at(row).toObject().value(QStringLiteral("id")).toString()); map_.insert(QStringLiteral("pilinRey"), p); refreshLayerControls(); persistToArchive(); });
    connect(editObjectButton_, &QToolButton::clicked, this, &PilinReyEditor::editSelectedObject);
    connect(duplicateObjectButton_, &QToolButton::clicked, this, &PilinReyEditor::duplicateSelectedObject);
    connect(linkAtlasButton_, &QToolButton::clicked, this, &PilinReyEditor::linkSelectedToAtlas);
    connect(deleteObjectButton_, &QToolButton::clicked, this, &PilinReyEditor::deleteSelectedObject);

    connect(generateButton, &QToolButton::clicked, this, [this]() {
        QDialog dialog(this); dialog.setWindowTitle(tr("Generar terreno"));
        auto* layout = new QVBoxLayout(&dialog);
        auto* seed = new QSpinBox(&dialog); seed->setRange(1, 999999999); seed->setValue(QRandomGenerator::global()->bounded(1, 999999999));
        auto* continents = new QSpinBox(&dialog); continents->setRange(1, 6); continents->setValue(3);
        auto* islands = new QSpinBox(&dialog); islands->setRange(0, 20); islands->setValue(6);
        auto* rough = new QSlider(Qt::Horizontal, &dialog); rough->setRange(0, 100); rough->setValue(58);
        layout->addWidget(new QLabel(tr("Semilla"), &dialog)); layout->addWidget(seed); layout->addWidget(new QLabel(tr("Continentes"), &dialog)); layout->addWidget(continents); layout->addWidget(new QLabel(tr("Islas"), &dialog)); layout->addWidget(islands); layout->addWidget(new QLabel(tr("Irregularidad"), &dialog)); layout->addWidget(rough);
        auto* buttons = new QHBoxLayout; auto* cancel = new QPushButton(tr("Cancelar"), &dialog); auto* apply = new QPushButton(tr("Generar"), &dialog); buttons->addStretch(); buttons->addWidget(cancel); buttons->addWidget(apply); layout->addLayout(buttons); connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject); connect(apply, &QPushButton::clicked, &dialog, &QDialog::accept);
        if (dialog.exec() != QDialog::Accepted) return;
        const int seedValue = seed->value(), continentCount = continents->value(), islandCount = islands->value(), roughness = rough->value();
        mutateDocument([&](QJsonObject& p) {
            QJsonArray layers = p.value(QStringLiteral("layers")).toArray();
            if (layers.isEmpty()) return;
            QJsonObject terrain = layers.first().toObject(); QJsonArray objects = terrain.value(QStringLiteral("objects")).toArray();
            for (int i = objects.size() - 1; i >= 0; --i) { const QString type = objects.at(i).toObject().value(QStringLiteral("type")).toString(); if (type == QStringLiteral("land") || type == QStringLiteral("sea")) objects.removeAt(i); }
            QRandomGenerator rng(static_cast<quint32>(seedValue));
            const double w = p.value(QStringLiteral("width")).toDouble(4096), h = p.value(QStringLiteral("height")).toDouble(2304);
            for (int c = 0; c < continentCount; ++c) {
                const double cx = w * (.14 + .72 * rng.generateDouble()), cy = h * (.16 + .68 * rng.generateDouble());
                const double base = std::min(w, h) * (.10 + .055 * rng.generateDouble());
                QJsonArray pts;
                double x = cx - base * 1.4, y = cy;
                const int steps = 10 + roughness / 10;
                for (int s = 0; s < steps; ++s) { x += base * (2.8 / steps); y += (rng.generateDouble() - .5) * base * (.20 + roughness / 130.0); const double r = base * (.72 + rng.generateDouble() * (.35 + roughness / 180.0)); pts.append(pointJson(x, y, r)); }
                objects.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("mapobj"))}, {QStringLiteral("type"), QStringLiteral("land")}, {QStringLiteral("points"), pts}});
            }
            for (int i = 0; i < islandCount; ++i) { const double x = w * (.08 + .84 * rng.generateDouble()), y = h * (.08 + .84 * rng.generateDouble()), r = std::min(w, h) * (.018 + .028 * rng.generateDouble()); QJsonArray pts{pointJson(x, y, r)}; objects.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("mapobj"))}, {QStringLiteral("type"), QStringLiteral("land")}, {QStringLiteral("points"), pts}}); }
            terrain.insert(QStringLiteral("objects"), objects); layers.replace(0, terrain); p.insert(QStringLiteral("layers"), layers); p.insert(QStringLiteral("generatorSeed"), seedValue);
        });
    });

    auto* undoShortcut = new QShortcut(QKeySequence::Undo, this); connect(undoShortcut, &QShortcut::activated, this, &PilinReyEditor::undo);
    auto* redoShortcut = new QShortcut(QKeySequence::Redo, this); connect(redoShortcut, &QShortcut::activated, this, &PilinReyEditor::redo);
    auto* copyShortcut = new QShortcut(QKeySequence::Copy, this); connect(copyShortcut, &QShortcut::activated, this, &PilinReyEditor::copySelectedObject);
    auto* pasteShortcut = new QShortcut(QKeySequence::Paste, this); connect(pasteShortcut, &QShortcut::activated, this, &PilinReyEditor::pasteCopiedObject);

    setStyleSheet(QStringLiteral(
        "#pilinReyEditor,#pilinCanvasHost{background:#2b2e32;}"
        "#pilinToolPalette,#pilinTopCommands,#pilinPopover,#pilinSelectionPopover{background:palette(base);color:palette(text);border:1px solid palette(mid);border-radius:6px;}"
        "QToolButton#pilinMapTool,QToolButton#pilinCommand{background:transparent;color:palette(text);border:0;border-radius:4px;font-size:12pt;}"
        "QToolButton#pilinMapTool:hover,QToolButton#pilinCommand:hover{background:palette(alternate-base);}"
        "QToolButton#pilinMapTool:checked{background:#1769c2;color:white;}"
        "#pilinPopoverTitle{font-weight:700;}"
        "#pilinStatus{background:rgba(18,21,24,.80);color:white;border-radius:4px;padding:4px 7px;}"
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
    for (QFrame* panel : {layersPopover_, templatePopover_}) if (panel && panel->isVisible()) { panel->adjustSize(); panel->move(qMax(m, canvasHost_->width() - panel->width() - m), y); panel->raise(); y += panel->height() + 8; }
    if (selectionPopover_ && selectionPopover_->isVisible()) { selectionPopover_->adjustSize(); selectionPopover_->move(qMax(m, canvasHost_->width() - selectionPopover_->width() - m), qMax(m, canvasHost_->height() - selectionPopover_->height() - 42)); selectionPopover_->raise(); }
    if (status_) { status_->adjustSize(); status_->move(m, qMax(m, canvasHost_->height() - status_->height() - m)); status_->raise(); }
}

void PilinReyEditor::setMap(const QJsonObject& map) {
    map_ = map; ensurePilinDocument(); undoStack_.clear(); redoStack_.clear(); selectedObjectId_.clear();
    refreshing_ = true; templateOpacity_->setValue(qRound(map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("template")).toObject().value(QStringLiteral("opacity")).toDouble(.35) * 100)); refreshing_ = false;
    refreshLayers(); refreshSelectionControls(); refreshViewport();
}

void PilinReyEditor::ensurePilinDocument() {
    QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject(); p.insert(QStringLiteral("version"), 5);
    if (!p.contains(QStringLiteral("width"))) p.insert(QStringLiteral("width"), 4096); if (!p.contains(QStringLiteral("height"))) p.insert(QStringLiteral("height"), 2304);
    QJsonObject templ = p.value(QStringLiteral("template")).toObject(); if (!templ.contains(QStringLiteral("dataUrl"))) templ.insert(QStringLiteral("dataUrl"), map_.value(QStringLiteral("backgroundImageDataUrl")).toString()); if (!templ.contains(QStringLiteral("opacity"))) templ.insert(QStringLiteral("opacity"), .35); if (!templ.contains(QStringLiteral("visible"))) templ.insert(QStringLiteral("visible"), true); p.insert(QStringLiteral("template"), templ);
    QJsonArray layers = p.value(QStringLiteral("layers")).toArray();
    if (layers.isEmpty()) {
        const QStringList names{tr("Terreno"), tr("Hidrografía"), tr("Relieve y vegetación"), tr("Fronteras y regiones"), tr("Caminos"), tr("Asentamientos"), tr("Etiquetas")};
        for (const QString& name : names) layers.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("layer"))}, {QStringLiteral("name"), name}, {QStringLiteral("visible"), true}, {QStringLiteral("locked"), false}, {QStringLiteral("opacity"), 1.0}, {QStringLiteral("objects"), QJsonArray()}});
        p.insert(QStringLiteral("activeLayerId"), layers.first().toObject().value(QStringLiteral("id")).toString());
    }
    p.insert(QStringLiteral("layers"), layers); if (!p.contains(QStringLiteral("activeLayerId")) && !layers.isEmpty()) p.insert(QStringLiteral("activeLayerId"), layers.first().toObject().value(QStringLiteral("id")).toString()); map_.insert(QStringLiteral("pilinRey"), p);
}

void PilinReyEditor::refreshLayers() {
    refreshing_ = true; layers_->clear(); const QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject(); const QString active = p.value(QStringLiteral("activeLayerId")).toString(); const QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); int current = 0;
    for (int i = 0; i < layers.size(); ++i) { const QJsonObject layer = layers.at(i).toObject(); auto* item = new QListWidgetItem(layer.value(QStringLiteral("name")).toString(tr("Capa"))); item->setData(Qt::UserRole, layer.value(QStringLiteral("id")).toString()); item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable); item->setCheckState(layer.value(QStringLiteral("visible")).toBool(true) ? Qt::Checked : Qt::Unchecked); layers_->addItem(item); if (layer.value(QStringLiteral("id")).toString() == active) current = i; }
    if (layers_->count()) layers_->setCurrentRow(current); refreshing_ = false; refreshLayerControls(); undoButton_->setEnabled(!undoStack_.isEmpty()); redoButton_->setEnabled(!redoStack_.isEmpty());
}

void PilinReyEditor::refreshLayerControls() {
    const bool old = refreshing_; refreshing_ = true; const QString active = activeLayerId(); bool found = false;
    for (const QJsonValue& v : map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray()) { const QJsonObject layer = v.toObject(); if (layer.value(QStringLiteral("id")).toString() != active) continue; layerLocked_->setChecked(layer.value(QStringLiteral("locked")).toBool(false)); layerOpacity_->setValue(qRound(layer.value(QStringLiteral("opacity")).toDouble(1) * 100)); found = true; break; }
    layerLocked_->setEnabled(found); layerOpacity_->setEnabled(found); refreshing_ = old;
}

void PilinReyEditor::refreshSelectionControls() {
    const QJsonObject object = selectedObject(); const bool has = !object.isEmpty(); selectionPopover_->setVisible(has); editObjectButton_->setEnabled(has); duplicateObjectButton_->setEnabled(has); deleteObjectButton_->setEnabled(has); linkAtlasButton_->setEnabled(has && object.value(QStringLiteral("type")).toString() == QStringLiteral("settlement"));
    if (has) { QString label = object.value(QStringLiteral("label")).toString(); if (label.isEmpty()) label = object.value(QStringLiteral("text")).toString(); if (label.isEmpty()) label = object.value(QStringLiteral("type")).toString(); selectionLabel_->setText(tr("Selección · %1").arg(label)); }
    layoutFloatingPanels();
}

void PilinReyEditor::refreshViewport() { if (viewport_) { viewport_->setDocument(map_); viewport_->setSelection(selectedObjectId_); } }
void PilinReyEditor::chooseTemplate() { const QString path = QFileDialog::getOpenFileName(this, tr("Plantilla"), QString(), tr("Imágenes (*.png *.jpg *.jpeg *.webp)")); if (path.isEmpty()) return; const QString data = imageToDataUrl(path); if (data.isEmpty()) return; mutateDocument([&](QJsonObject& p) { QJsonObject t = p.value(QStringLiteral("template")).toObject(); t.insert(QStringLiteral("dataUrl"), data); t.insert(QStringLiteral("visible"), true); p.insert(QStringLiteral("template"), t); }); map_.insert(QStringLiteral("backgroundImageDataUrl"), data); persistToArchive(); }
void PilinReyEditor::clearTemplate() { mutateDocument([](QJsonObject& p) { QJsonObject t = p.value(QStringLiteral("template")).toObject(); t.insert(QStringLiteral("dataUrl"), QString()); p.insert(QStringLiteral("template"), t); }); map_.remove(QStringLiteral("backgroundImageDataUrl")); persistToArchive(); }
void PilinReyEditor::rotateTemplate(double) {}
void PilinReyEditor::scaleTemplate(double) {}

void PilinReyEditor::addLayer() { bool ok = false; const QString name = QInputDialog::getText(this, tr("Nueva capa"), tr("Nombre:"), QLineEdit::Normal, tr("Nueva capa"), &ok).trimmed(); if (!ok || name.isEmpty()) return; mutateDocument([&](QJsonObject& p) { QJsonArray a = p.value(QStringLiteral("layers")).toArray(); const QString id = uid(QStringLiteral("layer")); a.append(QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("name"), name}, {QStringLiteral("visible"), true}, {QStringLiteral("locked"), false}, {QStringLiteral("opacity"), 1.0}, {QStringLiteral("objects"), QJsonArray()}}); p.insert(QStringLiteral("layers"), a); p.insert(QStringLiteral("activeLayerId"), id); }); }
void PilinReyEditor::duplicateLayer() { const int row = layers_->currentRow(); if (row < 0) return; mutateDocument([&](QJsonObject& p) { QJsonArray a = p.value(QStringLiteral("layers")).toArray(); if (row >= a.size()) return; QJsonObject copy = a.at(row).toObject(); const QString id = uid(QStringLiteral("layer")); copy.insert(QStringLiteral("id"), id); copy.insert(QStringLiteral("name"), copy.value(QStringLiteral("name")).toString() + tr(" copia")); QJsonArray objects = copy.value(QStringLiteral("objects")).toArray(); for (int i = 0; i < objects.size(); ++i) { QJsonObject o = objects.at(i).toObject(); o.insert(QStringLiteral("id"), uid(QStringLiteral("mapobj"))); objects.replace(i, o); } copy.insert(QStringLiteral("objects"), objects); a.insert(row + 1, copy); p.insert(QStringLiteral("layers"), a); p.insert(QStringLiteral("activeLayerId"), id); }); }
void PilinReyEditor::removeLayer() { const int row = layers_->currentRow(); if (row < 0) return; mutateDocument([&](QJsonObject& p) { QJsonArray a = p.value(QStringLiteral("layers")).toArray(); if (a.size() <= 1 || row >= a.size()) return; a.removeAt(row); p.insert(QStringLiteral("layers"), a); p.insert(QStringLiteral("activeLayerId"), a.at(qMin(row, a.size() - 1)).toObject().value(QStringLiteral("id")).toString()); }); }
void PilinReyEditor::moveLayer(int delta) { const int row = layers_->currentRow(), target = row + delta; if (row < 0) return; mutateDocument([&](QJsonObject& p) { QJsonArray a = p.value(QStringLiteral("layers")).toArray(); if (target < 0 || target >= a.size()) return; const QJsonValue v = a.at(row); a.removeAt(row); a.insert(target, v); p.insert(QStringLiteral("layers"), a); }); }
void PilinReyEditor::setLayerLocked(bool locked) { const QString active = activeLayerId(); mutateDocument([&](QJsonObject& p) { QJsonArray a = p.value(QStringLiteral("layers")).toArray(); for (int i = 0; i < a.size(); ++i) { QJsonObject l = a.at(i).toObject(); if (l.value(QStringLiteral("id")).toString() != active) continue; l.insert(QStringLiteral("locked"), locked); a.replace(i, l); break; } p.insert(QStringLiteral("layers"), a); }); }
void PilinReyEditor::setLayerOpacity(int value) { const QString active = activeLayerId(); mutateDocument([&](QJsonObject& p) { QJsonArray a = p.value(QStringLiteral("layers")).toArray(); for (int i = 0; i < a.size(); ++i) { QJsonObject l = a.at(i).toObject(); if (l.value(QStringLiteral("id")).toString() != active) continue; l.insert(QStringLiteral("opacity"), value / 100.0); a.replace(i, l); break; } p.insert(QStringLiteral("layers"), a); }); }
void PilinReyEditor::setActiveTool(Tool tool) { viewport_->setTool(tool); }
QString PilinReyEditor::activeLayerId() const { return map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("activeLayerId")).toString(); }

void PilinReyEditor::addPathObject(const QString& type, const QJsonArray& points) {
    if (type.isEmpty() || points.isEmpty()) return;
    mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); int target = layers_->currentRow(); if (target < 0 || target >= layers.size()) target = 0; QJsonObject layer = layers.at(target).toObject(); if (layer.value(QStringLiteral("locked")).toBool(false)) return; QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); const QString id = uid(QStringLiteral("mapobj")); QJsonObject object{{QStringLiteral("id"), id}, {QStringLiteral("type"), type}, {QStringLiteral("points"), points}}; if (type == QStringLiteral("region")) object.insert(QStringLiteral("closed"), true); objects.append(object); layer.insert(QStringLiteral("objects"), objects); layers.replace(target, layer); p.insert(QStringLiteral("layers"), layers); selectedObjectId_ = id; });
}

void PilinReyEditor::addSettlement(double x, double y) { bool ok = false; QString name = QInputDialog::getText(this, tr("Asentamiento"), tr("Nombre:"), QLineEdit::Normal, tr("Poblado"), &ok).trimmed(); if (!ok || name.isEmpty()) return; const QStringList kinds{tr("Capital"), tr("Ciudad"), tr("Villa"), tr("Pueblo"), tr("Aldea"), tr("Puerto"), tr("Fortaleza"), tr("Ruina")}; QString kind = QInputDialog::getItem(this, tr("Asentamiento"), tr("Tipo:"), kinds, 3, false, &ok); if (!ok) return; mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); int target = layers_->currentRow(); if (target < 0 || target >= layers.size()) target = layers.size() - 1; QJsonObject layer = layers.at(target).toObject(); QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); const QString id = uid(QStringLiteral("mapobj")); objects.append(QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("type"), QStringLiteral("settlement")}, {QStringLiteral("kind"), kind}, {QStringLiteral("label"), name}, {QStringLiteral("x"), x}, {QStringLiteral("y"), y}}); layer.insert(QStringLiteral("objects"), objects); layers.replace(target, layer); p.insert(QStringLiteral("layers"), layers); selectedObjectId_ = id; }); }
void PilinReyEditor::addLabel(double x, double y) { bool ok = false; const QString text = QInputDialog::getText(this, tr("Etiqueta"), tr("Texto:"), QLineEdit::Normal, QString(), &ok).trimmed(); if (!ok || text.isEmpty()) return; mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); int target = layers_->currentRow(); if (target < 0 || target >= layers.size()) target = layers.size() - 1; QJsonObject layer = layers.at(target).toObject(); QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); const QString id = uid(QStringLiteral("mapobj")); objects.append(QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("type"), QStringLiteral("label")}, {QStringLiteral("text"), text}, {QStringLiteral("x"), x}, {QStringLiteral("y"), y}}); layer.insert(QStringLiteral("objects"), objects); layers.replace(target, layer); p.insert(QStringLiteral("layers"), layers); selectedObjectId_ = id; }); }
void PilinReyEditor::selectObject(const QString& id) { selectedObjectId_ = id; refreshSelectionControls(); if (viewport_) viewport_->setSelection(id); }

QJsonObject PilinReyEditor::selectedObject() const { if (selectedObjectId_.isEmpty()) return {}; for (const QJsonValue& lv : map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray()) for (const QJsonValue& ov : lv.toObject().value(QStringLiteral("objects")).toArray()) { const QJsonObject o = ov.toObject(); if (o.value(QStringLiteral("id")).toString() == selectedObjectId_) return o; } return {}; }

void PilinReyEditor::movePointObject(const QString& id, double x, double y) { mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); for (int i = 0; i < layers.size(); ++i) { QJsonObject l = layers.at(i).toObject(); QJsonArray objects = l.value(QStringLiteral("objects")).toArray(); for (int j = 0; j < objects.size(); ++j) { QJsonObject o = objects.at(j).toObject(); if (o.value(QStringLiteral("id")).toString() != id) continue; o.insert(QStringLiteral("x"), x); o.insert(QStringLiteral("y"), y); objects.replace(j, o); l.insert(QStringLiteral("objects"), objects); layers.replace(i, l); p.insert(QStringLiteral("layers"), layers); emit markerMoved(id, x, y); return; } } }); }
void PilinReyEditor::movePathPoint(const QString& id, int index, double x, double y) { mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); for (int i = 0; i < layers.size(); ++i) { QJsonObject l = layers.at(i).toObject(); QJsonArray objects = l.value(QStringLiteral("objects")).toArray(); for (int j = 0; j < objects.size(); ++j) { QJsonObject o = objects.at(j).toObject(); if (o.value(QStringLiteral("id")).toString() != id) continue; QJsonArray pts = o.value(QStringLiteral("points")).toArray(); if (index < 0 || index >= pts.size()) return; QJsonObject point = pts.at(index).toObject(); point.insert(QStringLiteral("x"), x); point.insert(QStringLiteral("y"), y); pts.replace(index, point); if (o.value(QStringLiteral("closed")).toBool(false) && pts.size() > 2) { if (index == 0) pts.replace(pts.size() - 1, point); else if (index == pts.size() - 1) pts.replace(0, point); } o.insert(QStringLiteral("points"), pts); objects.replace(j, o); l.insert(QStringLiteral("objects"), objects); layers.replace(i, l); p.insert(QStringLiteral("layers"), layers); return; } } }); }

void PilinReyEditor::editSelectedObject() { const QJsonObject selected = selectedObject(); if (selected.isEmpty()) return; const QString type = selected.value(QStringLiteral("type")).toString(); if (type != QStringLiteral("settlement") && type != QStringLiteral("label")) { QMessageBox::information(this, tr("Editar"), tr("Los trazados se editan directamente arrastrando sus nodos en el lienzo.")); return; } bool ok = false; const QString old = type == QStringLiteral("label") ? selected.value(QStringLiteral("text")).toString() : selected.value(QStringLiteral("label")).toString(); const QString text = QInputDialog::getText(this, tr("Editar"), tr("Texto:"), QLineEdit::Normal, old, &ok).trimmed(); if (!ok || text.isEmpty()) return; const QString id = selectedObjectId_; mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); for (int i = 0; i < layers.size(); ++i) { QJsonObject l = layers.at(i).toObject(); QJsonArray objects = l.value(QStringLiteral("objects")).toArray(); for (int j = 0; j < objects.size(); ++j) { QJsonObject o = objects.at(j).toObject(); if (o.value(QStringLiteral("id")).toString() != id) continue; o.insert(type == QStringLiteral("label") ? QStringLiteral("text") : QStringLiteral("label"), text); objects.replace(j, o); l.insert(QStringLiteral("objects"), objects); layers.replace(i, l); p.insert(QStringLiteral("layers"), layers); return; } } }); }
void PilinReyEditor::duplicateSelectedObject() { copiedObject_ = selectedObject(); pasteCopiedObject(); }
void PilinReyEditor::copySelectedObject() { copiedObject_ = selectedObject(); }
void PilinReyEditor::pasteCopiedObject() { if (copiedObject_.isEmpty()) return; QJsonObject source = copiedObject_; mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); int target = layers_->currentRow(); if (target < 0 || target >= layers.size()) target = 0; QJsonObject layer = layers.at(target).toObject(); QJsonObject copy = source; const QString id = uid(QStringLiteral("mapobj")); copy.insert(QStringLiteral("id"), id); if (copy.contains(QStringLiteral("x"))) { copy.insert(QStringLiteral("x"), copy.value(QStringLiteral("x")).toDouble() + 50); copy.insert(QStringLiteral("y"), copy.value(QStringLiteral("y")).toDouble() + 50); } else { QJsonArray pts = copy.value(QStringLiteral("points")).toArray(); for (int i = 0; i < pts.size(); ++i) { QJsonObject q = pts.at(i).toObject(); q.insert(QStringLiteral("x"), q.value(QStringLiteral("x")).toDouble() + 50); q.insert(QStringLiteral("y"), q.value(QStringLiteral("y")).toDouble() + 50); pts.replace(i, q); } copy.insert(QStringLiteral("points"), pts); } QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); objects.append(copy); layer.insert(QStringLiteral("objects"), objects); layers.replace(target, layer); p.insert(QStringLiteral("layers"), layers); selectedObjectId_ = id; copiedObject_ = copy; }); }
void PilinReyEditor::deleteSelectedObject() { if (selectedObjectId_.isEmpty()) return; const QString id = selectedObjectId_; mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); for (int i = 0; i < layers.size(); ++i) { QJsonObject l = layers.at(i).toObject(); QJsonArray objects = l.value(QStringLiteral("objects")).toArray(); for (int j = objects.size() - 1; j >= 0; --j) if (objects.at(j).toObject().value(QStringLiteral("id")).toString() == id) objects.removeAt(j); l.insert(QStringLiteral("objects"), objects); layers.replace(i, l); } p.insert(QStringLiteral("layers"), layers); }); selectedObjectId_.clear(); refreshSelectionControls(); }

void PilinReyEditor::linkSelectedToAtlas() { const QJsonObject selected = selectedObject(); if (selected.value(QStringLiteral("type")).toString() != QStringLiteral("settlement")) return; QWidget* cursor = parentWidget(); WorldPage* world = nullptr; while (cursor) { world = qobject_cast<WorldPage*>(cursor); if (world) break; cursor = cursor->parentWidget(); } if (!world || !world->document_) return; const QJsonArray entries = world->document_->array(QStringLiteral("world")); if (entries.isEmpty()) { QMessageBox::information(this, tr("Atlas vacío"), tr("Crea primero una entrada en Mundo → Atlas.")); return; } QStringList options, ids; options << tr("— Sin enlace —"); ids << QString(); for (const QJsonValue& v : entries) { const QJsonObject e = v.toObject(); options << e.value(QStringLiteral("name")).toString(tr("Sin nombre")); ids << e.value(QStringLiteral("id")).toString(); } bool ok = false; const QString choice = QInputDialog::getItem(this, tr("Enlazar Atlas"), tr("Entrada:"), options, 0, false, &ok); if (!ok) return; const int index = options.indexOf(choice); if (index < 0) return; const QString atlasId = ids.at(index), objectId = selectedObjectId_; mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); for (int i = 0; i < layers.size(); ++i) { QJsonObject l = layers.at(i).toObject(); QJsonArray objects = l.value(QStringLiteral("objects")).toArray(); for (int j = 0; j < objects.size(); ++j) { QJsonObject o = objects.at(j).toObject(); if (o.value(QStringLiteral("id")).toString() != objectId) continue; if (atlasId.isEmpty()) o.remove(QStringLiteral("atlasId")); else o.insert(QStringLiteral("atlasId"), atlasId); objects.replace(j, o); l.insert(QStringLiteral("objects"), objects); layers.replace(i, l); p.insert(QStringLiteral("layers"), layers); return; } } }); }

void PilinReyEditor::pushUndo() { undoStack_.append(map_); while (undoStack_.size() > 60) undoStack_.removeFirst(); redoStack_.clear(); }
void PilinReyEditor::mutateDocument(const std::function<void(QJsonObject&)>& mutation) { if (refreshing_) return; pushUndo(); QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject(); mutation(p); map_.insert(QStringLiteral("pilinRey"), p); refreshViewport(); refreshLayers(); refreshSelectionControls(); persistToArchive(); emit mapEdited(map_); }
void PilinReyEditor::undo() { if (undoStack_.isEmpty()) return; redoStack_.append(map_); map_ = undoStack_.takeLast(); selectedObjectId_.clear(); refreshLayers(); refreshSelectionControls(); refreshViewport(); persistToArchive(); emit mapEdited(map_); }
void PilinReyEditor::redo() { if (redoStack_.isEmpty()) return; undoStack_.append(map_); map_ = redoStack_.takeLast(); selectedObjectId_.clear(); refreshLayers(); refreshSelectionControls(); refreshViewport(); persistToArchive(); emit mapEdited(map_); }

void PilinReyEditor::exportMap() { const QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject(); const QSize logical(qMax(1, p.value(QStringLiteral("width")).toInt(4096)), qMax(1, p.value(QStringLiteral("height")).toInt(2304))); MapExportDialog dialog(logical, this); if (dialog.exec() != QDialog::Accepted) return; const QString format = dialog.format(); QString extension = QStringLiteral(".png"), filter = tr("PNG (*.png)"); if (format == QStringLiteral("svg")) { extension = QStringLiteral(".svg"); filter = tr("SVG (*.svg)"); } else if (format == QStringLiteral("pdf")) { extension = QStringLiteral(".pdf"); filter = tr("PDF (*.pdf)"); } QString path = QFileDialog::getSaveFileName(this, tr("Exportar mapa"), map_.value(QStringLiteral("name")).toString(tr("mapa")) + extension, filter); if (path.isEmpty()) return; if (!path.endsWith(extension, Qt::CaseInsensitive)) path += extension; QString error; bool ok = format == QStringLiteral("svg") ? MapExporter::exportSvg(map_, path, dialog.outputSize(), &error) : format == QStringLiteral("pdf") ? MapExporter::exportPdf(map_, path, dialog.outputSize(), &error) : MapExporter::exportPng(map_, path, dialog.outputSize(), &error); if (!ok) QMessageBox::critical(this, tr("No se pudo exportar"), error); }

void PilinReyEditor::persistToArchive() { QWidget* cursor = parentWidget(); WorldPage* world = nullptr; while (cursor) { world = qobject_cast<WorldPage*>(cursor); if (world) break; cursor = cursor->parentWidget(); } if (!world || !world->document_ || !world->mapList_) return; const int row = world->mapList_->currentRow(); QJsonArray maps = world->document_->array(QStringLiteral("maps")); if (row < 0 || row >= maps.size()) return; QJsonObject stored = maps.at(row).toObject(); stored.insert(QStringLiteral("pilinRey"), map_.value(QStringLiteral("pilinRey"))); if (map_.contains(QStringLiteral("backgroundImageDataUrl"))) stored.insert(QStringLiteral("backgroundImageDataUrl"), map_.value(QStringLiteral("backgroundImageDataUrl"))); maps.replace(row, stored); world->document_->setArray(QStringLiteral("maps"), maps); emit world->changed(); }

} // namespace wbw
