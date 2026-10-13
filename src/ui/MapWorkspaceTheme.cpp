#include "ui/MapWorkspace.h"

#include "core/ArchiveDocument.h"
#include "ui/PilinReyEditor.h"

#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>

namespace wbw {
namespace {

QJsonObject applyCartographicPreset(QJsonObject map) {
    QJsonObject pilin = map.value(QStringLiteral("pilinRey")).toObject();
    const QString style = map.value(QStringLiteral("style")).toString(QStringLiteral("Atlas"));
    if (pilin.value(QStringLiteral("stylePreset")).toString() == style) return map;

    QJsonObject theme = pilin.value(QStringLiteral("theme")).toObject();
    if (style.compare(QObject::tr("Pergamino"), Qt::CaseInsensitive) == 0 || style.compare(QStringLiteral("Pergamino"), Qt::CaseInsensitive) == 0) {
        theme.insert(QStringLiteral("sea"), QStringLiteral("#b8b295"));
        theme.insert(QStringLiteral("river"), QStringLiteral("#546d78"));
        theme.insert(QStringLiteral("road"), QStringLiteral("#765538"));
        theme.insert(QStringLiteral("border"), QStringLiteral("#824d54"));
        theme.insert(QStringLiteral("region"), QStringLiteral("#906a49"));
        theme.insert(QStringLiteral("symbol"), QStringLiteral("#30281f"));
        theme.insert(QStringLiteral("text"), QStringLiteral("#30281f"));
    } else if (style.compare(QObject::tr("Nocturno"), Qt::CaseInsensitive) == 0 || style.compare(QStringLiteral("Nocturno"), Qt::CaseInsensitive) == 0) {
        theme.insert(QStringLiteral("sea"), QStringLiteral("#284958"));
        theme.insert(QStringLiteral("river"), QStringLiteral("#72b5d0"));
        theme.insert(QStringLiteral("road"), QStringLiteral("#b49063"));
        theme.insert(QStringLiteral("border"), QStringLiteral("#c57482"));
        theme.insert(QStringLiteral("region"), QStringLiteral("#a88563"));
        theme.insert(QStringLiteral("symbol"), QStringLiteral("#ece3d2"));
        theme.insert(QStringLiteral("text"), QStringLiteral("#eee6d7"));
    } else {
        theme.insert(QStringLiteral("sea"), QStringLiteral("#789fad"));
        theme.insert(QStringLiteral("river"), QStringLiteral("#2d6382"));
        theme.insert(QStringLiteral("road"), QStringLiteral("#75512f"));
        theme.insert(QStringLiteral("border"), QStringLiteral("#7c4650"));
        theme.insert(QStringLiteral("region"), QStringLiteral("#8e6547"));
        theme.insert(QStringLiteral("symbol"), QStringLiteral("#2c2924"));
        theme.insert(QStringLiteral("text"), QStringLiteral("#29251f"));
    }
    pilin.insert(QStringLiteral("theme"), theme);
    pilin.insert(QStringLiteral("stylePreset"), style);
    map.insert(QStringLiteral("pilinRey"), pilin);
    return map;
}

QString mapEditorChrome(bool dark) {
    if (dark) {
        return QStringLiteral(R"QSS(
#pilinReyEditor,#pilinCanvasHost,#pilinQtViewport{background:#080b0f;}
#pilinToolPalette{background:rgba(13,18,24,242);border:1px solid #2d3842;border-radius:13px;}
#pilinTopCommands{background:rgba(13,18,24,238);border:1px solid #2d3842;border-radius:11px;}
#pilinPopover,#pilinGeneratorPopover,#pilinTerrainPopover,#pilinBiomePopover,#pilinSelectionPopover,#pilinToolOptions{background:rgba(14,19,25,248);color:#e4e0d8;border:1px solid #35414c;border-radius:11px;}
QToolButton#pilinMapTool{background:transparent;color:#9ca8b4;border:0;border-radius:8px;font-size:12pt;padding:0;}
QToolButton#pilinMapTool:hover{background:#18232d;color:#f6f1e8;}
QToolButton#pilinMapTool:checked{background:#20394b;color:#9fd2f2;border:1px solid #416f8f;}
QToolButton#pilinCommand{background:transparent;color:#aab4bd;border:0;border-radius:7px;padding:7px 9px;}
QToolButton#pilinCommand:hover{background:#1a242d;color:#f3efe8;}
QToolButton#pilinPrimaryCommand{background:#315f7e;color:#ffffff;border:1px solid #477c9e;border-radius:7px;padding:7px 11px;font-weight:700;}
QToolButton#pilinPrimaryCommand:hover{background:#3b7093;}
#pilinPopoverTitle,#pilinToolOptionsLabel{font-family:'Georgia';font-size:12pt;font-weight:600;color:#f0e9dc;}
#pilinPopoverCopy{color:#87939e;font-size:8.5pt;}
#pilinStatus{background:rgba(11,15,20,218);color:#88949f;border:1px solid #2b3540;border-radius:7px;padding:6px 9px;}
#pilinPopover QListWidget,#pilinGeneratorPopover QListWidget{background:#0b1015;color:#dce3e9;border:1px solid #2d3944;border-radius:8px;outline:0;}
#pilinPopover QListWidget::item{padding:8px;border-radius:6px;}
#pilinPopover QListWidget::item:selected{background:#20384a;color:#dcefff;}
#pilinPopover QPushButton,#pilinGeneratorPopover QPushButton,#pilinTerrainPopover QPushButton,#pilinBiomePopover QPushButton{background:#121a22;color:#d1d9df;border:1px solid #303c47;border-radius:7px;padding:8px 10px;text-align:left;}
#pilinPopover QPushButton:hover,#pilinGeneratorPopover QPushButton:hover,#pilinTerrainPopover QPushButton:hover,#pilinBiomePopover QPushButton:hover{background:#1b2731;color:#ffffff;}
#pilinBiomePopover QPushButton:checked{background:#234155;color:#b8def5;border-color:#4b7997;}
#pilinPrimaryAction{background:#356f96;color:#ffffff;border-color:#4b83a6;font-weight:700;}
)QSS");
    }

    return QStringLiteral(R"QSS(
#pilinReyEditor,#pilinCanvasHost,#pilinQtViewport{background:#cfc8bb;}
#pilinToolPalette{background:rgba(250,247,240,244);border:1px solid #c9beae;border-radius:13px;}
#pilinTopCommands{background:rgba(250,247,240,242);border:1px solid #c9beae;border-radius:11px;}
#pilinPopover,#pilinGeneratorPopover,#pilinTerrainPopover,#pilinBiomePopover,#pilinSelectionPopover,#pilinToolOptions{background:rgba(252,249,243,250);color:#37312b;border:1px solid #c8bdad;border-radius:11px;}
QToolButton#pilinMapTool{background:transparent;color:#625b52;border:0;border-radius:8px;font-size:12pt;padding:0;}
QToolButton#pilinMapTool:hover{background:#e7e0d5;color:#27231f;}
QToolButton#pilinMapTool:checked{background:#d8e3e8;color:#315d72;border:1px solid #90aebe;}
QToolButton#pilinCommand{background:transparent;color:#625b52;border:0;border-radius:7px;padding:7px 9px;}
QToolButton#pilinCommand:hover{background:#e9e2d8;color:#29251f;}
QToolButton#pilinPrimaryCommand{background:#557c91;color:#ffffff;border:1px solid #557c91;border-radius:7px;padding:7px 11px;font-weight:700;}
QToolButton#pilinPrimaryCommand:hover{background:#456f86;}
#pilinPopoverTitle,#pilinToolOptionsLabel{font-family:'Georgia';font-size:12pt;font-weight:600;color:#342e28;}
#pilinPopoverCopy{color:#7b7268;font-size:8.5pt;}
#pilinStatus{background:rgba(250,247,240,225);color:#6b635a;border:1px solid #c9beae;border-radius:7px;padding:6px 9px;}
#pilinPopover QListWidget,#pilinGeneratorPopover QListWidget{background:#fffdf9;color:#37312b;border:1px solid #d0c5b7;border-radius:8px;outline:0;}
#pilinPopover QListWidget::item{padding:8px;border-radius:6px;}
#pilinPopover QListWidget::item:selected{background:#dce6ea;color:#315d72;}
#pilinPopover QPushButton,#pilinGeneratorPopover QPushButton,#pilinTerrainPopover QPushButton,#pilinBiomePopover QPushButton{background:#f4efe7;color:#4a433b;border:1px solid #d4cabc;border-radius:7px;padding:8px 10px;text-align:left;}
#pilinPopover QPushButton:hover,#pilinGeneratorPopover QPushButton:hover,#pilinTerrainPopover QPushButton:hover,#pilinBiomePopover QPushButton:hover{background:#ebe3d8;color:#29251f;}
#pilinBiomePopover QPushButton:checked{background:#dbe7ea;color:#315d72;border-color:#92afbd;}
#pilinPrimaryAction{background:#557c91;color:#ffffff;border-color:#557c91;font-weight:700;}
)QSS");
}

} // namespace

void MapWorkspace::applyTheme() {
    const bool dark = qApp && qApp->property("wbwDarkMode").toBool();

    if (editor_) {
        if (document_ && currentRow_ >= 0) {
            const QJsonArray maps = document_->array(QStringLiteral("maps"));
            if (currentRow_ < maps.size()) editor_->setMap(applyCartographicPreset(maps.at(currentRow_).toObject()));
        }
        editor_->setArchiveDocument(document_);
        editor_->migrateLegacyMarkers();
        editor_->activateAzgaarGenerationPipeline();
        editor_->installCartographicFurniture();
        editor_->setStyleSheet(editor_->styleSheet() + mapEditorChrome(dark));
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
