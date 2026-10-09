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

    app.setStyleSheet(QStringLiteral(
        "QWidget{font-family:'Segoe UI';font-size:9.5pt;color:#1f2937;}"
        "QPushButton,QToolButton{background:#ffffff;color:#344054;border:1px solid #cfd6df;border-radius:2px;padding:6px 10px;min-height:18px;}"
        "QPushButton:hover,QToolButton:hover{background:#f7f9fc;border-color:#98a2b3;}"
        "QPushButton:pressed,QToolButton:pressed{background:#eef2f7;}"
        "QToolButton:checked{background:#1668d4;color:#ffffff;border-color:#1668d4;}"
        "QLineEdit,QTextEdit,QComboBox,QSpinBox,QDoubleSpinBox,QListWidget,QTreeWidget{background:#ffffff;color:#1f2937;border:1px solid #cfd6df;border-radius:2px;padding:5px;}"
        "QComboBox::drop-down,QSpinBox::up-button,QSpinBox::down-button,QDoubleSpinBox::up-button,QDoubleSpinBox::down-button{border:0;background:#f7f9fc;}"
        "QSlider::groove:horizontal{height:4px;background:#d8dee8;border-radius:2px;}"
        "QSlider::handle:horizontal{width:14px;margin:-5px 0;background:#1668d4;border:1px solid #0f5fc8;border-radius:7px;}"
        "QCheckBox{spacing:7px;}"
        "QCheckBox::indicator{width:14px;height:14px;border:1px solid #aab4c0;background:#ffffff;border-radius:2px;}"
        "QCheckBox::indicator:checked{background:#1668d4;border-color:#1668d4;}"
    ));

    wbw::MainWindow window;
    window.menuBar()->hide();
    window.show();
    return app.exec();
}
