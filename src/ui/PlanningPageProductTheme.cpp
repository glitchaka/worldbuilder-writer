#include "ui/PlanningPage.h"

#include <QApplication>

namespace wbw {

void PlanningPage::refreshProductTheme() {
    const bool dark = qApp && qApp->property("wbwDarkMode").toBool();
    if (dark) {
        setStyleSheet(QStringLiteral(R"QSS(
#planningPage,#planningTabs::pane,#planningCharacters,#planningBoard,#planningTheories,#planningTimeline{background:#0b1016;color:#d9e1e8;border:0;}
#planningTabs QTabBar::tab{background:transparent;color:#7d8995;border:0;border-bottom:2px solid transparent;padding:10px 16px;margin:0 2px;}
#planningTabs QTabBar::tab:hover{color:#dce4ea;background:#111922;}
#planningTabs QTabBar::tab:selected{color:#7bd9dc;border-bottom-color:#39c2c6;background:transparent;}
#planningHero{background:#0d141b;border:1px solid #27323d;border-radius:10px;}
#planningKicker,#planningField,#planningIndexTitle{color:#788692;font-size:8pt;font-weight:700;letter-spacing:.7px;}
#planningTitle{font-family:'Georgia';font-size:24pt;color:#f0ebe3;font-weight:600;}
#planningDescription{font-family:'Georgia';color:#87939e;}
#planningIndex,#planningEditor,#planningRelationPanel{background:#0d131a;border:1px solid #26313b;border-radius:9px;}
#planningCard{background:#10171e;border:1px solid #29343f;border-radius:9px;}
#planningCardTitle{font-family:'Georgia';font-size:14pt;color:#ece6de;font-weight:600;}
#characterImage{background:#111820;border:1px solid #303c47;color:#788692;border-radius:8px;}
#planningPrimary{background:#2e718e;color:#fff;border:1px solid #3e819d;border-radius:7px;padding:7px 10px;font-weight:600;}
#planningPage QLineEdit,#planningPage QTextEdit,#planningPage QComboBox,#planningPage QSpinBox,#planningPage QListWidget{background:#0f161e;color:#d8e0e6;border:1px solid #2b3742;border-radius:7px;selection-background-color:#2e5f72;selection-color:#fff;}
#planningPage QLineEdit,#planningPage QComboBox,#planningPage QSpinBox{padding:7px 9px;}
#planningPage QTextEdit{padding:8px;}
#planningPage QListWidget{outline:0;}
#planningPage QListWidget::item{padding:8px 7px;border-bottom:1px solid #202a33;}
#planningPage QListWidget::item:hover{background:#16212a;}
#planningPage QListWidget::item:selected{background:#183442;color:#8edee1;border-left:2px solid #3fc3c8;}
#planningPage QPushButton{background:#111922;color:#c8d1d9;border:1px solid #2d3944;border-radius:7px;padding:7px 10px;}
#planningPage QPushButton:hover{background:#1a2731;color:#fff;border-color:#3d5262;}
#planningPage QSplitter::handle{background:#0b1016;}
)QSS"));
    } else {
        setStyleSheet(QStringLiteral(R"QSS(
#planningPage,#planningTabs::pane,#planningCharacters,#planningBoard,#planningTheories,#planningTimeline{background:#ece8e0;color:#332e29;border:0;}
#planningTabs QTabBar::tab{background:transparent;color:#796f65;border:0;border-bottom:2px solid transparent;padding:10px 16px;margin:0 2px;}
#planningTabs QTabBar::tab:hover{color:#2d2924;background:#e7e0d5;}
#planningTabs QTabBar::tab:selected{color:#315f68;border-bottom-color:#4d969c;background:transparent;}
#planningHero{background:#f5f1ea;border:1px solid #d4cabd;border-radius:10px;}
#planningKicker,#planningField,#planningIndexTitle{color:#83776a;font-size:8pt;font-weight:700;letter-spacing:.7px;}
#planningTitle{font-family:'Georgia';font-size:24pt;color:#302a24;font-weight:600;}
#planningDescription{font-family:'Georgia';color:#776d63;}
#planningIndex,#planningEditor,#planningRelationPanel{background:#f8f5ef;border:1px solid #d3c8ba;border-radius:9px;}
#planningCard{background:#fbf8f2;border:1px solid #d5cbbf;border-radius:9px;}
#planningCardTitle{font-family:'Georgia';font-size:14pt;color:#342e28;font-weight:600;}
#characterImage{background:#eee8de;border:1px solid #d4c8b9;color:#7c7268;border-radius:8px;}
#planningPrimary{background:#557f8c;color:#fff;border:1px solid #557f8c;border-radius:7px;padding:7px 10px;font-weight:600;}
#planningPage QLineEdit,#planningPage QTextEdit,#planningPage QComboBox,#planningPage QSpinBox,#planningPage QListWidget{background:#fffdf9;color:#332e29;border:1px solid #d1c6b8;border-radius:7px;selection-background-color:#cfe1e3;selection-color:#27383b;}
#planningPage QLineEdit,#planningPage QComboBox,#planningPage QSpinBox{padding:7px 9px;}
#planningPage QTextEdit{padding:8px;}
#planningPage QListWidget{outline:0;}
#planningPage QListWidget::item{padding:8px 7px;border-bottom:1px solid #e5ddd3;}
#planningPage QListWidget::item:hover{background:#eee7dd;}
#planningPage QListWidget::item:selected{background:#dce8e9;color:#315f68;border-left:2px solid #4d969c;}
#planningPage QPushButton{background:#f4efe7;color:#4d453d;border:1px solid #d4cabc;border-radius:7px;padding:7px 10px;}
#planningPage QPushButton:hover{background:#e8dfd4;color:#29251f;border-color:#bdaF9f;}
#planningPage QSplitter::handle{background:#ece8e0;}
)QSS"));
    }
}

} // namespace wbw
