#include "ui/MainWindow.h"
#include "ui/SettingsWorkspace.h"
#include "ui/ThemeManager.h"
#include "ui/WritingPerformanceController.h"
#include "ui/WritingShortcutBridge.h"

#include <QApplication>
#include <QCoreApplication>
#include <QMenuBar>
#include <QSettings>

namespace {

void ensureFirstRunTheme() {
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
    if (!settings.contains(QStringLiteral("ui/theme"))) {
        settings.setValue(QStringLiteral("ui/theme"), QStringLiteral("dark"));
        settings.sync();
    }
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Worldbuilder Writer"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.5.0-dev"));
    QCoreApplication::setOrganizationName(QStringLiteral("Worldbuilder Writer"));
    app.setStyle(QStringLiteral("Fusion"));

    ensureFirstRunTheme();
    wbw::ThemeManager::applySaved();

    wbw::MainWindow window;
    window.setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    window.setMenuBar(nullptr);
    if (auto* settings = window.findChild<wbw::SettingsWorkspace*>()) settings->finalizeProductControls();
    wbw::installWritingShortcutBridge(&window);
    wbw::installWritingPerformanceController(&window);
    wbw::ThemeManager::applySaved();

    window.resize(1600, 1000);
    window.show();
    return app.exec();
}
