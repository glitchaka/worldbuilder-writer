#include "ui/PilinReyEditor.h"

#include "ui/MapExportDialog.h"
#include "ui/MapExporter.h"
#include "ui/WorldPage.h"

#include <SDL3/SDL.h>

#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QFont>
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

QColor paperColor() { return QColor(224, 210, 174); }
QColor landColor() { return QColor(197, 182, 143); }
QColor regionColor() { return QColor(188, 169, 128); }

QColor objectColor(const QString& type) {
    if (type == QStringLiteral("river")) return QColor(55, 105, 147);
    if (type == QStringLiteral("road")) return QColor(112, 78, 46);
    if (type == QStringLiteral("border")) return QColor(145, 63, 55);
    if (type == QStringLiteral("forest")) return QColor(47, 88, 52);
    if (type == QStringLiteral("mountain")) return QColor(73, 65, 59);
    if (type == QStringLiteral("settlement")) return QColor(67, 45, 34);
    if (type == QStringLiteral("label")) return QColor(37, 35, 32);
    if (type == QStringLiteral("region")) return QColor(113, 91, 57);
    return QColor(47, 45, 41);
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

double distanceSquared(const QPointF& a, const QPointF& b) {
    const double dx = a.x() - b.x();
    const double dy = a.y() - b.y();
    return dx * dx + dy * dy;
}

double pointSegmentDistanceSquared(const QPointF& point, const QPointF& a, const QPointF& b) {
    const QPointF ab = b - a;
    const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
    if (len2 <= 1e-9) return distanceSquared(point, a);
    const QPointF ap = point - a;
    const double t = std::clamp((ap.x() * ab.x() + ap.y() * ab.y()) / len2, 0.0, 1.0);
    return distanceSquared(point, a + ab * t);
}

std::vector<QPointF> smoothPoints(const QJsonArray& points) {
    std::vector<QPointF> source;
    source.reserve(static_cast<size_t>(points.size()));
    for (const QJsonValue& value : points) source.push_back(jsonPoint(value));
    if (source.size() < 3) return source;

    const bool closed = distanceSquared(source.front(), source.back()) < 1e-8;
    if (closed && source.size() > 3) source.pop_back();

    std::vector<QPointF> out;
    out.reserve(source.size() * 8 + 1);
    const int samples = 7;
    const size_t count = source.size();
    const size_t segments = closed ? count : count - 1;
    for (size_t i = 0; i < segments; ++i) {
        const size_t i1 = i;
        const size_t i2 = (i + 1) % count;
        const QPointF p0 = (i == 0 && !closed) ? source[i1] : source[(i + count - 1) % count];
        const QPointF p1 = source[i1];
        const QPointF p2 = source[i2];
        const QPointF p3 = (!closed && i2 + 1 >= count) ? p2 : source[(i2 + 1) % count];
        for (int s = 0; s < samples; ++s) {
            const double t = static_cast<double>(s) / samples;
            const double t2 = t * t;
            const double t3 = t2 * t;
            const double x = 0.5 * ((2.0 * p1.x()) + (-p0.x() + p2.x()) * t
                + (2.0 * p0.x() - 5.0 * p1.x() + 4.0 * p2.x() - p3.x()) * t2
                + (-p0.x() + 3.0 * p1.x() - 3.0 * p2.x() + p3.x()) * t3);
            const double y = 0.5 * ((2.0 * p1.y()) + (-p0.y() + p2.y()) * t
                + (2.0 * p0.y() - 5.0 * p1.y() + 4.0 * p2.y() - p3.y()) * t2
                + (-p0.y() + 3.0 * p1.y() - 3.0 * p2.y() + p3.y()) * t3);
            out.emplace_back(x, y);
        }
    }
    out.push_back(closed ? out.front() : source.back());
    return out;
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

    void setSelection(const QString& id) {
        selectedObjectId_ = id;
        renderFrame();
    }

    void setSnap(bool enabled) { snap_ = enabled; }

    void fitDocument() {
        const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        const double docW = std::max(1.0, pilin.value(QStringLiteral("width")).toDouble(4096.0));
        const double docH = std::max(1.0, pilin.value(QStringLiteral("height")).toDouble(2304.0));
        if (width() < 80 || height() < 80) return;
        zoom_ = std::clamp(std::min((width() - 56.0) / docW, (height() - 56.0) / docH), 0.05, 4.0);
        pan_.setX((width() - docW * zoom_) * 0.5);
        pan_.setY((height() - docH * zoom_) * 0.5);
        viewInitialized_ = true;
        renderFrame();
    }

protected:
    void showEvent(QShowEvent* event) override {
        QWidget::showEvent(event);
        ensureRenderer();
        if (!viewInitialized_) fitDocument();
        renderFrame();
    }

    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        renderFrame();
    }

    void paintEvent(QPaintEvent*) override { renderFrame(); }

    void wheelEvent(QWheelEvent* event) override {
        const QPointF before = screenToWorld(event->position());
        const double factor = event->angleDelta().y() > 0 ? 1.14 : (1.0 / 1.14);
        zoom_ = std::clamp(zoom_ * factor, 0.05, 10.0);
        const QPointF after = screenToWorld(event->position());
        pan_ += QPointF((after.x() - before.x()) * zoom_, (after.y() - before.y()) * zoom_);
        event->accept();
        renderFrame();
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Escape) {
            drawing_ = false;
            currentPoints_ = QJsonArray();
            draggingObjectId_.clear();
            draggingPathId_.clear();
            draggingPathPointIndex_ = -1;
            measureActive_ = false;
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
        if (event->button() != Qt::LeftButton) return;

        QPointF world = snapPoint(screenToWorld(event->position()));
        if (tool_ == Tool::Measure) {
            if (!measureActive_) {
                measureStart_ = world;
                measureEnd_ = world;
                measureActive_ = true;
            } else {
                measureEnd_ = world;
                measureActive_ = false;
            }
            renderFrame();
            event->accept();
            return;
        }

        if (tool_ == Tool::Eraser) {
            selectedObjectId_ = findObjectAt(world);
            if (onSelected) onSelected(selectedObjectId_);
            if (!selectedObjectId_.isEmpty() && onDeleteSelection) onDeleteSelection();
            event->accept();
            return;
        }

        if (tool_ == Tool::Select) {
            selectedObjectId_ = findObjectAt(world);
            draggingObjectId_.clear();
            draggingPathId_.clear();
            draggingPathPointIndex_ = -1;
            if (!selectedObjectId_.isEmpty()) {
                const QJsonObject selected = objectById(selectedObjectId_);
                const QString type = selected.value(QStringLiteral("type")).toString();
                if (type == QStringLiteral("settlement") || type == QStringLiteral("label")) {
                    draggingObjectId_ = selectedObjectId_;
                } else {
                    const int node = nearestNode(selected, world);
                    if (node >= 0) {
                        draggingPathId_ = selectedObjectId_;
                        draggingPathPointIndex_ = node;
                    }
                }
            }
            if (onSelected) onSelected(selectedObjectId_);
            renderFrame();
            event->accept();
            return;
        }

        if (tool_ == Tool::Settlement) {
            if (onSettlement) onSettlement(world.x(), world.y());
            event->accept();
            return;
        }
        if (tool_ == Tool::Label) {
            if (onLabel) onLabel(world.x(), world.y());
            event->accept();
            return;
        }

        const QString type = toolType(tool_);
        if (!type.isEmpty()) {
            drawing_ = true;
            currentPoints_ = QJsonArray{pointJson(world.x(), world.y())};
            event->accept();
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (panning_) {
            pan_ += event->position() - lastMouse_;
            lastMouse_ = event->position();
            renderFrame();
            return;
        }

        QPointF world = snapPoint(screenToWorld(event->position()));
        if (measureActive_) {
            measureEnd_ = world;
            const double d = std::sqrt(distanceSquared(measureStart_, measureEnd_));
            if (onStatus) onStatus(QStringLiteral("Medida %1 u · x %2 y %3 · %4%").arg(qRound(d)).arg(qRound(world.x())).arg(qRound(world.y())).arg(qRound(zoom_ * 100.0)));
            renderFrame();
            return;
        }

        if (onStatus) onStatus(QStringLiteral("x %1  y %2  ·  %3%").arg(qRound(world.x())).arg(qRound(world.y())).arg(qRound(zoom_ * 100.0)));

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

        if (!drawing_) return;
        if (!currentPoints_.isEmpty()) {
            const QPointF last = jsonPoint(currentPoints_.last());
            const double minDistance = std::max(4.0 / zoom_, 2.0);
            if (distanceSquared(world, last) < minDistance * minDistance) return;
        }
        currentPoints_.append(pointJson(world.x(), world.y()));
        renderFrame();
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::MiddleButton || panning_) {
            panning_ = false;
            return;
        }
        if (event->button() != Qt::LeftButton) return;

        if (!draggingObjectId_.isEmpty()) {
            const QJsonObject object = objectById(draggingObjectId_);
            if (!object.isEmpty() && onMovePoint) onMovePoint(draggingObjectId_, object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble());
            draggingObjectId_.clear();
            return;
        }
        if (!draggingPathId_.isEmpty() && draggingPathPointIndex_ >= 0) {
            const QJsonObject object = objectById(draggingPathId_);
            const QJsonArray points = object.value(QStringLiteral("points")).toArray();
            if (draggingPathPointIndex_ < points.size() && onMovePathPoint) {
                const QJsonObject p = points.at(draggingPathPointIndex_).toObject();
                onMovePathPoint(draggingPathId_, draggingPathPointIndex_, p.value(QStringLiteral("x")).toDouble(), p.value(QStringLiteral("y")).toDouble());
            }
            draggingPathId_.clear();
            draggingPathPointIndex_ = -1;
            return;
        }

        if (!drawing_) return;
        drawing_ = false;
        const QString type = toolType(tool_);
        if (closesAutomatically(tool_) && currentPoints_.size() >= 3) currentPoints_.append(currentPoints_.first());
        if (currentPoints_.size() >= 2 && onPath) onPath(type, currentPoints_);
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
        window_ = SDL_CreateWindow("Pilín Rey", width(), height(), SDL_WINDOW_RESIZABLE);
#endif
        if (!window_) return false;
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (!renderer_) return false;
        SDL_SetRenderVSync(renderer_, 1);
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
        return true;
    }

    QPointF screenToWorld(const QPointF& screen) const {
        return QPointF((screen.x() - pan_.x()) / zoom_, (screen.y() - pan_.y()) / zoom_);
    }

    QPointF worldToScreen(double x, double y) const {
        return QPointF(x * zoom_ + pan_.x(), y * zoom_ + pan_.y());
    }

    QPointF snapPoint(QPointF p) const {
        if (!snap_) return p;
        constexpr double grid = 25.0;
        p.setX(std::round(p.x() / grid) * grid);
        p.setY(std::round(p.y() / grid) * grid);
        return p;
    }

    QJsonObject objectById(const QString& id) const {
        const QJsonArray layers = map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray();
        for (const QJsonValue layerValue : layers) {
            for (const QJsonValue objectValue : layerValue.toObject().value(QStringLiteral("objects")).toArray()) {
                const QJsonObject object = objectValue.toObject();
                if (object.value(QStringLiteral("id")).toString() == id) return object;
            }
        }
        return {};
    }

    int nearestNode(const QJsonObject& object, const QPointF& world) const {
        const QJsonArray points = object.value(QStringLiteral("points")).toArray();
        const double tolerance = 14.0 / std::max(zoom_, 0.05);
        const double tolerance2 = tolerance * tolerance;
        int best = -1;
        double bestDistance = tolerance2;
        for (int i = 0; i < points.size(); ++i) {
            const double d = distanceSquared(world, jsonPoint(points.at(i)));
            if (d <= bestDistance) { best = i; bestDistance = d; }
        }
        return best;
    }

    QString findObjectAt(const QPointF& world) const {
        const double tolerance = 16.0 / std::max(zoom_, 0.05);
        const double tolerance2 = tolerance * tolerance;
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
                    continue;
                }
                const QJsonArray points = object.value(QStringLiteral("points")).toArray();
                if (points.size() < 2) continue;
                QPointF previous = jsonPoint(points.first());
                for (int i = 1; i < points.size(); ++i) {
                    const QPointF current = jsonPoint(points.at(i));
                    if (pointSegmentDistanceSquared(world, previous, current) <= tolerance2) return object.value(QStringLiteral("id")).toString();
                    previous = current;
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
                object.insert(QStringLiteral("x"), x);
                object.insert(QStringLiteral("y"), y);
                objects.replace(j, object);
                layer.insert(QStringLiteral("objects"), objects);
                layers.replace(i, layer);
                pilin.insert(QStringLiteral("layers"), layers);
                map_.insert(QStringLiteral("pilinRey"), pilin);
                return;
            }
        }
    }

    void updatePathPointLocal(const QString& id, int pointIndex, double x, double y) {
        QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();
        for (int i = 0; i < layers.size(); ++i) {
            QJsonObject layer = layers.at(i).toObject();
            if (layer.value(QStringLiteral("locked")).toBool(false)) continue;
            QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
            for (int j = 0; j < objects.size(); ++j) {
                QJsonObject object = objects.at(j).toObject();
                if (object.value(QStringLiteral("id")).toString() != id) continue;
                QJsonArray points = object.value(QStringLiteral("points")).toArray();
                if (pointIndex < 0 || pointIndex >= points.size()) return;
                points.replace(pointIndex, pointJson(x, y));
                if (object.value(QStringLiteral("closed")).toBool(false) && points.size() > 2) {
                    if (pointIndex == 0) points.replace(points.size() - 1, pointJson(x, y));
                    else if (pointIndex == points.size() - 1) points.replace(0, pointJson(x, y));
                }
                object.insert(QStringLiteral("points"), points);
                objects.replace(j, object);
                layer.insert(QStringLiteral("objects"), objects);
                layers.replace(i, layer);
                pilin.insert(QStringLiteral("layers"), layers);
                map_.insert(QStringLiteral("pilinRey"), pilin);
                return;
            }
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
        QImage image;
        if (!image.loadFromData(dataUrlBytes(dataUrl))) return;
        image = image.convertToFormat(QImage::Format_RGBA8888);
        templateTexture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, image.width(), image.height());
        if (!templateTexture_) return;
        SDL_UpdateTexture(templateTexture_, nullptr, image.constBits(), image.bytesPerLine());
        SDL_SetTextureBlendMode(templateTexture_, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(templateTexture_, SDL_SCALEMODE_LINEAR);
    }

    void setColor(const QColor& color, int alpha = 255) {
        SDL_SetRenderDrawColor(renderer_, color.red(), color.green(), color.blue(), alpha);
    }

    void drawScreenPolyline(const std::vector<QPointF>& points, const QColor& color, int alpha, bool dashed = false, int width = 1) {
        if (points.size() < 2) return;
        setColor(color, alpha);
        for (size_t i = 1; i < points.size(); ++i) {
            if (dashed && ((i / 3) % 2 == 1)) continue;
            const QPointF a = worldToScreen(points[i - 1].x(), points[i - 1].y());
            const QPointF b = worldToScreen(points[i].x(), points[i].y());
            for (int o = -(width / 2); o <= width / 2; ++o) {
                SDL_RenderLine(renderer_, static_cast<float>(a.x()), static_cast<float>(a.y() + o), static_cast<float>(b.x()), static_cast<float>(b.y() + o));
                if (width >= 3) SDL_RenderLine(renderer_, static_cast<float>(a.x() + o), static_cast<float>(a.y()), static_cast<float>(b.x() + o), static_cast<float>(b.y()));
            }
        }
    }

    void fillPolygon(const std::vector<QPointF>& worldPoints, const QColor& color, int alpha) {
        if (worldPoints.size() < 4) return;
        std::vector<QPointF> points;
        points.reserve(worldPoints.size());
        for (const QPointF& p : worldPoints) points.push_back(worldToScreen(p.x(), p.y()));
        double minY = points.front().y();
        double maxY = minY;
        for (const QPointF& p : points) { minY = std::min(minY, p.y()); maxY = std::max(maxY, p.y()); }
        setColor(color, alpha);
        const int start = qMax(0, static_cast<int>(std::floor(minY)));
        const int end = qMin(height() - 1, static_cast<int>(std::ceil(maxY)));
        for (int y = start; y <= end; ++y) {
            std::vector<double> xs;
            for (size_t i = 1; i < points.size(); ++i) {
                const QPointF a = points[i - 1];
                const QPointF b = points[i];
                if ((a.y() <= y && b.y() > y) || (b.y() <= y && a.y() > y)) {
                    const double x = a.x() + (y - a.y()) * (b.x() - a.x()) / (b.y() - a.y());
                    xs.push_back(x);
                }
            }
            std::sort(xs.begin(), xs.end());
            for (size_t i = 1; i < xs.size(); i += 2) SDL_RenderLine(renderer_, static_cast<float>(xs[i - 1]), static_cast<float>(y), static_cast<float>(xs[i]), static_cast<float>(y));
        }
    }

    void drawForest(const std::vector<QPointF>& points, int alpha) {
        setColor(objectColor(QStringLiteral("forest")), alpha);
        for (size_t i = 0; i < points.size(); i += 8) {
            const QPointF p = worldToScreen(points[i].x(), points[i].y());
            const float s = static_cast<float>(std::clamp(8.0 * zoom_, 4.0, 13.0));
            SDL_RenderLine(renderer_, static_cast<float>(p.x()), static_cast<float>(p.y() - s), static_cast<float>(p.x() - s * .58), static_cast<float>(p.y() + s * .20));
            SDL_RenderLine(renderer_, static_cast<float>(p.x()), static_cast<float>(p.y() - s), static_cast<float>(p.x() + s * .58), static_cast<float>(p.y() + s * .20));
            SDL_RenderLine(renderer_, static_cast<float>(p.x() - s * .45), static_cast<float>(p.y() - s * .20), static_cast<float>(p.x() + s * .45), static_cast<float>(p.y() - s * .20));
            SDL_RenderLine(renderer_, static_cast<float>(p.x()), static_cast<float>(p.y() + s * .20), static_cast<float>(p.x()), static_cast<float>(p.y() + s * .70));
        }
    }

    void drawMountains(const std::vector<QPointF>& points, int alpha) {
        setColor(objectColor(QStringLiteral("mountain")), alpha);
        for (size_t i = 0; i < points.size(); i += 9) {
            const QPointF p = worldToScreen(points[i].x(), points[i].y());
            const float s = static_cast<float>(std::clamp(12.0 * zoom_, 5.0, 18.0));
            SDL_RenderLine(renderer_, static_cast<float>(p.x() - s), static_cast<float>(p.y() + s * .55), static_cast<float>(p.x()), static_cast<float>(p.y() - s));
            SDL_RenderLine(renderer_, static_cast<float>(p.x()), static_cast<float>(p.y() - s), static_cast<float>(p.x() + s), static_cast<float>(p.y() + s * .55));
            SDL_RenderLine(renderer_, static_cast<float>(p.x() - s * .35), static_cast<float>(p.y() - s * .05), static_cast<float>(p.x()), static_cast<float>(p.y() + s * .18));
            SDL_RenderLine(renderer_, static_cast<float>(p.x()), static_cast<float>(p.y() + s * .18), static_cast<float>(p.x() + s * .30), static_cast<float>(p.y() - s * .12));
        }
    }

    void drawText(const QString& text, const QPointF& screenPoint, int alpha, bool emphasis = false) {
        if (text.trimmed().isEmpty()) return;
        QFont font(QStringLiteral("Georgia"), qBound(9, qRound(13.0 * std::sqrt(zoom_ + 0.15)), 22));
        font.setBold(emphasis);
        QFontMetrics metrics(font);
        const QSize size = metrics.size(Qt::TextSingleLine, text) + QSize(12, 8);
        QImage image(size, QImage::Format_RGBA8888);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        painter.setFont(font);
        painter.setPen(objectColor(QStringLiteral("label")));
        painter.drawText(image.rect().adjusted(6, 4, -6, -4), Qt::AlignLeft | Qt::AlignVCenter, text);
        painter.end();
        SDL_Texture* texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, image.width(), image.height());
        if (!texture) return;
        SDL_UpdateTexture(texture, nullptr, image.constBits(), image.bytesPerLine());
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(alpha));
        SDL_FRect dest{static_cast<float>(screenPoint.x()), static_cast<float>(screenPoint.y()), static_cast<float>(image.width()), static_cast<float>(image.height())};
        SDL_RenderTexture(renderer_, texture, nullptr, &dest);
        SDL_DestroyTexture(texture);
    }

    void drawPath(const QJsonObject& object, double opacity = 1.0) {
        const QString type = object.value(QStringLiteral("type")).toString();
        const int alpha = opacityByte(opacity);
        const std::vector<QPointF> smooth = smoothPoints(object.value(QStringLiteral("points")).toArray());
        if (smooth.size() < 2) return;

        if ((type == QStringLiteral("coast") || type == QStringLiteral("region")) && smooth.size() >= 4) {
            fillPolygon(smooth, type == QStringLiteral("coast") ? landColor() : regionColor(), qRound(alpha * (type == QStringLiteral("coast") ? 0.72 : 0.36)));
        }

        if (type == QStringLiteral("forest")) drawForest(smooth, alpha);
        else if (type == QStringLiteral("mountain")) drawMountains(smooth, alpha);
        else if (type == QStringLiteral("river")) {
            drawScreenPolyline(smooth, QColor(42, 86, 124), alpha, false, zoom_ > 0.30 ? 4 : 2);
            drawScreenPolyline(smooth, QColor(99, 151, 190), qRound(alpha * .85), false, zoom_ > 0.30 ? 2 : 1);
        } else if (type == QStringLiteral("road")) {
            drawScreenPolyline(smooth, QColor(73, 54, 37), qRound(alpha * .8), false, zoom_ > 0.5 ? 3 : 1);
            drawScreenPolyline(smooth, QColor(169, 137, 91), alpha, true, zoom_ > 0.5 ? 1 : 1);
        } else if (type == QStringLiteral("border")) {
            drawScreenPolyline(smooth, objectColor(type), alpha, true, zoom_ > 0.5 ? 2 : 1);
        } else if (type == QStringLiteral("coast")) {
            drawScreenPolyline(smooth, QColor(40, 42, 39), alpha, false, zoom_ > 0.35 ? 3 : 1);
            if (zoom_ > 0.28) drawScreenPolyline(smooth, QColor(121, 104, 73), qRound(alpha * .50), false, 1);
        } else if (type == QStringLiteral("region")) {
            drawScreenPolyline(smooth, objectColor(type), alpha, true, 2);
        } else drawScreenPolyline(smooth, objectColor(type), alpha, false, 1);

        if (object.value(QStringLiteral("id")).toString() == selectedObjectId_) {
            const QJsonArray raw = object.value(QStringLiteral("points")).toArray();
            setColor(QColor(31, 116, 205), 255);
            for (int i = 0; i < raw.size(); ++i) {
                if (i == raw.size() - 1 && object.value(QStringLiteral("closed")).toBool(false)) continue;
                const QPointF p = worldToScreen(jsonPoint(raw.at(i)).x(), jsonPoint(raw.at(i)).y());
                SDL_FRect marker{static_cast<float>(p.x() - 4.0), static_cast<float>(p.y() - 4.0), 8.0f, 8.0f};
                SDL_RenderFillRect(renderer_, &marker);
            }
        }
    }

    void drawPointObject(const QJsonObject& object, double opacity) {
        const QString type = object.value(QStringLiteral("type")).toString();
        const QPointF p = worldToScreen(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble());
        const int alpha = opacityByte(opacity);
        if (type == QStringLiteral("label")) {
            drawText(object.value(QStringLiteral("text")).toString(), p, alpha, false);
        } else {
            const QString kind = object.value(QStringLiteral("kind")).toString();
            const float r = static_cast<float>(std::clamp(6.0 * std::sqrt(zoom_ + .1), 3.5, 8.0));
            setColor(QColor(50, 42, 35), alpha);
            if (kind.contains(QStringLiteral("Puerto"), Qt::CaseInsensitive)) {
                SDL_FRect rect{static_cast<float>(p.x() - r), static_cast<float>(p.y() - r), r * 2.0f, r * 2.0f};
                SDL_RenderRect(renderer_, &rect);
            } else {
                SDL_FRect rect{static_cast<float>(p.x() - r), static_cast<float>(p.y() - r), r * 2.0f, r * 2.0f};
                SDL_RenderFillRect(renderer_, &rect);
            }
            if (kind.contains(QStringLiteral("Capital"), Qt::CaseInsensitive)) {
                SDL_FRect outer{static_cast<float>(p.x() - r - 3), static_cast<float>(p.y() - r - 3), r * 2.0f + 6, r * 2.0f + 6};
                SDL_RenderRect(renderer_, &outer);
            }
            drawText(object.value(QStringLiteral("label")).toString(), p + QPointF(r + 5.0, -10.0), alpha, kind.contains(QStringLiteral("Capital"), Qt::CaseInsensitive));
        }

        if (object.value(QStringLiteral("id")).toString() == selectedObjectId_) {
            setColor(QColor(31, 116, 205), 255);
            SDL_FRect selected{static_cast<float>(p.x() - 11.0), static_cast<float>(p.y() - 11.0), 22.0f, 22.0f};
            SDL_RenderRect(renderer_, &selected);
        }
    }

    void renderFrame() {
        if (!isVisible() || !ensureRenderer()) return;
        setColor(paperColor(), 255);
        SDL_RenderClear(renderer_);

        const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        const double docW = pilin.value(QStringLiteral("width")).toDouble(4096.0);
        const double docH = pilin.value(QStringLiteral("height")).toDouble(2304.0);
        const QPointF topLeft = worldToScreen(0.0, 0.0);
        const QPointF bottomRight = worldToScreen(docW, docH);
        setColor(QColor(96, 81, 61), 80);
        SDL_FRect boundary{static_cast<float>(topLeft.x()), static_cast<float>(topLeft.y()), static_cast<float>(bottomRight.x() - topLeft.x()), static_cast<float>(bottomRight.y() - topLeft.y())};
        SDL_RenderRect(renderer_, &boundary);

        if (snap_ && zoom_ > 0.22) {
            setColor(QColor(108, 96, 74), 28);
            const double step = 100.0;
            for (double x = 0; x <= docW; x += step) {
                const QPointF a = worldToScreen(x, 0); const QPointF b = worldToScreen(x, docH);
                SDL_RenderLine(renderer_, static_cast<float>(a.x()), static_cast<float>(a.y()), static_cast<float>(b.x()), static_cast<float>(b.y()));
            }
            for (double y = 0; y <= docH; y += step) {
                const QPointF a = worldToScreen(0, y); const QPointF b = worldToScreen(docW, y);
                SDL_RenderLine(renderer_, static_cast<float>(a.x()), static_cast<float>(a.y()), static_cast<float>(b.x()), static_cast<float>(b.y()));
            }
        }

        updateTemplateTexture();
        const QJsonObject templ = pilin.value(QStringLiteral("template")).toObject();
        if (templateTexture_ && templ.value(QStringLiteral("visible")).toBool(true)) {
            const double x = templ.value(QStringLiteral("x")).toDouble(0.0);
            const double y = templ.value(QStringLiteral("y")).toDouble(0.0);
            const double scale = std::clamp(templ.value(QStringLiteral("scale")).toDouble(1.0), 0.05, 10.0);
            const double rotation = templ.value(QStringLiteral("rotation")).toDouble(0.0);
            const QPointF tl = worldToScreen(x, y);
            SDL_FRect dest{static_cast<float>(tl.x()), static_cast<float>(tl.y()), static_cast<float>(docW * zoom_ * scale), static_cast<float>(docH * zoom_ * scale)};
            SDL_SetTextureAlphaMod(templateTexture_, static_cast<Uint8>(opacityByte(templ.value(QStringLiteral("opacity")).toDouble(0.35))));
            SDL_RenderTextureRotated(renderer_, templateTexture_, nullptr, &dest, rotation, nullptr, SDL_FLIP_NONE);
        }

        for (const QJsonValue layerValue : pilin.value(QStringLiteral("layers")).toArray()) {
            const QJsonObject layer = layerValue.toObject();
            if (!layer.value(QStringLiteral("visible")).toBool(true)) continue;
            const double opacity = layer.value(QStringLiteral("opacity")).toDouble(1.0);
            for (const QJsonValue objectValue : layer.value(QStringLiteral("objects")).toArray()) {
                const QJsonObject object = objectValue.toObject();
                const QString type = object.value(QStringLiteral("type")).toString();
                if (type == QStringLiteral("settlement") || type == QStringLiteral("label")) drawPointObject(object, opacity);
                else drawPath(object, opacity);
            }
        }

        if (drawing_ && currentPoints_.size() >= 2) {
            QJsonObject preview{{QStringLiteral("type"), toolType(tool_)}, {QStringLiteral("points"), currentPoints_}};
            if (closesAutomatically(tool_)) preview.insert(QStringLiteral("closed"), true);
            drawPath(preview, 1.0);
        }

        if (measureActive_) {
            const QPointF a = worldToScreen(measureStart_.x(), measureStart_.y());
            const QPointF b = worldToScreen(measureEnd_.x(), measureEnd_.y());
            setColor(QColor(31, 116, 205), 220);
            SDL_RenderLine(renderer_, static_cast<float>(a.x()), static_cast<float>(a.y()), static_cast<float>(b.x()), static_cast<float>(b.y()));
        }

        SDL_RenderPresent(renderer_);
    }

    QJsonObject map_;
    Tool tool_ = Tool::Select;
    bool panning_ = false;
    bool drawing_ = false;
    bool viewInitialized_ = false;
    bool snap_ = true;
    bool measureActive_ = false;
    QPointF measureStart_;
    QPointF measureEnd_;
    QPointF lastMouse_;
    QPointF pan_{24.0, 24.0};
    double zoom_ = 0.2;
    QJsonArray currentPoints_;
    QString selectedObjectId_;
    QString draggingObjectId_;
    QString draggingPathId_;
    int draggingPathPointIndex_ = -1;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* templateTexture_ = nullptr;
    bool templateDirty_ = true;
};

PilinReyEditor::PilinReyEditor(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("pilinReyEditor"));
    buildUi();
}

PilinReyEditor::~PilinReyEditor() = default;

void PilinReyEditor::buildUi() {
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

    toolRail_ = new QWidget(canvasHost_);
    toolRail_->setObjectName(QStringLiteral("pilinToolRail"));
    auto* rail = new QVBoxLayout(toolRail_);
    rail->setContentsMargins(4, 4, 4, 4);
    rail->setSpacing(2);

    struct ToolDef { Tool tool; const char* symbol; const char* label; };
    const ToolDef defs[] = {
        {Tool::Select, "↖", "Seleccionar y editar nodos"},
        {Tool::Pan, "✥", "Mover lienzo"},
        {Tool::Coast, "≈", "Costa / masa de tierra"},
        {Tool::Region, "◇", "Región cerrada"},
        {Tool::River, "∿", "Río"},
        {Tool::Road, "━", "Camino"},
        {Tool::Border, "┄", "Frontera"},
        {Tool::Forest, "♣", "Bosque"},
        {Tool::Mountain, "△", "Montañas"},
        {Tool::Settlement, "●", "Asentamiento"},
        {Tool::Label, "T", "Etiqueta"},
        {Tool::Eraser, "⌫", "Borrador de objetos"},
        {Tool::Measure, "↔", "Medir distancia"}
    };
    for (const auto& def : defs) {
        auto* button = iconButton(QString::fromUtf8(def.symbol), tr(def.label), toolRail_, QStringLiteral("pilinMapTool"));
        button->setCheckable(true);
        button->setAutoExclusive(true);
        if (def.tool == Tool::Select) button->setChecked(true);
        connect(button, &QToolButton::clicked, this, [this, tool = def.tool]() { setActiveTool(tool); });
        toolButtons_.append(button);
        rail->addWidget(button);
    }
    rail->addStretch(1);

    topCommands_ = new QFrame(canvasHost_);
    topCommands_->setObjectName(QStringLiteral("pilinTopCommands"));
    auto* commands = new QHBoxLayout(topCommands_);
    commands->setContentsMargins(4, 4, 4, 4);
    commands->setSpacing(2);
    layersButton_ = iconButton(QStringLiteral("▱"), tr("Capas"), topCommands_);
    templateButton_ = iconButton(QStringLiteral("▧"), tr("Plantilla"), topCommands_);
    auto* fit = iconButton(QStringLiteral("⌗"), tr("Encajar documento"), topCommands_);
    undoButton_ = iconButton(QStringLiteral("↶"), tr("Deshacer"), topCommands_);
    redoButton_ = iconButton(QStringLiteral("↷"), tr("Rehacer"), topCommands_);
    auto* exportButton = iconButton(QStringLiteral("⇩"), tr("Exportar mapa"), topCommands_);
    snapCheck_ = new QCheckBox(tr("Ajustar"), topCommands_);
    snapCheck_->setChecked(true);
    commands->addWidget(layersButton_);
    commands->addWidget(templateButton_);
    commands->addWidget(fit);
    commands->addSpacing(4);
    commands->addWidget(undoButton_);
    commands->addWidget(redoButton_);
    commands->addSpacing(4);
    commands->addWidget(exportButton);
    commands->addSpacing(6);
    commands->addWidget(snapCheck_);

    layersPopover_ = new QFrame(canvasHost_);
    layersPopover_->setObjectName(QStringLiteral("pilinPopover"));
    layersPopover_->setFixedWidth(280);
    auto* layerLayout = new QVBoxLayout(layersPopover_);
    layerLayout->setContentsMargins(10, 10, 10, 10);
    layerLayout->setSpacing(6);
    auto* layerTitle = new QLabel(tr("Capas"), layersPopover_);
    layerTitle->setObjectName(QStringLiteral("pilinPopoverTitle"));
    layers_ = new QListWidget(layersPopover_);
    layers_->setMinimumHeight(190);
    auto* layerButtons = new QHBoxLayout;
    auto* add = iconButton(QStringLiteral("+"), tr("Nueva capa"), layersPopover_);
    auto* duplicate = iconButton(QStringLiteral("⧉"), tr("Duplicar capa"), layersPopover_);
    auto* remove = iconButton(QStringLiteral("−"), tr("Eliminar capa"), layersPopover_);
    auto* up = iconButton(QStringLiteral("↑"), tr("Subir capa"), layersPopover_);
    auto* down = iconButton(QStringLiteral("↓"), tr("Bajar capa"), layersPopover_);
    layerButtons->addWidget(add); layerButtons->addWidget(duplicate); layerButtons->addWidget(remove); layerButtons->addStretch(); layerButtons->addWidget(up); layerButtons->addWidget(down);
    layerLocked_ = new QCheckBox(tr("Bloquear capa"), layersPopover_);
    layerOpacity_ = new QSlider(Qt::Horizontal, layersPopover_);
    layerOpacity_->setRange(0, 100); layerOpacity_->setValue(100);
    layerLayout->addWidget(layerTitle);
    layerLayout->addWidget(layers_);
    layerLayout->addLayout(layerButtons);
    layerLayout->addWidget(layerLocked_);
    layerLayout->addWidget(new QLabel(tr("Opacidad"), layersPopover_));
    layerLayout->addWidget(layerOpacity_);
    layersPopover_->hide();

    templatePopover_ = new QFrame(canvasHost_);
    templatePopover_->setObjectName(QStringLiteral("pilinPopover"));
    templatePopover_->setFixedWidth(280);
    auto* templateLayout = new QVBoxLayout(templatePopover_);
    templateLayout->setContentsMargins(10, 10, 10, 10);
    templateLayout->setSpacing(6);
    auto* templateTitle = new QLabel(tr("Plantilla"), templatePopover_);
    templateTitle->setObjectName(QStringLiteral("pilinPopoverTitle"));
    auto* templateButtons = new QHBoxLayout;
    auto* loadTemplate = iconButton(QStringLiteral("＋"), tr("Cargar plantilla"), templatePopover_);
    auto* clearTemplateButton = iconButton(QStringLiteral("×"), tr("Quitar plantilla"), templatePopover_);
    auto* rotateLeft = iconButton(QStringLiteral("↶"), tr("Girar 15° a la izquierda"), templatePopover_);
    auto* rotateRight = iconButton(QStringLiteral("↷"), tr("Girar 15° a la derecha"), templatePopover_);
    auto* scaleDown = iconButton(QStringLiteral("−"), tr("Reducir plantilla"), templatePopover_);
    auto* scaleUp = iconButton(QStringLiteral("+"), tr("Ampliar plantilla"), templatePopover_);
    templateButtons->addWidget(loadTemplate); templateButtons->addWidget(clearTemplateButton); templateButtons->addStretch(); templateButtons->addWidget(rotateLeft); templateButtons->addWidget(rotateRight); templateButtons->addWidget(scaleDown); templateButtons->addWidget(scaleUp);
    templateOpacity_ = new QSlider(Qt::Horizontal, templatePopover_);
    templateOpacity_->setRange(0, 100); templateOpacity_->setValue(35);
    templateLayout->addWidget(templateTitle);
    templateLayout->addLayout(templateButtons);
    templateLayout->addWidget(new QLabel(tr("Opacidad"), templatePopover_));
    templateLayout->addWidget(templateOpacity_);
    templatePopover_->hide();

    selectionPopover_ = new QFrame(canvasHost_);
    selectionPopover_->setObjectName(QStringLiteral("pilinSelectionPopover"));
    selectionPopover_->setFixedWidth(220);
    auto* selectionLayout = new QVBoxLayout(selectionPopover_);
    selectionLayout->setContentsMargins(9, 8, 9, 8);
    selectionLayout->setSpacing(5);
    selectionLabel_ = new QLabel(tr("Selección"), selectionPopover_);
    selectionLabel_->setObjectName(QStringLiteral("pilinPopoverTitle"));
    auto* actionRow = new QHBoxLayout;
    editObjectButton_ = iconButton(QStringLiteral("✎"), tr("Editar"), selectionPopover_);
    duplicateObjectButton_ = iconButton(QStringLiteral("⧉"), tr("Duplicar"), selectionPopover_);
    linkAtlasButton_ = iconButton(QStringLiteral("⌁"), tr("Enlazar con Atlas"), selectionPopover_);
    deleteObjectButton_ = iconButton(QStringLiteral("×"), tr("Eliminar"), selectionPopover_);
    actionRow->addWidget(editObjectButton_); actionRow->addWidget(duplicateObjectButton_); actionRow->addWidget(linkAtlasButton_); actionRow->addStretch(); actionRow->addWidget(deleteObjectButton_);
    selectionLayout->addWidget(selectionLabel_);
    selectionLayout->addLayout(actionRow);
    selectionPopover_->hide();

    status_ = new QLabel(tr("Pilín Rey · listo"), canvasHost_);
    status_->setObjectName(QStringLiteral("pilinStatus"));
    status_->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    viewport_->onPath = [this](const QString& type, const QJsonArray& points) { addPathObject(type, points); };
    viewport_->onSettlement = [this](double x, double y) { addSettlement(x, y); };
    viewport_->onLabel = [this](double x, double y) { addLabel(x, y); };
    viewport_->onSelected = [this](const QString& id) { selectObject(id); };
    viewport_->onMovePoint = [this](const QString& id, double x, double y) { movePointObject(id, x, y); };
    viewport_->onMovePathPoint = [this](const QString& id, int pointIndex, double x, double y) { movePathPoint(id, pointIndex, x, y); };
    viewport_->onDeleteSelection = [this]() { deleteSelectedObject(); };
    viewport_->onStatus = [this](const QString& text) { status_->setText(text); };

    connect(layersButton_, &QToolButton::clicked, this, [this]() { templatePopover_->hide(); layersPopover_->setVisible(!layersPopover_->isVisible()); layoutFloatingPanels(); });
    connect(templateButton_, &QToolButton::clicked, this, [this]() { layersPopover_->hide(); templatePopover_->setVisible(!templatePopover_->isVisible()); layoutFloatingPanels(); });
    connect(fit, &QToolButton::clicked, viewport_, &PilinReyViewport::fitDocument);
    connect(undoButton_, &QToolButton::clicked, this, &PilinReyEditor::undo);
    connect(redoButton_, &QToolButton::clicked, this, &PilinReyEditor::redo);
    connect(exportButton, &QToolButton::clicked, this, &PilinReyEditor::exportMap);
    connect(snapCheck_, &QCheckBox::toggled, viewport_, &PilinReyViewport::setSnap);

    connect(loadTemplate, &QToolButton::clicked, this, &PilinReyEditor::chooseTemplate);
    connect(clearTemplateButton, &QToolButton::clicked, this, &PilinReyEditor::clearTemplate);
    connect(rotateLeft, &QToolButton::clicked, this, [this]() { rotateTemplate(-15.0); });
    connect(rotateRight, &QToolButton::clicked, this, [this]() { rotateTemplate(15.0); });
    connect(scaleDown, &QToolButton::clicked, this, [this]() { scaleTemplate(0.9); });
    connect(scaleUp, &QToolButton::clicked, this, [this]() { scaleTemplate(1.1); });
    connect(templateOpacity_, &QSlider::valueChanged, this, [this](int value) {
        if (refreshing_) return;
        mutateDocument([&](QJsonObject& pilin) {
            QJsonObject templ = pilin.value(QStringLiteral("template")).toObject();
            templ.insert(QStringLiteral("opacity"), value / 100.0);
            pilin.insert(QStringLiteral("template"), templ);
        });
    });

    connect(add, &QToolButton::clicked, this, &PilinReyEditor::addLayer);
    connect(duplicate, &QToolButton::clicked, this, &PilinReyEditor::duplicateLayer);
    connect(remove, &QToolButton::clicked, this, &PilinReyEditor::removeLayer);
    connect(up, &QToolButton::clicked, this, [this]() { moveLayer(-1); });
    connect(down, &QToolButton::clicked, this, [this]() { moveLayer(1); });
    connect(layerLocked_, &QCheckBox::toggled, this, [this](bool checked) { if (!refreshing_) setLayerLocked(checked); });
    connect(layerOpacity_, &QSlider::valueChanged, this, [this](int value) { if (!refreshing_) setLayerOpacity(value); });
    connect(layers_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (refreshing_ || row < 0) return;
        QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        const QJsonArray array = pilin.value(QStringLiteral("layers")).toArray();
        if (row >= array.size()) return;
        pilin.insert(QStringLiteral("activeLayerId"), array.at(row).toObject().value(QStringLiteral("id")).toString());
        map_.insert(QStringLiteral("pilinRey"), pilin);
        refreshLayerControls();
        persistToArchive();
    });
    connect(layers_, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        if (refreshing_) return;
        const QString id = item->data(Qt::UserRole).toString();
        mutateDocument([&](QJsonObject& pilin) {
            QJsonArray array = pilin.value(QStringLiteral("layers")).toArray();
            for (int i = 0; i < array.size(); ++i) {
                QJsonObject layer = array.at(i).toObject();
                if (layer.value(QStringLiteral("id")).toString() != id) continue;
                layer.insert(QStringLiteral("name"), item->text());
                layer.insert(QStringLiteral("visible"), item->checkState() == Qt::Checked);
                array.replace(i, layer);
                break;
            }
            pilin.insert(QStringLiteral("layers"), array);
        });
    });

    connect(editObjectButton_, &QToolButton::clicked, this, &PilinReyEditor::editSelectedObject);
    connect(duplicateObjectButton_, &QToolButton::clicked, this, &PilinReyEditor::duplicateSelectedObject);
    connect(linkAtlasButton_, &QToolButton::clicked, this, &PilinReyEditor::linkSelectedToAtlas);
    connect(deleteObjectButton_, &QToolButton::clicked, this, &PilinReyEditor::deleteSelectedObject);

    auto* undoShortcut = new QShortcut(QKeySequence::Undo, this);
    auto* redoShortcut = new QShortcut(QKeySequence::Redo, this);
    auto* copyShortcut = new QShortcut(QKeySequence::Copy, this);
    auto* pasteShortcut = new QShortcut(QKeySequence::Paste, this);
    connect(undoShortcut, &QShortcut::activated, this, &PilinReyEditor::undo);
    connect(redoShortcut, &QShortcut::activated, this, &PilinReyEditor::redo);
    connect(copyShortcut, &QShortcut::activated, this, &PilinReyEditor::copySelectedObject);
    connect(pasteShortcut, &QShortcut::activated, this, &PilinReyEditor::pasteCopiedObject);

    setStyleSheet(QStringLiteral(
        "#pilinReyEditor,#pilinCanvasHost{background:transparent;}"
        "#pilinToolRail,#pilinTopCommands,#pilinPopover,#pilinSelectionPopover{background:palette(base);color:palette(text);border:1px solid palette(mid);border-radius:5px;}"
        "QToolButton#pilinMapTool,QToolButton#pilinCommand{background:transparent;color:palette(text);border:0;border-radius:4px;padding:0;font-size:12pt;}"
        "QToolButton#pilinMapTool:hover,QToolButton#pilinCommand:hover{background:palette(alternate-base);}"
        "QToolButton#pilinMapTool:checked{background:#1769c2;color:#ffffff;}"
        "#pilinPopoverTitle{font-weight:700;color:palette(text);}"
        "#pilinStatus{background:rgba(20,24,29,0.74);color:#ffffff;border-radius:4px;padding:4px 7px;}"
    ));

    refreshSelectionControls();
}

void PilinReyEditor::resizeEvent(QResizeEvent* event) { QWidget::resizeEvent(event); layoutFloatingPanels(); }
void PilinReyEditor::showEvent(QShowEvent* event) { QWidget::showEvent(event); layoutFloatingPanels(); }

void PilinReyEditor::layoutFloatingPanels() {
    if (!canvasHost_) return;
    const int margin = 10;
    if (toolRail_) { toolRail_->adjustSize(); toolRail_->move(margin, margin); toolRail_->raise(); }
    if (topCommands_) { topCommands_->adjustSize(); topCommands_->move(qMax(margin, canvasHost_->width() - topCommands_->width() - margin), margin); topCommands_->raise(); }
    int y = margin + (topCommands_ ? topCommands_->height() : 0) + 8;
    for (QFrame* panel : {layersPopover_, templatePopover_}) {
        if (!panel || !panel->isVisible()) continue;
        panel->adjustSize(); panel->move(qMax(margin, canvasHost_->width() - panel->width() - margin), y); panel->raise(); y += panel->height() + 8;
    }
    if (selectionPopover_ && selectionPopover_->isVisible()) {
        selectionPopover_->adjustSize();
        selectionPopover_->move(qMax(margin, canvasHost_->width() - selectionPopover_->width() - margin), qMax(margin, canvasHost_->height() - selectionPopover_->height() - 38));
        selectionPopover_->raise();
    }
    if (status_) { status_->adjustSize(); status_->move(margin, qMax(margin, canvasHost_->height() - status_->height() - margin)); status_->raise(); }
}

void PilinReyEditor::setMap(const QJsonObject& map) {
    map_ = map;
    ensurePilinDocument();
    undoStack_.clear(); redoStack_.clear(); selectedObjectId_.clear();
    refreshing_ = true;
    const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
    templateOpacity_->setValue(qRound(pilin.value(QStringLiteral("template")).toObject().value(QStringLiteral("opacity")).toDouble(0.35) * 100.0));
    refreshing_ = false;
    refreshLayers(); refreshSelectionControls(); refreshViewport();
}

void PilinReyEditor::ensurePilinDocument() {
    QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
    if (!pilin.contains(QStringLiteral("version"))) pilin.insert(QStringLiteral("version"), 4);
    if (!pilin.contains(QStringLiteral("width"))) pilin.insert(QStringLiteral("width"), 4096);
    if (!pilin.contains(QStringLiteral("height"))) pilin.insert(QStringLiteral("height"), 2304);
    QJsonObject templ = pilin.value(QStringLiteral("template")).toObject();
    if (!templ.contains(QStringLiteral("dataUrl"))) templ.insert(QStringLiteral("dataUrl"), map_.value(QStringLiteral("backgroundImageDataUrl")).toString());
    if (!templ.contains(QStringLiteral("opacity"))) templ.insert(QStringLiteral("opacity"), 0.35);
    if (!templ.contains(QStringLiteral("visible"))) templ.insert(QStringLiteral("visible"), true);
    if (!templ.contains(QStringLiteral("locked"))) templ.insert(QStringLiteral("locked"), true);
    if (!templ.contains(QStringLiteral("x"))) templ.insert(QStringLiteral("x"), 0.0);
    if (!templ.contains(QStringLiteral("y"))) templ.insert(QStringLiteral("y"), 0.0);
    if (!templ.contains(QStringLiteral("scale"))) templ.insert(QStringLiteral("scale"), 1.0);
    if (!templ.contains(QStringLiteral("rotation"))) templ.insert(QStringLiteral("rotation"), 0.0);
    pilin.insert(QStringLiteral("template"), templ);

    QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();
    if (layers.isEmpty()) {
        const QStringList names{tr("Costa y regiones"), tr("Hidrografía"), tr("Relieve"), tr("Vegetación"), tr("Fronteras"), tr("Caminos"), tr("Asentamientos"), tr("Etiquetas")};
        for (const QString& name : names) layers.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("layer"))}, {QStringLiteral("name"), name}, {QStringLiteral("visible"), true}, {QStringLiteral("locked"), false}, {QStringLiteral("opacity"), 1.0}, {QStringLiteral("objects"), QJsonArray()}});
        pilin.insert(QStringLiteral("activeLayerId"), layers.first().toObject().value(QStringLiteral("id")).toString());
    }
    for (int i = 0; i < layers.size(); ++i) {
        QJsonObject layer = layers.at(i).toObject();
        if (!layer.contains(QStringLiteral("locked"))) layer.insert(QStringLiteral("locked"), false);
        if (!layer.contains(QStringLiteral("opacity"))) layer.insert(QStringLiteral("opacity"), 1.0);
        layers.replace(i, layer);
    }
    pilin.insert(QStringLiteral("layers"), layers);
    if (!pilin.contains(QStringLiteral("activeLayerId")) && !layers.isEmpty()) pilin.insert(QStringLiteral("activeLayerId"), layers.first().toObject().value(QStringLiteral("id")).toString());
    map_.insert(QStringLiteral("pilinRey"), pilin);
}

void PilinReyEditor::refreshLayers() {
    refreshing_ = true;
    layers_->clear();
    const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
    const QString active = pilin.value(QStringLiteral("activeLayerId")).toString();
    const QJsonArray array = pilin.value(QStringLiteral("layers")).toArray();
    int activeRow = 0;
    for (int i = 0; i < array.size(); ++i) {
        const QJsonObject layer = array.at(i).toObject();
        auto* item = new QListWidgetItem(layer.value(QStringLiteral("name")).toString(tr("Capa")));
        item->setData(Qt::UserRole, layer.value(QStringLiteral("id")).toString());
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
        item->setCheckState(layer.value(QStringLiteral("visible")).toBool(true) ? Qt::Checked : Qt::Unchecked);
        if (layer.value(QStringLiteral("locked")).toBool(false)) { QFont font = item->font(); font.setItalic(true); item->setFont(font); }
        layers_->addItem(item);
        if (layer.value(QStringLiteral("id")).toString() == active) activeRow = i;
    }
    if (layers_->count()) layers_->setCurrentRow(activeRow);
    refreshing_ = false;
    refreshLayerControls();
    undoButton_->setEnabled(!undoStack_.isEmpty());
    redoButton_->setEnabled(!redoStack_.isEmpty());
}

void PilinReyEditor::refreshLayerControls() {
    const bool old = refreshing_; refreshing_ = true;
    const QString active = activeLayerId();
    const QJsonArray array = map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray();
    bool found = false;
    for (const QJsonValue value : array) {
        const QJsonObject layer = value.toObject();
        if (layer.value(QStringLiteral("id")).toString() != active) continue;
        layerLocked_->setChecked(layer.value(QStringLiteral("locked")).toBool(false));
        layerOpacity_->setValue(qRound(layer.value(QStringLiteral("opacity")).toDouble(1.0) * 100.0));
        found = true; break;
    }
    layerLocked_->setEnabled(found); layerOpacity_->setEnabled(found); refreshing_ = old;
}

void PilinReyEditor::refreshSelectionControls() {
    const QJsonObject object = selectedObject();
    const bool has = !object.isEmpty();
    selectionPopover_->setVisible(has);
    editObjectButton_->setEnabled(has); duplicateObjectButton_->setEnabled(has); deleteObjectButton_->setEnabled(has);
    linkAtlasButton_->setEnabled(has && object.value(QStringLiteral("type")).toString() == QStringLiteral("settlement"));
    if (has) {
        QString label = object.value(QStringLiteral("label")).toString();
        if (label.isEmpty()) label = object.value(QStringLiteral("text")).toString();
        if (label.isEmpty()) label = object.value(QStringLiteral("type")).toString();
        selectionLabel_->setText(tr("Selección · %1").arg(label));
    }
    layoutFloatingPanels();
}

void PilinReyEditor::refreshViewport() { if (viewport_) { viewport_->setDocument(map_); viewport_->setSelection(selectedObjectId_); } }

void PilinReyEditor::chooseTemplate() {
    const QString path = QFileDialog::getOpenFileName(this, tr("Plantilla para Pilín Rey"), QString(), tr("Imágenes (*.png *.jpg *.jpeg *.webp)"));
    if (path.isEmpty()) return;
    const QString dataUrl = imageToDataUrl(path); if (dataUrl.isEmpty()) return;
    mutateDocument([&](QJsonObject& pilin) { QJsonObject templ = pilin.value(QStringLiteral("template")).toObject(); templ.insert(QStringLiteral("dataUrl"), dataUrl); templ.insert(QStringLiteral("visible"), true); pilin.insert(QStringLiteral("template"), templ); });
    map_.insert(QStringLiteral("backgroundImageDataUrl"), dataUrl); persistToArchive();
}

void PilinReyEditor::clearTemplate() {
    mutateDocument([](QJsonObject& pilin) { QJsonObject templ = pilin.value(QStringLiteral("template")).toObject(); templ.insert(QStringLiteral("dataUrl"), QString()); pilin.insert(QStringLiteral("template"), templ); });
    map_.remove(QStringLiteral("backgroundImageDataUrl")); persistToArchive();
}

void PilinReyEditor::rotateTemplate(double delta) { mutateDocument([&](QJsonObject& pilin) { QJsonObject templ = pilin.value(QStringLiteral("template")).toObject(); templ.insert(QStringLiteral("rotation"), templ.value(QStringLiteral("rotation")).toDouble() + delta); pilin.insert(QStringLiteral("template"), templ); }); }
void PilinReyEditor::scaleTemplate(double factor) { mutateDocument([&](QJsonObject& pilin) { QJsonObject templ = pilin.value(QStringLiteral("template")).toObject(); templ.insert(QStringLiteral("scale"), std::clamp(templ.value(QStringLiteral("scale")).toDouble(1.0) * factor, 0.05, 10.0)); pilin.insert(QStringLiteral("template"), templ); }); }

void PilinReyEditor::addLayer() {
    bool ok = false; const QString name = QInputDialog::getText(this, tr("Nueva capa"), tr("Nombre:"), QLineEdit::Normal, tr("Nueva capa"), &ok).trimmed(); if (!ok || name.isEmpty()) return;
    mutateDocument([&](QJsonObject& pilin) { QJsonArray array = pilin.value(QStringLiteral("layers")).toArray(); const QString id = uid(QStringLiteral("layer")); array.append(QJsonObject{{QStringLiteral("id"), id},{QStringLiteral("name"),name},{QStringLiteral("visible"),true},{QStringLiteral("locked"),false},{QStringLiteral("opacity"),1.0},{QStringLiteral("objects"),QJsonArray()}}); pilin.insert(QStringLiteral("layers"),array); pilin.insert(QStringLiteral("activeLayerId"),id); });
}

void PilinReyEditor::duplicateLayer() {
    const int row = layers_->currentRow(); if (row < 0) return;
    mutateDocument([&](QJsonObject& pilin) { QJsonArray array = pilin.value(QStringLiteral("layers")).toArray(); if (row >= array.size()) return; QJsonObject copy = array.at(row).toObject(); const QString id = uid(QStringLiteral("layer")); copy.insert(QStringLiteral("id"),id); copy.insert(QStringLiteral("name"),copy.value(QStringLiteral("name")).toString(tr("Capa"))+tr(" copia")); copy.insert(QStringLiteral("locked"),false); QJsonArray objects=copy.value(QStringLiteral("objects")).toArray(); for(int i=0;i<objects.size();++i){QJsonObject o=objects.at(i).toObject();o.insert(QStringLiteral("id"),uid(QStringLiteral("mapobj")));objects.replace(i,o);} copy.insert(QStringLiteral("objects"),objects); array.insert(row+1,copy); pilin.insert(QStringLiteral("layers"),array); pilin.insert(QStringLiteral("activeLayerId"),id); });
}

void PilinReyEditor::removeLayer() {
    const int row = layers_->currentRow(); if (row < 0) return;
    mutateDocument([&](QJsonObject& pilin) { QJsonArray array=pilin.value(QStringLiteral("layers")).toArray(); if(row>=array.size()||array.size()<=1)return; array.removeAt(row); pilin.insert(QStringLiteral("layers"),array); pilin.insert(QStringLiteral("activeLayerId"),array.at(qMin(row,array.size()-1)).toObject().value(QStringLiteral("id")).toString()); }); selectedObjectId_.clear();
}

void PilinReyEditor::moveLayer(int delta) {
    const int row=layers_->currentRow(); const int target=row+delta; if(row<0)return;
    mutateDocument([&](QJsonObject& pilin){QJsonArray array=pilin.value(QStringLiteral("layers")).toArray();if(target<0||target>=array.size())return;const QJsonValue v=array.at(row);array.removeAt(row);array.insert(target,v);pilin.insert(QStringLiteral("layers"),array);}); if(target>=0&&target<layers_->count())layers_->setCurrentRow(target);
}

void PilinReyEditor::setLayerLocked(bool locked) { const QString active=activeLayerId(); if(active.isEmpty())return; mutateDocument([&](QJsonObject& pilin){QJsonArray array=pilin.value(QStringLiteral("layers")).toArray();for(int i=0;i<array.size();++i){QJsonObject l=array.at(i).toObject();if(l.value(QStringLiteral("id")).toString()!=active)continue;l.insert(QStringLiteral("locked"),locked);array.replace(i,l);break;}pilin.insert(QStringLiteral("layers"),array);}); }
void PilinReyEditor::setLayerOpacity(int value) { const QString active=activeLayerId(); if(active.isEmpty())return; mutateDocument([&](QJsonObject& pilin){QJsonArray array=pilin.value(QStringLiteral("layers")).toArray();for(int i=0;i<array.size();++i){QJsonObject l=array.at(i).toObject();if(l.value(QStringLiteral("id")).toString()!=active)continue;l.insert(QStringLiteral("opacity"),std::clamp(value/100.0,0.0,1.0));array.replace(i,l);break;}pilin.insert(QStringLiteral("layers"),array);}); }

void PilinReyEditor::setActiveTool(Tool tool) { viewport_->setTool(tool); }
QString PilinReyEditor::activeLayerId() const { return map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("activeLayerId")).toString(); }

void PilinReyEditor::addPathObject(const QString& type, const QJsonArray& points) {
    if(type.isEmpty()||points.size()<2)return; const QString active=activeLayerId();
    mutateDocument([&](QJsonObject& pilin){QJsonArray layers=pilin.value(QStringLiteral("layers")).toArray();for(int i=0;i<layers.size();++i){QJsonObject layer=layers.at(i).toObject();if(layer.value(QStringLiteral("id")).toString()!=active||layer.value(QStringLiteral("locked")).toBool(false))continue;QJsonArray objects=layer.value(QStringLiteral("objects")).toArray();const QString id=uid(QStringLiteral("mapobj"));QJsonObject object{{QStringLiteral("id"),id},{QStringLiteral("type"),type},{QStringLiteral("points"),points}};if(type==QStringLiteral("coast")||type==QStringLiteral("region"))object.insert(QStringLiteral("closed"),true);objects.append(object);layer.insert(QStringLiteral("objects"),objects);layers.replace(i,layer);pilin.insert(QStringLiteral("layers"),layers);selectedObjectId_=id;break;}});
}

void PilinReyEditor::addSettlement(double x,double y){bool ok=false;QString label=QInputDialog::getText(this,tr("Asentamiento"),tr("Nombre:"),QLineEdit::Normal,tr("Poblado"),&ok).trimmed();if(!ok||label.isEmpty())return;const QStringList kinds{tr("Capital"),tr("Ciudad"),tr("Villa"),tr("Pueblo"),tr("Aldea"),tr("Puerto"),tr("Fortaleza"),tr("Ruina")};QString kind=QInputDialog::getItem(this,tr("Asentamiento"),tr("Tipo:"),kinds,3,false,&ok);if(!ok)return;const QString active=activeLayerId();mutateDocument([&](QJsonObject& pilin){QJsonArray layers=pilin.value(QStringLiteral("layers")).toArray();for(int i=0;i<layers.size();++i){QJsonObject layer=layers.at(i).toObject();if(layer.value(QStringLiteral("id")).toString()!=active||layer.value(QStringLiteral("locked")).toBool(false))continue;QJsonArray objects=layer.value(QStringLiteral("objects")).toArray();const QString id=uid(QStringLiteral("mapobj"));objects.append(QJsonObject{{QStringLiteral("id"),id},{QStringLiteral("type"),QStringLiteral("settlement")},{QStringLiteral("kind"),kind},{QStringLiteral("label"),label},{QStringLiteral("x"),x},{QStringLiteral("y"),y}});layer.insert(QStringLiteral("objects"),objects);layers.replace(i,layer);pilin.insert(QStringLiteral("layers"),layers);selectedObjectId_=id;break;}});}

void PilinReyEditor::addLabel(double x,double y){bool ok=false;QString text=QInputDialog::getText(this,tr("Etiqueta"),tr("Texto:"),QLineEdit::Normal,QString(),&ok).trimmed();if(!ok||text.isEmpty())return;const QString active=activeLayerId();mutateDocument([&](QJsonObject& pilin){QJsonArray layers=pilin.value(QStringLiteral("layers")).toArray();for(int i=0;i<layers.size();++i){QJsonObject layer=layers.at(i).toObject();if(layer.value(QStringLiteral("id")).toString()!=active||layer.value(QStringLiteral("locked")).toBool(false))continue;QJsonArray objects=layer.value(QStringLiteral("objects")).toArray();const QString id=uid(QStringLiteral("mapobj"));objects.append(QJsonObject{{QStringLiteral("id"),id},{QStringLiteral("type"),QStringLiteral("label")},{QStringLiteral("text"),text},{QStringLiteral("x"),x},{QStringLiteral("y"),y}});layer.insert(QStringLiteral("objects"),objects);layers.replace(i,layer);pilin.insert(QStringLiteral("layers"),layers);selectedObjectId_=id;break;}});}

void PilinReyEditor::selectObject(const QString& id){selectedObjectId_=id;refreshSelectionControls();if(viewport_)viewport_->setSelection(id);}

QJsonObject PilinReyEditor::selectedObject() const{if(selectedObjectId_.isEmpty())return{};const QJsonArray layers=map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray();for(const QJsonValue lv:layers)for(const QJsonValue ov:lv.toObject().value(QStringLiteral("objects")).toArray()){const QJsonObject o=ov.toObject();if(o.value(QStringLiteral("id")).toString()==selectedObjectId_)return o;}return{};}

void PilinReyEditor::movePointObject(const QString& id,double x,double y){mutateDocument([&](QJsonObject& pilin){QJsonArray layers=pilin.value(QStringLiteral("layers")).toArray();for(int i=0;i<layers.size();++i){QJsonObject layer=layers.at(i).toObject();if(layer.value(QStringLiteral("locked")).toBool(false))continue;QJsonArray objects=layer.value(QStringLiteral("objects")).toArray();for(int j=0;j<objects.size();++j){QJsonObject o=objects.at(j).toObject();if(o.value(QStringLiteral("id")).toString()!=id)continue;o.insert(QStringLiteral("x"),x);o.insert(QStringLiteral("y"),y);objects.replace(j,o);layer.insert(QStringLiteral("objects"),objects);layers.replace(i,layer);pilin.insert(QStringLiteral("layers"),layers);emit markerMoved(id,x,y);return;}}});}

void PilinReyEditor::movePathPoint(const QString& id,int pointIndex,double x,double y){mutateDocument([&](QJsonObject& pilin){QJsonArray layers=pilin.value(QStringLiteral("layers")).toArray();for(int i=0;i<layers.size();++i){QJsonObject layer=layers.at(i).toObject();if(layer.value(QStringLiteral("locked")).toBool(false))continue;QJsonArray objects=layer.value(QStringLiteral("objects")).toArray();for(int j=0;j<objects.size();++j){QJsonObject o=objects.at(j).toObject();if(o.value(QStringLiteral("id")).toString()!=id)continue;QJsonArray points=o.value(QStringLiteral("points")).toArray();if(pointIndex<0||pointIndex>=points.size())return;points.replace(pointIndex,pointJson(x,y));if(o.value(QStringLiteral("closed")).toBool(false)&&points.size()>2){if(pointIndex==0)points.replace(points.size()-1,pointJson(x,y));else if(pointIndex==points.size()-1)points.replace(0,pointJson(x,y));}o.insert(QStringLiteral("points"),points);objects.replace(j,o);layer.insert(QStringLiteral("objects"),objects);layers.replace(i,layer);pilin.insert(QStringLiteral("layers"),layers);return;}}});}

void PilinReyEditor::editSelectedObject(){const QJsonObject selected=selectedObject();if(selected.isEmpty())return;const QString id=selectedObjectId_;const QString type=selected.value(QStringLiteral("type")).toString();if(type!=QStringLiteral("settlement")&&type!=QStringLiteral("label")){QMessageBox::information(this,tr("Editar trazado"),tr("Selecciona el trazado y arrastra los nodos azules directamente en el lienzo."));return;}bool ok=false;QString text=type==QStringLiteral("label")?selected.value(QStringLiteral("text")).toString():selected.value(QStringLiteral("label")).toString();text=QInputDialog::getText(this,tr("Editar"),tr("Texto:"),QLineEdit::Normal,text,&ok).trimmed();if(!ok||text.isEmpty())return;mutateDocument([&](QJsonObject& pilin){QJsonArray layers=pilin.value(QStringLiteral("layers")).toArray();for(int i=0;i<layers.size();++i){QJsonObject layer=layers.at(i).toObject();QJsonArray objects=layer.value(QStringLiteral("objects")).toArray();for(int j=0;j<objects.size();++j){QJsonObject o=objects.at(j).toObject();if(o.value(QStringLiteral("id")).toString()!=id)continue;o.insert(type==QStringLiteral("label")?QStringLiteral("text"):QStringLiteral("label"),text);objects.replace(j,o);layer.insert(QStringLiteral("objects"),objects);layers.replace(i,layer);pilin.insert(QStringLiteral("layers"),layers);return;}}});}

void PilinReyEditor::duplicateSelectedObject(){const QJsonObject selected=selectedObject();if(selected.isEmpty())return;const QString active=activeLayerId();mutateDocument([&](QJsonObject& pilin){QJsonArray layers=pilin.value(QStringLiteral("layers")).toArray();for(int i=0;i<layers.size();++i){QJsonObject layer=layers.at(i).toObject();if(layer.value(QStringLiteral("id")).toString()!=active||layer.value(QStringLiteral("locked")).toBool(false))continue;QJsonObject copy=selected;const QString id=uid(QStringLiteral("mapobj"));copy.insert(QStringLiteral("id"),id);if(copy.contains(QStringLiteral("x"))){copy.insert(QStringLiteral("x"),copy.value(QStringLiteral("x")).toDouble()+40.0);copy.insert(QStringLiteral("y"),copy.value(QStringLiteral("y")).toDouble()+40.0);}else{QJsonArray pts=copy.value(QStringLiteral("points")).toArray();for(int p=0;p<pts.size();++p){QPointF q=jsonPoint(pts.at(p));pts.replace(p,pointJson(q.x()+40.0,q.y()+40.0));}copy.insert(QStringLiteral("points"),pts);}QJsonArray objects=layer.value(QStringLiteral("objects")).toArray();objects.append(copy);layer.insert(QStringLiteral("objects"),objects);layers.replace(i,layer);pilin.insert(QStringLiteral("layers"),layers);selectedObjectId_=id;return;}});}

void PilinReyEditor::copySelectedObject(){copiedObject_=selectedObject();}
void PilinReyEditor::pasteCopiedObject(){if(copiedObject_.isEmpty())return;const QString old=selectedObjectId_;selectedObjectId_=copiedObject_.value(QStringLiteral("id")).toString();QJsonObject original=copiedObject_;copiedObject_=original;selectedObjectId_=old;const QString active=activeLayerId();mutateDocument([&](QJsonObject& pilin){QJsonArray layers=pilin.value(QStringLiteral("layers")).toArray();for(int i=0;i<layers.size();++i){QJsonObject layer=layers.at(i).toObject();if(layer.value(QStringLiteral("id")).toString()!=active||layer.value(QStringLiteral("locked")).toBool(false))continue;QJsonObject copy=original;const QString id=uid(QStringLiteral("mapobj"));copy.insert(QStringLiteral("id"),id);if(copy.contains(QStringLiteral("x"))){copy.insert(QStringLiteral("x"),copy.value(QStringLiteral("x")).toDouble()+50.0);copy.insert(QStringLiteral("y"),copy.value(QStringLiteral("y")).toDouble()+50.0);}else{QJsonArray pts=copy.value(QStringLiteral("points")).toArray();for(int p=0;p<pts.size();++p){QPointF q=jsonPoint(pts.at(p));pts.replace(p,pointJson(q.x()+50.0,q.y()+50.0));}copy.insert(QStringLiteral("points"),pts);}QJsonArray objects=layer.value(QStringLiteral("objects")).toArray();objects.append(copy);layer.insert(QStringLiteral("objects"),objects);layers.replace(i,layer);pilin.insert(QStringLiteral("layers"),layers);selectedObjectId_=id;copiedObject_=copy;return;}});}

void PilinReyEditor::deleteSelectedObject(){if(selectedObjectId_.isEmpty())return;const QString id=selectedObjectId_;mutateDocument([&](QJsonObject& pilin){QJsonArray layers=pilin.value(QStringLiteral("layers")).toArray();for(int i=0;i<layers.size();++i){QJsonObject layer=layers.at(i).toObject();if(layer.value(QStringLiteral("locked")).toBool(false))continue;QJsonArray objects=layer.value(QStringLiteral("objects")).toArray();for(int j=objects.size()-1;j>=0;--j){if(objects.at(j).toObject().value(QStringLiteral("id")).toString()!=id)continue;objects.removeAt(j);layer.insert(QStringLiteral("objects"),objects);layers.replace(i,layer);pilin.insert(QStringLiteral("layers"),layers);return;}}});selectedObjectId_.clear();refreshSelectionControls();refreshViewport();}

void PilinReyEditor::linkSelectedToAtlas(){const QJsonObject selected=selectedObject();if(selected.value(QStringLiteral("type")).toString()!=QStringLiteral("settlement"))return;QWidget* cursor=parentWidget();WorldPage* worldPage=nullptr;while(cursor){worldPage=qobject_cast<WorldPage*>(cursor);if(worldPage)break;cursor=cursor->parentWidget();}if(!worldPage||!worldPage->document_)return;const QJsonArray entries=worldPage->document_->array(QStringLiteral("world"));if(entries.isEmpty()){QMessageBox::information(this,tr("Atlas vacío"),tr("Crea primero una entrada en Mundo → Atlas."));return;}QStringList options;QStringList ids;options<<tr("— Sin enlace —");ids<<QString();for(const QJsonValue v:entries){QJsonObject e=v.toObject();QString name=e.value(QStringLiteral("name")).toString(tr("Sin nombre"));QString kind=e.value(QStringLiteral("kind")).toString();options<<(kind.isEmpty()?name:QStringLiteral("%1 · %2").arg(kind,name));ids<<e.value(QStringLiteral("id")).toString();}bool ok=false;QString choice=QInputDialog::getItem(this,tr("Enlazar con Atlas"),tr("Entrada del Atlas:"),options,0,false,&ok);if(!ok)return;int index=options.indexOf(choice);if(index<0)return;QString atlasId=ids.at(index),selectedId=selectedObjectId_;mutateDocument([&](QJsonObject& pilin){QJsonArray layers=pilin.value(QStringLiteral("layers")).toArray();for(int i=0;i<layers.size();++i){QJsonObject layer=layers.at(i).toObject();QJsonArray objects=layer.value(QStringLiteral("objects")).toArray();for(int j=0;j<objects.size();++j){QJsonObject o=objects.at(j).toObject();if(o.value(QStringLiteral("id")).toString()!=selectedId)continue;if(atlasId.isEmpty()){o.remove(QStringLiteral("atlasId"));o.remove(QStringLiteral("atlasName"));}else{o.insert(QStringLiteral("atlasId"),atlasId);o.insert(QStringLiteral("atlasName"),entries.at(index-1).toObject().value(QStringLiteral("name")).toString());}objects.replace(j,o);layer.insert(QStringLiteral("objects"),objects);layers.replace(i,layer);pilin.insert(QStringLiteral("layers"),layers);return;}}});}

void PilinReyEditor::pushUndo(){undoStack_.append(map_);while(undoStack_.size()>60)undoStack_.removeFirst();redoStack_.clear();}
void PilinReyEditor::mutateDocument(const std::function<void(QJsonObject&)>& mutation){if(refreshing_)return;pushUndo();QJsonObject pilin=map_.value(QStringLiteral("pilinRey")).toObject();mutation(pilin);map_.insert(QStringLiteral("pilinRey"),pilin);refreshViewport();refreshLayers();refreshSelectionControls();persistToArchive();emit mapEdited(map_);}
void PilinReyEditor::undo(){if(undoStack_.isEmpty())return;redoStack_.append(map_);map_=undoStack_.takeLast();selectedObjectId_.clear();refreshLayers();refreshSelectionControls();refreshViewport();persistToArchive();emit mapEdited(map_);}
void PilinReyEditor::redo(){if(redoStack_.isEmpty())return;undoStack_.append(map_);map_=redoStack_.takeLast();selectedObjectId_.clear();refreshLayers();refreshSelectionControls();refreshViewport();persistToArchive();emit mapEdited(map_);}

void PilinReyEditor::exportMap(){const QJsonObject pilin=map_.value(QStringLiteral("pilinRey")).toObject();const QSize logicalSize(qMax(1,pilin.value(QStringLiteral("width")).toInt(4096)),qMax(1,pilin.value(QStringLiteral("height")).toInt(2304)));MapExportDialog dialog(logicalSize,this);if(dialog.exec()!=QDialog::Accepted)return;const QString format=dialog.format();QString extension=QStringLiteral(".png"),filter=tr("PNG (*.png)");if(format==QStringLiteral("svg")){extension=QStringLiteral(".svg");filter=tr("SVG (*.svg)");}else if(format==QStringLiteral("pdf")){extension=QStringLiteral(".pdf");filter=tr("PDF (*.pdf)");}QString base=map_.value(QStringLiteral("name")).toString().trimmed();if(base.isEmpty())base=tr("mapa");QString path=QFileDialog::getSaveFileName(this,tr("Exportar mapa"),base+extension,filter);if(path.isEmpty())return;if(!path.endsWith(extension,Qt::CaseInsensitive))path+=extension;QString error;bool ok=false;if(format==QStringLiteral("svg"))ok=MapExporter::exportSvg(map_,path,dialog.outputSize(),&error);else if(format==QStringLiteral("pdf"))ok=MapExporter::exportPdf(map_,path,dialog.outputSize(),&error);else ok=MapExporter::exportPng(map_,path,dialog.outputSize(),&error);if(!ok)QMessageBox::critical(this,tr("No se pudo exportar"),error.isEmpty()?tr("La exportación del mapa falló."):error);else status_->setText(tr("Exportado · %1").arg(path));}

void PilinReyEditor::persistToArchive(){QWidget* cursor=parentWidget();WorldPage* world=nullptr;while(cursor){world=qobject_cast<WorldPage*>(cursor);if(world)break;cursor=cursor->parentWidget();}if(!world||!world->document_||!world->mapList_)return;const int row=world->mapList_->currentRow();QJsonArray maps=world->document_->array(QStringLiteral("maps"));if(row<0||row>=maps.size())return;QJsonObject stored=maps.at(row).toObject();stored.insert(QStringLiteral("pilinRey"),map_.value(QStringLiteral("pilinRey")));if(map_.contains(QStringLiteral("backgroundImageDataUrl")))stored.insert(QStringLiteral("backgroundImageDataUrl"),map_.value(QStringLiteral("backgroundImageDataUrl")));else stored.remove(QStringLiteral("backgroundImageDataUrl"));maps.replace(row,stored);world->document_->setArray(QStringLiteral("maps"),maps);emit world->changed();}

} // namespace wbw
