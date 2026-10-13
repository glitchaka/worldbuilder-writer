#include "ui/MapWorkspace.h"
#include "ui/PilinReyEditor.h"

#include <QApplication>
#include <QLineEdit>

namespace wbw {

void MapWorkspace::applyTheme() {
    if (editor_) {
        editor_->setArchiveDocument(document_);
        editor_->migrateLegacyMarkers();
        editor_->installCartographicFurniture();
    }

    if (name_ && !property("wbwMapNameLive").toBool()) {
        setProperty("wbwMapNameLive", true);
        connect(name_, &QLineEdit::textEdited, this, [this](const QString&) { renameMap(); });
    }

    const bool dark = qApp && qApp->property("wbwDarkMode").toBool();
    if (dark) {
        setStyleSheet(QStringLiteral(R"QSS(
#mapWorkspace{background:#090d12;color:#dce3e9;}
#mapWorkspaceChrome{background:#0d1218;border-bottom:1px solid #242c35;}
#mapWorkspaceKicker{color:#ba925f;font-size:7.5pt;font-weight:700;letter-spacing:1.2px;padding:0 4px;}
#mapWorkspacePicker{background:#111820;color:#e6e0d7;border:1px solid #303a44;border-radius:7px;padding:5px 8px;}
#mapWorkspaceName{background:transparent;color:#f0ebe3;border:0;border-left:1px solid #2a333c;border-radius:0;padding:5px 10px;font-family:'Georgia';font-size:10.5pt;}
#mapWorkspaceChromeButton,#mapWorkspaceTextButton{background:transparent;color:#a7b1bb;border:0;border-radius:7px;padding:6px 8px;}
#mapWorkspaceChromeButton:hover,#mapWorkspaceTextButton:hover{background:#18212a;color:#ffffff;}
#mapWorkspaceHint{color:#697581;font-size:8pt;}
#mapWorkspaceCanvasHost{background:#090d12;}
#mapWorkspaceEmpty{background:#0b1016;border:0;}
#mapWorkspaceEmptyMark{color:#6f98b8;font-size:42pt;}
#mapWorkspaceEmptyTitle{color:#f0ebe3;font-family:'Georgia';font-size:22pt;font-weight:600;}
#mapWorkspaceEmptyCopy{color:#7e8994;font-size:9.5pt;}
#mapWorkspacePrimary{background:#2e78b3;color:#ffffff;border:0;border-radius:8px;padding:9px 18px;font-weight:700;}
#mapWorkspacePrimary:hover{background:#3788c8;}
)QSS"));
    } else {
        setStyleSheet(QStringLiteral(R"QSS(
#mapWorkspace{background:#e9e4da;color:#302c27;}
#mapWorkspaceChrome{background:#f7f4ee;border-bottom:1px solid #d3cabd;}
#mapWorkspaceKicker{color:#8b6338;font-size:7.5pt;font-weight:700;letter-spacing:1.2px;padding:0 4px;}
#mapWorkspacePicker{background:#fffdf9;color:#332f2a;border:1px solid #d2c9bc;border-radius:7px;padding:5px 8px;}
#mapWorkspaceName{background:transparent;color:#312c26;border:0;border-left:1px solid #d4cabd;border-radius:0;padding:5px 10px;font-family:'Georgia';font-size:10.5pt;}
#mapWorkspaceChromeButton,#mapWorkspaceTextButton{background:transparent;color:#655f57;border:0;border-radius:7px;padding:6px 8px;}
#mapWorkspaceChromeButton:hover,#mapWorkspaceTextButton:hover{background:#e6dfd4;color:#29251f;}
#mapWorkspaceHint{color:#837a6e;font-size:8pt;}
#mapWorkspaceCanvasHost{background:#d8d1c5;}
#mapWorkspaceEmpty{background:#efebe3;border:0;}
#mapWorkspaceEmptyMark{color:#6d8da3;font-size:42pt;}
#mapWorkspaceEmptyTitle{color:#342f29;font-family:'Georgia';font-size:22pt;font-weight:600;}
#mapWorkspaceEmptyCopy{color:#746d64;font-size:9.5pt;}
#mapWorkspacePrimary{background:#487b9b;color:#ffffff;border:0;border-radius:8px;padding:9px 18px;font-weight:700;}
#mapWorkspacePrimary:hover{background:#3c6e8d;}
)QSS"));
    }
}

} // namespace wbw
