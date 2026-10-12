#include "ui/MapWorkflowController.h"
#include "ui/PilinReyEditor.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QPointer>
#include <QTimer>
#include <QToolButton>
#include <QWidget>

namespace wbw {
namespace {

QToolButton* findMapTool(QWidget* editor, const QString& startsWith) {
    if (!editor) return nullptr;
    for (QToolButton* button : editor->findChildren<QToolButton*>(QStringLiteral("pilinMapTool"))) {
        if (button->toolTip().startsWith(startsWith, Qt::CaseInsensitive)) return button;
    }
    return nullptr;
}

QToolButton* workflowButton(QWidget* parent, const QString& text, const QString& tip, QToolButton* target) {
    auto* button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("mapWorkflowButton"));
    button->setText(text);
    button->setToolTip(tip);
    button->setCheckable(true);
    button->setAutoExclusive(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(32);
    if (target) {
        QObject::connect(button, &QToolButton::clicked, target, &QToolButton::click);
        QObject::connect(target, &QToolButton::toggled, button, &QToolButton::setChecked);
        button->setChecked(target->isChecked());
    } else {
        button->setEnabled(false);
    }
    return button;
}

QToolButton* terrainAction(QWidget* parent, const QString& text, const QString& tip) {
    auto* button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("mapTerrainAction"));
    button->setText(text);
    button->setToolTip(tip);
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(32);
    return button;
}

class MapWorkflowFilter final : public QObject {
public:
    explicit MapWorkflowFilter(QMainWindow* window) : QObject(window), window_(window) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        if ((watched == window_ || watched == canvas_) && (event->type() == QEvent::Show || event->type() == QEvent::Resize))
            schedule();
        return false;
    }

    void apply() {
        pending_ = false;
        if (!window_) return;
        auto* editor = window_->findChild<PilinReyEditor*>(QStringLiteral("pilinReyEditor"));
        QWidget* canvas = editor ? editor->findChild<QWidget*>(QStringLiteral("pilinCanvasHost")) : nullptr;
        if (!editor || !canvas) return;
        canvas_ = canvas;

        QFrame* strip = editor->findChild<QFrame*>(QStringLiteral("mapWorkflowStrip"));
        if (!strip) {
            strip = new QFrame(canvas);
            strip->setObjectName(QStringLiteral("mapWorkflowStrip"));
            auto* row = new QHBoxLayout(strip);
            row->setContentsMargins(10, 7, 10, 7);
            row->setSpacing(4);

            auto* generate = new QToolButton(strip);
            generate->setObjectName(QStringLiteral("mapGenerateWorld"));
            generate->setText(QObject::tr("✦ Generar mundo"));
            generate->setToolTip(QObject::tr("Generar un mundo editable con heightmap, costas, biomas, regiones, ríos, rutas y asentamientos"));
            generate->setCursor(Qt::PointingHandCursor);
            generate->setMinimumHeight(32);
            QObject::connect(generate, &QToolButton::clicked, editor, &PilinReyEditor::generateProceduralWorld);
            row->addWidget(generate);
            row->addSpacing(6);

            row->addWidget(workflowButton(strip, QObject::tr("Elevar"), QObject::tr("Añadir terreno y modificar la costa"), findMapTool(editor, QObject::tr("Pintar tierra"))));
            row->addWidget(workflowButton(strip, QObject::tr("Bajar"), QObject::tr("Rebajar terreno y abrir agua"), findMapTool(editor, QObject::tr("Recortar tierra"))));
            row->addWidget(workflowButton(strip, QObject::tr("Cordillera"), QObject::tr("Elevar una cordillera sobre el heightmap"), findMapTool(editor, QObject::tr("Pincel de cordillera"))));

            auto* smooth = terrainAction(strip, QObject::tr("Suavizar"), QObject::tr("Suavizar el heightmap completo"));
            auto* erode = terrainAction(strip, QObject::tr("Erosión"), QObject::tr("Aplicar erosión al relieve"));
            auto* normalize = terrainAction(strip, QObject::tr("Normalizar"), QObject::tr("Normalizar las alturas del terreno"));
            QObject::connect(smooth, &QToolButton::clicked, editor, &PilinReyEditor::smoothTerrain);
            QObject::connect(erode, &QToolButton::clicked, editor, &PilinReyEditor::erodeTerrain);
            QObject::connect(normalize, &QToolButton::clicked, editor, &PilinReyEditor::normalizeTerrain);
            row->addWidget(smooth);
            row->addWidget(erode);
            row->addWidget(normalize);
            row->addSpacing(6);

            row->addWidget(workflowButton(strip, QObject::tr("Regiones"), QObject::tr("Dibujar regiones políticas editables"), findMapTool(editor, QObject::tr("Región"))));
            row->addWidget(workflowButton(strip, QObject::tr("Ríos"), QObject::tr("Dibujar y corregir ríos"), findMapTool(editor, QObject::tr("Río"))));
            row->addWidget(workflowButton(strip, QObject::tr("Caminos"), QObject::tr("Dibujar rutas y caminos"), findMapTool(editor, QObject::tr("Camino"))));
            row->addWidget(workflowButton(strip, QObject::tr("Bosques"), QObject::tr("Pintar masas forestales"), findMapTool(editor, QObject::tr("Pincel de bosque"))));
            row->addWidget(workflowButton(strip, QObject::tr("Símbolos"), QObject::tr("Colocar ciudades, edificios y assets"), findMapTool(editor, QObject::tr("Asset"))));
            row->addWidget(workflowButton(strip, QObject::tr("Texto"), QObject::tr("Añadir etiquetas cartográficas"), findMapTool(editor, QObject::tr("Etiqueta"))));
            row->addStretch(1);

            strip->setStyleSheet(QStringLiteral(R"QSS(
#mapWorkflowStrip{background:rgba(11,15,20,247);border:1px solid #2d3945;border-radius:10px;}
#mapGenerateWorld{background:#245f96;color:#f6fbff;border:1px solid #397fba;border-radius:7px;padding:6px 11px;font-weight:700;}
#mapGenerateWorld:hover{background:#2f78b8;}
#mapWorkflowButton,#mapTerrainAction{background:transparent;color:#aeb9c4;border:1px solid transparent;border-radius:7px;padding:5px 9px;}
#mapWorkflowButton:hover,#mapTerrainAction:hover{background:#17212b;color:#f7fbff;}
#mapWorkflowButton:checked{background:#162b40;color:#8fd0ff;border-color:#315e83;font-weight:700;}
#mapTerrainAction{border-color:#2b3947;color:#c5d2de;}
)QSS"));
            canvas->installEventFilter(this);
        }

        strip->adjustSize();
        const int left = 310;
        const int rightMargin = 14;
        const int maxWidth = qMax(320, canvas->width() - left - rightMargin);
        if (strip->width() > maxWidth) strip->resize(maxWidth, strip->height());
        strip->move(left, qMax(52, canvas->height() - strip->height() - 14));
        strip->show();
        strip->raise();
    }

private:
    void schedule() {
        if (pending_) return;
        pending_ = true;
        QTimer::singleShot(45, this, [this]() { apply(); });
    }

    QPointer<QMainWindow> window_;
    QPointer<QWidget> canvas_;
    bool pending_ = false;
};

} // namespace

void installMapWorkflowController(QMainWindow* window) {
    if (!window || window->property("wbwMapWorkflowControllerV4").toBool()) return;
    window->setProperty("wbwMapWorkflowControllerV4", true);
    auto* filter = new MapWorkflowFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
