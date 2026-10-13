#include "ui/PilinReyEditor.h"

#include "core/ArchiveDocument.h"
#include "ui/MapExportDialog.h"
#include "ui/MapExporter.h"
#include "ui/MapTerrainEngine.h"
#include "ui/WorldPage.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
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
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace wbw {
namespace {

QString uid(const QString& prefix) {
    return prefix + QLatin1Char('-') + QUuid::createUuid().toString(QUuid::Id128);
}

QPointF jsonPoint(const QJsonValue& value) {
    const QJsonObject p = value.toObject();
    return QPointF(p.value(QStringLiteral("x")).toDouble(), p.value(QStringLiteral("y")).toDouble());
}

QJsonObject pointJson(const QPointF& point, double radius = 0.0) {
    QJsonObject result{{QStringLiteral("x"), point.x()}, {QStringLiteral("y"), point.y()}};
    if (radius > 0.0) result.insert(QStringLiteral("radius"), radius);
    return result;
}

QColor themeColor(const QJsonObject& pilin, const QString& key, const QColor& fallback) {
    const QColor color(pilin.value(QStringLiteral("theme")).toObject().value(key).toString());
    return color.isValid() ? color : fallback;
}

QColor mix(const QColor& a, const QColor& b, double t) {
    t = std::clamp(t, 0.0, 1.0);
    return QColor(qRound(a.red() * (1.0 - t) + b.red() * t),
                  qRound(a.green() * (1.0 - t) + b.green() * t),
                  qRound(a.blue() * (1.0 - t) + b.blue() * t));
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

QToolButton* toolButton(const QString& text, const QString& tip, QWidget* parent, const QString& name) {
    auto* button = new QToolButton(parent);
    button->setObjectName(name);
    button->setText(text);
    button->setToolTip(tip);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

QString typeForTool(PilinReyEditor::Tool tool) {
    switch (tool) {
        case PilinReyEditor::Tool::Coast: return QStringLiteral("terrainRaise");
        case PilinReyEditor::Tool::Eraser: return QStringLiteral("terrainLower");
        case PilinReyEditor::Tool::Mountain: return QStringLiteral("terrainRidge");
        case PilinReyEditor::Tool::Forest: return QStringLiteral("biomePaint");
        case PilinReyEditor::Tool::River: return QStringLiteral("river");
        case PilinReyEditor::Tool::Road: return QStringLiteral("road");
        case PilinReyEditor::Tool::Border: return QStringLiteral("border");
        case PilinReyEditor::Tool::Region: return QStringLiteral("region");
        default: return {};
    }
}

bool brushTool(PilinReyEditor::Tool tool) {
    return tool == PilinReyEditor::Tool::Coast || tool == PilinReyEditor::Tool::Eraser ||
           tool == PilinReyEditor::Tool::Mountain || tool == PilinReyEditor::Tool::Forest;
}

bool nodeTool(PilinReyEditor::Tool tool) {
    return tool == PilinReyEditor::Tool::River || tool == PilinReyEditor::Tool::Road ||
           tool == PilinReyEditor::Tool::Border || tool == PilinReyEditor::Tool::Region;
}

QPainterPath smoothPath(const QJsonArray& points, const std::function<QPointF(const QPointF&)>& transform) {
    QPainterPath path;
    if (points.isEmpty()) return path;
    QPointF previous = transform(jsonPoint(points.first()));
    path.moveTo(previous);
    for (int i = 1; i < points.size(); ++i) {
        const QPointF current = transform(jsonPoint(points.at(i)));
        const QPointF mid = (previous + current) * .5;
        path.quadTo(previous, mid);
        previous = current;
    }
    path.lineTo(previous);
    return path;
}

quint32 hashCell(int x, int y, quint32 seed) {
    quint32 h = seed ^ static_cast<quint32>(x * 374761393u) ^ static_cast<quint32>(y * 668265263u);
    h = (h ^ (h >> 13u)) * 1274126177u;
    return h ^ (h >> 16u);
}

double noise01(int x, int y, quint32 seed) {
    return static_cast<double>(hashCell(x, y, seed) & 0xffffu) / 65535.0;
}

QJsonArray buildBiomes(const MapTerrainEngine& terrain, quint32 seed, double seaLevel) {
    QJsonArray biomes;
    for (int y = 0; y < terrain.height(); ++y) {
        for (int x = 0; x < terrain.width(); ++x) {
            const double h = terrain.at(x, y);
            int biome = 0;
            if (h >= seaLevel) {
                const double latitude = std::abs((static_cast<double>(y) / std::max(1, terrain.height() - 1)) * 2.0 - 1.0);
                const double moisture = noise01(x / 4, y / 4, seed ^ 0x91e10da5u) * .68 + noise01(x / 11, y / 11, seed ^ 0x7f4a7c15u) * .32;
                if (h > .84) biome = 6;
                else if (h > .72) biome = 5;
                else if (latitude > .78) biome = 7;
                else if (moisture > .68) biome = 2;
                else if (moisture > .52) biome = 3;
                else if (moisture < .25 && latitude < .62) biome = 4;
                else biome = 1;
            }
            biomes.append(biome);
        }
    }
    return biomes;
}

QVector<QPointF> chooseSettlements(const MapTerrainEngine& terrain, quint32 seed, double seaLevel, int count) {
    struct Candidate { int x; int y; double score; };
    QVector<Candidate> candidates;
    for (int y = 4; y < terrain.height() - 4; y += 3) {
        for (int x = 4; x < terrain.width() - 4; x += 3) {
            const double h = terrain.at(x, y);
            if (h < seaLevel + .025 || h > .78) continue;
            int water = 0;
            for (int oy = -2; oy <= 2; ++oy)
                for (int ox = -2; ox <= 2; ++ox)
                    if (terrain.at(x + ox, y + oy) < seaLevel) ++water;
            const double coast = std::min(1.0, water / 8.0);
            const double lowland = 1.0 - std::abs(h - .52) * 2.0;
            const double jitter = noise01(x, y, seed ^ 0xa24baed5u) * .45;
            candidates.append({x, y, coast * .75 + lowland * .48 + jitter});
        }
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
    QVector<QPointF> result;
    for (const Candidate& c : candidates) {
        if (result.size() >= count) break;
        bool near = false;
        for (const QPointF& p : result) if (std::hypot(p.x() - c.x, p.y() - c.y) < 18.0) { near = true; break; }
        if (!near) result.append(QPointF(c.x, c.y));
    }
    return result;
}

QJsonArray buildRegions(const MapTerrainEngine& terrain, const QVector<QPointF>& capitals, double seaLevel) {
    QJsonArray result;
    for (int y = 0; y < terrain.height(); ++y) {
        for (int x = 0; x < terrain.width(); ++x) {
            if (terrain.at(x, y) < seaLevel || capitals.isEmpty()) { result.append(-1); continue; }
            int best = 0;
            double bestDistance = std::numeric_limits<double>::max();
            for (int i = 0; i < capitals.size(); ++i) {
                const double dx = x - capitals.at(i).x();
                const double dy = y - capitals.at(i).y();
                const double ridgePenalty = std::max(0.0, static_cast<double>(terrain.at(x, y)) - .68) * 900.0;
                const double score = dx * dx + dy * dy + ridgePenalty;
                if (score < bestDistance) { bestDistance = score; best = i; }
            }
            result.append(best);
        }
    }
    return result;
}

QString generatedName(int index, quint32 seed) {
    static const QStringList starts{QStringLiteral("Ald"),QStringLiteral("Bel"),QStringLiteral("Cor"),QStringLiteral("Dor"),QStringLiteral("Eir"),QStringLiteral("Fal"),QStringLiteral("Gal"),QStringLiteral("Lor"),QStringLiteral("Mor"),QStringLiteral("Nor"),QStringLiteral("Or"),QStringLiteral("Rav"),QStringLiteral("Sol"),QStringLiteral("Tor"),QStringLiteral("Val")};
    static const QStringList ends{QStringLiteral("ara"),QStringLiteral("en"),QStringLiteral("or"),QStringLiteral("ia"),QStringLiteral("eth"),QStringLiteral("una"),QStringLiteral("al"),QStringLiteral("is"),QStringLiteral("on")};
    const quint32 h = hashCell(index, index * 7 + 3, seed);
    return starts.at(static_cast<int>(h % starts.size())) + ends.at(static_cast<int>((h >> 8u) % ends.size()));
}

int layerIndexByName(QJsonArray& layers, const QString& name) {
    for (int i = 0; i < layers.size(); ++i)
        if (layers.at(i).toObject().value(QStringLiteral("name")).toString().compare(name, Qt::CaseInsensitive) == 0) return i;
    layers.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("layer"))}, {QStringLiteral("name"), name},
                              {QStringLiteral("visible"), true}, {QStringLiteral("locked"), false},
                              {QStringLiteral("opacity"), 1.0}, {QStringLiteral("objects"), QJsonArray()}});
    return layers.size() - 1;
}

QJsonObject terrainWithMetadata(const MapTerrainEngine& terrain, const QJsonObject& previous) {
    QJsonObject result = terrain.toJson();
    for (const QString& key : {QStringLiteral("seaLevel"), QStringLiteral("generatedAt"), QStringLiteral("generator"),
                               QStringLiteral("biomes"), QStringLiteral("regions"), QStringLiteral("regionNames")})
        if (previous.contains(key)) result.insert(key, previous.value(key));
    return result;
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
        setMinimumSize(480, 320);
    }

    void setDocument(const QJsonObject& map) {
        map_ = map;
        templateImage_ = QImage();
        const QString data = map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("template")).toObject().value(QStringLiteral("dataUrl")).toString();
        if (!data.isEmpty()) templateImage_.loadFromData(dataUrlBytes(data));
        rebuildTerrainCache();
        if (!viewInitialized_) fitDocument();
        update();
    }

    void setTool(Tool tool) {
        tool_ = tool;
        stroke_ = {};
        nodes_ = {};
        drawing_ = false;
        draggingId_.clear();
        draggingPathId_.clear();
        setCursor(tool == Tool::Pan ? Qt::OpenHandCursor : tool == Tool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
        update();
    }

    void setSelection(const QStringList& ids) { selected_ = ids; update(); }
    void setSnap(bool enabled) { snap_ = enabled; update(); }
    void setGrid(bool enabled) { grid_ = enabled; update(); }
    void setBrushRadius(int value) { brushRadius_ = qBound(20, value, 700); update(); }
    void setDensity(int value) { strength_ = qBound(10, value, 100); update(); }
    void setStrokeWidth(int value) { strokeWidth_ = qBound(1, value, 24); update(); }
    void setSymbolSize(int value) { symbolSize_ = qBound(16, value, 160); update(); }

    void fitDocument() {
        const QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject();
        const double docW = std::max(1.0, p.value(QStringLiteral("width")).toDouble(4096));
        const double docH = std::max(1.0, p.value(QStringLiteral("height")).toDouble(2304));
        if (width() < 100 || height() < 100) return;
        zoom_ = std::clamp(std::min((width() - 104.0) / docW, (height() - 92.0) / docH), .025, 10.0);
        pan_ = QPointF((width() - docW * zoom_) * .5, (height() - docH * zoom_) * .5);
        viewInitialized_ = true;
        autoFit_ = true;
        update();
    }

protected:
    void resizeEvent(QResizeEvent* event) override { QWidget::resizeEvent(event); if (autoFit_) fitDocument(); }

    void wheelEvent(QWheelEvent* event) override {
        autoFit_ = false;
        const QPointF cursor = event->position();
        const QPointF before = screenToWorld(cursor);
        zoom_ = std::clamp(zoom_ * (event->angleDelta().y() > 0 ? 1.12 : 1.0 / 1.12), .02, 14.0);
        pan_ = cursor - QPointF(before.x() * zoom_, before.y() * zoom_);
        update();
        event->accept();
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Delete && onDeleteSelection) { onDeleteSelection(); event->accept(); return; }
        if (event->key() == Qt::Key_Escape) {
            stroke_ = {}; nodes_ = {}; drawing_ = false; draggingId_.clear(); draggingPathId_.clear(); update(); event->accept(); return;
        }
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
            const Hit hit = hitTest(cursorWorld_);
            const bool extend = event->modifiers().testFlag(Qt::ShiftModifier) || event->modifiers().testFlag(Qt::ControlModifier);
            if (!extend) selected_.clear();
            if (!hit.id.isEmpty()) {
                if (extend && selected_.contains(hit.id)) selected_.removeAll(hit.id); else if (!selected_.contains(hit.id)) selected_.append(hit.id);
                const QJsonObject object = objectById(hit.id);
                if (object.contains(QStringLiteral("x"))) {
                    draggingId_ = hit.id;
                    dragOrigin_ = QPointF(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble());
                    dragStart_ = cursorWorld_;
                } else if (hit.pointIndex >= 0) {
                    draggingPathId_ = hit.id;
                    draggingPointIndex_ = hit.pointIndex;
                }
            }
            if (onSelection) onSelection(selected_);
            update();
            return;
        }

        if (brushTool(tool_)) {
            drawing_ = true;
            stroke_ = QJsonArray{pointJson(cursorWorld_, brushRadius_)};
            update();
            return;
        }
        if (nodeTool(tool_)) {
            nodes_.append(pointJson(cursorWorld_));
            if (onStatus) onStatus(QObject::tr("Añade nodos · doble clic o clic derecho para terminar"));
            update();
            return;
        }
        if (tool_ == Tool::Settlement && onSettlement) onSettlement(cursorWorld_.x(), cursorWorld_.y());
        else if (tool_ == Tool::Stamp && onStamp) onStamp(cursorWorld_.x(), cursorWorld_.y());
        else if (tool_ == Tool::Label && onLabel) onLabel(cursorWorld_.x(), cursorWorld_.y());
        else if (tool_ == Tool::Measure) { measureA_ = cursorWorld_; measureB_ = cursorWorld_; measuring_ = true; update(); }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        cursorWorld_ = snapped(screenToWorld(event->position()));
        if (panning_) { pan_ += event->position() - lastMouse_; lastMouse_ = event->position(); update(); return; }
        if (drawing_) {
            if (stroke_.isEmpty() || QLineF(jsonPoint(stroke_.last()), cursorWorld_).length() > std::max(10.0, brushRadius_ * .18))
                stroke_.append(pointJson(cursorWorld_, brushRadius_));
            update();
            return;
        }
        if (!draggingPathId_.isEmpty()) { update(); return; }
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
        if (!draggingPathId_.isEmpty()) {
            if (onMovePathPoint) onMovePathPoint(draggingPathId_, draggingPointIndex_, cursorWorld_.x(), cursorWorld_.y());
            draggingPathId_.clear(); draggingPointIndex_ = -1; return;
        }
        measuring_ = false;
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && nodeTool(tool_)) {
            nodes_.append(pointJson(snapped(screenToWorld(event->position()))));
            finishNodes(); event->accept(); return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }

    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), QColor(QStringLiteral("#090d12")));

        const QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject();
        const double docW = p.value(QStringLiteral("width")).toDouble(4096.0);
        const double docH = p.value(QStringLiteral("height")).toDouble(2304.0);
        const QRectF page(worldToScreen(QPointF(0, 0)), worldToScreen(QPointF(docW, docH)));

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 125));
        painter.drawRoundedRect(page.translated(8, 11), 7, 7);

        painter.save();
        painter.setClipRect(page);
        if (!terrainCache_.isNull()) {
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
            painter.drawImage(page, terrainCache_);
        } else {
            painter.fillRect(page, themeColor(p, QStringLiteral("sea"), QColor(QStringLiteral("#6f99aa"))));
        }
        drawTemplate(painter, page, p);
        if (grid_) drawGrid(painter, page, docW, docH);
        drawRegionBoundaries(painter, page, p);
        for (const QJsonValue& layerValue : p.value(QStringLiteral("layers")).toArray()) {
            const QJsonObject layer = layerValue.toObject();
            if (!layer.value(QStringLiteral("visible")).toBool(true)) continue;
            painter.save();
            painter.setOpacity(std::clamp(layer.value(QStringLiteral("opacity")).toDouble(1.0), 0.0, 1.0));
            for (const QJsonValue& objectValue : layer.value(QStringLiteral("objects")).toArray()) drawObject(painter, objectValue.toObject(), p);
            painter.restore();
        }
        drawPreview(painter, p);
        painter.restore();

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(QStringLiteral("#3b4650")), 1.2));
        painter.drawRoundedRect(page, 5, 5);
    }

private:
    struct Hit { QString id; int pointIndex = -1; };

    QPointF worldToScreen(const QPointF& p) const { return QPointF(p.x() * zoom_ + pan_.x(), p.y() * zoom_ + pan_.y()); }
    QPointF screenToWorld(const QPointF& p) const { return QPointF((p.x() - pan_.x()) / zoom_, (p.y() - pan_.y()) / zoom_); }
    QPointF snapped(const QPointF& p) const {
        if (!snap_) return p;
        constexpr double step = 25.0;
        return QPointF(std::round(p.x() / step) * step, std::round(p.y() / step) * step);
    }

    void rebuildTerrainCache() {
        const QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject();
        const QJsonObject model = p.value(QStringLiteral("terrainModel")).toObject();
        const int tw = model.value(QStringLiteral("width")).toInt();
        const int th = model.value(QStringLiteral("height")).toInt();
        const QJsonArray values = model.value(QStringLiteral("values")).toArray();
        const QJsonArray biomes = model.value(QStringLiteral("biomes")).toArray();
        if (tw < 2 || th < 2 || values.size() < tw * th) { terrainCache_ = QImage(); return; }

        const double sea = std::clamp(model.value(QStringLiteral("seaLevel")).toDouble(.42), .05, .85);
        const QColor deepSea = themeColor(p, QStringLiteral("sea"), QColor(QStringLiteral("#628d9e"))).darker(142);
        const QColor midSea = themeColor(p, QStringLiteral("sea"), QColor(QStringLiteral("#789fad")));
        const QColor shallowSea(QStringLiteral("#a9c7c4"));
        const QColor beach(QStringLiteral("#cbb98c"));
        const QColor meadow(QStringLiteral("#8d9e70"));
        const QColor forest(QStringLiteral("#526e4e"));
        const QColor woodland(QStringLiteral("#71845d"));
        const QColor dry(QStringLiteral("#b89869"));
        const QColor upland(QStringLiteral("#927b5e"));
        const QColor rock(QStringLiteral("#756d65"));
        const QColor snow(QStringLiteral("#dddcd5"));
        const QColor tundra(QStringLiteral("#a7aa91"));

        auto hAt = [&](int x, int y) {
            x = std::clamp(x, 0, tw - 1); y = std::clamp(y, 0, th - 1);
            return values.at(y * tw + x).toInt() / 255.0;
        };
        auto biomeAt = [&](int x, int y) {
            if (biomes.size() < tw * th) return 1;
            x = std::clamp(x, 0, tw - 1); y = std::clamp(y, 0, th - 1);
            return biomes.at(y * tw + x).toInt(1);
        };

        terrainCache_ = QImage(tw, th, QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < th; ++y) {
            QRgb* row = reinterpret_cast<QRgb*>(terrainCache_.scanLine(y));
            for (int x = 0; x < tw; ++x) {
                const double h = hAt(x, y);
                QColor color;
                if (h < sea) {
                    const double depth = std::clamp(h / sea, 0.0, 1.0);
                    color = depth < .66 ? mix(deepSea, midSea, depth / .66) : mix(midSea, shallowSea, (depth - .66) / .34);
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
                }
                const double dx = hAt(x + 1, y) - hAt(x - 1, y);
                const double dy = hAt(x, y + 1) - hAt(x, y - 1);
                const double shade = std::clamp(.97 - dx * 1.35 - dy * .9, .64, 1.22);
                color.setRed(std::clamp(qRound(color.red() * shade), 0, 255));
                color.setGreen(std::clamp(qRound(color.green() * shade), 0, 255));
                color.setBlue(std::clamp(qRound(color.blue() * shade), 0, 255));
                row[x] = color.rgba();
            }
        }
    }

    void drawRegionBoundaries(QPainter& painter, const QRectF& page, const QJsonObject& p) const {
        const QJsonObject model = p.value(QStringLiteral("terrainModel")).toObject();
        const int tw = model.value(QStringLiteral("width")).toInt();
        const int th = model.value(QStringLiteral("height")).toInt();
        const QJsonArray regions = model.value(QStringLiteral("regions")).toArray();
        if (tw < 2 || th < 2 || regions.size() < tw * th) return;
        const double sx = page.width() / tw, sy = page.height() / th;
        auto rAt = [&](int x, int y) { x = std::clamp(x, 0, tw - 1); y = std::clamp(y, 0, th - 1); return regions.at(y * tw + x).toInt(-1); };
        painter.save();
        painter.setPen(QPen(QColor(73, 63, 72, 112), std::max(.7, sx * .28), Qt::DashLine));
        for (int y = 1; y < th - 1; ++y) for (int x = 1; x < tw - 1; ++x) {
            const int region = rAt(x, y); if (region < 0) continue;
            if (rAt(x + 1, y) >= 0 && rAt(x + 1, y) != region)
                painter.drawLine(QPointF(page.left() + (x + 1) * sx, page.top() + y * sy), QPointF(page.left() + (x + 1) * sx, page.top() + (y + 1) * sy));
            if (rAt(x, y + 1) >= 0 && rAt(x, y + 1) != region)
                painter.drawLine(QPointF(page.left() + x * sx, page.top() + (y + 1) * sy), QPointF(page.left() + (x + 1) * sx, page.top() + (y + 1) * sy));
        }
        painter.restore();
    }

    void drawTemplate(QPainter& painter, const QRectF& page, const QJsonObject& p) const {
        if (templateImage_.isNull()) return;
        const QJsonObject t = p.value(QStringLiteral("template")).toObject();
        if (!t.value(QStringLiteral("visible")).toBool(true)) return;
        const double scale = std::clamp(t.value(QStringLiteral("scale")).toDouble(1.0), .05, 5.0);
        const QSizeF size(page.width() * scale, page.height() * scale);
        const QPointF center = page.center() + QPointF(t.value(QStringLiteral("x")).toDouble() * zoom_, t.value(QStringLiteral("y")).toDouble() * zoom_);
        const QRectF target(center.x() - size.width() / 2.0, center.y() - size.height() / 2.0, size.width(), size.height());
        painter.save(); painter.setOpacity(std::clamp(t.value(QStringLiteral("opacity")).toDouble(.25), 0.0, 1.0)); painter.translate(target.center()); painter.rotate(t.value(QStringLiteral("rotation")).toDouble()); painter.translate(-target.center()); painter.drawImage(target, templateImage_); painter.restore();
    }

    void drawGrid(QPainter& painter, const QRectF& page, double docW, double docH) const {
        painter.save(); painter.setPen(QPen(QColor(33, 42, 48, 50), 1));
        for (double x = 0; x <= docW; x += 200) painter.drawLine(worldToScreen(QPointF(x, 0)), worldToScreen(QPointF(x, docH)));
        for (double y = 0; y <= docH; y += 200) painter.drawLine(worldToScreen(QPointF(0, y)), worldToScreen(QPointF(docW, y)));
        painter.restore();
    }

    void drawObject(QPainter& painter, const QJsonObject& object, const QJsonObject& p) const {
        const QString type = object.value(QStringLiteral("type")).toString();
        const bool selected = selected_.contains(object.value(QStringLiteral("id")).toString());
        if (type == QStringLiteral("settlement")) {
            const QPointF pt = worldToScreen(QPointF(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble()));
            const double s = std::clamp(6.0 + zoom_ * 24.0, 6.0, 14.0);
            const QColor ink = themeColor(p, QStringLiteral("symbol"), QColor(QStringLiteral("#2c2924")));
            painter.setPen(QPen(QColor(238, 228, 205, 210), 3.5)); painter.setBrush(ink); painter.drawEllipse(pt, s * .46, s * .46);
            painter.setPen(ink); QFont font(QStringLiteral("Georgia")); font.setPointSizeF(std::clamp(8.0 + zoom_ * 5.0, 8.0, 12.0)); font.setBold(true); painter.setFont(font);
            painter.drawText(QPointF(pt.x() + s * .8, pt.y() + 4), object.value(QStringLiteral("label")).toString());
            if (selected) { painter.setBrush(Qt::NoBrush); painter.setPen(QPen(QColor(QStringLiteral("#74b8ee")), 1.5, Qt::DashLine)); painter.drawEllipse(pt, s * 1.35, s * 1.35); }
            return;
        }
        if (type == QStringLiteral("stamp")) {
            const QString data = object.value(QStringLiteral("dataUrl")).toString();
            QImage image; if (!data.isEmpty()) image.loadFromData(dataUrlBytes(data));
            if (image.isNull()) return;
            const QPointF pt = worldToScreen(QPointF(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble()));
            const double size = std::clamp(object.value(QStringLiteral("size")).toDouble(120.0) * object.value(QStringLiteral("scale")).toDouble(1.0) * zoom_, 12.0, 140.0);
            painter.save(); painter.translate(pt); painter.rotate(object.value(QStringLiteral("rotation")).toDouble()); painter.setOpacity(std::clamp(object.value(QStringLiteral("opacity")).toDouble(1.0), 0.0, 1.0)); painter.drawImage(QRectF(-size / 2, -size / 2, size, size), image); painter.restore();
            return;
        }
        if (type == QStringLiteral("label")) {
            const QPointF pt = worldToScreen(QPointF(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble()));
            painter.save(); painter.translate(pt); painter.rotate(object.value(QStringLiteral("rotation")).toDouble());
            QFont font(QStringLiteral("Georgia")); font.setPointSizeF(std::clamp(object.value(QStringLiteral("fontSize")).toDouble(54) * zoom_ * .55, 8.0, 34.0)); font.setBold(object.value(QStringLiteral("bold")).toBool(false)); font.setLetterSpacing(QFont::PercentageSpacing, 104); painter.setFont(font);
            QPainterPath glyphs; glyphs.addText(QPointF(0, 0), font, object.value(QStringLiteral("text")).toString());
            painter.setBrush(themeColor(p, QStringLiteral("text"), QColor(QStringLiteral("#29251f")))); painter.setPen(QPen(QColor(239, 229, 205, 180), 2.5)); painter.drawPath(glyphs); painter.setPen(QPen(themeColor(p, QStringLiteral("text"), QColor(QStringLiteral("#29251f"))), .7)); painter.drawPath(glyphs); painter.restore(); return;
        }
        const QJsonArray points = object.value(QStringLiteral("points")).toArray(); if (points.size() < 2) return;
        const QPainterPath path = smoothPath(points, [this](const QPointF& value) { return worldToScreen(value); });
        const double width = std::max(1.0, object.value(QStringLiteral("width")).toDouble(strokeWidth_) * std::max(.55, zoom_));
        painter.save(); painter.setBrush(Qt::NoBrush);
        if (type == QStringLiteral("river")) {
            const QColor c = themeColor(p, QStringLiteral("river"), QColor(QStringLiteral("#2d6382")));
            painter.setPen(QPen(c.darker(145), width + 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); painter.drawPath(path);
            painter.setPen(QPen(c.lighter(150), std::max(1.0, width * .45), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); painter.drawPath(path);
        } else if (type == QStringLiteral("road")) {
            const QColor c = themeColor(p, QStringLiteral("road"), QColor(QStringLiteral("#75512f")));
            painter.setPen(QPen(c.darker(140), width + 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); painter.drawPath(path);
            painter.setPen(QPen(c.lighter(160), std::max(1.0, width * .36), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); painter.drawPath(path);
        } else if (type == QStringLiteral("border")) {
            painter.setPen(QPen(themeColor(p, QStringLiteral("border"), QColor(QStringLiteral("#7c4650"))), width, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin)); painter.drawPath(path);
        } else if (type == QStringLiteral("region")) {
            QColor c = themeColor(p, QStringLiteral("region"), QColor(QStringLiteral("#8e6547"))); QColor fill = c; fill.setAlpha(32);
            painter.setPen(QPen(c, width, Qt::DashLine)); painter.setBrush(fill); painter.drawPath(path);
        }
        painter.restore();
        if (selected) {
            painter.setPen(QPen(QColor(QStringLiteral("#74b8ee")), 1.4)); painter.setBrush(QColor(QStringLiteral("#74b8ee")));
            for (const QJsonValue& value : points) { const QPointF pt = worldToScreen(jsonPoint(value)); painter.drawRect(QRectF(pt.x() - 3, pt.y() - 3, 6, 6)); }
        }
    }

    void drawPreview(QPainter& painter, const QJsonObject& p) const {
        if (!nodes_.isEmpty()) {
            QJsonArray preview = nodes_; preview.append(pointJson(cursorWorld_));
            drawObject(painter, QJsonObject{{QStringLiteral("type"), typeForTool(tool_)}, {QStringLiteral("points"), preview}, {QStringLiteral("width"), strokeWidth_}}, p);
        }
        if (brushTool(tool_)) {
            const QPointF center = worldToScreen(cursorWorld_);
            painter.setBrush(QColor(116, 184, 238, tool_ == Tool::Forest ? 35 : 20));
            painter.setPen(QPen(QColor(116, 184, 238, 205), 1.2));
            painter.drawEllipse(center, brushRadius_ * zoom_, brushRadius_ * zoom_);
        }
        if (measuring_) {
            painter.setPen(QPen(QColor(QStringLiteral("#d7b06f")), 1.5, Qt::DashLine));
            painter.drawLine(worldToScreen(measureA_), worldToScreen(measureB_));
        }
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

    Hit hitTest(const QPointF& point) const {
        const double tolerance = 18.0 / std::max(.03, zoom_);
        const QJsonArray layers = map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray();
        for (int li = layers.size() - 1; li >= 0; --li) {
            const QJsonObject layer = layers.at(li).toObject();
            if (!layer.value(QStringLiteral("visible")).toBool(true) || layer.value(QStringLiteral("locked")).toBool(false)) continue;
            const QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
            for (int oi = objects.size() - 1; oi >= 0; --oi) {
                const QJsonObject object = objects.at(oi).toObject();
                const QString id = object.value(QStringLiteral("id")).toString();
                if (object.contains(QStringLiteral("x"))) {
                    const QPointF center(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble());
                    if (QLineF(center, point).length() <= std::max(tolerance, object.value(QStringLiteral("size")).toDouble(80) * .5)) return {id, -1};
                } else {
                    const QJsonArray points = object.value(QStringLiteral("points")).toArray();
                    for (int pi = 0; pi < points.size(); ++pi) if (QLineF(jsonPoint(points.at(pi)), point).length() <= tolerance) return {id, pi};
                }
            }
        }
        return {};
    }

    QJsonObject map_;
    QImage templateImage_;
    QImage terrainCache_;
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
    int strength_ = 58;
    int strokeWidth_ = 5;
    int symbolSize_ = 64;
    double zoom_ = .2;
    QPointF pan_{40, 40};
    QPointF lastMouse_;
    QPointF cursorWorld_;
    QPointF measureA_;
    QPointF measureB_;
    QString draggingId_;
    QString draggingPathId_;
    int draggingPointIndex_ = -1;
    QPointF dragStart_;
    QPointF dragOrigin_;
};

PilinReyEditor::PilinReyEditor(QWidget* parent) : QWidget(parent) { buildUi(); }
PilinReyEditor::~PilinReyEditor() = default;

void PilinReyEditor::buildUi() {
    setObjectName(QStringLiteral("pilinReyEditor"));
    setProperty("selectedBiome", 2);
    setProperty("terrainStrength", 58);

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

    // Compact tool rail: it never steals a permanent document column from the map.
    toolRail_ = new QFrame(canvasHost_);
    toolRail_->setObjectName(QStringLiteral("pilinToolPalette"));
    auto* tools = new QVBoxLayout(toolRail_);
    tools->setContentsMargins(7, 7, 7, 7);
    tools->setSpacing(3);

    struct Def { Tool tool; const char* glyph; const char* tip; };
    const Def defs[] = {
        {Tool::Select, "↖", "Seleccionar y editar"}, {Tool::Pan, "✥", "Mover lienzo"},
        {Tool::Coast, "+", "Elevar terreno"}, {Tool::Eraser, "−", "Hundir terreno"},
        {Tool::Mountain, "⌃", "Modelar cordillera"}, {Tool::Forest, "◐", "Pintar bioma"},
        {Tool::River, "∿", "Trazar río"}, {Tool::Road, "━", "Trazar ruta"},
        {Tool::Border, "┄", "Trazar frontera"}, {Tool::Region, "◇", "Dibujar región"},
        {Tool::Settlement, "●", "Añadir asentamiento"}, {Tool::Label, "T", "Añadir etiqueta"},
        {Tool::Measure, "↔", "Medir"}
    };
    for (const Def& def : defs) {
        auto* button = toolButton(QString::fromUtf8(def.glyph), tr(def.tip), toolRail_, QStringLiteral("pilinMapTool"));
        button->setCheckable(true);
        button->setAutoExclusive(true);
        button->setFixedSize(38, 38);
        if (def.tool == Tool::Select) button->setChecked(true);
        connect(button, &QToolButton::clicked, this, [this, tool = def.tool]() { setActiveTool(tool); });
        toolButtons_.append(button);
        tools->addWidget(button);
    }
    tools->addStretch(1);

    topCommands_ = new QFrame(canvasHost_);
    topCommands_->setObjectName(QStringLiteral("pilinTopCommands"));
    auto* commands = new QHBoxLayout(topCommands_);
    commands->setContentsMargins(8, 5, 8, 5);
    commands->setSpacing(3);

    undoButton_ = toolButton(QStringLiteral("↶"), tr("Deshacer"), topCommands_, QStringLiteral("pilinCommand"));
    redoButton_ = toolButton(QStringLiteral("↷"), tr("Rehacer"), topCommands_, QStringLiteral("pilinCommand"));
    auto* generate = toolButton(tr("Generar"), tr("Generación de mundo"), topCommands_, QStringLiteral("pilinCommand"));
    auto* terrain = toolButton(tr("Terreno"), tr("Operaciones de relieve"), topCommands_, QStringLiteral("pilinCommand"));
    auto* biomes = toolButton(tr("Biomas"), tr("Paleta de biomas"), topCommands_, QStringLiteral("pilinCommand"));
    layersButton_ = toolButton(tr("Capas"), tr("Capas del mapa"), topCommands_, QStringLiteral("pilinCommand"));
    assetsButton_ = toolButton(tr("Símbolos"), tr("Símbolos importados"), topCommands_, QStringLiteral("pilinCommand"));
    templateButton_ = toolButton(tr("Referencia"), tr("Imagen de referencia"), topCommands_, QStringLiteral("pilinCommand"));
    appearanceButton_ = toolButton(tr("Estilo"), tr("Paleta cartográfica"), topCommands_, QStringLiteral("pilinCommand"));
    auto* fit = toolButton(tr("Encajar"), tr("Encajar mapa"), topCommands_, QStringLiteral("pilinCommand"));
    auto* exportButton = toolButton(tr("Exportar"), tr("Exportar mapa"), topCommands_, QStringLiteral("pilinPrimaryCommand"));
    snapCheck_ = new QCheckBox(tr("Ajuste"), topCommands_);
    gridCheck_ = new QCheckBox(tr("Retícula"), topCommands_);

    commands->addWidget(undoButton_); commands->addWidget(redoButton_); commands->addSpacing(5);
    commands->addWidget(generate); commands->addWidget(terrain); commands->addWidget(biomes); commands->addWidget(layersButton_);
    commands->addWidget(assetsButton_); commands->addWidget(templateButton_); commands->addWidget(appearanceButton_); commands->addWidget(fit);
    commands->addStretch(1); commands->addWidget(snapCheck_); commands->addWidget(gridCheck_); commands->addWidget(exportButton);

    auto makePopover = [this](const QString& titleText, const QString& objectName = QStringLiteral("pilinPopover")) {
        auto* frame = new QFrame(canvasHost_);
        frame->setObjectName(objectName);
        auto* layout = new QVBoxLayout(frame);
        layout->setContentsMargins(14, 14, 14, 14);
        layout->setSpacing(8);
        auto* title = new QLabel(titleText, frame);
        title->setObjectName(QStringLiteral("pilinPopoverTitle"));
        layout->addWidget(title);
        return frame;
    };

    auto* generationPopover = makePopover(tr("Generación de mundo"), QStringLiteral("pilinGeneratorPopover"));
    {
        auto* layout = qobject_cast<QVBoxLayout*>(generationPopover->layout());
        auto* copy = new QLabel(tr("Crea un relieve editable con costas, biomas, ríos, regiones y asentamientos. Todo queda convertido en datos del mapa, no en una imagen plana."), generationPopover);
        copy->setObjectName(QStringLiteral("pilinPopoverCopy")); copy->setWordWrap(true); layout->addWidget(copy);
        auto* form = new QFormLayout; form->setHorizontalSpacing(12); form->setVerticalSpacing(7);
        auto* seed = new QSpinBox(generationPopover); seed->setRange(1, 2147483647); seed->setValue(42137);
        auto* continents = new QSpinBox(generationPopover); continents->setRange(1, 8); continents->setValue(3);
        auto* islands = new QSpinBox(generationPopover); islands->setRange(0, 40); islands->setValue(12);
        auto* roughness = new QSlider(Qt::Horizontal, generationPopover); roughness->setRange(30, 82); roughness->setValue(58);
        auto* sea = new QSlider(Qt::Horizontal, generationPopover); sea->setRange(28, 58); sea->setValue(42);
        form->addRow(tr("Semilla"), seed); form->addRow(tr("Continentes"), continents); form->addRow(tr("Islas"), islands); form->addRow(tr("Relieve"), roughness); form->addRow(tr("Nivel del mar"), sea);
        layout->addLayout(form);
        auto* buttons = new QHBoxLayout;
        auto* blank = new QPushButton(tr("Mapa vacío"), generationPopover);
        auto* build = new QPushButton(tr("Generar mundo"), generationPopover); build->setObjectName(QStringLiteral("pilinPrimaryAction"));
        buttons->addWidget(blank); buttons->addStretch(); buttons->addWidget(build); layout->addLayout(buttons);
        connect(blank, &QPushButton::clicked, this, [this, generationPopover]() {
            if (QMessageBox::question(this, tr("Mapa vacío"), tr("Esto sustituirá el relieve actual. Las capas vectoriales se conservarán. ¿Continuar?")) != QMessageBox::Yes) return;
            mutateDocument([&](QJsonObject& p) {
                MapTerrainEngine model(320, 180, 1); model.clear(.26f);
                QJsonObject terrainJson = model.toJson(); terrainJson.insert(QStringLiteral("seaLevel"), .42); terrainJson.insert(QStringLiteral("generator"), QStringLiteral("blank-heightmap"));
                terrainJson.insert(QStringLiteral("biomes"), QJsonArray()); terrainJson.insert(QStringLiteral("regions"), QJsonArray()); p.insert(QStringLiteral("terrainModel"), terrainJson);
            });
            generationPopover->hide();
        });
        connect(build, &QPushButton::clicked, this, [this, seed, continents, islands, roughness, sea, generationPopover]() {
            const quint32 seedValue = static_cast<quint32>(seed->value());
            const double seaLevel = sea->value() / 100.0;
            MapTerrainEngine model(320, 180, seedValue);
            model.generateWorld(continents->value(), islands->value(), roughness->value() / 100.0f);
            model.erode(2, .18f);
            const QVector<QPointF> settlements = chooseSettlements(model, seedValue, seaLevel, qBound(6, continents->value() * 5 + islands->value() / 4, 24));
            QVector<QPointF> capitals;
            for (int i = 0; i < qMin(continents->value(), settlements.size()); ++i) capitals.append(settlements.at(i));
            mutateDocument([&](QJsonObject& p) {
                QJsonObject terrainJson = model.toJson();
                terrainJson.insert(QStringLiteral("seaLevel"), seaLevel);
                terrainJson.insert(QStringLiteral("generator"), QStringLiteral("native-heightmap-product"));
                terrainJson.insert(QStringLiteral("biomes"), buildBiomes(model, seedValue, seaLevel));
                terrainJson.insert(QStringLiteral("regions"), buildRegions(model, capitals, seaLevel));
                QJsonArray regionNames; for (int i = 0; i < capitals.size(); ++i) regionNames.append(tr("Región de %1").arg(generatedName(i, seedValue))); terrainJson.insert(QStringLiteral("regionNames"), regionNames);
                p.insert(QStringLiteral("terrainModel"), terrainJson);

                QJsonArray layers = p.value(QStringLiteral("layers")).toArray();
                const int hydroIndex = layerIndexByName(layers, tr("Hidrografía"));
                const int settlementsIndex = layerIndexByName(layers, tr("Asentamientos"));
                QJsonObject hydro = layers.at(hydroIndex).toObject();
                QJsonArray hydroObjects;
                for (const QJsonValue& riverValue : model.traceRivers(qBound(8, continents->value() * 8 + islands->value() / 3, 34), seaLevel)) {
                    QJsonArray points;
                    for (const QJsonValue& pointValue : riverValue.toArray()) {
                        const QJsonObject point = pointValue.toObject();
                        points.append(QJsonObject{{QStringLiteral("x"), point.value(QStringLiteral("x")).toDouble() * p.value(QStringLiteral("width")).toDouble(4096.0)},
                                                  {QStringLiteral("y"), point.value(QStringLiteral("y")).toDouble() * p.value(QStringLiteral("height")).toDouble(2304.0)}});
                    }
                    if (points.size() >= 3) hydroObjects.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("river"))}, {QStringLiteral("type"), QStringLiteral("river")}, {QStringLiteral("points"), points}, {QStringLiteral("width"), 3.0}, {QStringLiteral("generated"), true}});
                }
                hydro.insert(QStringLiteral("objects"), hydroObjects); layers.replace(hydroIndex, hydro);

                QJsonObject settlementLayer = layers.at(settlementsIndex).toObject();
                QJsonArray settlementObjects;
                const double docW = p.value(QStringLiteral("width")).toDouble(4096.0), docH = p.value(QStringLiteral("height")).toDouble(2304.0);
                for (int i = 0; i < settlements.size(); ++i) {
                    const QPointF s = settlements.at(i);
                    settlementObjects.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("settlement"))}, {QStringLiteral("type"), QStringLiteral("settlement")},
                        {QStringLiteral("label"), generatedName(i, seedValue)}, {QStringLiteral("x"), s.x() / std::max(1, model.width() - 1) * docW},
                        {QStringLiteral("y"), s.y() / std::max(1, model.height() - 1) * docH}, {QStringLiteral("generated"), true},
                        {QStringLiteral("settlementType"), i < capitals.size() ? tr("Capital") : tr("Pueblo")}});
                }
                settlementLayer.insert(QStringLiteral("objects"), settlementObjects); layers.replace(settlementsIndex, settlementLayer);
                p.insert(QStringLiteral("layers"), layers);
            });
            generationPopover->hide();
        });
    }

    auto* terrainPopover = makePopover(tr("Relieve"), QStringLiteral("pilinTerrainPopover"));
    {
        auto* layout = qobject_cast<QVBoxLayout*>(terrainPopover->layout());
        auto* copy = new QLabel(tr("Modela el heightmap: las costas, el sombreado y el relieve se recalculan desde la elevación."), terrainPopover); copy->setObjectName(QStringLiteral("pilinPopoverCopy")); copy->setWordWrap(true); layout->addWidget(copy);
        auto* smooth = new QPushButton(tr("Suavizar relieve"), terrainPopover);
        auto* erode = new QPushButton(tr("Erosión"), terrainPopover);
        auto* normalize = new QPushButton(tr("Normalizar alturas"), terrainPopover);
        layout->addWidget(smooth); layout->addWidget(erode); layout->addWidget(normalize);
        connect(smooth, &QPushButton::clicked, this, &PilinReyEditor::smoothTerrain);
        connect(erode, &QPushButton::clicked, this, &PilinReyEditor::erodeTerrain);
        connect(normalize, &QPushButton::clicked, this, &PilinReyEditor::normalizeTerrain);
    }

    auto* biomePopover = makePopover(tr("Biomas"), QStringLiteral("pilinBiomePopover"));
    {
        auto* layout = qobject_cast<QVBoxLayout*>(biomePopover->layout());
        auto* copy = new QLabel(tr("Pinta cobertura continua sobre el terreno. No se colocan árboles o montañas repetidos como sellos."), biomePopover); copy->setObjectName(QStringLiteral("pilinPopoverCopy")); copy->setWordWrap(true); layout->addWidget(copy);
        const QList<QPair<int, QString>> choices{{1, tr("Pradera")}, {2, tr("Bosque")}, {3, tr("Arboleda")}, {4, tr("Seco / desierto")}, {5, tr("Alpino")}, {7, tr("Tundra")}};
        for (const auto& choice : choices) {
            auto* button = new QPushButton(choice.second, biomePopover);
            button->setCheckable(true);
            if (choice.first == 2) button->setChecked(true);
            connect(button, &QPushButton::clicked, this, [this, biomePopover, value = choice.first, choices](bool) {
                setProperty("selectedBiome", value);
                for (QPushButton* sibling : biomePopover->findChildren<QPushButton*>()) sibling->setChecked(false);
                if (auto* senderButton = qobject_cast<QPushButton*>(sender())) senderButton->setChecked(true);
                setActiveTool(Tool::Forest);
                for (QToolButton* tool : toolButtons_) if (tool->toolTip() == tr("Pintar bioma")) tool->setChecked(true);
            });
            layout->addWidget(button);
        }
    }

    layersPopover_ = makePopover(tr("Capas"));
    {
        auto* layout = qobject_cast<QVBoxLayout*>(layersPopover_->layout());
        layers_ = new QListWidget(layersPopover_); layers_->setMinimumHeight(210); layout->addWidget(layers_);
        auto* row = new QHBoxLayout;
        auto* add = toolButton(QStringLiteral("+"), tr("Nueva capa"), layersPopover_, QStringLiteral("pilinCommand"));
        auto* dup = toolButton(QStringLiteral("⧉"), tr("Duplicar capa"), layersPopover_, QStringLiteral("pilinCommand"));
        auto* del = toolButton(QStringLiteral("−"), tr("Eliminar capa"), layersPopover_, QStringLiteral("pilinCommand"));
        auto* up = toolButton(QStringLiteral("↑"), tr("Subir"), layersPopover_, QStringLiteral("pilinCommand"));
        auto* down = toolButton(QStringLiteral("↓"), tr("Bajar"), layersPopover_, QStringLiteral("pilinCommand"));
        row->addWidget(add); row->addWidget(dup); row->addWidget(del); row->addStretch(); row->addWidget(up); row->addWidget(down); layout->addLayout(row);
        layerLocked_ = new QCheckBox(tr("Bloquear capa"), layersPopover_);
        layerOpacity_ = new QSlider(Qt::Horizontal, layersPopover_); layerOpacity_->setRange(0, 100); layerOpacity_->setValue(100);
        layout->addWidget(layerLocked_); layout->addWidget(new QLabel(tr("Opacidad"), layersPopover_)); layout->addWidget(layerOpacity_);
        connect(add, &QToolButton::clicked, this, &PilinReyEditor::addLayer); connect(dup, &QToolButton::clicked, this, &PilinReyEditor::duplicateLayer);
        connect(del, &QToolButton::clicked, this, &PilinReyEditor::removeLayer); connect(up, &QToolButton::clicked, this, [this]() { moveLayer(-1); }); connect(down, &QToolButton::clicked, this, [this]() { moveLayer(1); });
    }

    assetsPopover_ = makePopover(tr("Símbolos importados"));
    {
        auto* layout = qobject_cast<QVBoxLayout*>(assetsPopover_->layout());
        auto* note = new QLabel(tr("Worldbuilder Writer no inserta una biblioteca artificial de sellos. Importa símbolos cartográficos propios o históricos en PNG/JPG/WebP."), assetsPopover_); note->setObjectName(QStringLiteral("pilinPopoverCopy")); note->setWordWrap(true); layout->addWidget(note);
        assets_ = new QListWidget(assetsPopover_); assets_->setViewMode(QListView::IconMode); assets_->setResizeMode(QListView::Adjust); assets_->setMovement(QListView::Static); assets_->setGridSize(QSize(92, 70)); assets_->setMinimumHeight(190); layout->addWidget(assets_);
        auto* import = new QPushButton(tr("Importar símbolo…"), assetsPopover_); layout->addWidget(import); connect(import, &QPushButton::clicked, this, &PilinReyEditor::importAsset);
    }

    templatePopover_ = makePopover(tr("Referencia cartográfica"));
    {
        auto* layout = qobject_cast<QVBoxLayout*>(templatePopover_->layout());
        auto* note = new QLabel(tr("Superpone temporalmente un mapa real o una carta de referencia. No se convierte en el mapa final."), templatePopover_); note->setObjectName(QStringLiteral("pilinPopoverCopy")); note->setWordWrap(true); layout->addWidget(note);
        auto* buttons = new QHBoxLayout; auto* load = new QPushButton(tr("Cargar…"), templatePopover_); auto* clear = new QPushButton(tr("Quitar"), templatePopover_); buttons->addWidget(load); buttons->addWidget(clear); layout->addLayout(buttons);
        templateOpacity_ = new QSlider(Qt::Horizontal, templatePopover_); templateOpacity_->setRange(0, 100); templateOpacity_->setValue(25);
        templateScale_ = new QSlider(Qt::Horizontal, templatePopover_); templateScale_->setRange(10, 300); templateScale_->setValue(100);
        templateRotation_ = new QSlider(Qt::Horizontal, templatePopover_); templateRotation_->setRange(-180, 180);
        layout->addWidget(new QLabel(tr("Opacidad"), templatePopover_)); layout->addWidget(templateOpacity_); layout->addWidget(new QLabel(tr("Escala"), templatePopover_)); layout->addWidget(templateScale_); layout->addWidget(new QLabel(tr("Rotación"), templatePopover_)); layout->addWidget(templateRotation_);
        connect(load, &QPushButton::clicked, this, &PilinReyEditor::chooseTemplate); connect(clear, &QPushButton::clicked, this, &PilinReyEditor::clearTemplate);
    }

    appearancePopover_ = makePopover(tr("Lenguaje cartográfico"));
    {
        auto* layout = qobject_cast<QVBoxLayout*>(appearancePopover_->layout());
        auto* note = new QLabel(tr("Ajusta la paleta del mapa sin convertirla en decoración de interfaz."), appearancePopover_); note->setObjectName(QStringLiteral("pilinPopoverCopy")); note->setWordWrap(true); layout->addWidget(note);
        const QList<QPair<QString, QString>> entries{{QStringLiteral("sea"), tr("Mar")}, {QStringLiteral("river"), tr("Ríos")}, {QStringLiteral("road"), tr("Caminos")}, {QStringLiteral("border"), tr("Fronteras")}, {QStringLiteral("region"), tr("Regiones")}, {QStringLiteral("text"), tr("Texto")}};
        for (const auto& entry : entries) { auto* button = new QPushButton(entry.second, appearancePopover_); connect(button, &QPushButton::clicked, this, [this, key = entry.first]() { setThemeColor(key); }); layout->addWidget(button); }
    }

    auto* options = new QFrame(canvasHost_);
    options->setObjectName(QStringLiteral("pilinToolOptions"));
    auto* optionLayout = new QVBoxLayout(options); optionLayout->setContentsMargins(12, 10, 12, 10); optionLayout->setSpacing(6);
    toolOptionsLabel_ = new QLabel(tr("Herramienta"), options); toolOptionsLabel_->setObjectName(QStringLiteral("pilinToolOptionsLabel")); optionLayout->addWidget(toolOptionsLabel_);
    optionLayout->addWidget(new QLabel(tr("Radio"), options)); brushRadiusSlider_ = new QSlider(Qt::Horizontal, options); brushRadiusSlider_->setRange(20, 700); brushRadiusSlider_->setValue(brushRadius_); brushRadiusSlider_->setMinimumWidth(180); optionLayout->addWidget(brushRadiusSlider_);
    optionLayout->addWidget(new QLabel(tr("Intensidad"), options)); densitySlider_ = new QSlider(Qt::Horizontal, options); densitySlider_->setRange(10, 100); densitySlider_->setValue(density_); optionLayout->addWidget(densitySlider_);
    optionLayout->addWidget(new QLabel(tr("Ancho de línea"), options)); strokeWidthSlider_ = new QSlider(Qt::Horizontal, options); strokeWidthSlider_->setRange(1, 24); strokeWidthSlider_->setValue(strokeWidth_); optionLayout->addWidget(strokeWidthSlider_);
    symbolSizeSlider_ = new QSlider(Qt::Horizontal, options); symbolSizeSlider_->setRange(16, 160); symbolSizeSlider_->setValue(symbolSize_); symbolSizeSlider_->hide();
    options->hide();

    selectionPopover_ = new QFrame(canvasHost_);
    selectionPopover_->setObjectName(QStringLiteral("pilinSelectionPopover"));
    auto* selectionLayout = new QVBoxLayout(selectionPopover_); selectionLayout->setContentsMargins(10, 9, 10, 9); selectionLayout->setSpacing(6);
    selectionLabel_ = new QLabel(tr("Selección"), selectionPopover_); selectionLabel_->setObjectName(QStringLiteral("pilinPopoverTitle")); selectionLayout->addWidget(selectionLabel_);
    auto* actions = new QHBoxLayout;
    editObjectButton_ = toolButton(QStringLiteral("✎"), tr("Editar"), selectionPopover_, QStringLiteral("pilinCommand"));
    duplicateObjectButton_ = toolButton(QStringLiteral("⧉"), tr("Duplicar"), selectionPopover_, QStringLiteral("pilinCommand"));
    linkAtlasButton_ = toolButton(QStringLiteral("⌁"), tr("Enlazar Atlas"), selectionPopover_, QStringLiteral("pilinCommand"));
    deleteObjectButton_ = toolButton(QStringLiteral("×"), tr("Eliminar"), selectionPopover_, QStringLiteral("pilinCommand"));
    actions->addWidget(editObjectButton_); actions->addWidget(duplicateObjectButton_); actions->addWidget(linkAtlasButton_); actions->addWidget(deleteObjectButton_); selectionLayout->addLayout(actions);
    auto* transforms = new QHBoxLayout;
    rotateLeftButton_ = toolButton(QStringLiteral("↺"), tr("Rotar −15°"), selectionPopover_, QStringLiteral("pilinCommand"));
    rotateRightButton_ = toolButton(QStringLiteral("↻"), tr("Rotar +15°"), selectionPopover_, QStringLiteral("pilinCommand"));
    scaleDownButton_ = toolButton(QStringLiteral("−"), tr("Reducir"), selectionPopover_, QStringLiteral("pilinCommand"));
    scaleUpButton_ = toolButton(QStringLiteral("+"), tr("Aumentar"), selectionPopover_, QStringLiteral("pilinCommand"));
    transforms->addWidget(rotateLeftButton_); transforms->addWidget(rotateRightButton_); transforms->addWidget(scaleDownButton_); transforms->addWidget(scaleUpButton_); selectionLayout->addLayout(transforms);

    status_ = new QLabel(tr("Mapa · listo"), canvasHost_); status_->setObjectName(QStringLiteral("pilinStatus")); status_->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    viewport_->onPath = [this](const QString& type, const QJsonArray& points) { addPathObject(type, points); };
    viewport_->onSettlement = [this](double x, double y) { addSettlement(x, y); };
    viewport_->onStamp = [this](double x, double y) { addStamp(x, y); };
    viewport_->onLabel = [this](double x, double y) { addLabel(x, y); };
    viewport_->onSelection = [this](const QStringList& ids) { selectObjects(ids); };
    viewport_->onMovePoint = [this](const QString& id, double x, double y) { movePointObject(id, x, y); };
    viewport_->onMovePathPoint = [this](const QString& id, int pointIndex, double x, double y) { movePathPoint(id, pointIndex, x, y); };
    viewport_->onDeleteSelection = [this]() { deleteSelectedObject(); };
    viewport_->onStatus = [this](const QString& text) { status_->setText(text); };

    connect(brushRadiusSlider_, &QSlider::valueChanged, this, [this](int value) { brushRadius_ = value; viewport_->setBrushRadius(value); });
    connect(densitySlider_, &QSlider::valueChanged, this, [this](int value) { density_ = value; setProperty("terrainStrength", value); viewport_->setDensity(value); });
    connect(strokeWidthSlider_, &QSlider::valueChanged, this, [this](int value) { strokeWidth_ = value; viewport_->setStrokeWidth(value); });
    connect(symbolSizeSlider_, &QSlider::valueChanged, this, [this](int value) { symbolSize_ = value; viewport_->setSymbolSize(value); });

    auto hideFloating = [this]() {
        const QStringList names{QStringLiteral("pilinGeneratorPopover"), QStringLiteral("pilinTerrainPopover"), QStringLiteral("pilinBiomePopover"), QStringLiteral("pilinPopover")};
        for (const QString& name : names) for (QFrame* frame : canvasHost_->findChildren<QFrame*>(name)) frame->hide();
    };
    auto toggle = [this, hideFloating](QFrame* panel) {
        const bool show = !panel->isVisible(); hideFloating(); panel->setVisible(show); layoutFloatingPanels();
    };
    connect(generate, &QToolButton::clicked, this, [toggle, generationPopover]() { toggle(generationPopover); });
    connect(terrain, &QToolButton::clicked, this, [toggle, terrainPopover]() { toggle(terrainPopover); });
    connect(biomes, &QToolButton::clicked, this, [toggle, biomePopover]() { toggle(biomePopover); });
    connect(layersButton_, &QToolButton::clicked, this, [this, toggle]() { toggle(layersPopover_); });
    connect(assetsButton_, &QToolButton::clicked, this, [this, toggle]() { toggle(assetsPopover_); });
    connect(templateButton_, &QToolButton::clicked, this, [this, toggle]() { toggle(templatePopover_); });
    connect(appearanceButton_, &QToolButton::clicked, this, [this, toggle]() { toggle(appearancePopover_); });
    connect(fit, &QToolButton::clicked, viewport_, &PilinReyViewport::fitDocument);
    connect(undoButton_, &QToolButton::clicked, this, &PilinReyEditor::undo); connect(redoButton_, &QToolButton::clicked, this, &PilinReyEditor::redo);
    connect(exportButton, &QToolButton::clicked, this, &PilinReyEditor::exportMap);
    connect(snapCheck_, &QCheckBox::toggled, viewport_, &PilinReyViewport::setSnap); connect(gridCheck_, &QCheckBox::toggled, viewport_, &PilinReyViewport::setGrid);

    connect(layerLocked_, &QCheckBox::toggled, this, [this](bool value) { if (!refreshing_) setLayerLocked(value); });
    connect(layerOpacity_, &QSlider::valueChanged, this, [this](int value) { if (!refreshing_) setLayerOpacity(value); });
    connect(layers_, &QListWidget::itemChanged, this, &PilinReyEditor::applyLayerItem);
    connect(layers_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (refreshing_ || row < 0) return;
        QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject(); QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); if (row >= layers.size()) return;
        p.insert(QStringLiteral("activeLayerId"), layers.at(row).toObject().value(QStringLiteral("id")).toString()); map_.insert(QStringLiteral("pilinRey"), p); refreshLayerControls(); persistToArchive();
    });
    connect(assets_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (!item || item->data(Qt::UserRole + 1).toString().isEmpty()) return;
        selectedAssetKind_ = QStringLiteral("custom"); selectedAssetData_ = item->data(Qt::UserRole + 1).toString(); setActiveTool(Tool::Stamp); assetsPopover_->hide();
    });
    connect(templateOpacity_, &QSlider::valueChanged, this, [this](int value) { if (!refreshing_) mutateDocument([&](QJsonObject& p) { QJsonObject t = p.value(QStringLiteral("template")).toObject(); t.insert(QStringLiteral("opacity"), value / 100.0); p.insert(QStringLiteral("template"), t); }); });
    connect(templateScale_, &QSlider::valueChanged, this, [this](int value) { if (!refreshing_) mutateDocument([&](QJsonObject& p) { QJsonObject t = p.value(QStringLiteral("template")).toObject(); t.insert(QStringLiteral("scale"), value / 100.0); p.insert(QStringLiteral("template"), t); }); });
    connect(templateRotation_, &QSlider::valueChanged, this, [this](int value) { if (!refreshing_) mutateDocument([&](QJsonObject& p) { QJsonObject t = p.value(QStringLiteral("template")).toObject(); t.insert(QStringLiteral("rotation"), value); p.insert(QStringLiteral("template"), t); }); });

    connect(editObjectButton_, &QToolButton::clicked, this, &PilinReyEditor::editSelectedObject); connect(duplicateObjectButton_, &QToolButton::clicked, this, &PilinReyEditor::duplicateSelectedObject);
    connect(deleteObjectButton_, &QToolButton::clicked, this, &PilinReyEditor::deleteSelectedObject); connect(linkAtlasButton_, &QToolButton::clicked, this, &PilinReyEditor::linkSelectedToAtlas);
    connect(rotateLeftButton_, &QToolButton::clicked, this, [this]() { transformSelection(1.0, -15); }); connect(rotateRightButton_, &QToolButton::clicked, this, [this]() { transformSelection(1.0, 15); });
    connect(scaleDownButton_, &QToolButton::clicked, this, [this]() { transformSelection(.9, 0); }); connect(scaleUpButton_, &QToolButton::clicked, this, [this]() { transformSelection(1.1, 0); });

    auto* undoShortcut = new QShortcut(QKeySequence::Undo, this); connect(undoShortcut, &QShortcut::activated, this, &PilinReyEditor::undo);
    auto* redoShortcut = new QShortcut(QKeySequence::Redo, this); connect(redoShortcut, &QShortcut::activated, this, &PilinReyEditor::redo);
    auto* copyShortcut = new QShortcut(QKeySequence::Copy, this); connect(copyShortcut, &QShortcut::activated, this, &PilinReyEditor::copySelectedObject);
    auto* pasteShortcut = new QShortcut(QKeySequence::Paste, this); connect(pasteShortcut, &QShortcut::activated, this, &PilinReyEditor::pasteCopiedObject);
    auto* duplicateShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_D), this); connect(duplicateShortcut, &QShortcut::activated, this, &PilinReyEditor::duplicateSelectedObject);

    for (QFrame* frame : canvasHost_->findChildren<QFrame*>()) if (frame != toolRail_ && frame != topCommands_) frame->hide();

    setStyleSheet(QStringLiteral(R"QSS(
#pilinReyEditor,#pilinCanvasHost,#pilinQtViewport{background:#090d12;}
#pilinToolPalette{background:rgba(10,14,19,246);border:1px solid #27313a;border-radius:12px;}
#pilinTopCommands,#pilinPopover,#pilinGeneratorPopover,#pilinTerrainPopover,#pilinBiomePopover,#pilinSelectionPopover,#pilinToolOptions{background:rgba(12,17,23,247);color:#d9e0e7;border:1px solid #2c3741;border-radius:10px;}
QToolButton#pilinMapTool{background:transparent;color:#93a0ab;border:0;border-radius:8px;font-size:13pt;padding:0;}
QToolButton#pilinMapTool:hover{background:#17212b;color:#f2f6f9;} QToolButton#pilinMapTool:checked{background:#17334a;color:#8dccff;border:1px solid #2d6690;}
QToolButton#pilinCommand,QToolButton#pilinPrimaryCommand{background:transparent;color:#aeb8c2;border:0;border-radius:7px;padding:7px 9px;}
QToolButton#pilinCommand:hover{background:#18222c;color:#f3f6f8;} QToolButton#pilinPrimaryCommand{background:#286fa9;color:white;font-weight:700;} QToolButton#pilinPrimaryCommand:hover{background:#3282c4;}
#pilinPopoverTitle,#pilinToolOptionsLabel{color:#f0ece5;font-family:'Georgia';font-size:12pt;font-weight:600;} #pilinPopoverCopy{color:#81909d;font-size:8.5pt;}
#pilinPopover QListWidget,#pilinGeneratorPopover QListWidget{background:#0a0f14;color:#d9dfe5;border:1px solid #29343e;border-radius:8px;outline:0;} #pilinPopover QListWidget::item{padding:8px;border-radius:6px;} #pilinPopover QListWidget::item:selected{background:#17334a;color:#b8ddfb;}
#pilinPopover QPushButton,#pilinGeneratorPopover QPushButton,#pilinTerrainPopover QPushButton,#pilinBiomePopover QPushButton{background:#111922;color:#cbd4dc;border:1px solid #2e3944;border-radius:7px;padding:8px 10px;text-align:left;} #pilinPopover QPushButton:hover,#pilinGeneratorPopover QPushButton:hover,#pilinTerrainPopover QPushButton:hover,#pilinBiomePopover QPushButton:hover{background:#192631;color:#ffffff;} #pilinBiomePopover QPushButton:checked{background:#19384d;color:#9ed9ff;border-color:#34729b;}
#pilinPrimaryAction{background:#2b79b9!important;color:#fff!important;border-color:#2b79b9!important;font-weight:700;}
QLineEdit,QSpinBox,QComboBox{background:#0a1118;color:#e0e6eb;border:1px solid #2d3944;border-radius:7px;padding:6px 8px;} QCheckBox{color:#9da9b4;spacing:7px;} QSlider::groove:horizontal{height:4px;background:#303b45;border-radius:2px;} QSlider::handle:horizontal{width:13px;margin:-4px 0;background:#6eacd8;border-radius:6px;}
#pilinStatus{background:rgba(10,14,19,230);color:#9ca8b3;border:1px solid #2a343d;border-radius:7px;padding:6px 9px;}
)QSS"));
}

void PilinReyEditor::resizeEvent(QResizeEvent* event) { QWidget::resizeEvent(event); layoutFloatingPanels(); }
void PilinReyEditor::showEvent(QShowEvent* event) { QWidget::showEvent(event); layoutFloatingPanels(); }

void PilinReyEditor::layoutFloatingPanels() {
    if (!canvasHost_) return;
    const int margin = 14;
    if (toolRail_) { toolRail_->adjustSize(); toolRail_->setFixedWidth(54); toolRail_->move(margin, qMax(72, (canvasHost_->height() - toolRail_->height()) / 2)); toolRail_->raise(); }
    if (topCommands_) { topCommands_->adjustSize(); topCommands_->move(qMax(82, (canvasHost_->width() - topCommands_->width()) / 2), margin); topCommands_->raise(); }
    int y = 66;
    for (QFrame* frame : canvasHost_->findChildren<QFrame*>()) {
        if (!frame->isVisible() || frame == toolRail_ || frame == topCommands_ || frame == selectionPopover_ || frame->objectName() == QStringLiteral("pilinToolOptions")) continue;
        frame->setMinimumWidth(300); frame->setMaximumWidth(340); frame->adjustSize(); frame->move(canvasHost_->width() - frame->width() - margin, y); frame->raise(); y = frame->geometry().bottom() + 10;
    }
    if (selectionPopover_ && selectionPopover_->isVisible()) { selectionPopover_->adjustSize(); selectionPopover_->move(canvasHost_->width() - selectionPopover_->width() - margin, canvasHost_->height() - selectionPopover_->height() - 24); selectionPopover_->raise(); }
    if (QWidget* options = canvasHost_->findChild<QWidget*>(QStringLiteral("pilinToolOptions")); options && options->isVisible()) { options->adjustSize(); options->move(78, qMax(76, (canvasHost_->height() - options->height()) / 2)); options->raise(); }
    if (status_) { status_->adjustSize(); status_->move(margin, canvasHost_->height() - status_->height() - margin); status_->raise(); }
}

void PilinReyEditor::setMap(const QJsonObject& map) {
    map_ = map; ensurePilinDocument(); undoStack_.clear(); redoStack_.clear(); selectedObjectIds_.clear(); refreshLayers(); refreshAssets(); refreshSelectionControls(); refreshViewport();
    QTimer::singleShot(0, viewport_, [this]() { if (viewport_) viewport_->fitDocument(); });
}

void PilinReyEditor::ensurePilinDocument() {
    QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject();
    p.insert(QStringLiteral("version"), 9);
    if (!p.contains(QStringLiteral("width"))) p.insert(QStringLiteral("width"), 4096);
    if (!p.contains(QStringLiteral("height"))) p.insert(QStringLiteral("height"), 2304);
    QJsonObject theme = p.value(QStringLiteral("theme")).toObject();
    const QList<QPair<QString, QString>> defaults{{QStringLiteral("sea"),QStringLiteral("#789fad")},{QStringLiteral("river"),QStringLiteral("#2d6382")},{QStringLiteral("road"),QStringLiteral("#75512f")},{QStringLiteral("border"),QStringLiteral("#7c4650")},{QStringLiteral("region"),QStringLiteral("#8e6547")},{QStringLiteral("symbol"),QStringLiteral("#2c2924")},{QStringLiteral("text"),QStringLiteral("#29251f")}};
    for (const auto& item : defaults) if (!theme.contains(item.first)) theme.insert(item.first, item.second); p.insert(QStringLiteral("theme"), theme);
    QJsonObject t = p.value(QStringLiteral("template")).toObject();
    if (!t.contains(QStringLiteral("dataUrl"))) t.insert(QStringLiteral("dataUrl"), map_.value(QStringLiteral("backgroundImageDataUrl")).toString());
    if (!t.contains(QStringLiteral("opacity"))) t.insert(QStringLiteral("opacity"), .25); if (!t.contains(QStringLiteral("visible"))) t.insert(QStringLiteral("visible"), true); if (!t.contains(QStringLiteral("scale"))) t.insert(QStringLiteral("scale"), 1.0); if (!t.contains(QStringLiteral("rotation"))) t.insert(QStringLiteral("rotation"), 0.0); p.insert(QStringLiteral("template"), t);
    QJsonArray layers = p.value(QStringLiteral("layers")).toArray();
    if (layers.isEmpty()) {
        for (const QString& name : {tr("Hidrografía"), tr("Fronteras y regiones"), tr("Rutas"), tr("Asentamientos"), tr("Símbolos"), tr("Etiquetas")})
            layers.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("layer"))}, {QStringLiteral("name"), name}, {QStringLiteral("visible"), true}, {QStringLiteral("locked"), false}, {QStringLiteral("opacity"), 1.0}, {QStringLiteral("objects"), QJsonArray()}});
        p.insert(QStringLiteral("activeLayerId"), layers.first().toObject().value(QStringLiteral("id")).toString());
    }
    p.insert(QStringLiteral("layers"), layers); if (!p.contains(QStringLiteral("assets"))) p.insert(QStringLiteral("assets"), QJsonArray());
    if (!p.contains(QStringLiteral("terrainModel"))) {
        MapTerrainEngine terrain(320, 180, 1); terrain.clear(.26f); QJsonObject model = terrain.toJson(); model.insert(QStringLiteral("seaLevel"), .42); model.insert(QStringLiteral("generator"), QStringLiteral("blank-heightmap")); p.insert(QStringLiteral("terrainModel"), model);
    }
    map_.insert(QStringLiteral("pilinRey"), p);
}

void PilinReyEditor::refreshLayers() {
    refreshing_ = true; layers_->clear(); const QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject(); const QString active = p.value(QStringLiteral("activeLayerId")).toString(); const QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); int current = 0;
    for (int i = 0; i < layers.size(); ++i) { const QJsonObject layer = layers.at(i).toObject(); auto* item = new QListWidgetItem(layer.value(QStringLiteral("name")).toString()); item->setData(Qt::UserRole, layer.value(QStringLiteral("id")).toString()); item->setFlags(item->flags() | Qt::ItemIsEditable | Qt::ItemIsUserCheckable); item->setCheckState(layer.value(QStringLiteral("visible")).toBool(true) ? Qt::Checked : Qt::Unchecked); layers_->addItem(item); if (layer.value(QStringLiteral("id")).toString() == active) current = i; }
    if (layers_->count()) layers_->setCurrentRow(current); refreshing_ = false; refreshLayerControls(); undoButton_->setEnabled(!undoStack_.isEmpty()); redoButton_->setEnabled(!redoStack_.isEmpty());
}

void PilinReyEditor::refreshLayerControls() {
    const bool old = refreshing_; refreshing_ = true; const QString active = activeLayerId();
    for (const QJsonValue& value : map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray()) { const QJsonObject layer = value.toObject(); if (layer.value(QStringLiteral("id")).toString() != active) continue; layerLocked_->setChecked(layer.value(QStringLiteral("locked")).toBool(false)); layerOpacity_->setValue(qRound(layer.value(QStringLiteral("opacity")).toDouble(1.0) * 100)); break; }
    refreshing_ = old;
}

void PilinReyEditor::refreshAssets() {
    refreshing_ = true; assets_->clear();
    for (const QJsonValue& value : map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("assets")).toArray()) { const QJsonObject asset = value.toObject(); auto* item = new QListWidgetItem(QStringLiteral("▧\n") + asset.value(QStringLiteral("name")).toString(tr("Símbolo"))); item->setTextAlignment(Qt::AlignCenter); item->setData(Qt::UserRole + 1, asset.value(QStringLiteral("dataUrl")).toString()); assets_->addItem(item); }
    refreshing_ = false;
}

void PilinReyEditor::refreshSelectionControls() {
    const QJsonArray objects = selectedObjects(); const bool has = !objects.isEmpty(); selectionPopover_->setVisible(has); editObjectButton_->setEnabled(objects.size() == 1); duplicateObjectButton_->setEnabled(has); deleteObjectButton_->setEnabled(has); linkAtlasButton_->setEnabled(objects.size() == 1 && objects.first().toObject().value(QStringLiteral("type")).toString() == QStringLiteral("settlement"));
    selectionLabel_->setText(has ? (objects.size() == 1 ? tr("Selección · %1").arg(objects.first().toObject().value(QStringLiteral("label")).toString(objects.first().toObject().value(QStringLiteral("text")).toString(objects.first().toObject().value(QStringLiteral("type")).toString()))) : tr("%1 objetos seleccionados").arg(objects.size())) : tr("Selección")); layoutFloatingPanels();
}

void PilinReyEditor::refreshViewport() { if (viewport_) { viewport_->setDocument(map_); viewport_->setSelection(selectedObjectIds_); } }

void PilinReyEditor::chooseTemplate() { const QString path = QFileDialog::getOpenFileName(this, tr("Referencia cartográfica"), {}, tr("Imágenes (*.png *.jpg *.jpeg *.webp)")); if (path.isEmpty()) return; const QString data = imageToDataUrl(path); if (data.isEmpty()) return; mutateDocument([&](QJsonObject& p) { QJsonObject t = p.value(QStringLiteral("template")).toObject(); t.insert(QStringLiteral("dataUrl"), data); t.insert(QStringLiteral("visible"), true); p.insert(QStringLiteral("template"), t); }); map_.insert(QStringLiteral("backgroundImageDataUrl"), data); persistToArchive(); }
void PilinReyEditor::clearTemplate() { mutateDocument([](QJsonObject& p) { QJsonObject t = p.value(QStringLiteral("template")).toObject(); t.insert(QStringLiteral("dataUrl"), QString()); p.insert(QStringLiteral("template"), t); }); map_.remove(QStringLiteral("backgroundImageDataUrl")); persistToArchive(); }
void PilinReyEditor::rotateTemplate(double delta) { mutateDocument([&](QJsonObject& p) { QJsonObject t = p.value(QStringLiteral("template")).toObject(); t.insert(QStringLiteral("rotation"), t.value(QStringLiteral("rotation")).toDouble() + delta); p.insert(QStringLiteral("template"), t); }); }
void PilinReyEditor::scaleTemplate(double factor) { mutateDocument([&](QJsonObject& p) { QJsonObject t = p.value(QStringLiteral("template")).toObject(); t.insert(QStringLiteral("scale"), std::clamp(t.value(QStringLiteral("scale")).toDouble(1.0) * factor, .05, 5.0)); p.insert(QStringLiteral("template"), t); }); }

void PilinReyEditor::addLayer() { bool ok = false; const QString name = QInputDialog::getText(this, tr("Nueva capa"), tr("Nombre:"), QLineEdit::Normal, tr("Nueva capa"), &ok).trimmed(); if (!ok || name.isEmpty()) return; mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); const QString id = uid(QStringLiteral("layer")); layers.append(QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("name"), name}, {QStringLiteral("visible"), true}, {QStringLiteral("locked"), false}, {QStringLiteral("opacity"), 1.0}, {QStringLiteral("objects"), QJsonArray()}}); p.insert(QStringLiteral("layers"), layers); p.insert(QStringLiteral("activeLayerId"), id); }); }
void PilinReyEditor::duplicateLayer() { const int row = layers_->currentRow(); if (row < 0) return; mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); if (row >= layers.size()) return; QJsonObject copy = layers.at(row).toObject(); const QString id = uid(QStringLiteral("layer")); copy.insert(QStringLiteral("id"), id); copy.insert(QStringLiteral("name"), copy.value(QStringLiteral("name")).toString() + tr(" copia")); QJsonArray objects = copy.value(QStringLiteral("objects")).toArray(); for (int i = 0; i < objects.size(); ++i) { QJsonObject object = objects.at(i).toObject(); object.insert(QStringLiteral("id"), uid(QStringLiteral("mapobj"))); objects.replace(i, object); } copy.insert(QStringLiteral("objects"), objects); layers.insert(row + 1, copy); p.insert(QStringLiteral("layers"), layers); p.insert(QStringLiteral("activeLayerId"), id); }); }
void PilinReyEditor::removeLayer() { const int row = layers_->currentRow(); if (row < 0) return; mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); if (layers.size() <= 1 || row >= layers.size()) return; layers.removeAt(row); p.insert(QStringLiteral("layers"), layers); p.insert(QStringLiteral("activeLayerId"), layers.at(qMin(row, layers.size() - 1)).toObject().value(QStringLiteral("id")).toString()); }); }
void PilinReyEditor::moveLayer(int delta) { const int row = layers_->currentRow(), target = row + delta; if (row < 0) return; mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); if (target < 0 || target >= layers.size()) return; const QJsonValue value = layers.at(row); layers.removeAt(row); layers.insert(target, value); p.insert(QStringLiteral("layers"), layers); }); }
void PilinReyEditor::setLayerLocked(bool locked) { const QString active = activeLayerId(); mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); for (int i = 0; i < layers.size(); ++i) { QJsonObject layer = layers.at(i).toObject(); if (layer.value(QStringLiteral("id")).toString() != active) continue; layer.insert(QStringLiteral("locked"), locked); layers.replace(i, layer); break; } p.insert(QStringLiteral("layers"), layers); }); }
void PilinReyEditor::setLayerOpacity(int value) { const QString active = activeLayerId(); mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); for (int i = 0; i < layers.size(); ++i) { QJsonObject layer = layers.at(i).toObject(); if (layer.value(QStringLiteral("id")).toString() != active) continue; layer.insert(QStringLiteral("opacity"), value / 100.0); layers.replace(i, layer); break; } p.insert(QStringLiteral("layers"), layers); }); }
void PilinReyEditor::applyLayerItem(QListWidgetItem* item) { if (refreshing_ || !item) return; const QString id = item->data(Qt::UserRole).toString(); const QString name = item->text().trimmed(); const bool visible = item->checkState() == Qt::Checked; mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); for (int i = 0; i < layers.size(); ++i) { QJsonObject layer = layers.at(i).toObject(); if (layer.value(QStringLiteral("id")).toString() != id) continue; layer.insert(QStringLiteral("name"), name.isEmpty() ? tr("Capa") : name); layer.insert(QStringLiteral("visible"), visible); layers.replace(i, layer); break; } p.insert(QStringLiteral("layers"), layers); }); }

void PilinReyEditor::setActiveTool(Tool tool) {
    viewport_->setTool(tool);
    if (QWidget* options = canvasHost_->findChild<QWidget*>(QStringLiteral("pilinToolOptions"))) {
        const bool show = brushTool(tool) || nodeTool(tool); options->setVisible(show);
        toolOptionsLabel_->setText(tool == Tool::Coast ? tr("Elevar terreno") : tool == Tool::Eraser ? tr("Hundir terreno") : tool == Tool::Mountain ? tr("Cordillera") : tool == Tool::Forest ? tr("Bioma") : tool == Tool::River ? tr("Río") : tool == Tool::Road ? tr("Ruta") : tool == Tool::Border ? tr("Frontera") : tool == Tool::Region ? tr("Región") : tr("Herramienta"));
    }
    layoutFloatingPanels();
}

QString PilinReyEditor::activeLayerId() const { return map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("activeLayerId")).toString(); }

void PilinReyEditor::addPathObject(const QString& type, const QJsonArray& points) {
    if (type.isEmpty() || points.isEmpty()) return;

    if (type == QStringLiteral("terrainRaise") || type == QStringLiteral("terrainLower") || type == QStringLiteral("terrainRidge")) {
        mutateDocument([&](QJsonObject& p) {
            const QJsonObject previous = p.value(QStringLiteral("terrainModel")).toObject();
            MapTerrainEngine terrain = MapTerrainEngine::fromJson(previous);
            const double docW = std::max(1.0, p.value(QStringLiteral("width")).toDouble(4096.0));
            const double docH = std::max(1.0, p.value(QStringLiteral("height")).toDouble(2304.0));
            const double strength = property("terrainStrength").toInt() / 100.0;
            auto terrainPoint = [&](const QJsonValue& value) { const QJsonObject point = value.toObject(); return QPointF(point.value(QStringLiteral("x")).toDouble() / docW * (terrain.width() - 1), point.value(QStringLiteral("y")).toDouble() / docH * (terrain.height() - 1)); };
            auto terrainRadius = [&](const QJsonValue& value) { const double radius = value.toObject().value(QStringLiteral("radius")).toDouble(brushRadius_); return std::max(1.5, radius * .5 * (terrain.width() / docW + terrain.height() / docH)); };
            if (type == QStringLiteral("terrainRidge") && points.size() >= 2) {
                for (int i = 1; i < points.size(); ++i) terrain.addRange(terrainPoint(points.at(i - 1)), terrainPoint(points.at(i)), terrainRadius(points.at(i)) * .58, .11f + static_cast<float>(strength * .13), .10f);
            } else {
                for (const QJsonValue& value : points) {
                    const QPointF center = terrainPoint(value); const double radius = terrainRadius(value);
                    if (type == QStringLiteral("terrainRaise")) terrain.addHill(center, radius, .06f + static_cast<float>(strength * .15));
                    else terrain.addPit(center, radius, .07f + static_cast<float>(strength * .16));
                }
            }
            terrain.smooth(1);
            p.insert(QStringLiteral("terrainModel"), terrainWithMetadata(terrain, previous));
        });
        return;
    }

    if (type == QStringLiteral("biomePaint")) {
        mutateDocument([&](QJsonObject& p) {
            QJsonObject model = p.value(QStringLiteral("terrainModel")).toObject(); const int tw = model.value(QStringLiteral("width")).toInt(), th = model.value(QStringLiteral("height")).toInt(); if (tw <= 0 || th <= 0) return;
            QJsonArray biomes = model.value(QStringLiteral("biomes")).toArray(); while (biomes.size() < tw * th) biomes.append(1);
            const double docW = std::max(1.0, p.value(QStringLiteral("width")).toDouble(4096.0)), docH = std::max(1.0, p.value(QStringLiteral("height")).toDouble(2304.0)); const int biome = property("selectedBiome").toInt();
            for (const QJsonValue& value : points) {
                const QJsonObject point = value.toObject(); const double gx = point.value(QStringLiteral("x")).toDouble() / docW * (tw - 1), gy = point.value(QStringLiteral("y")).toDouble() / docH * (th - 1); const double radiusDoc = point.value(QStringLiteral("radius")).toDouble(brushRadius_); const double radius = std::max(1.0, radiusDoc * .5 * (tw / docW + th / docH));
                const int minX = qMax(0, static_cast<int>(std::floor(gx - radius))), maxX = qMin(tw - 1, static_cast<int>(std::ceil(gx + radius))), minY = qMax(0, static_cast<int>(std::floor(gy - radius))), maxY = qMin(th - 1, static_cast<int>(std::ceil(gy + radius)));
                for (int y = minY; y <= maxY; ++y) for (int x = minX; x <= maxX; ++x) if (std::hypot(x - gx, y - gy) <= radius) biomes.replace(y * tw + x, biome);
            }
            model.insert(QStringLiteral("biomes"), biomes); p.insert(QStringLiteral("terrainModel"), model);
        });
        return;
    }

    mutateDocument([&](QJsonObject& p) {
        QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); int target = layers_->currentRow(); if (target < 0 || target >= layers.size()) target = 0; QJsonObject layer = layers.at(target).toObject(); if (layer.value(QStringLiteral("locked")).toBool(false)) return;
        QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); const QString id = uid(QStringLiteral("mapobj")); QJsonObject object{{QStringLiteral("id"), id}, {QStringLiteral("type"), type}, {QStringLiteral("points"), points}, {QStringLiteral("width"), strokeWidth_}}; if (type == QStringLiteral("region")) { object.insert(QStringLiteral("closed"), true); object.insert(QStringLiteral("fillOpacity"), .12); } objects.append(object); layer.insert(QStringLiteral("objects"), objects); layers.replace(target, layer); p.insert(QStringLiteral("layers"), layers); selectedObjectIds_ = {id};
    });
}

void PilinReyEditor::addSettlement(double x, double y) {
    bool ok = false; const QString name = QInputDialog::getText(this, tr("Asentamiento"), tr("Nombre:"), QLineEdit::Normal, tr("Nuevo asentamiento"), &ok).trimmed(); if (!ok || name.isEmpty()) return;
    mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); int target = layers_->currentRow(); if (target < 0 || target >= layers.size()) target = qMax(0, layerIndexByName(layers, tr("Asentamientos"))); QJsonObject layer = layers.at(target).toObject(); QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); const QString id = uid(QStringLiteral("settlement")); objects.append(QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("type"), QStringLiteral("settlement")}, {QStringLiteral("label"), name}, {QStringLiteral("settlementType"), tr("Pueblo")}, {QStringLiteral("x"), x}, {QStringLiteral("y"), y}, {QStringLiteral("scale"), 1.0}}); layer.insert(QStringLiteral("objects"), objects); layers.replace(target, layer); p.insert(QStringLiteral("layers"), layers); selectedObjectIds_ = {id}; });
}

void PilinReyEditor::addStamp(double x, double y) {
    if (selectedAssetData_.isEmpty()) return;
    mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); int target = layers_->currentRow(); if (target < 0 || target >= layers.size()) target = 0; QJsonObject layer = layers.at(target).toObject(); QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); const QString id = uid(QStringLiteral("symbol")); objects.append(QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("type"), QStringLiteral("stamp")}, {QStringLiteral("dataUrl"), selectedAssetData_}, {QStringLiteral("x"), x}, {QStringLiteral("y"), y}, {QStringLiteral("scale"), 1.0}, {QStringLiteral("rotation"), 0.0}, {QStringLiteral("size"), symbolSize_ * 2.2}, {QStringLiteral("opacity"), 1.0}}); layer.insert(QStringLiteral("objects"), objects); layers.replace(target, layer); p.insert(QStringLiteral("layers"), layers); selectedObjectIds_ = {id}; });
}

void PilinReyEditor::addLabel(double x, double y) {
    bool ok = false; const QString text = QInputDialog::getText(this, tr("Etiqueta"), tr("Texto:"), QLineEdit::Normal, {}, &ok).trimmed(); if (!ok || text.isEmpty()) return;
    mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); int target = layers_->currentRow(); if (target < 0 || target >= layers.size()) target = 0; QJsonObject layer = layers.at(target).toObject(); QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); const QString id = uid(QStringLiteral("label")); objects.append(QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("type"), QStringLiteral("label")}, {QStringLiteral("text"), text}, {QStringLiteral("x"), x}, {QStringLiteral("y"), y}, {QStringLiteral("fontSize"), 54}, {QStringLiteral("rotation"), 0.0}, {QStringLiteral("bold"), false}}); layer.insert(QStringLiteral("objects"), objects); layers.replace(target, layer); p.insert(QStringLiteral("layers"), layers); selectedObjectIds_ = {id}; });
}

void PilinReyEditor::selectObjects(const QStringList& ids) { selectedObjectIds_ = ids; refreshSelectionControls(); viewport_->setSelection(ids); }
QJsonObject PilinReyEditor::selectedObject() const { if (selectedObjectIds_.isEmpty()) return {}; const QString id = selectedObjectIds_.first(); for (const QJsonValue& layerValue : map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray()) for (const QJsonValue& objectValue : layerValue.toObject().value(QStringLiteral("objects")).toArray()) { const QJsonObject object = objectValue.toObject(); if (object.value(QStringLiteral("id")).toString() == id) return object; } return {}; }
QJsonArray PilinReyEditor::selectedObjects() const { QJsonArray result; for (const QString& id : selectedObjectIds_) for (const QJsonValue& layerValue : map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray()) for (const QJsonValue& objectValue : layerValue.toObject().value(QStringLiteral("objects")).toArray()) { const QJsonObject object = objectValue.toObject(); if (object.value(QStringLiteral("id")).toString() == id) { result.append(object); break; } } return result; }

void PilinReyEditor::movePointObject(const QString& id, double x, double y) { mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); for (int li = 0; li < layers.size(); ++li) { QJsonObject layer = layers.at(li).toObject(); QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); for (int oi = 0; oi < objects.size(); ++oi) { QJsonObject object = objects.at(oi).toObject(); if (object.value(QStringLiteral("id")).toString() != id) continue; object.insert(QStringLiteral("x"), x); object.insert(QStringLiteral("y"), y); objects.replace(oi, object); layer.insert(QStringLiteral("objects"), objects); layers.replace(li, layer); p.insert(QStringLiteral("layers"), layers); emit markerMoved(id, x, y); return; } } }); }

void PilinReyEditor::movePathPoint(const QString& id, int pointIndex, double x, double y) {
    if (pointIndex < 0) return;
    mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); for (int li = 0; li < layers.size(); ++li) { QJsonObject layer = layers.at(li).toObject(); QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); for (int oi = 0; oi < objects.size(); ++oi) { QJsonObject object = objects.at(oi).toObject(); if (object.value(QStringLiteral("id")).toString() != id) continue; QJsonArray points = object.value(QStringLiteral("points")).toArray(); if (pointIndex >= points.size()) return; QJsonObject point = points.at(pointIndex).toObject(); point.insert(QStringLiteral("x"), x); point.insert(QStringLiteral("y"), y); points.replace(pointIndex, point); if (object.value(QStringLiteral("closed")).toBool(false) && points.size() > 2) { if (pointIndex == 0) points.replace(points.size() - 1, point); else if (pointIndex == points.size() - 1) points.replace(0, point); } object.insert(QStringLiteral("points"), points); objects.replace(oi, object); layer.insert(QStringLiteral("objects"), objects); layers.replace(li, layer); p.insert(QStringLiteral("layers"), layers); return; } } });
}

void PilinReyEditor::editSelectedObject() {
    if (selectedObjectIds_.size() != 1) return; const QString id = selectedObjectIds_.first(); QJsonObject object = selectedObject(); if (object.isEmpty()) return; const QString type = object.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("label")) { editSelectedLabelStyle(); return; }
    if (type != QStringLiteral("settlement")) return;

    QDialog dialog(this); dialog.setWindowTitle(tr("Editar asentamiento")); dialog.setModal(true); dialog.resize(430, 360);
    auto* root = new QVBoxLayout(&dialog); root->setContentsMargins(16, 16, 16, 16); root->setSpacing(12);
    auto* heading = new QLabel(tr("Ficha cartográfica"), &dialog); heading->setStyleSheet(QStringLiteral("font-family:'Georgia';font-size:16pt;font-weight:600;color:#f0ece5;")); root->addWidget(heading);
    auto* form = new QFormLayout; form->setVerticalSpacing(9); auto* name = new QLineEdit(object.value(QStringLiteral("label")).toString(), &dialog); auto* kind = new QComboBox(&dialog); kind->addItems({tr("Aldea"), tr("Pueblo"), tr("Ciudad"), tr("Capital"), tr("Fortaleza"), tr("Puerto")}); const int currentKind = kind->findText(object.value(QStringLiteral("settlementType")).toString()); if (currentKind >= 0) kind->setCurrentIndex(currentKind); auto* group = new QLineEdit(object.value(QStringLiteral("group")).toString(), &dialog); auto* population = new QSpinBox(&dialog); population->setRange(0, 50000000); population->setGroupSeparatorShown(true); population->setValue(object.value(QStringLiteral("population")).toInt()); auto* atlas = new QLineEdit(object.value(QStringLiteral("atlasId")).toString(), &dialog); atlas->setPlaceholderText(tr("Ficha del Atlas (opcional)")); form->addRow(tr("Nombre"), name); form->addRow(tr("Tipo"), kind); form->addRow(tr("Región / reino"), group); form->addRow(tr("Población"), population); form->addRow(tr("Atlas"), atlas); root->addLayout(form);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog); root->addWidget(buttons); connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept); connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.setStyleSheet(QStringLiteral("QDialog{background:#0c1117;color:#dfe5eb;}QLineEdit,QComboBox,QSpinBox{background:#101820;color:#e4e9ee;border:1px solid #31404d;border-radius:7px;padding:7px;}QPushButton{background:#17212b;color:#dbe3ea;border:1px solid #394a59;border-radius:7px;padding:8px 12px;}"));
    if (dialog.exec() != QDialog::Accepted) return;
    mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); for (int li = 0; li < layers.size(); ++li) { QJsonObject layer = layers.at(li).toObject(); QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); for (int oi = 0; oi < objects.size(); ++oi) { QJsonObject current = objects.at(oi).toObject(); if (current.value(QStringLiteral("id")).toString() != id) continue; current.insert(QStringLiteral("label"), name->text().trimmed()); current.insert(QStringLiteral("settlementType"), kind->currentText()); current.insert(QStringLiteral("group"), group->text().trimmed()); current.insert(QStringLiteral("population"), population->value()); current.insert(QStringLiteral("atlasId"), atlas->text().trimmed()); objects.replace(oi, current); layer.insert(QStringLiteral("objects"), objects); layers.replace(li, layer); p.insert(QStringLiteral("layers"), layers); return; } } });
}

void PilinReyEditor::duplicateSelectedObject() { if (selectedObjectIds_.isEmpty()) return; clipboardObjects_ = selectedObjects(); pasteCopiedObject(); }
void PilinReyEditor::deleteSelectedObject() { if (selectedObjectIds_.isEmpty()) return; mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); for (int li = 0; li < layers.size(); ++li) { QJsonObject layer = layers.at(li).toObject(); QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); for (int oi = objects.size() - 1; oi >= 0; --oi) if (selectedObjectIds_.contains(objects.at(oi).toObject().value(QStringLiteral("id")).toString())) objects.removeAt(oi); layer.insert(QStringLiteral("objects"), objects); layers.replace(li, layer); } p.insert(QStringLiteral("layers"), layers); selectedObjectIds_.clear(); }); }
void PilinReyEditor::copySelectedObject() { clipboardObjects_ = selectedObjects(); }
void PilinReyEditor::pasteCopiedObject() { if (clipboardObjects_.isEmpty()) return; mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); int target = qMax(0, layers_->currentRow()); if (target >= layers.size()) target = 0; QJsonObject layer = layers.at(target).toObject(); QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); QStringList ids; for (const QJsonValue& value : clipboardObjects_) { QJsonObject object = value.toObject(); const QString id = uid(QStringLiteral("mapobj")); object.insert(QStringLiteral("id"), id); if (object.contains(QStringLiteral("x"))) { object.insert(QStringLiteral("x"), object.value(QStringLiteral("x")).toDouble() + 80); object.insert(QStringLiteral("y"), object.value(QStringLiteral("y")).toDouble() + 80); } else { QJsonArray points = object.value(QStringLiteral("points")).toArray(); for (int i = 0; i < points.size(); ++i) { QJsonObject point = points.at(i).toObject(); point.insert(QStringLiteral("x"), point.value(QStringLiteral("x")).toDouble() + 80); point.insert(QStringLiteral("y"), point.value(QStringLiteral("y")).toDouble() + 80); points.replace(i, point); } object.insert(QStringLiteral("points"), points); } objects.append(object); ids << id; } layer.insert(QStringLiteral("objects"), objects); layers.replace(target, layer); p.insert(QStringLiteral("layers"), layers); selectedObjectIds_ = ids; }); }

void PilinReyEditor::linkSelectedToAtlas() { QJsonObject object = selectedObject(); if (object.value(QStringLiteral("type")).toString() != QStringLiteral("settlement")) return; QWidget* cursor = parentWidget(); WorldPage* world = nullptr; while (cursor) { world = qobject_cast<WorldPage*>(cursor); if (world) break; cursor = cursor->parentWidget(); } if (!world || !world->document_) return; const QJsonArray atlasEntries = world->document_->array(QStringLiteral("world")); if (atlasEntries.isEmpty()) { QMessageBox::information(this, tr("Atlas"), tr("Crea primero una entrada en el Atlas.")); return; } QStringList names, ids; for (const QJsonValue& value : atlasEntries) { const QJsonObject entry = value.toObject(); names << entry.value(QStringLiteral("name")).toString(tr("Entrada sin nombre")); ids << entry.value(QStringLiteral("id")).toString(); } bool ok = false; const QString chosen = QInputDialog::getItem(this, tr("Enlazar con Atlas"), tr("Entrada:"), names, 0, false, &ok); if (!ok) return; const int index = names.indexOf(chosen); if (index < 0) return; const QString id = selectedObjectIds_.first(); mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); for (int li = 0; li < layers.size(); ++li) { QJsonObject layer = layers.at(li).toObject(); QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); for (int oi = 0; oi < objects.size(); ++oi) { QJsonObject current = objects.at(oi).toObject(); if (current.value(QStringLiteral("id")).toString() != id) continue; current.insert(QStringLiteral("atlasId"), ids.at(index)); objects.replace(oi, current); layer.insert(QStringLiteral("objects"), objects); layers.replace(li, layer); p.insert(QStringLiteral("layers"), layers); return; } } }); }

void PilinReyEditor::transformSelection(double scaleFactor, double rotationDegrees) { if (selectedObjectIds_.isEmpty()) return; mutateDocument([&](QJsonObject& p) { QJsonArray layers = p.value(QStringLiteral("layers")).toArray(); for (int li = 0; li < layers.size(); ++li) { QJsonObject layer = layers.at(li).toObject(); QJsonArray objects = layer.value(QStringLiteral("objects")).toArray(); for (int oi = 0; oi < objects.size(); ++oi) { QJsonObject object = objects.at(oi).toObject(); if (!selectedObjectIds_.contains(object.value(QStringLiteral("id")).toString())) continue; if (object.contains(QStringLiteral("scale"))) object.insert(QStringLiteral("scale"), std::clamp(object.value(QStringLiteral("scale")).toDouble(1.0) * scaleFactor, .1, 12.0)); if (object.contains(QStringLiteral("rotation"))) object.insert(QStringLiteral("rotation"), object.value(QStringLiteral("rotation")).toDouble() + rotationDegrees); objects.replace(oi, object); } layer.insert(QStringLiteral("objects"), objects); layers.replace(li, layer); } p.insert(QStringLiteral("layers"), layers); }); }

void PilinReyEditor::importAsset() { const QString path = QFileDialog::getOpenFileName(this, tr("Importar símbolo cartográfico"), {}, tr("Imágenes (*.png *.jpg *.jpeg *.webp)")); if (path.isEmpty()) return; const QString data = imageToDataUrl(path); if (data.isEmpty()) return; mutateDocument([&](QJsonObject& p) { QJsonArray assets = p.value(QStringLiteral("assets")).toArray(); assets.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("asset"))}, {QStringLiteral("name"), QFileInfo(path).completeBaseName()}, {QStringLiteral("dataUrl"), data}}); p.insert(QStringLiteral("assets"), assets); }); }
void PilinReyEditor::setThemeColor(const QString& key) { const QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject(); const QColor current(p.value(QStringLiteral("theme")).toObject().value(key).toString()); const QColor color = QColorDialog::getColor(current.isValid() ? current : Qt::white, this, tr("Color cartográfico")); if (!color.isValid()) return; mutateDocument([&](QJsonObject& target) { QJsonObject theme = target.value(QStringLiteral("theme")).toObject(); theme.insert(key, color.name(QColor::HexRgb)); target.insert(QStringLiteral("theme"), theme); }); }

void PilinReyEditor::mutateDocument(const std::function<void(QJsonObject&)>& mutation) { if (refreshing_) return; pushUndo(); QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject(); mutation(p); map_.insert(QStringLiteral("pilinRey"), p); refreshViewport(); refreshLayers(); refreshAssets(); refreshSelectionControls(); persistToArchive(); emit mapEdited(map_); }
void PilinReyEditor::pushUndo() { undoStack_.append(map_); while (undoStack_.size() > 40) undoStack_.removeFirst(); redoStack_.clear(); }
void PilinReyEditor::undo() { if (undoStack_.isEmpty()) return; redoStack_.append(map_); map_ = undoStack_.takeLast(); selectedObjectIds_.clear(); refreshLayers(); refreshAssets(); refreshSelectionControls(); refreshViewport(); persistToArchive(); emit mapEdited(map_); }
void PilinReyEditor::redo() { if (redoStack_.isEmpty()) return; undoStack_.append(map_); map_ = redoStack_.takeLast(); selectedObjectIds_.clear(); refreshLayers(); refreshAssets(); refreshSelectionControls(); refreshViewport(); persistToArchive(); emit mapEdited(map_); }

void PilinReyEditor::exportMap() { const QJsonObject p = map_.value(QStringLiteral("pilinRey")).toObject(); const QSize logical(qMax(1, p.value(QStringLiteral("width")).toInt(4096)), qMax(1, p.value(QStringLiteral("height")).toInt(2304))); MapExportDialog dialog(logical, this); if (dialog.exec() != QDialog::Accepted) return; const QString format = dialog.format(); QString extension = QStringLiteral(".png"), filter = tr("PNG (*.png)"); if (format == QStringLiteral("svg")) { extension = QStringLiteral(".svg"); filter = tr("SVG (*.svg)"); } else if (format == QStringLiteral("pdf")) { extension = QStringLiteral(".pdf"); filter = tr("PDF (*.pdf)"); } QString path = QFileDialog::getSaveFileName(this, tr("Exportar mapa"), map_.value(QStringLiteral("name")).toString(tr("mapa")) + extension, filter); if (path.isEmpty()) return; if (!path.endsWith(extension, Qt::CaseInsensitive)) path += extension; QString error; const bool ok = format == QStringLiteral("svg") ? MapExporter::exportSvg(map_, path, dialog.outputSize(), &error) : format == QStringLiteral("pdf") ? MapExporter::exportPdf(map_, path, dialog.outputSize(), &error) : MapExporter::exportPng(map_, path, dialog.outputSize(), &error); if (!ok) QMessageBox::critical(this, tr("No se pudo exportar"), error); }

void PilinReyEditor::persistToArchive() { QWidget* cursor = parentWidget(); WorldPage* world = nullptr; while (cursor) { world = qobject_cast<WorldPage*>(cursor); if (world) break; cursor = cursor->parentWidget(); } if (!world || !world->document_ || !world->mapList_) return; const int row = world->mapList_->currentRow(); QJsonArray maps = world->document_->array(QStringLiteral("maps")); if (row < 0 || row >= maps.size()) return; QJsonObject stored = maps.at(row).toObject(); stored.insert(QStringLiteral("pilinRey"), map_.value(QStringLiteral("pilinRey"))); if (map_.contains(QStringLiteral("backgroundImageDataUrl"))) stored.insert(QStringLiteral("backgroundImageDataUrl"), map_.value(QStringLiteral("backgroundImageDataUrl"))); maps.replace(row, stored); world->document_->setArray(QStringLiteral("maps"), maps); emit world->changed(); }

} // namespace wbw

