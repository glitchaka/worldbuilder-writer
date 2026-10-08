#include "ui/PilinReyEditor.h"

#include "core/ArchiveDocument.h"
#include "ui/WorldPage.h"

#include <SDL3/SDL.h>

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPaintEngine>
#include <QPushButton>
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

QColor stylePaper(const QString& style) {
    if (style == QStringLiteral("Portulano")) return QColor(225, 213, 174);
    if (style == QStringLiteral("Mappa mundi")) return QColor(214, 194, 147);
    if (style == QStringLiteral("Ptolemaico")) return QColor(232, 220, 187);
    if (style == QStringLiteral("Islámico medieval")) return QColor(222, 207, 161);
    if (style == QStringLiteral("Señorial")) return QColor(225, 210, 171);
    if (style == QStringLiteral("Militar")) return QColor(218, 217, 197);
    if (style == QStringLiteral("Político")) return QColor(228, 224, 207);
    return QColor(224, 210, 174);
}

QColor typeColor(const QString& type) {
    if (type == QStringLiteral("river")) return QColor(66, 108, 130);
    if (type == QStringLiteral("road")) return QColor(115, 87, 57);
    if (type == QStringLiteral("border")) return QColor(153, 70, 57);
    if (type == QStringLiteral("forest")) return QColor(61, 99, 61);
    if (type == QStringLiteral("mountain")) return QColor(82, 73, 65);
    if (type == QStringLiteral("settlement")) return QColor(84, 52, 38);
    if (type == QStringLiteral("label")) return QColor(45, 42, 37);
    return QColor(58, 51, 43);
}

QString toolType(PilinReyEditor::Tool tool) {
    switch (tool) {
        case PilinReyEditor::Tool::Coast: return QStringLiteral("coast");
        case PilinReyEditor::Tool::River: return QStringLiteral("river");
        case PilinReyEditor::Tool::Road: return QStringLiteral("road");
        case PilinReyEditor::Tool::Border: return QStringLiteral("border");
        case PilinReyEditor::Tool::Forest: return QStringLiteral("forest");
        case PilinReyEditor::Tool::Mountain: return QStringLiteral("mountain");
        default: return {};
    }
}

int opacityByte(double opacity) {
    return qRound(std::clamp(opacity, 0.0, 1.0) * 255.0);
}

} // namespace

class PilinReyViewport final : public QWidget {
public:
    using Tool = PilinReyEditor::Tool;
    std::function<void(const QString&, const QJsonArray&)> onPath;
    std::function<void(double, double)> onSettlement;
    std::function<void(double, double)> onLabel;
    std::function<void(const QString&)> onStatus;

    explicit PilinReyViewport(QWidget* parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_NativeWindow, true);
        setAttribute(Qt::WA_PaintOnScreen, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setMinimumSize(540, 360);
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
        renderFrame();
    }

    void setTool(Tool tool) {
        tool_ = tool;
        drawing_ = false;
        currentPoints_ = QJsonArray();
        renderFrame();
    }

protected:
    void showEvent(QShowEvent* event) override {
        QWidget::showEvent(event);
        ensureRenderer();
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
        zoom_ = std::clamp(zoom_ * factor, 0.08, 8.0);
        const QPointF after = screenToWorld(event->position());
        pan_ += QPointF((after.x() - before.x()) * zoom_, (after.y() - before.y()) * zoom_);
        event->accept();
        renderFrame();
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

        const QPointF world = screenToWorld(event->position());
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
            const QPointF delta = event->position() - lastMouse_;
            pan_ += delta;
            lastMouse_ = event->position();
            renderFrame();
            return;
        }
        const QPointF world = screenToWorld(event->position());
        if (onStatus) onStatus(QStringLiteral("x %1 · y %2 · %3%").arg(qRound(world.x())).arg(qRound(world.y())).arg(qRound(zoom_ * 100.0)));
        if (!drawing_) return;
        if (!currentPoints_.isEmpty()) {
            const QJsonObject last = currentPoints_.last().toObject();
            const double dx = world.x() - last.value(QStringLiteral("x")).toDouble();
            const double dy = world.y() - last.value(QStringLiteral("y")).toDouble();
            if (dx * dx + dy * dy < std::pow(6.0 / zoom_, 2.0)) return;
        }
        currentPoints_.append(pointJson(world.x(), world.y()));
        renderFrame();
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::MiddleButton || panning_) {
            panning_ = false;
            return;
        }
        if (event->button() != Qt::LeftButton || !drawing_) return;
        drawing_ = false;
        const QString type = toolType(tool_);
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
        SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER,
                               reinterpret_cast<void*>(static_cast<quintptr>(winId())));
        window_ = SDL_CreateWindowWithProperties(props);
        SDL_DestroyProperties(props);
#else
        window_ = SDL_CreateWindow("Pilín Rey", width(), height(), SDL_WINDOW_RESIZABLE);
#endif
        if (!window_) {
            if (onStatus) onStatus(QStringLiteral("SDL window: %1").arg(QString::fromUtf8(SDL_GetError())));
            return false;
        }
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (!renderer_) {
            if (onStatus) onStatus(QStringLiteral("SDL renderer: %1").arg(QString::fromUtf8(SDL_GetError())));
            return false;
        }
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

    void updateTemplateTexture() {
        if (!renderer_ || !templateDirty_) return;
        templateDirty_ = false;
        if (templateTexture_) {
            SDL_DestroyTexture(templateTexture_);
            templateTexture_ = nullptr;
        }
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

    void setTypeColor(const QString& type, int alpha) {
        const QColor color = typeColor(type);
        SDL_SetRenderDrawColor(renderer_, color.red(), color.green(), color.blue(), alpha);
    }

    void drawPath(const QJsonObject& object, double opacity = 1.0) {
        const QString type = object.value(QStringLiteral("type")).toString();
        const int alpha = opacityByte(opacity);
        setTypeColor(type, alpha);
        const QJsonArray points = object.value(QStringLiteral("points")).toArray();
        if (points.size() < 2) return;
        std::vector<SDL_FPoint> line;
        line.reserve(static_cast<size_t>(points.size()));
        for (const QJsonValue value : points) {
            const QJsonObject point = value.toObject();
            const QPointF p = worldToScreen(point.value(QStringLiteral("x")).toDouble(), point.value(QStringLiteral("y")).toDouble());
            line.push_back(SDL_FPoint{static_cast<float>(p.x()), static_cast<float>(p.y())});
        }
        if (type == QStringLiteral("forest")) {
            for (size_t i = 0; i < line.size(); i += 4) {
                const float x = line[i].x;
                const float y = line[i].y;
                const float s = static_cast<float>(std::clamp(8.0 * zoom_, 3.0, 18.0));
                SDL_RenderLine(renderer_, x, y - s, x - s * 0.55f, y + s * 0.35f);
                SDL_RenderLine(renderer_, x, y - s, x + s * 0.55f, y + s * 0.35f);
                SDL_RenderLine(renderer_, x - s * 0.42f, y, x + s * 0.42f, y);
                SDL_RenderLine(renderer_, x, y + s * 0.35f, x, y + s * 0.8f);
            }
            return;
        }
        if (type == QStringLiteral("mountain")) {
            for (size_t i = 0; i < line.size(); i += 5) {
                const float x = line[i].x;
                const float y = line[i].y;
                const float s = static_cast<float>(std::clamp(12.0 * zoom_, 4.0, 25.0));
                SDL_RenderLine(renderer_, x - s, y + s * 0.55f, x, y - s);
                SDL_RenderLine(renderer_, x, y - s, x + s, y + s * 0.55f);
                SDL_RenderLine(renderer_, x - s * 0.32f, y - s * 0.02f, x, y + s * 0.22f);
                SDL_RenderLine(renderer_, x, y + s * 0.22f, x + s * 0.27f, y - s * 0.08f);
            }
            return;
        }
        SDL_RenderLines(renderer_, line.data(), static_cast<int>(line.size()));
        if (type == QStringLiteral("border")) {
            setTypeColor(type, qRound(alpha * 0.55));
            for (size_t i = 0; i + 1 < line.size(); i += 4)
                SDL_RenderLine(renderer_, line[i].x, line[i].y, line[i + 1].x, line[i + 1].y);
        }
    }

    void drawSettlement(const QJsonObject& object, double opacity) {
        const QPointF p = worldToScreen(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble());
        const int alpha = opacityByte(opacity);
        setTypeColor(QStringLiteral("settlement"), alpha);
        const float r = static_cast<float>(std::clamp(5.0 * zoom_, 3.0, 10.0));
        SDL_FRect rect{static_cast<float>(p.x() - r), static_cast<float>(p.y() - r), r * 2.0f, r * 2.0f};
        SDL_RenderFillRect(renderer_, &rect);
        SDL_SetRenderDrawColor(renderer_, 238, 223, 184, alpha);
        SDL_RenderLine(renderer_, static_cast<float>(p.x() - r), static_cast<float>(p.y()), static_cast<float>(p.x() + r), static_cast<float>(p.y()));
        SDL_RenderLine(renderer_, static_cast<float>(p.x()), static_cast<float>(p.y() - r), static_cast<float>(p.x()), static_cast<float>(p.y() + r));
    }

    void drawLabel(const QJsonObject& object, double opacity) {
        const QPointF p = worldToScreen(object.value(QStringLiteral("x")).toDouble(), object.value(QStringLiteral("y")).toDouble());
        setTypeColor(QStringLiteral("label"), opacityByte(opacity));
        const QByteArray text = object.value(QStringLiteral("text")).toString().toUtf8();
        if (!text.isEmpty()) SDL_RenderDebugText(renderer_, static_cast<float>(p.x()), static_cast<float>(p.y()), text.constData());
    }

    void renderFrame() {
        if (!isVisible() || !ensureRenderer()) return;
        const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        const QString style = pilin.value(QStringLiteral("cartographicStyle")).toString(QStringLiteral("Fantasía clásica"));
        const QColor paper = stylePaper(style);
        SDL_SetRenderDrawColor(renderer_, paper.red(), paper.green(), paper.blue(), 255);
        SDL_RenderClear(renderer_);

        updateTemplateTexture();
        const QJsonObject templ = pilin.value(QStringLiteral("template")).toObject();
        if (templateTexture_ && templ.value(QStringLiteral("visible")).toBool(true)) {
            const double docW = pilin.value(QStringLiteral("width")).toDouble(4096.0);
            const double docH = pilin.value(QStringLiteral("height")).toDouble(2304.0);
            const QPointF topLeft = worldToScreen(0.0, 0.0);
            SDL_FRect dest{static_cast<float>(topLeft.x()), static_cast<float>(topLeft.y()), static_cast<float>(docW * zoom_), static_cast<float>(docH * zoom_)};
            SDL_SetTextureAlphaMod(templateTexture_, static_cast<Uint8>(opacityByte(templ.value(QStringLiteral("opacity")).toDouble(0.35))));
            SDL_RenderTexture(renderer_, templateTexture_, nullptr, &dest);
        }

        for (const QJsonValue layerValue : pilin.value(QStringLiteral("layers")).toArray()) {
            const QJsonObject layer = layerValue.toObject();
            if (!layer.value(QStringLiteral("visible")).toBool(true)) continue;
            const double opacity = layer.value(QStringLiteral("opacity")).toDouble(1.0);
            for (const QJsonValue objectValue : layer.value(QStringLiteral("objects")).toArray()) {
                const QJsonObject object = objectValue.toObject();
                const QString type = object.value(QStringLiteral("type")).toString();
                if (type == QStringLiteral("settlement")) drawSettlement(object, opacity);
                else if (type == QStringLiteral("label")) drawLabel(object, opacity);
                else drawPath(object, opacity);
            }
        }
        if (drawing_ && currentPoints_.size() >= 2)
            drawPath(QJsonObject{{QStringLiteral("type"), toolType(tool_)}, {QStringLiteral("points"), currentPoints_}}, 1.0);
        SDL_RenderPresent(renderer_);
    }

    QJsonObject map_;
    Tool tool_ = Tool::Select;
    bool panning_ = false;
    bool drawing_ = false;
    QPointF lastMouse_;
    QPointF pan_{40.0, 40.0};
    double zoom_ = 0.22;
    QJsonArray currentPoints_;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* templateTexture_ = nullptr;
    bool templateDirty_ = true;
};

PilinReyEditor::PilinReyEditor(QWidget* parent) : QWidget(parent) {
    buildUi();
}

PilinReyEditor::~PilinReyEditor() = default;

void PilinReyEditor::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(7);

    auto* titleRow = new QHBoxLayout;
    auto* title = new QLabel(QStringLiteral("PILÍN REY"));
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    title->setFont(titleFont);
    auto* subtitle = new QLabel(tr("cartografía por capas · SDL3"));
    subtitle->setStyleSheet(QStringLiteral("color:#667085;"));
    style_ = new QComboBox;
    style_->addItems({tr("Fantasía clásica"), tr("Portulano"), tr("Mappa mundi"), tr("Ptolemaico"), tr("Islámico medieval"), tr("Señorial"), tr("Militar"), tr("Político")});
    titleRow->addWidget(title);
    titleRow->addWidget(subtitle);
    titleRow->addStretch();
    titleRow->addWidget(new QLabel(tr("Tradición")));
    titleRow->addWidget(style_);
    root->addLayout(titleRow);

    auto* toolbar = new QHBoxLayout;
    struct ToolDef { Tool tool; const char* label; };
    const ToolDef defs[] = {
        {Tool::Select, "Seleccionar"}, {Tool::Pan, "Mover"}, {Tool::Coast, "Costa"},
        {Tool::River, "Río"}, {Tool::Road, "Camino"}, {Tool::Border, "Frontera"},
        {Tool::Forest, "Bosque"}, {Tool::Mountain, "Montañas"}, {Tool::Settlement, "Asentamiento"},
        {Tool::Label, "Etiqueta"}
    };
    for (const auto& def : defs) {
        auto* button = new QToolButton;
        button->setText(tr(def.label));
        button->setCheckable(true);
        button->setAutoExclusive(true);
        if (def.tool == Tool::Select) button->setChecked(true);
        connect(button, &QToolButton::clicked, this, [this, tool = def.tool]() { setActiveTool(tool); });
        toolButtons_.append(button);
        toolbar->addWidget(button);
    }
    toolbar->addStretch();
    undoButton_ = new QPushButton(tr("Deshacer"));
    redoButton_ = new QPushButton(tr("Rehacer"));
    toolbar->addWidget(undoButton_);
    toolbar->addWidget(redoButton_);
    root->addLayout(toolbar);

    auto* body = new QHBoxLayout;
    viewport_ = new PilinReyViewport;
    viewport_->onPath = [this](const QString& type, const QJsonArray& points) { addPathObject(type, points); };
    viewport_->onSettlement = [this](double x, double y) { addSettlement(x, y); };
    viewport_->onLabel = [this](double x, double y) { addLabel(x, y); };
    viewport_->onStatus = [this](const QString& text) { status_->setText(text); };
    body->addWidget(viewport_, 1);

    auto* panel = new QWidget;
    panel->setFixedWidth(238);
    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(8, 0, 0, 0);
    panelLayout->addWidget(new QLabel(tr("Capas")));
    layers_ = new QListWidget;
    panelLayout->addWidget(layers_, 1);

    auto* layerButtons = new QHBoxLayout;
    auto* add = new QPushButton(tr("+"));
    auto* duplicate = new QPushButton(tr("Duplicar"));
    auto* remove = new QPushButton(tr("−"));
    auto* up = new QPushButton(tr("↑"));
    auto* down = new QPushButton(tr("↓"));
    layerButtons->addWidget(add);
    layerButtons->addWidget(duplicate);
    layerButtons->addWidget(remove);
    layerButtons->addWidget(up);
    layerButtons->addWidget(down);
    panelLayout->addLayout(layerButtons);

    layerLocked_ = new QCheckBox(tr("Bloquear capa"));
    layerOpacity_ = new QSlider(Qt::Horizontal);
    layerOpacity_->setRange(0, 100);
    layerOpacity_->setValue(100);
    panelLayout->addWidget(layerLocked_);
    panelLayout->addWidget(new QLabel(tr("Opacidad de capa")));
    panelLayout->addWidget(layerOpacity_);

    panelLayout->addSpacing(8);
    panelLayout->addWidget(new QLabel(tr("Plantilla")));
    auto* templateButtons = new QHBoxLayout;
    auto* loadTemplate = new QPushButton(tr("Cargar…"));
    auto* clearTemplateButton = new QPushButton(tr("Quitar"));
    templateButtons->addWidget(loadTemplate);
    templateButtons->addWidget(clearTemplateButton);
    panelLayout->addLayout(templateButtons);
    templateOpacity_ = new QSlider(Qt::Horizontal);
    templateOpacity_->setRange(0, 100);
    templateOpacity_->setValue(35);
    panelLayout->addWidget(new QLabel(tr("Opacidad de plantilla")));
    panelLayout->addWidget(templateOpacity_);
    auto* templateHint = new QLabel(tr("Carga un boceto, mapa antiguo o referencia y dibuja en capas sobre él."));
    templateHint->setWordWrap(true);
    templateHint->setStyleSheet(QStringLiteral("color:#667085;"));
    panelLayout->addWidget(templateHint);
    body->addWidget(panel);
    root->addLayout(body, 1);

    status_ = new QLabel(tr("Pilín Rey listo"));
    status_->setStyleSheet(QStringLiteral("color:#667085; padding:2px 4px;"));
    root->addWidget(status_);

    connect(style_, &QComboBox::currentTextChanged, this, [this](const QString& value) {
        if (!refreshing_) mutateDocument([&](QJsonObject& pilin) { pilin.insert(QStringLiteral("cartographicStyle"), value); });
    });
    connect(templateOpacity_, &QSlider::valueChanged, this, [this](int value) {
        if (refreshing_) return;
        mutateDocument([&](QJsonObject& pilin) {
            QJsonObject templ = pilin.value(QStringLiteral("template")).toObject();
            templ.insert(QStringLiteral("opacity"), value / 100.0);
            pilin.insert(QStringLiteral("template"), templ);
        });
    });
    connect(layerLocked_, &QCheckBox::toggled, this, [this](bool checked) {
        if (!refreshing_) setLayerLocked(checked);
    });
    connect(layerOpacity_, &QSlider::valueChanged, this, [this](int value) {
        if (!refreshing_) setLayerOpacity(value);
    });
    connect(loadTemplate, &QPushButton::clicked, this, &PilinReyEditor::chooseTemplate);
    connect(clearTemplateButton, &QPushButton::clicked, this, &PilinReyEditor::clearTemplate);
    connect(add, &QPushButton::clicked, this, &PilinReyEditor::addLayer);
    connect(duplicate, &QPushButton::clicked, this, &PilinReyEditor::duplicateLayer);
    connect(remove, &QPushButton::clicked, this, &PilinReyEditor::removeLayer);
    connect(up, &QPushButton::clicked, this, [this]() { moveLayer(-1); });
    connect(down, &QPushButton::clicked, this, [this]() { moveLayer(1); });
    connect(undoButton_, &QPushButton::clicked, this, &PilinReyEditor::undo);
    connect(redoButton_, &QPushButton::clicked, this, &PilinReyEditor::redo);

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
}

void PilinReyEditor::setMap(const QJsonObject& map) {
    map_ = map;
    ensurePilinDocument();
    undoStack_.clear();
    redoStack_.clear();
    refreshing_ = true;
    const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
    const QString style = pilin.value(QStringLiteral("cartographicStyle")).toString(QStringLiteral("Fantasía clásica"));
    const int styleIndex = style_->findText(style);
    if (styleIndex >= 0) style_->setCurrentIndex(styleIndex);
    templateOpacity_->setValue(qRound(pilin.value(QStringLiteral("template")).toObject().value(QStringLiteral("opacity")).toDouble(0.35) * 100.0));
    refreshing_ = false;
    refreshLayers();
    refreshViewport();
}

void PilinReyEditor::ensurePilinDocument() {
    QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
    if (!pilin.contains(QStringLiteral("version"))) pilin.insert(QStringLiteral("version"), 2);
    if (!pilin.contains(QStringLiteral("width"))) pilin.insert(QStringLiteral("width"), 4096);
    if (!pilin.contains(QStringLiteral("height"))) pilin.insert(QStringLiteral("height"), 2304);
    if (!pilin.contains(QStringLiteral("cartographicStyle"))) pilin.insert(QStringLiteral("cartographicStyle"), QStringLiteral("Fantasía clásica"));

    QJsonObject templ = pilin.value(QStringLiteral("template")).toObject();
    if (!templ.contains(QStringLiteral("dataUrl"))) templ.insert(QStringLiteral("dataUrl"), map_.value(QStringLiteral("backgroundImageDataUrl")).toString());
    if (!templ.contains(QStringLiteral("opacity"))) templ.insert(QStringLiteral("opacity"), 0.35);
    if (!templ.contains(QStringLiteral("visible"))) templ.insert(QStringLiteral("visible"), true);
    if (!templ.contains(QStringLiteral("locked"))) templ.insert(QStringLiteral("locked"), true);
    pilin.insert(QStringLiteral("template"), templ);

    QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();
    if (layers.isEmpty()) {
        const QStringList names{tr("Costa"), tr("Hidrografía"), tr("Relieve"), tr("Vegetación"), tr("Fronteras"), tr("Caminos"), tr("Asentamientos"), tr("Etiquetas")};
        for (const QString& name : names) {
            layers.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("layer"))}, {QStringLiteral("name"), name}, {QStringLiteral("visible"), true}, {QStringLiteral("locked"), false}, {QStringLiteral("opacity"), 1.0}, {QStringLiteral("objects"), QJsonArray()}});
        }
        pilin.insert(QStringLiteral("layers"), layers);
        pilin.insert(QStringLiteral("activeLayerId"), layers.first().toObject().value(QStringLiteral("id")).toString());
    } else {
        for (int i = 0; i < layers.size(); ++i) {
            QJsonObject layer = layers.at(i).toObject();
            if (!layer.contains(QStringLiteral("locked"))) layer.insert(QStringLiteral("locked"), false);
            if (!layer.contains(QStringLiteral("opacity"))) layer.insert(QStringLiteral("opacity"), 1.0);
            layers.replace(i, layer);
        }
        pilin.insert(QStringLiteral("layers"), layers);
        if (!pilin.contains(QStringLiteral("activeLayerId")))
            pilin.insert(QStringLiteral("activeLayerId"), layers.first().toObject().value(QStringLiteral("id")).toString());
    }
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
        QString label = layer.value(QStringLiteral("name")).toString(tr("Capa"));
        if (layer.value(QStringLiteral("locked")).toBool(false)) label.prepend(QStringLiteral("🔒 "));
        auto* item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, layer.value(QStringLiteral("id")).toString());
        item->setData(Qt::UserRole + 1, layer.value(QStringLiteral("name")).toString(tr("Capa")));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
        item->setCheckState(layer.value(QStringLiteral("visible")).toBool(true) ? Qt::Checked : Qt::Unchecked);
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
    const bool wasRefreshing = refreshing_;
    refreshing_ = true;
    const QString active = activeLayerId();
    const QJsonArray layers = map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray();
    bool found = false;
    for (const QJsonValue value : layers) {
        const QJsonObject layer = value.toObject();
        if (layer.value(QStringLiteral("id")).toString() != active) continue;
        layerLocked_->setChecked(layer.value(QStringLiteral("locked")).toBool(false));
        layerOpacity_->setValue(qRound(layer.value(QStringLiteral("opacity")).toDouble(1.0) * 100.0));
        found = true;
        break;
    }
    layerLocked_->setEnabled(found);
    layerOpacity_->setEnabled(found);
    refreshing_ = wasRefreshing;
}

void PilinReyEditor::refreshViewport() {
    if (viewport_) viewport_->setDocument(map_);
}

void PilinReyEditor::chooseTemplate() {
    const QString path = QFileDialog::getOpenFileName(this, tr("Plantilla para Pilín Rey"), QString(), tr("Imágenes (*.png *.jpg *.jpeg *.webp)"));
    if (path.isEmpty()) return;
    const QString dataUrl = imageToDataUrl(path);
    if (dataUrl.isEmpty()) return;
    mutateDocument([&](QJsonObject& pilin) {
        QJsonObject templ = pilin.value(QStringLiteral("template")).toObject();
        templ.insert(QStringLiteral("dataUrl"), dataUrl);
        templ.insert(QStringLiteral("visible"), true);
        templ.insert(QStringLiteral("locked"), true);
        pilin.insert(QStringLiteral("template"), templ);
    });
    map_.insert(QStringLiteral("backgroundImageDataUrl"), dataUrl);
    persistToArchive();
}

void PilinReyEditor::clearTemplate() {
    mutateDocument([](QJsonObject& pilin) {
        QJsonObject templ = pilin.value(QStringLiteral("template")).toObject();
        templ.insert(QStringLiteral("dataUrl"), QString());
        pilin.insert(QStringLiteral("template"), templ);
    });
    map_.remove(QStringLiteral("backgroundImageDataUrl"));
    persistToArchive();
}

void PilinReyEditor::addLayer() {
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Nueva capa"), tr("Nombre:"), QLineEdit::Normal, tr("Nueva capa"), &ok).trimmed();
    if (!ok || name.isEmpty()) return;
    mutateDocument([&](QJsonObject& pilin) {
        QJsonArray array = pilin.value(QStringLiteral("layers")).toArray();
        const QString id = uid(QStringLiteral("layer"));
        array.append(QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("name"), name}, {QStringLiteral("visible"), true}, {QStringLiteral("locked"), false}, {QStringLiteral("opacity"), 1.0}, {QStringLiteral("objects"), QJsonArray()}});
        pilin.insert(QStringLiteral("layers"), array);
        pilin.insert(QStringLiteral("activeLayerId"), id);
    });
}

void PilinReyEditor::duplicateLayer() {
    const int row = layers_->currentRow();
    if (row < 0) return;
    mutateDocument([&](QJsonObject& pilin) {
        QJsonArray array = pilin.value(QStringLiteral("layers")).toArray();
        if (row >= array.size()) return;
        QJsonObject copy = array.at(row).toObject();
        const QString id = uid(QStringLiteral("layer"));
        copy.insert(QStringLiteral("id"), id);
        copy.insert(QStringLiteral("name"), copy.value(QStringLiteral("name")).toString(tr("Capa")) + tr(" copia"));
        copy.insert(QStringLiteral("locked"), false);
        QJsonArray objects = copy.value(QStringLiteral("objects")).toArray();
        for (int i = 0; i < objects.size(); ++i) {
            QJsonObject object = objects.at(i).toObject();
            object.insert(QStringLiteral("id"), uid(QStringLiteral("mapobj")));
            objects.replace(i, object);
        }
        copy.insert(QStringLiteral("objects"), objects);
        array.insert(row + 1, copy);
        pilin.insert(QStringLiteral("layers"), array);
        pilin.insert(QStringLiteral("activeLayerId"), id);
    });
}

void PilinReyEditor::removeLayer() {
    const int row = layers_->currentRow();
    if (row < 0) return;
    mutateDocument([&](QJsonObject& pilin) {
        QJsonArray array = pilin.value(QStringLiteral("layers")).toArray();
        if (row >= array.size() || array.size() <= 1) return;
        array.removeAt(row);
        pilin.insert(QStringLiteral("layers"), array);
        pilin.insert(QStringLiteral("activeLayerId"), array.at(qMin(row, array.size() - 1)).toObject().value(QStringLiteral("id")).toString());
    });
}

void PilinReyEditor::moveLayer(int delta) {
    const int row = layers_->currentRow();
    if (row < 0) return;
    const int target = row + delta;
    mutateDocument([&](QJsonObject& pilin) {
        QJsonArray array = pilin.value(QStringLiteral("layers")).toArray();
        if (target < 0 || target >= array.size()) return;
        const QJsonValue value = array.at(row);
        array.removeAt(row);
        array.insert(target, value);
        pilin.insert(QStringLiteral("layers"), array);
    });
    if (target >= 0 && target < layers_->count()) layers_->setCurrentRow(target);
}

void PilinReyEditor::setLayerLocked(bool locked) {
    const QString active = activeLayerId();
    if (active.isEmpty()) return;
    mutateDocument([&](QJsonObject& pilin) {
        QJsonArray array = pilin.value(QStringLiteral("layers")).toArray();
        for (int i = 0; i < array.size(); ++i) {
            QJsonObject layer = array.at(i).toObject();
            if (layer.value(QStringLiteral("id")).toString() != active) continue;
            layer.insert(QStringLiteral("locked"), locked);
            array.replace(i, layer);
            break;
        }
        pilin.insert(QStringLiteral("layers"), array);
    });
}

void PilinReyEditor::setLayerOpacity(int value) {
    const QString active = activeLayerId();
    if (active.isEmpty()) return;
    mutateDocument([&](QJsonObject& pilin) {
        QJsonArray array = pilin.value(QStringLiteral("layers")).toArray();
        for (int i = 0; i < array.size(); ++i) {
            QJsonObject layer = array.at(i).toObject();
            if (layer.value(QStringLiteral("id")).toString() != active) continue;
            layer.insert(QStringLiteral("opacity"), std::clamp(value / 100.0, 0.0, 1.0));
            array.replace(i, layer);
            break;
        }
        pilin.insert(QStringLiteral("layers"), array);
    });
}

void PilinReyEditor::setActiveTool(Tool tool) {
    viewport_->setTool(tool);
}

QString PilinReyEditor::activeLayerId() const {
    return map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("activeLayerId")).toString();
}

void PilinReyEditor::addPathObject(const QString& type, const QJsonArray& points) {
    if (type.isEmpty() || points.size() < 2) return;
    const QString active = activeLayerId();
    mutateDocument([&](QJsonObject& pilin) {
        QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();
        for (int i = 0; i < layers.size(); ++i) {
            QJsonObject layer = layers.at(i).toObject();
            if (layer.value(QStringLiteral("id")).toString() != active || layer.value(QStringLiteral("locked")).toBool(false)) continue;
            QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
            objects.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("mapobj"))}, {QStringLiteral("type"), type}, {QStringLiteral("points"), points}});
            layer.insert(QStringLiteral("objects"), objects);
            layers.replace(i, layer);
            break;
        }
        pilin.insert(QStringLiteral("layers"), layers);
    });
}

void PilinReyEditor::addSettlement(double x, double y) {
    bool ok = false;
    const QString label = QInputDialog::getText(this, tr("Asentamiento"), tr("Nombre:"), QLineEdit::Normal, tr("Poblado"), &ok).trimmed();
    if (!ok || label.isEmpty()) return;
    const QStringList kinds{tr("Capital"), tr("Ciudad"), tr("Villa"), tr("Pueblo"), tr("Aldea"), tr("Puerto"), tr("Fortaleza"), tr("Ruina")};
    const QString kind = QInputDialog::getItem(this, tr("Asentamiento"), tr("Tipo:"), kinds, 3, false, &ok);
    if (!ok) return;
    const QString active = activeLayerId();
    mutateDocument([&](QJsonObject& pilin) {
        QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();
        for (int i = 0; i < layers.size(); ++i) {
            QJsonObject layer = layers.at(i).toObject();
            if (layer.value(QStringLiteral("id")).toString() != active || layer.value(QStringLiteral("locked")).toBool(false)) continue;
            QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
            objects.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("mapobj"))}, {QStringLiteral("type"), QStringLiteral("settlement")}, {QStringLiteral("kind"), kind}, {QStringLiteral("label"), label}, {QStringLiteral("x"), x}, {QStringLiteral("y"), y}});
            layer.insert(QStringLiteral("objects"), objects);
            layers.replace(i, layer);
            break;
        }
        pilin.insert(QStringLiteral("layers"), layers);
    });
}

void PilinReyEditor::addLabel(double x, double y) {
    bool ok = false;
    const QString text = QInputDialog::getText(this, tr("Etiqueta"), tr("Texto:"), QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || text.isEmpty()) return;
    const QString active = activeLayerId();
    mutateDocument([&](QJsonObject& pilin) {
        QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();
        for (int i = 0; i < layers.size(); ++i) {
            QJsonObject layer = layers.at(i).toObject();
            if (layer.value(QStringLiteral("id")).toString() != active || layer.value(QStringLiteral("locked")).toBool(false)) continue;
            QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
            objects.append(QJsonObject{{QStringLiteral("id"), uid(QStringLiteral("mapobj"))}, {QStringLiteral("type"), QStringLiteral("label")}, {QStringLiteral("text"), text}, {QStringLiteral("x"), x}, {QStringLiteral("y"), y}});
            layer.insert(QStringLiteral("objects"), objects);
            layers.replace(i, layer);
            break;
        }
        pilin.insert(QStringLiteral("layers"), layers);
    });
}

void PilinReyEditor::pushUndo() {
    undoStack_.append(map_);
    while (undoStack_.size() > 60) undoStack_.removeFirst();
    redoStack_.clear();
}

void PilinReyEditor::mutateDocument(const std::function<void(QJsonObject&)>& mutation) {
    if (refreshing_) return;
    pushUndo();
    QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
    mutation(pilin);
    map_.insert(QStringLiteral("pilinRey"), pilin);
    refreshViewport();
    refreshLayers();
    persistToArchive();
    emit mapEdited(map_);
}

void PilinReyEditor::undo() {
    if (undoStack_.isEmpty()) return;
    redoStack_.append(map_);
    map_ = undoStack_.takeLast();
    refreshLayers();
    refreshViewport();
    persistToArchive();
    emit mapEdited(map_);
}

void PilinReyEditor::redo() {
    if (redoStack_.isEmpty()) return;
    undoStack_.append(map_);
    map_ = redoStack_.takeLast();
    refreshLayers();
    refreshViewport();
    persistToArchive();
    emit mapEdited(map_);
}

void PilinReyEditor::persistToArchive() {
    QWidget* cursor = parentWidget();
    WorldPage* world = nullptr;
    while (cursor) {
        world = qobject_cast<WorldPage*>(cursor);
        if (world) break;
        cursor = cursor->parentWidget();
    }
    if (!world || !world->document_ || !world->mapList_) return;
    const int row = world->mapList_->currentRow();
    QJsonArray maps = world->document_->array(QStringLiteral("maps"));
    if (row < 0 || row >= maps.size()) return;
    QJsonObject stored = maps.at(row).toObject();
    stored.insert(QStringLiteral("pilinRey"), map_.value(QStringLiteral("pilinRey")));
    if (map_.contains(QStringLiteral("backgroundImageDataUrl"))) stored.insert(QStringLiteral("backgroundImageDataUrl"), map_.value(QStringLiteral("backgroundImageDataUrl")));
    else stored.remove(QStringLiteral("backgroundImageDataUrl"));
    maps.replace(row, stored);
    world->document_->setArray(QStringLiteral("maps"), maps);
    emit world->changed();
}

} // namespace wbw
