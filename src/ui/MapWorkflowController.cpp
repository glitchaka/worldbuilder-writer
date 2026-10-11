#include "ui/MapWorkflowController.h"
#include "ui/PilinReyEditor.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
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

QToolButton* makeWorkflowButton(QWidget* parent, const QString& text, const QString& tip, QToolButton* target) {
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
    } else button->setEnabled(false);
    return button;
}

class MapWorkflowFilter final : public QObject {
public:
    explicit MapWorkflowFilter(QMainWindow* window) : QObject(window), window_(window) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        Q_UNUSED(watched);
        if (event->type() == QEvent::Show || event->type() == QEvent::Resize || event->type() == QEvent::LayoutRequest) schedule();
        return QObject::eventFilter(watched, event);
    }

    void apply() {
        pending_ = false;
        if (!window_) return;
        auto* editor = window_->findChild<PilinReyEditor*>(QStringLiteral("pilinReyEditor"));
        QWidget* canvas = editor ? editor->findChild<QWidget*>(QStringLiteral("pilinCanvasHost")) : nullptr;
        if (!editor || !canvas) return;

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
            generate->setToolTip(QObject::tr("Crear heightmap, costas y red fluvial editables"));
            generate->setCursor(Qt::PointingHandCursor);
            generate->setMinimumHeight(32);
            QObject::connect(generate, &QToolButton::clicked, editor, &PilinReyEditor::generateProceduralWorld);
            row->addWidget(generate);
            row->addSpacing(6);

            row->addWidget(makeWorkflowButton(strip, QObject::tr("Terreno"), QObject::tr("Dibujar tierra y costa"), findMapTool(editor, QObject::tr("Pintar tierra"))));
            row->addWidget(makeWorkflowButton(strip, QObject::tr("Agua"), QObject::tr("Recortar costa / volver a mar"), findMapTool(editor, QObject::tr("Recortar tierra"))));
            row->addWidget(makeWorkflowButton(strip, QObject::tr("Regiones"), QObject::tr("Dibujar regiones políticas"), findMapTool(editor, QObject::tr("Región"))));
            row->addWidget(makeWorkflowButton(strip, QObject::tr("Ríos"), QObject::tr("Dibujar ríos"), findMapTool(editor, QObject::tr("Río"))));
            row->addWidget(makeWorkflowButton(strip, QObject::tr("Caminos"), QObject::tr("Dibujar caminos"), findMapTool(editor, QObject::tr("Camino"))));
            row->addWidget(makeWorkflowButton(strip, QObject::tr("Relieve"), QObject::tr("Pintar montañas"), findMapTool(editor, QObject::tr("Pincel de cordillera"))));
            row->addWidget(makeWorkflowButton(strip, QObject::tr("Bosques"), QObject::tr("Pintar bosques"), findMapTool(editor, QObject::tr("Pincel de bosque"))));
            row->addWidget(makeWorkflowButton(strip, QObject::tr("Símbolos"), QObject::tr("Colocar assets y sellos"), findMapTool(editor, QObject::tr("Asset"))));
            row->addWidget(makeWorkflowButton(strip, QObject::tr("Texto"), QObject::tr("Añadir etiquetas"), findMapTool(editor, QObject::tr("Etiqueta"))));
            row->addStretch(1);

            strip->setStyleSheet(QStringLiteral(R"QSS(
#mapWorkflowStrip{background:rgba(11,15,20,246);border:1px solid #2d3945;border-radius:10px;}
#mapGenerateWorld{background:#245f96;color:#f6fbff;border:1px solid #397fba;border-radius:7px;padding:6px 11px;font-weight:700;}
#mapGenerateWorld:hover{background:#2f78b8;}
#mapWorkflowButton{background:transparent;color:#aeb9c4;border:1px solid transparent;border-radius:7px;padding:5px 9px;}
#mapWorkflowButton:hover{background:#17212b;color:#f7fbff;}
#mapWorkflowButton:checked{background:#162b40;color:#8fd0ff;border-color:#315e83;font-weight:700;}
)QSS"));
        }

        strip->adjustSize();
        const int left = 224;
        const int rightMargin = 14;
        const int maxWidth = qMax(300, canvas->width() - left - rightMargin);
        if (strip->width() > maxWidth) strip->resize(maxWidth, strip->height());
        strip->move(left, qMax(52, canvas->height() - strip->height() - 14));
        strip->show();
        strip->raise();
    }

private:
    void schedule() {
        if (pending_) return;
        pending_ = true;
        QTimer::singleShot(0, this, [this]() { apply(); });
    }
    QPointer<QMainWindow> window_;
    bool pending_ = false;
};

} // namespace

void installMapWorkflowController(QMainWindow* window) {
    if (!window || window->property("wbwMapWorkflowControllerV2").toBool()) return;
    window->setProperty("wbwMapWorkflowControllerV2", true);
    auto* filter = new MapWorkflowFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
