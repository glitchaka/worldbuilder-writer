#include "ui/MainWindow.h"
#include "ui/SettingsWorkspace.h"
#include "ui/ThemeManager.h"
#include "ui/WritingPerformanceController.h"
#include "ui/WritingShortcutBridge.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QListWidget>
#include <QMenuBar>
#include <QSettings>
#include <QTimer>

namespace {

void ensureFirstRunTheme() {
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
    if (!settings.contains(QStringLiteral("ui/theme"))) {
        settings.setValue(QStringLiteral("ui/theme"), QStringLiteral("dark"));
        settings.sync();
    }
}

bool hasArgument(const QStringList& arguments, const QString& argument) {
    return arguments.contains(argument, Qt::CaseInsensitive);
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
    if (auto* settings = window.findChild<wbw::SettingsWorkspace*>()) {
        settings->finalizeProductControls();
        settings->refreshProductTheme();
        QObject::connect(settings, &wbw::SettingsWorkspace::preferencesChanged,
                         settings, &wbw::SettingsWorkspace::refreshProductTheme);
    }
    wbw::installWritingShortcutBridge(&window);
    wbw::installWritingPerformanceController(&window);
    wbw::ThemeManager::applySaved();
    window.initializeProductSurface();

    window.resize(1600, 1000);
    window.show();

    // CI-only native smoke traversal. Normal launches never enter this path.
    // It deliberately visits every product workspace so lazy map creation,
    // review restructuring, Atlas/editorial reparenting and settings surfaces
    // are constructed inside a real QApplication before the job can pass.
    if (hasArgument(app.arguments(), QStringLiteral("--smoke-all-workspaces"))) {
        if (auto* navigation = window.findChild<QListWidget*>(QStringLiteral("productNavigation"))) {
            auto* step = new QTimer(&window);
            step->setInterval(180);
            auto* row = new int(0);
            QObject::connect(step, &QTimer::timeout, &window, [navigation, step, row]() {
                if (*row >= navigation->count()) {
                    step->stop();
                    delete row;
                    QTimer::singleShot(120, qApp, &QCoreApplication::quit);
                    return;
                }
                navigation->setCurrentRow((*row)++);
                QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            });
            step->start();
        } else {
            QTimer::singleShot(0, qApp, []() { QCoreApplication::exit(2); });
        }
    }

    return app.exec();
}
