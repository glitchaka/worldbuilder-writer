#include "ui/MapMockupController.h"

#include <QCheckBox>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPointer>
#include <QSettings>
#include <QTimer>
#include <QToolButton>
#include <QWidget>

namespace wbw {
namespace {

QString popoverTitle(QFrame* frame) {
    if (!frame) return {};
    const auto labels = frame->findChildren<QLabel*>();
    for (QLabel* label : labels) {
        const QString text = label->text().trimmed();
        if (text == QObject::tr("Capas") || text == QObject::tr("Assets") ||
            text == QObject::tr("Assets y sellos") || text == QObject::tr("Plantilla") ||
            text == QObject::tr("Apariencia") || text == QObject::tr("Selección")) return text;
    }
    return {};
}

QString toolCaption(const QString& tip) {
    if (tip.startsWith(QObject::tr("Seleccionar"))) return QObject::tr("↖  Selección");
    if (tip.startsWith(QObject::tr("Mover lienzo"))) return QObject::tr("✥  Mano / mover");
    if (tip.startsWith(QObject::tr("Pintar tierra"))) return QObject::tr("▰  Tierra / costa");
    if (tip.startsWith(QObject::tr("Recortar tierra"))) return QObject::tr("◌  Mar / borrar");
    if (tip.startsWith(QObject::tr("Río"))) return QObject::tr("∿  Ríos");
    if (tip.startsWith(QObject::tr("Camino"))) return QObject::tr("━  Caminos");
    if (tip.startsWith(QObject::tr("Frontera"))) return QObject::tr("┄  Fronteras");
    if (tip.startsWith(QObject::tr("Región"))) return QObject::tr("◇  Regiones");
    if (tip.startsWith(QObject::tr("Pincel de bosque"))) return QObject::tr("♣  Bosques");
    if (tip.startsWith(QObject::tr("Pincel de cordillera"))) return QObject::tr("△  Montañas");
    if (tip.startsWith(QObject::tr("Asentamiento"))) return QObject::tr("●  Asentamientos");
    if (tip.startsWith(QObject::tr("Asset"))) return QObject::tr("✦  Assets / sellos");
    if (tip.startsWith(QObject::tr("Etiqueta"))) return QObject::tr("T  Etiquetas");
    if (tip.startsWith(QObject::tr("Medir"))) return QObject::tr("↔  Medir");
    return {};
}

void configureToolRail(QWidget* rail) {
    if (!rail || rail->property("wbwMapRailConfigured").toBool()) return;
    rail->setProperty("wbwMapRailConfigured", true);
    rail->setMinimumWidth(188);
    rail->setMaximumWidth(188);
    if (auto* grid = qobject_cast<QGridLayout*>(rail->layout())) {
        grid->setContentsMargins(8, 42, 8, 8);
        grid->setHorizontalSpacing(4);
        grid->setVerticalSpacing(3);
    }
    auto* title = new QLabel(QObject::tr("Pinceles y terreno"), rail);
    title->setObjectName(QStringLiteral("pilinPaletteTitle"));
    title->setGeometry(12, 10, 164, 24);
    title->show();
    const auto tools = rail->findChildren<QToolButton*>(QStringLiteral("pilinMapTool"));
    for (QToolButton* button : tools) {
        const QString caption = toolCaption(button->toolTip());
        if (!caption.isEmpty()) button->setText(caption);
        button->setMinimumSize(168, 30);
        button->setMaximumSize(168, 30);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    }
}

void normalizePopoverTitles(QWidget* editor) {
    if (!editor) return;
    const auto frames = editor->findChildren<QFrame*>(QStringLiteral("pilinPopover"));
    for (QFrame* frame : frames) {
        for (QLabel* label : frame->findChildren<QLabel*>()) {
            if (label->text().trimmed() == QObject::tr("Assets")) { label->setText(QObject::tr("Assets y sellos")); break; }
        }
    }
}

void applyPersistentDefaults(QWidget* editor) {
    if (!editor || editor->property("wbwMapDefaultsApplied").toBool()) return;
    editor->setProperty("wbwMapDefaultsApplied", true);
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
    const bool snap = settings.value(QStringLiteral("map/defaultSnap"), true).toBool();
    const bool grid = settings.value(QStringLiteral("map/defaultGrid"), false).toBool();
    for (QCheckBox* check : editor->findChildren<QCheckBox*>()) {
        if (check->text() == QObject::tr("Ajustar")) check->setChecked(snap);
        else if (check->text() == QObject::tr("Cuadrícula")) check->setChecked(grid);
    }
}

class MapWorkspaceFilter final : public QObject {
public:
    explicit MapWorkspaceFilter(QMainWindow* window) : QObject(window), window_(window) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        const bool windowEvent = watched == window_ && (event->type() == QEvent::Show || event->type() == QEvent::Resize);
        const bool mapShown = watched && watched->objectName() == QStringLiteral("pilinReyEditor") && event->type() == QEvent::Show;
        if (windowEvent || mapShown) QTimer::singleShot(0, this, [this]() { apply(); });
        return QObject::eventFilter(watched, event);
    }

    void apply() {
        if (!window_) return;
        QWidget* editor = window_->findChild<QWidget*>(QStringLiteral("pilinReyEditor"));
        if (!editor) return;
        QWidget* canvas = editor->findChild<QWidget*>(QStringLiteral("pilinCanvasHost"));
        QWidget* rail = editor->findChild<QWidget*>(QStringLiteral("pilinToolPalette"));
        QWidget* commands = editor->findChild<QWidget*>(QStringLiteral("pilinTopCommands"));
        if (!canvas || canvas->width() < 100 || canvas->height() < 100) return;

        configureToolRail(rail);
        normalizePopoverTitles(editor);
        applyPersistentDefaults(editor);

        if (!editor->property("wbwMapVisualStyleApplied").toBool()) {
            editor->setProperty("wbwMapVisualStyleApplied", true);
            editor->setStyleSheet(QStringLiteral(
                "#pilinReyEditor{background:#0f1114;}"
                "#pilinCanvasHost{background:#0f1114;}"
                "#pilinToolPalette,#pilinTopCommands,#pilinPopover,#pilinToolOptions{background:#181b20;border:1px solid #313640;border-radius:10px;}"
                "#pilinPaletteTitle{color:#f2f3f5;font-size:10pt;font-weight:700;}"
                "#pilinMapTool,#pilinCommand{background:transparent;color:#d9dde4;border:0;border-radius:7px;text-align:left;padding:0 9px;}"
                "#pilinCommand{padding:0 7px;}"
                "#pilinMapTool:hover,#pilinCommand:hover{background:#262b33;}"
                "#pilinMapTool:checked{background:#c59a5d;color:#111315;font-weight:700;}"
                "#pilinPopoverTitle{color:#f0f2f5;font-size:11pt;font-weight:700;}"
                "#pilinTinyLabel,#pilinToolOptionsLabel{color:#aab1bd;font-size:8pt;}"
                "#pilinToolOptions{min-width:180px;}"
                "QListWidget{background:#12151a;color:#e5e8ed;border:1px solid #2b3038;border-radius:7px;}"
                "QListWidget::item{padding:7px;border-radius:5px;}"
                "QListWidget::item:selected{background:#2b313a;color:#ffffff;}"
                "QSlider::groove:horizontal{height:4px;background:#303640;border-radius:2px;}"
                "QSlider::handle:horizontal{width:12px;margin:-4px 0;background:#c59a5d;border-radius:6px;}"
                "QCheckBox{color:#c9ced6;}"
            ));
        }

        const int margin = 14;
        if (rail) {
            rail->adjustSize();
            rail->resize(188, qMin(rail->sizeHint().height(), canvas->height() - margin * 2));
            rail->move(margin, qMax(margin, (canvas->height() - rail->height()) / 2));
            rail->raise();
        }
        if (commands) {
            commands->adjustSize();
            commands->move(qMax(margin + (rail ? rail->width() + 10 : 0), (canvas->width() - commands->width()) / 2), margin);
            commands->raise();
        }

        QFrame* assets = nullptr;
        QFrame* layers = nullptr;
        const auto frames = editor->findChildren<QFrame*>(QStringLiteral("pilinPopover"));
        for (QFrame* frame : frames) {
            const QString title = popoverTitle(frame);
            if (title == QObject::tr("Assets") || title == QObject::tr("Assets y sellos")) assets = frame;
            else if (title == QObject::tr("Capas")) layers = frame;
        }

        if (assets) {
            assets->setMinimumWidth(260);
            assets->setMaximumWidth(300);
            assets->adjustSize();
            const bool enoughRoom = canvas->width() >= 1080;
            if (enoughRoom && editor->isVisible()) assets->show();
            if (!enoughRoom && !assets->underMouse()) assets->hide();
            if (assets->isVisible()) {
                assets->move(qMax(margin, canvas->width() - assets->width() - margin), 66);
                assets->raise();
            }
        }

        if (layers && layers->isVisible()) {
            layers->setMinimumWidth(260);
            layers->setMaximumWidth(300);
            layers->adjustSize();
            layers->move(qMax(margin, canvas->width() - layers->width() - margin), 66);
            layers->raise();
        }

        int contextualY = assets && assets->isVisible() ? assets->geometry().bottom() + 10 : 66;
        for (QFrame* frame : frames) {
            if (frame == assets || frame == layers || !frame->isVisible()) continue;
            frame->adjustSize();
            frame->move(qMax(margin, canvas->width() - frame->width() - margin), contextualY);
            frame->raise();
            contextualY = frame->geometry().bottom() + 10;
        }

        if (QWidget* options = editor->findChild<QWidget*>(QStringLiteral("pilinToolOptions")); options && options->isVisible()) {
            options->adjustSize();
            const int left = margin + (rail ? rail->width() : 0) + 10;
            options->move(left, qMax(66, (canvas->height() - options->height()) / 2));
            options->raise();
        }
    }

private:
    QPointer<QMainWindow> window_;
};

} // namespace

void installMapMockupController(QMainWindow* window) {
    if (!window || window->property("wbwMapMockupController").toBool()) return;
    window->setProperty("wbwMapMockupController", true);
    auto* filter = new MapWorkspaceFilter(window);
    window->installEventFilter(filter);
    if (QWidget* editor = window->findChild<QWidget*>(QStringLiteral("pilinReyEditor"))) editor->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
