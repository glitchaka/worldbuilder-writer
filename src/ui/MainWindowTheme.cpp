#include "ui/MainWindow.h"

#include "ui/MapWorkspace.h"
#include "ui/SettingsWorkspace.h"
#include "ui/WritingPage.h"

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QWidget>

namespace wbw {

void MainWindow::refreshProductTheme() {
    if (settingsPage_ && !property("wbwProductThemeConnected").toBool()) {
        setProperty("wbwProductThemeConnected", true);
        connect(settingsPage_, &SettingsWorkspace::preferencesChanged, this, [this]() {
            refreshProductTheme();
            if (writingPage_) writingPage_->refreshProductChromeTheme();
            if (mapPage_) mapPage_->applyTheme();
        });
    }

    const bool dark = qApp && qApp->property("wbwDarkMode").toBool();
    QWidget* shell = findChild<QWidget*>(QStringLiteral("topShell"));
    if (!shell) shell = findChild<QWidget*>(QStringLiteral("productTopBar"));
    if (!shell) return;

    shell->setStyleSheet(dark ? QStringLiteral(R"QSS(
#topShell,#productTopBar{background:#0d1218;border-bottom:1px solid #252f39;}
#appMark{background:#111b27;color:#a9d3ff;border:1px solid #315a7e;border-radius:7px;font-family:'Georgia';font-size:10pt;font-weight:700;}
#appBrand{color:#eee8df;font-family:'Georgia';font-size:10.5pt;font-weight:600;}
#topDivider{color:#303943;}
#productNavigation{background:transparent;border:0;outline:0;color:#929da7;font-size:9pt;}
#productNavigation::item{background:transparent;border:0;border-bottom:2px solid transparent;border-radius:0;padding:0 10px;margin:0 1px;}
#productNavigation::item:hover{background:#121a22;color:#dbe3e9;}
#productNavigation::item:selected{background:transparent;color:#60d7d8;border-bottom:2px solid #32c7c9;}
#globalSearch{background:#0a1016;color:#cbd4dc;border:1px solid #2a3540;border-radius:8px;padding:7px 11px;}
#globalSearch:focus{border-color:#3d738f;background:#0d151d;}
#saveState{color:#71808c;font-size:8pt;padding:0 5px;}
#windowMinimize,#windowMaximize,#windowClose{background:transparent;color:#8f99a3;border:0;border-radius:0;padding:0;font-size:10pt;}
#windowMinimize:hover,#windowMaximize:hover{background:#18212a;color:#ffffff;}
#windowClose:hover{background:#a83d45;color:#ffffff;}
)QSS") : QStringLiteral(R"QSS(
#topShell,#productTopBar{background:#f7f5f0;border-bottom:1px solid #d6cfc4;}
#appMark{background:#eef3f5;color:#416f86;border:1px solid #9fb7c4;border-radius:7px;font-family:'Georgia';font-size:10pt;font-weight:700;}
#appBrand{color:#302d28;font-family:'Georgia';font-size:10.5pt;font-weight:600;}
#topDivider{color:#d4ccc0;}
#productNavigation{background:transparent;border:0;outline:0;color:#746e65;font-size:9pt;}
#productNavigation::item{background:transparent;border:0;border-bottom:2px solid transparent;border-radius:0;padding:0 10px;margin:0 1px;}
#productNavigation::item:hover{background:#ebe5db;color:#2e2a25;}
#productNavigation::item:selected{background:transparent;color:#2c7478;border-bottom:2px solid #3a9598;}
#globalSearch{background:#fffdf8;color:#312e29;border:1px solid #d2cbc0;border-radius:8px;padding:7px 11px;}
#globalSearch:focus{border-color:#789bab;background:#ffffff;}
#saveState{color:#787168;font-size:8pt;padding:0 5px;}
#windowMinimize,#windowMaximize,#windowClose{background:transparent;color:#625d56;border:0;border-radius:0;padding:0;font-size:10pt;}
#windowMinimize:hover,#windowMaximize:hover{background:#e7e1d8;color:#29251f;}
#windowClose:hover{background:#b84950;color:#ffffff;}
)QSS"));
}

} // namespace wbw
