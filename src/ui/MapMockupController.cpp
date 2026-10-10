#include "ui/MapMockupController.h"

#include <QEvent>
#include <QFrame>
#include <QLabel>
#include <QMainWindow>
#include <QPointer>
#include <QTimer>
#include <QWidget>

namespace wbw {
namespace {

QString popoverTitle(QFrame* frame) {
    if (!frame) return {};
    const auto labels = frame->findChildren<QLabel*>();
    for (QLabel* label : labels) {
        const QString text = label->text().trimmed();
        if (text == QObject::tr("Capas") || text == QObject::tr("Assets") ||
            text == QObject::tr("Plantilla") || text == QObject::tr("Apariencia") ||
            text == QObject::tr("Selección")) return text;
    }
    return {};
}

class MapWorkspaceFilter final : public QObject {
public:
    explicit MapWorkspaceFilter(QMainWindow* window)
        : QObject(window), window_(window) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::Show || event->type() == QEvent::Resize ||
            event->type() == QEvent::LayoutRequest) {
            QTimer::singleShot(0, this, [this]() { apply(); });
        }
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

        editor->setStyleSheet(QStringLiteral(
            "#pilinReyEditor{background:#101214;}"
            "#pilinCanvasHost{background:#101214;}"
            "#pilinToolPalette,#pilinTopCommands,#pilinPopover,#pilinToolOptions{"
            "background:#171a1f;border:1px solid #30343c;border-radius:10px;}"
            "#pilinMapTool,#pilinCommand{background:transparent;color:#d8dde6;border:0;border-radius:7px;}"
            "#pilinMapTool:hover,#pilinCommand:hover{background:#252a32;}"
            "#pilinMapTool:checked{background:#c59a5d;color:#111315;}"
            "#pilinPopoverTitle{color:#f0f2f5;font-size:11pt;font-weight:700;}"
            "#pilinTinyLabel,#pilinToolOptionsLabel{color:#aab1bd;font-size:8pt;}"
            "QListWidget{background:#12151a;color:#e5e8ed;border:1px solid #2b3038;border-radius:7px;}"
            "QListWidget::item{padding:7px;border-radius:5px;}"
            "QListWidget::item:selected{background:#2b313a;color:#ffffff;}"
            "QSlider::groove:horizontal{height:4px;background:#303640;border-radius:2px;}"
            "QSlider::handle:horizontal{width:12px;margin:-4px 0;background:#c59a5d;border-radius:6px;}"
            "QCheckBox{color:#c9ced6;}"
        ));

        const int margin = 14;
        if (rail) {
            rail->adjustSize();
            rail->move(margin, qMax(margin, (canvas->height() - rail->height()) / 2));
            rail->raise();
        }
        if (commands) {
            commands->adjustSize();
            commands->move(qMax(margin, (canvas->width() - commands->width()) / 2), margin);
            commands->raise();
        }

        QFrame* assets = nullptr;
        QFrame* layers = nullptr;
        const auto frames = editor->findChildren<QFrame*>(QStringLiteral("pilinPopover"));
        for (QFrame* frame : frames) {
            const QString title = popoverTitle(frame);
            if (title == QObject::tr("Assets")) assets = frame;
            else if (title == QObject::tr("Capas")) layers = frame;
        }

        // The approved map reference keeps the asset library available beside the canvas.
        if (assets) {
            assets->setMinimumWidth(248);
            assets->setMaximumWidth(300);
            assets->adjustSize();
            const int x = qMax(margin, canvas->width() - assets->width() - margin);
            assets->move(x, 66);
            if (editor->isVisible()) assets->show();
            assets->raise();
        }
        // Layers remains a real contextual panel opened from the top command bar.
        if (layers && layers->isVisible()) {
            layers->adjustSize();
            layers->move(qMax(margin, canvas->width() - layers->width() - margin), 66);
            layers->raise();
        }

        // Any other open contextual card stacks under the command bar on the right.
        int contextualY = assets && assets->isVisible() ? assets->geometry().bottom() + 10 : 66;
        for (QFrame* frame : frames) {
            if (frame == assets || frame == layers || !frame->isVisible()) continue;
            frame->adjustSize();
            frame->move(qMax(margin, canvas->width() - frame->width() - margin), contextualY);
            frame->raise();
            contextualY = frame->geometry().bottom() + 10;
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
    const auto widgets = window->findChildren<QWidget*>();
    for (QWidget* widget : widgets) widget->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
