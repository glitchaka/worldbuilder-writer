#include "ui/MapSelectionController.h"

#include "ui/PilinReyEditor.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPointer>
#include <QShortcut>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace wbw {
namespace {

QToolButton* command(QWidget* parent, const QString& text, const QString& tip) {
    auto* button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("pilinCommand"));
    button->setText(text);
    button->setToolTip(tip);
    button->setMinimumSize(32, 30);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

class MapSelectionFilter final : public QObject {
public:
    explicit MapSelectionFilter(QMainWindow* window) : QObject(window), window_(window) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        Q_UNUSED(watched);
        if (event->type() == QEvent::Show || event->type() == QEvent::LayoutRequest || event->type() == QEvent::ParentChange)
            QTimer::singleShot(0, this, [this]() { apply(); });
        return QObject::eventFilter(watched, event);
    }

    void apply() {
        if (!window_) return;
        auto* editor = window_->findChild<PilinReyEditor*>(QStringLiteral("pilinReyEditor"));
        auto* panel = window_->findChild<QFrame*>(QStringLiteral("pilinSelectionPopover"));
        if (!editor || !panel) return;

        if (!editor->property("wbwSelectionGeometryShortcuts").toBool()) {
            editor->setProperty("wbwSelectionGeometryShortcuts", true);
            auto makeShortcut = [editor](const QKeySequence& key, double dx, double dy) {
                auto* shortcut = new QShortcut(key, editor);
                shortcut->setContext(Qt::WidgetWithChildrenShortcut);
                QObject::connect(shortcut, &QShortcut::activated, editor, [editor, dx, dy]() { editor->nudgeSelection(dx, dy); });
            };
            makeShortcut(QKeySequence(QStringLiteral("Alt+Left")), -25.0, 0.0);
            makeShortcut(QKeySequence(QStringLiteral("Alt+Right")), 25.0, 0.0);
            makeShortcut(QKeySequence(QStringLiteral("Alt+Up")), 0.0, -25.0);
            makeShortcut(QKeySequence(QStringLiteral("Alt+Down")), 0.0, 25.0);
        }

        if (panel->property("wbwSelectionGeometryControls").toBool()) return;
        auto* layout = qobject_cast<QVBoxLayout*>(panel->layout());
        if (!layout) return;

        auto* label = new QLabel(QObject::tr("MOVER / GEOMETRÍA"), panel);
        label->setObjectName(QStringLiteral("pilinTinyLabel"));
        auto* movement = new QHBoxLayout;
        movement->setSpacing(4);
        auto* left = command(panel, QStringLiteral("←"), QObject::tr("Mover selección a la izquierda"));
        auto* up = command(panel, QStringLiteral("↑"), QObject::tr("Mover selección arriba"));
        auto* down = command(panel, QStringLiteral("↓"), QObject::tr("Mover selección abajo"));
        auto* right = command(panel, QStringLiteral("→"), QObject::tr("Mover selección a la derecha"));
        auto* shrink = command(panel, QStringLiteral("↙"), QObject::tr("Reducir geometría de trazados"));
        auto* grow = command(panel, QStringLiteral("↗"), QObject::tr("Aumentar geometría de trazados"));
        movement->addWidget(left);
        movement->addWidget(up);
        movement->addWidget(down);
        movement->addWidget(right);
        movement->addSpacing(5);
        movement->addWidget(shrink);
        movement->addWidget(grow);
        layout->addWidget(label);
        layout->addLayout(movement);

        QObject::connect(left, &QToolButton::clicked, editor, [editor]() { editor->nudgeSelection(-25.0, 0.0); });
        QObject::connect(right, &QToolButton::clicked, editor, [editor]() { editor->nudgeSelection(25.0, 0.0); });
        QObject::connect(up, &QToolButton::clicked, editor, [editor]() { editor->nudgeSelection(0.0, -25.0); });
        QObject::connect(down, &QToolButton::clicked, editor, [editor]() { editor->nudgeSelection(0.0, 25.0); });
        QObject::connect(shrink, &QToolButton::clicked, editor, [editor]() { editor->transformSelectedPathGeometry(0.9, 0.0); });
        QObject::connect(grow, &QToolButton::clicked, editor, [editor]() { editor->transformSelectedPathGeometry(1.1, 0.0); });

        panel->setProperty("wbwSelectionGeometryControls", true);
        panel->adjustSize();
    }

private:
    QPointer<QMainWindow> window_;
};

} // namespace

void installMapSelectionController(QMainWindow* window) {
    if (!window || window->property("wbwMapSelectionControllerV1").toBool()) return;
    window->setProperty("wbwMapSelectionControllerV1", true);
    auto* filter = new MapSelectionFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
