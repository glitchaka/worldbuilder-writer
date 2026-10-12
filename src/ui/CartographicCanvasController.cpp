#include "ui/CartographicCanvasController.h"
#include "ui/CartographicCanvasV3.h"
#include "ui/PilinReyEditor.h"

#include <QEvent>
#include <QJsonObject>
#include <QMainWindow>
#include <QPointer>
#include <QTimer>
#include <QWidget>

namespace wbw {
namespace {

class CartographicCanvasFilter final : public QObject {
public:
    explicit CartographicCanvasFilter(QMainWindow* window) : QObject(window), window_(window) {
        renderTimer_.setSingleShot(true);
        renderTimer_.setInterval(180);
        connect(&renderTimer_, &QTimer::timeout, this, [this]() {
            if (!canvas_ || !editor_ || !canvas_->isVisible()) {
                pendingMap_ = QJsonObject();
                return;
            }
            if (pendingMap_.isEmpty()) pendingMap_ = editor_->map();
            canvas_->setMap(pendingMap_);
            pendingMap_ = QJsonObject();
        });
    }

    bool eventFilter(QObject*, QEvent* event) override {
        if (event->type() == QEvent::Show || event->type() == QEvent::Resize)
            scheduleLayout();
        return false;
    }

    void apply() {
        layoutPending_ = false;
        if (!window_) return;

        auto* editor = window_->findChild<PilinReyEditor*>(QStringLiteral("pilinReyEditor"));
        if (!editor) return;
        editor_ = editor;

        auto* input = editor->findChild<QWidget*>(QStringLiteral("pilinQtViewport"));
        if (!input || !input->parentWidget()) return;

        auto* canvas = input->parentWidget()->findChild<CartographicCanvasV3*>(QStringLiteral("cartographicCanvasV3"), Qt::FindDirectChildrenOnly);
        if (!canvas) {
            if (QWidget* old = input->parentWidget()->findChild<QWidget*>(QStringLiteral("cartographicCanvasV2"), Qt::FindDirectChildrenOnly))
                old->deleteLater();

            canvas = new CartographicCanvasV3(editor, input, input->parentWidget());
            canvas->setObjectName(QStringLiteral("cartographicCanvasV3"));
            canvas->setGeometry(input->geometry());
            canvas->show();
            canvas->raise();
            canvas->setMap(editor->map());
            canvas->fitDocument();
            input->hide();
            canvas_ = canvas;

            QObject::connect(editor, &PilinReyEditor::mapEdited, this, [this](const QJsonObject& map) {
                // El mapa completo puede ser grande. Si el módulo no está visible no mantenemos
                // snapshots intermedios. Cuando vuelve a mostrarse se toma el estado actual.
                if (!canvas_ || !canvas_->isVisible()) {
                    pendingMap_ = QJsonObject();
                    renderTimer_.stop();
                    return;
                }
                pendingMap_ = map;
                renderTimer_.start();
            });

            input->parentWidget()->installEventFilter(this);
        } else {
            canvas_ = canvas;
        }

        canvas_->setGeometry(input->geometry());
        canvas_->raise();

        if (canvas_->isVisible()) {
            pendingMap_ = editor->map();
            renderTimer_.start();
        }
    }

private:
    void scheduleLayout() {
        if (layoutPending_) return;
        layoutPending_ = true;
        QTimer::singleShot(0, this, [this]() { apply(); });
    }

    QPointer<QMainWindow> window_;
    QPointer<PilinReyEditor> editor_;
    QPointer<CartographicCanvasV3> canvas_;
    QTimer renderTimer_;
    QJsonObject pendingMap_;
    bool layoutPending_ = false;
};

} // namespace

void installCartographicCanvasController(QMainWindow* window) {
    if (!window || window->property("wbwCartographicCanvasV5").toBool()) return;
    window->setProperty("wbwCartographicCanvasV5", true);
    auto* filter = new CartographicCanvasFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
