#include "ui/SettingsDialog.h"

#include <QEvent>
#include <QLabel>
#include <QMouseEvent>

namespace wbw {

bool SettingsDialog::event(QEvent* event) {
    if (event && event->type() == QEvent::Polish && !property("wbwProductDialogChrome").toBool()) {
        setProperty("wbwProductDialogChrome", true);
        setObjectName(QStringLiteral("settingsWorkspace"));

        // Keep configuration inside the product visual language. The native
        // Windows title bar was visually unrelated to the frameless main app
        // and could show the OS "No responde" chrome over the workspace.
        setWindowFlag(Qt::FramelessWindowHint, true);
        setAttribute(Qt::WA_StyledBackground, true);

        const QList<QLabel*> dragHandles{
            findChild<QLabel*>(QStringLiteral("dialogTitle")),
            findChild<QLabel*>(QStringLiteral("dialogDescription"))
        };
        for (QLabel* handle : dragHandles) {
            if (!handle) continue;
            handle->installEventFilter(this);
            handle->setCursor(Qt::SizeAllCursor);
        }
    }
    return QDialog::event(event);
}

bool SettingsDialog::eventFilter(QObject* watched, QEvent* event) {
    Q_UNUSED(watched);
    if (!event) return QDialog::eventFilter(watched, event);

    if (event->type() == QEvent::MouseButtonPress) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton) {
            dragging_ = true;
            dragOffset_ = mouse->globalPosition().toPoint() - frameGeometry().topLeft();
            return true;
        }
    } else if (event->type() == QEvent::MouseMove) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (dragging_ && (mouse->buttons() & Qt::LeftButton)) {
            move(mouse->globalPosition().toPoint() - dragOffset_);
            return true;
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton && dragging_) {
            dragging_ = false;
            return true;
        }
    }

    return QDialog::eventFilter(watched, event);
}

} // namespace wbw
