#include "ui/SettingsWorkspace.h"

#include <QApplication>

namespace wbw {

void SettingsWorkspace::refreshProductTheme() {
    const bool dark = qApp && qApp->property("wbwDarkMode").toBool();
    if (dark) {
        setStyleSheet(QStringLiteral(R"QSS(
#settingsWorkspace,#settingsContent,#settingsWorkspacePages,#settingsWorkspacePage{background:#0b0f14;color:#dfe4ea;}
#settingsSide{background:#0d1218;border-right:1px solid #202a34;}
#settingsNavKicker{color:#8793a0;font-size:7.5pt;font-weight:700;letter-spacing:1.2px;}
#settingsWorkspaceNavigation{background:transparent;border:0;outline:0;color:#a7b0ba;}
#settingsWorkspaceNavigation::item{border:0;border-radius:8px;padding:9px 10px;margin:1px 0;}
#settingsWorkspaceNavigation::item:hover{background:#121a23;color:#eef4fa;}
#settingsWorkspaceNavigation::item:selected{background:#16283b;color:#8ecbff;border-left:2px solid #3f9ef6;}
#settingsWorkspaceTitle{font-family:'Georgia';font-size:28pt;font-weight:600;color:#f4f0e8;}
#settingsWorkspaceCopy{color:#818b96;font-size:10pt;}
#settingsPageTitle{font-family:'Georgia';font-size:19pt;font-weight:600;color:#f2eee7;}
#settingsPageCopy{color:#7f8993;font-size:9pt;}
#settingsCard{background:#10161d;border:1px solid #27323d;border-radius:12px;}
#settingsCardTitle{font-family:'Georgia';font-size:13pt;font-weight:700;color:#f2eee8;}
#settingsCardCopy,#settingsOutputText,#cloudStatus{color:#89939d;}
#settingsWorkspace QLineEdit,#settingsWorkspace QComboBox,#settingsWorkspace QSpinBox,#settingsWorkspace QKeySequenceEdit{background:#0c1218;color:#e4e9ee;border:1px solid #2d3945;border-radius:7px;padding:7px 9px;min-height:22px;}
#settingsWorkspace QLineEdit:focus,#settingsWorkspace QComboBox:focus,#settingsWorkspace QSpinBox:focus,#settingsWorkspace QKeySequenceEdit:focus{border-color:#4a91c7;}
#settingsWorkspace QCheckBox{color:#c5ccd4;spacing:8px;}
#settingsPrimary{background:#2c7fca;color:#f7fbff;border:0;border-radius:7px;padding:8px 12px;font-weight:700;}
#settingsSecondary{background:#111820;color:#c9d0d7;border:1px solid #32404d;border-radius:7px;padding:8px 11px;}
#cloudBackups{background:#0c1218;color:#dbe2e9;border:1px solid #2d3945;border-radius:8px;outline:0;}
#cloudBackups::item{padding:9px;border-bottom:1px solid #202a34;}
#cloudBackups::item:selected{background:#17283a;color:#9fd2ff;}
)QSS"));
    } else {
        setStyleSheet(QStringLiteral(R"QSS(
#settingsWorkspace,#settingsContent,#settingsWorkspacePages,#settingsWorkspacePage{background:#ece8e0;color:#342e28;}
#settingsSide{background:#f5f1ea;border-right:1px solid #d4cabd;}
#settingsNavKicker{color:#85796c;font-size:7.5pt;font-weight:700;letter-spacing:1.2px;}
#settingsWorkspaceNavigation{background:transparent;border:0;outline:0;color:#6e655c;}
#settingsWorkspaceNavigation::item{border:0;border-radius:8px;padding:9px 10px;margin:1px 0;}
#settingsWorkspaceNavigation::item:hover{background:#e8e1d6;color:#2b2621;}
#settingsWorkspaceNavigation::item:selected{background:#dce8e9;color:#315f68;border-left:2px solid #4d969c;}
#settingsWorkspaceTitle{font-family:'Georgia';font-size:28pt;font-weight:600;color:#302a24;}
#settingsWorkspaceCopy{color:#786f66;font-size:10pt;}
#settingsPageTitle{font-family:'Georgia';font-size:19pt;font-weight:600;color:#342e28;}
#settingsPageCopy{color:#7a7067;font-size:9pt;}
#settingsCard{background:#fbf8f2;border:1px solid #d4c9bb;border-radius:12px;}
#settingsCardTitle{font-family:'Georgia';font-size:13pt;font-weight:700;color:#342e28;}
#settingsCardCopy,#settingsOutputText,#cloudStatus{color:#786f66;}
#settingsWorkspace QLineEdit,#settingsWorkspace QComboBox,#settingsWorkspace QSpinBox,#settingsWorkspace QKeySequenceEdit{background:#fffdf9;color:#332e29;border:1px solid #d1c6b8;border-radius:7px;padding:7px 9px;min-height:22px;}
#settingsWorkspace QLineEdit:focus,#settingsWorkspace QComboBox:focus,#settingsWorkspace QSpinBox:focus,#settingsWorkspace QKeySequenceEdit:focus{border-color:#6d9ca7;}
#settingsWorkspace QCheckBox{color:#514a43;spacing:8px;}
#settingsPrimary{background:#557f8c;color:#fff;border:0;border-radius:7px;padding:8px 12px;font-weight:700;}
#settingsSecondary{background:#f4efe7;color:#4d453d;border:1px solid #d4cabc;border-radius:7px;padding:8px 11px;}
#cloudBackups{background:#fffdf9;color:#342e28;border:1px solid #d1c6b8;border-radius:8px;outline:0;}
#cloudBackups::item{padding:9px;border-bottom:1px solid #e5ddd3;}
#cloudBackups::item:selected{background:#dce8e9;color:#315f68;}
)QSS"));
    }
}

} // namespace wbw
