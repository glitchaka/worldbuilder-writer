#include "ui/CartographicCanvasController.h"
#include "ui/CartographicCanvasV2.h"
#include "ui/PilinReyEditor.h"

#include <QEvent>
#include <QMainWindow>
#include <QPointer>
#include <QTimer>
#include <QWidget>

namespace wbw {
namespace {

class CartographicCanvasFilter final : public QObject {
public:
    explicit CartographicCanvasFilter(QMainWindow* window) : QObject(window), window_(window) {}

    bool eventFilter(QObject*, QEvent* event) override {
        if (event->type() == QEvent::Show || event->type() == QEvent::Resize || event->type() == QEvent::LayoutRequest || event->type() == QEvent::ParentChange)
            schedule();
        return false;
    }

    void apply() {
        pending_ = false;
        if (!window_) return;
        auto* editor = window_->findChild<PilinReyEditor*>(QStringLiteral("pilinReyEditor"));
        if (!editor) return;
        auto* input = editor->findChild<QWidget*>(QStringLiteral("pilinQtViewport"));
        if (!input || !input->parentWidget()) return;

        auto* canvas = input->parentWidget()->findChild<CartographicCanvasV2*>(QStringLiteral("cartographicCanvasV2"), Qt::FindDirectChildrenOnly);
        if (!canvas) {
            canvas = new CartographicCanvasV2(editor, input, input->parentWidget());
            canvas->setObjectName(QStringLiteral("cartographicCanvasV2"));
            canvas->setGeometry(input->geometry());
            canvas->show();
            canvas->raise();
            canvas->setMap(editor->map());
            canvas->fitDocument();
            input->hide();
            QObject::connect(editor, &PilinReyEditor::mapEdited, canvas, [canvas](const QJsonObject& map) { canvas->setMap(map); });
            input->parentWidget()->installEventFilter(this);
            editor->installEventFilter(this);
        }
        canvas->setGeometry(input->geometry());
        canvas->raise();
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

void installCartographicCanvasController(QMainWindow* window) {
    if (!window || window->property("wbwCartographicCanvasV2").toBool()) return;
    window->setProperty("wbwCartographicCanvasV2", true);
    auto* filter = new CartographicCanvasFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
