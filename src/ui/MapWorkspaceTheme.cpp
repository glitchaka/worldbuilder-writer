#include "ui/MapWorkspace.h"

#include "ui/PilinReyEditor.h"

#include <QApplication>
#include <QLineEdit>

namespace wbw {
namespace {

QString mapEditorChrome(bool dark) {
    if (dark) {
        return QStringLiteral(R"QSS(
#pilinReyEditor,#pilinCanvasHost,#pilinQtViewport{background:#080b0f;}
#pilinStudioToolbar{background:#0c1117;border:0;border-bottom:1px solid #26313b;border-radius:0;}
#pilinStudioBody{background:#090d12;}
#pilinStudioLeft,#pilinStudioRight{background:#0d131a;border:0;}
#pilinStudioLeft{border-right:1px solid #26313b;}
#pilinStudioRight{border-left:1px solid #26313b;}
#pilinStudioTitle{color:#f0ebe3;}
#pilinStudioCopy,#pilinStudioValue{color:#7e8b96;}
#pilinStudioSection{color:#7c8994;}
#pilinStudioField{color:#aab5bf;}
QToolButton#pilinStudioTool{background:#111922;color:#aab6c0;border:1px solid #293641;border-radius:8px;}
QToolButton#pilinStudioTool:hover{background:#182530;color:#fff;border-color:#385064;}
QToolButton#pilinStudioTool:checked{background:#17384a;color:#9bd9e7;border-color:#3f8199;}
QToolButton#pilinStudioPanelToggle{background:transparent;color:#87949f;border:1px solid transparent;border-radius:7px;}
QToolButton#pilinStudioPanelToggle:hover{background:#17212a;color:#eef2f4;}
QToolButton#pilinStudioPanelToggle:checked{background:#142833;color:#9bd9e7;border-color:#31576a;}
#pilinStudioAction{background:#111922;color:#c7d0d8;border:1px solid #2d3944;border-radius:7px;}
#pilinStudioTabs::pane{border:0;background:#0d131a;}
#pilinStudioTabs QTabBar::tab{background:#0d131a;color:#7f8b96;border:0;border-bottom:2px solid transparent;}
#pilinStudioTabs QTabBar::tab:selected{color:#dbe6ec;border-bottom-color:#3fc1c6;}
#pilinPopover,#pilinGeneratorPopover,#pilinTerrainPopover,#pilinBiomePopover,#pilinSelectionPopover{background:#0d131a;color:#e4e0d8;border:1px solid #35414c;border-radius:10px;}
#pilinStudioRight #pilinPopover{border:0;border-radius:0;}
#pilinAssetSearch,#pilinAssetCategory{background:#101820;color:#dce4ea;border:1px solid #2b3742;border-radius:7px;}
#pilinImportAssetFolder{background:#16232d;color:#cfd9e0;border:1px solid #334554;border-radius:7px;}
QToolButton#pilinCommand{background:transparent;color:#aab4bd;border:0;border-radius:7px;}
QToolButton#pilinCommand:hover{background:#1a242d;color:#f3efe8;}
QToolButton#pilinPrimaryCommand{background:#315f7e;color:#fff;border:1px solid #477c9e;border-radius:7px;font-weight:700;}
#pilinPopoverTitle{color:#f0e9dc;}
#pilinPopoverCopy{color:#87939e;}
#pilinStatus{background:rgba(11,15,20,218);color:#88949f;border:1px solid #2b3540;border-radius:7px;}
)QSS");
    }

    return QStringLiteral(R"QSS(
#pilinReyEditor,#pilinCanvasHost,#pilinQtViewport{background:#d8d1c5;}
#pilinStudioToolbar{background:#f7f4ee;border:0;border-bottom:1px solid #d3cabd;border-radius:0;}
#pilinStudioBody{background:#d8d1c5;}
#pilinStudioLeft,#pilinStudioRight{background:#f3efe8;border:0;}
#pilinStudioLeft{border-right:1px solid #d0c6b8;}
#pilinStudioRight{border-left:1px solid #d0c6b8;}
#pilinStudioTitle{color:#342f29;}
#pilinStudioCopy,#pilinStudioValue{color:#7b7268;}
#pilinStudioSection{color:#80766a;}
#pilinStudioField{color:#625a51;}
QToolButton#pilinStudioTool{background:#fbf8f2;color:#5d564d;border:1px solid #d3c9bc;border-radius:8px;}
QToolButton#pilinStudioTool:hover{background:#eee7dd;color:#29251f;border-color:#b9a998;}
QToolButton#pilinStudioTool:checked{background:#dbe7ea;color:#315d72;border-color:#92afbd;}
QToolButton#pilinStudioPanelToggle{background:transparent;color:#71695f;border:1px solid transparent;border-radius:7px;}
QToolButton#pilinStudioPanelToggle:hover{background:#e9e2d8;color:#29251f;}
QToolButton#pilinStudioPanelToggle:checked{background:#dbe7ea;color:#315d72;border-color:#92afbd;}
#pilinStudioAction{background:#fbf8f2;color:#4a433b;border:1px solid #d4cabc;border-radius:7px;}
#pilinStudioTabs::pane{border:0;background:#f3efe8;}
#pilinStudioTabs QTabBar::tab{background:#f3efe8;color:#756d64;border:0;border-bottom:2px solid transparent;}
#pilinStudioTabs QTabBar::tab:selected{color:#315d72;border-bottom-color:#5e91a8;}
#pilinPopover,#pilinGeneratorPopover,#pilinTerrainPopover,#pilinBiomePopover,#pilinSelectionPopover{background:#f7f3ec;color:#37312b;border:1px solid #c8bdad;border-radius:10px;}
#pilinStudioRight #pilinPopover{border:0;border-radius:0;}
#pilinAssetSearch,#pilinAssetCategory{background:#fffdf9;color:#312e29;border:1px solid #d2cbc0;border-radius:7px;}
#pilinImportAssetFolder{background:#eee7dd;color:#494139;border:1px solid #cec2b3;border-radius:7px;}
QToolButton#pilinCommand{background:transparent;color:#625b52;border:0;border-radius:7px;}
QToolButton#pilinCommand:hover{background:#e9e2d8;color:#29251f;}
QToolButton#pilinPrimaryCommand{background:#557c91;color:#fff;border:1px solid #557c91;border-radius:7px;font-weight:700;}
#pilinPopoverTitle{color:#342e28;}
#pilinPopoverCopy{color:#7b7268;}
#pilinStatus{background:rgba(250,247,240,225);color:#6b635a;border:1px solid #c9beae;border-radius:7px;}
)QSS");
}

} // namespace

void MapWorkspace::applyTheme() {
    const bool dark = qApp && qApp->property("wbwDarkMode").toBool();

    // Infrastructure is created once. A theme change must never rebuild,
    // reparent or reset the cartographic editor.
    if (editor_) {
        if (!editor_->property("wbwMapInfrastructureReady").toBool()) {
            editor_->setProperty("wbwMapInfrastructureReady", true);
            editor_->activateAzgaarGenerationPipeline();
            editor_->configureProductWorkspace();
            editor_->installWorkspacePanelControls();
            editor_->installCartographicFurniture();
        } else {
            editor_->installWorkspacePanelControls();
        }
        editor_->applyMapStylePreset(editor_->map().value(QStringLiteral("style")).toString(QStringLiteral("Atlas")));

        // configureProductWorkspace contains structural QSS (sizes, padding,
        // typography and layout-specific selectors). Preserve that immutable
        // base and append only the current palette. Replacing the whole sheet
        // here was what could visually collapse the studio after toggling theme.
        constexpr const char* baseProperty = "wbwMapWorkspaceBaseStyle";
        if (!editor_->property(baseProperty).isValid())
            editor_->setProperty(baseProperty, editor_->styleSheet());
        editor_->setStyleSheet(editor_->property(baseProperty).toString() + mapEditorChrome(dark));
    }

    if (name_ && !property("wbwMapNameLive").toBool()) {
        setProperty("wbwMapNameLive", true);
        connect(name_, &QLineEdit::textEdited, this, [this](const QString&) { renameMap(); });
    }

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
)QSS"));
    }
}

} // namespace wbw
