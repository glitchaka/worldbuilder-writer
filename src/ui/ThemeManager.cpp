#include "ui/ThemeManager.h"

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QSettings>

namespace wbw {
namespace {

QPalette lightPalette() {
    QPalette p;
    p.setColor(QPalette::Window, QColor(QStringLiteral("#f2f0eb")));
    p.setColor(QPalette::WindowText, QColor(QStringLiteral("#262521")));
    p.setColor(QPalette::Base, QColor(QStringLiteral("#fbfaf7")));
    p.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#eeeae2")));
    p.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#fffdf8")));
    p.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#262521")));
    p.setColor(QPalette::Text, QColor(QStringLiteral("#262521")));
    p.setColor(QPalette::Button, QColor(QStringLiteral("#ebe7df")));
    p.setColor(QPalette::ButtonText, QColor(QStringLiteral("#34322d")));
    p.setColor(QPalette::BrightText, QColor(QStringLiteral("#9f2e25")));
    p.setColor(QPalette::Highlight, QColor(QStringLiteral("#c9a36f")));
    p.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#171612")));
    p.setColor(QPalette::Mid, QColor(QStringLiteral("#d4cec3")));
    return p;
}

QPalette darkPalette() {
    QPalette p;
    p.setColor(QPalette::Window, QColor(QStringLiteral("#0a0c0f")));
    p.setColor(QPalette::WindowText, QColor(QStringLiteral("#d9d4ca")));
    p.setColor(QPalette::Base, QColor(QStringLiteral("#111416")));
    p.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#171a1b")));
    p.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#171918")));
    p.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#f1ede4")));
    p.setColor(QPalette::Text, QColor(QStringLiteral("#ddd8cf")));
    p.setColor(QPalette::Button, QColor(QStringLiteral("#151819")));
    p.setColor(QPalette::ButtonText, QColor(QStringLiteral("#cac6be")));
    p.setColor(QPalette::BrightText, QColor(QStringLiteral("#df766b")));
    p.setColor(QPalette::Highlight, QColor(QStringLiteral("#5a4630")));
    p.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#fff7e8")));
    p.setColor(QPalette::Mid, QColor(QStringLiteral("#34352f")));
    return p;
}

QString lightStyle() {
    return QStringLiteral(R"QSS(
QWidget{font-family:'Segoe UI';font-size:9pt;}
QPushButton,QToolButton{border-radius:7px;padding:6px 9px;}
QPushButton{background:#ebe7df;color:#35322d;border:1px solid #d1c9bc;}
QPushButton:hover,QToolButton:hover{background:#e1dbd0;}
QToolButton{background:transparent;color:#4b4740;border:0;}
QLineEdit,QTextEdit,QPlainTextEdit,QComboBox,QSpinBox,QDoubleSpinBox,QKeySequenceEdit{background:#fbfaf7;color:#262521;border:1px solid #d2cbc0;border-radius:7px;padding:7px 9px;selection-background-color:#c9a36f;selection-color:#171612;}
QAbstractItemView{background:#fbfaf7;color:#262521;border:1px solid #d2cbc0;outline:0;selection-background-color:#dfd0ba;selection-color:#26211b;}
QTabWidget::pane{border:0;background:transparent;}
QTabBar::tab{background:transparent;color:#6f6a61;border:0;border-bottom:2px solid transparent;padding:9px 12px;}
QTabBar::tab:selected{color:#6d4c2e;border-bottom:2px solid #a87a47;}
QScrollBar:vertical{width:8px;background:transparent;} QScrollBar::handle:vertical{background:#bcb4a8;border-radius:4px;min-height:28px;}
QScrollBar:horizontal{height:8px;background:transparent;} QScrollBar::handle:horizontal{background:#bcb4a8;border-radius:4px;min-width:28px;}
#appTitleBar{background:#eeeae2;border-bottom:1px solid #d5cec2;}
#windowMinimize,#windowMaximize,#windowClose{background:transparent;border:0;border-radius:0;padding:0;color:#565149;}
#windowClose:hover{background:#b9382d;color:#fff;}
)QSS");
}

QString darkStyle() {
    return QStringLiteral(R"QSS(
QWidget{font-family:'Segoe UI';font-size:9pt;}
QPushButton,QToolButton{border-radius:7px;padding:6px 9px;}
QPushButton{background:#151819;color:#cac6be;border:1px solid #34352f;}
QPushButton:hover,QToolButton:hover{background:#20211f;color:#fff9ee;}
QToolButton{background:transparent;color:#c5c0b7;border:0;}
QLineEdit,QTextEdit,QPlainTextEdit,QComboBox,QSpinBox,QDoubleSpinBox,QKeySequenceEdit{background:#0d1013;color:#ddd8cf;border:1px solid #33342f;border-radius:7px;padding:7px 9px;selection-background-color:#5a4630;selection-color:#fff7e8;}
QAbstractItemView{background:#0d1013;color:#d4cfc6;border:1px solid #30312c;outline:0;selection-background-color:#28251f;selection-color:#e7c28a;}
QTabWidget::pane{border:0;background:transparent;}
QTabBar::tab{background:transparent;color:#85827b;border:0;border-bottom:2px solid transparent;padding:9px 12px;}
QTabBar::tab:hover{color:#d8d3ca;} QTabBar::tab:selected{color:#e3bd86;border-bottom:2px solid #b88a52;}
QCheckBox{spacing:7px;} QCheckBox::indicator{width:13px;height:13px;border:1px solid #555248;background:#0d1013;border-radius:3px;} QCheckBox::indicator:checked{background:#b88a52;border-color:#b88a52;}
QSlider::groove:horizontal{height:4px;background:#34342f;border-radius:2px;} QSlider::handle:horizontal{width:12px;margin:-4px 0;background:#b88a52;border-radius:6px;}
QScrollBar:vertical{width:8px;background:transparent;} QScrollBar::handle:vertical{background:#3a3933;border-radius:4px;min-height:28px;}
QScrollBar:horizontal{height:8px;background:transparent;} QScrollBar::handle:horizontal{background:#3a3933;border-radius:4px;min-width:28px;}
#appRoot,#workspaceShell,#contentShell,#pageStack{background:#0a0c0f;}
#appTitleBar{background:#080a0c;border-bottom:1px solid #25251f;}
#windowMinimize,#windowMaximize,#windowClose{background:transparent;border:0;border-radius:0;padding:0;color:#99958c;}
#windowMinimize:hover,#windowMaximize:hover{background:#1a1c1b;color:#f4efe4;} #windowClose:hover{background:#b9382d;color:#fff;}
)QSS");
}

} // namespace

ThemeManager::Mode ThemeManager::savedMode() {
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
    return settings.value(QStringLiteral("ui/theme"), QStringLiteral("dark")).toString() == QStringLiteral("dark") ? Mode::Dark : Mode::Light;
}

void ThemeManager::applySaved() { apply(savedMode()); }

void ThemeManager::apply(Mode mode) {
    if (!qApp) return;
    const bool dark = mode == Mode::Dark;
    qApp->setPalette(dark ? darkPalette() : lightPalette());
    qApp->setStyleSheet(dark ? darkStyle() : lightStyle());
    qApp->setProperty("wbwDarkMode", dark);
    // Module-specific style sheets intentionally remain local. The global theme provides
    // only neutral control defaults so it cannot repaint approved workspaces after they build.
}

void ThemeManager::saveAndApply(Mode mode) {
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
    settings.setValue(QStringLiteral("ui/theme"), mode == Mode::Dark ? QStringLiteral("dark") : QStringLiteral("light"));
    settings.sync();
    apply(mode);
}

} // namespace wbw
