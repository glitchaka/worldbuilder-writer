#include "ui/MainWindow.h"

#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QMenuBar>
#include <QPalette>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Worldbuilder Writer"));
    QCoreApplication::setOrganizationName(QStringLiteral("Worldbuilder Writer"));

    app.setStyle(QStringLiteral("Fusion"));
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#edf1f5")));
    palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#1f2937")));
    palette.setColor(QPalette::Base, QColor(QStringLiteral("#ffffff")));
    palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#f7f9fc")));
    palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#ffffff")));
    palette.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#1f2937")));
    palette.setColor(QPalette::Text, QColor(QStringLiteral("#1f2937")));
    palette.setColor(QPalette::Button, QColor(QStringLiteral("#ffffff")));
    palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#344054")));
    palette.setColor(QPalette::BrightText, QColor(QStringLiteral("#b42318")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#dceafe")));
    palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#175cd3")));
    app.setPalette(palette);

    wbw::MainWindow window;
    window.menuBar()->hide();
    window.show();
    return app.exec();
}
