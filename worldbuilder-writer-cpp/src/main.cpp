#include "ui/MainWindow.h"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Worldbuilder Writer"));
    QCoreApplication::setOrganizationName(QStringLiteral("Worldbuilder Writer"));

    wbw::MainWindow window;
    window.show();
    return app.exec();
}
