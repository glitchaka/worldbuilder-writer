#include "ui/RcVisualFixture.h"

#include <QApplication>
#include <QCoreApplication>
#include <QMainWindow>
#include <QTimer>
#include <QWidget>

namespace {

void scheduleRcVisualFixture() {
    if (!QCoreApplication::arguments().contains(QStringLiteral("--rc-smoke"))) return;
    QTimer::singleShot(450, qApp, []() {
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            if (auto* window = qobject_cast<QMainWindow*>(widget)) {
                wbw::applyRcVisualFixture(window);
                return;
            }
        }
    });
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(scheduleRcVisualFixture)
