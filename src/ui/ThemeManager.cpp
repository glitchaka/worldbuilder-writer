#include "ui/ThemeManager.h"

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QSettings>
#include <QWidget>

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
)QSS");
}

QString mainSurface(bool dark) {
    if (dark) return QStringLiteral(R"QSS(
/* WBW-SURFACE-DARK */
#appRoot,#workspaceShell,#contentShell,#pageStack{background:#0b0f14;color:#d8e0e8;}
#sideRail{background:#090d12;border-right:1px solid #202934;}
#appMark{background:#111b27;color:#a9d3ff;border:1px solid #315a7e;}
#sideNavigation{background:transparent;color:#8d99a6;}
#sideNavigation::item:hover{background:#121a23;} #sideNavigation::item:selected{background:#16283b;border-left:2px solid #3f9ef6;}
#navigationGlyph{color:#9aa8b7;} #navigationLabel{color:#8d99a6;}
#topShell{background:#0c1117;border-bottom:1px solid #202a34;}
#projectTitle{color:#f0ece4;} #saveState{color:#78838e;}
#globalSearch{background:#0a1016;color:#cbd4dc;border:1px solid #273440;}
)QSS");
    return QStringLiteral(R"QSS(
/* WBW-SURFACE-LIGHT */
#appRoot,#workspaceShell,#contentShell,#pageStack{background:#f2f0eb;color:#2d2a25;}
#sideRail{background:#e9e5dd;border-right:1px solid #d0c8bc;}
#appMark{background:#f7f3eb;color:#72543a;border:1px solid #baa184;}
#sideNavigation{background:transparent;color:#6d675f;}
#sideNavigation::item:hover{background:#dfd9cf;} #sideNavigation::item:selected{background:#d8e4ec;border-left:2px solid #457aa3;}
#navigationGlyph{color:#746e65;} #navigationLabel{color:#746e65;}
#topShell{background:#f7f5f0;border-bottom:1px solid #d6cfc4;}
#projectTitle{color:#302c27;} #saveState{color:#787168;}
#globalSearch{background:#fffdf8;color:#312e29;border:1px solid #d2cbc0;}
)QSS");
}

QString titleSurface(bool dark) {
    return dark ? QStringLiteral(R"QSS(
/* WBW-TITLE-DARK */
#appTitleBar{background:#090d12;border-bottom:1px solid #202832;}
#titleBarMark{background:#132131;color:#9ed0ff;border:1px solid #315a7e;}
#titleBarBrand{color:#e7edf2;} #titleBarDivider{color:#3f4b57;} #titleBarWorkspace{color:#75818d;}
#windowMinimize,#windowMaximize,#windowClose{color:#8e99a4;background:transparent;}
)QSS") : QStringLiteral(R"QSS(
/* WBW-TITLE-LIGHT */
#appTitleBar{background:#eeeae2;border-bottom:1px solid #d5cec2;}
#titleBarMark{background:#f8f5ef;color:#745437;border:1px solid #bba384;}
#titleBarBrand{color:#302d28;} #titleBarDivider{color:#aaa196;} #titleBarWorkspace{color:#777066;}
#windowMinimize,#windowMaximize,#windowClose{color:#565149;background:transparent;}
)QSS");
}

QString librarySurface(bool dark) {
    return dark ? QStringLiteral(R"QSS(
/* WBW-LIBRARY-DARK */
#projectHubPage,#hubScroll,#hubScroll>QWidget>QWidget,#hubShell,#hubCardsHost{background:#0b0f14;color:#dfe4ea;}
#hubTitle,#projectCardTitle,#newProjectTitle{color:#f3efe8;} #hubDescription,#projectCardArchive,#newProjectCopy,#projectCardGenre,#projectSaved,#projectStatLabel{color:#7f8995;}
#hubStatusFilter,#hubSearch{background:#0f151c;color:#dfe5eb;border:1px solid #2b3440;}
#projectCard,#projectCardNew{background:#11171e;border:1px solid #2b343d;} #projectCard:hover{background:#141b23;border-color:#426b8d;}
#projectStatValue{color:#edf2f7;} #projectCardCover{background:#151e28;color:#a9bac9;}
)QSS") : QStringLiteral(R"QSS(
/* WBW-LIBRARY-LIGHT */
#projectHubPage,#hubScroll,#hubScroll>QWidget>QWidget,#hubShell,#hubCardsHost{background:#f2f0eb;color:#2f2b26;}
#hubTitle,#projectCardTitle,#newProjectTitle{color:#302b25;} #hubDescription,#projectCardArchive,#newProjectCopy,#projectCardGenre,#projectSaved,#projectStatLabel{color:#777067;}
#hubStatusFilter,#hubSearch{background:#fffdf8;color:#312e29;border:1px solid #d4cdc1;}
#projectCard,#projectCardNew{background:#fbfaf7;border:1px solid #d6cfc4;} #projectCard:hover{background:#ffffff;border-color:#a88c6d;}
#projectCardNew{border-color:#bcb3a6;} #projectCardCover{background:#e6dfd3;color:#6f6254;}
#projectStatValue{color:#342f29;} #newProjectMark{background:#f4efe7;color:#765334;border:1px solid #b08e69;}
#newProjectOption{color:#77522f;} #projectDelete{color:#80786e;} #hubEmptyState{color:#7d756b;}
)QSS");
}

QString writingSurface(bool dark) {
    return dark ? QStringLiteral(R"QSS(
/* WBW-WRITING-DARK */
#writingPage,#writingTabs,#writingEditorTab,#sceneBoardTab,#editorPanel,#sceneBoardCanvas{background:#0b1016;color:#d8dee7;}
#writingCommandBar,#sceneBoardToolbar{background:#0d141c;border-bottom:1px solid #28313c;}
#indexPanel,#sceneBoardOutlinePanel{background:#0c1219;border-right:1px solid #25303b;}
#metadataPanel{background:#0c1219;border-left:1px solid #25303b;}
#sceneEditor{background:#0b1016;color:#e6dfd5;}
#manuscriptTree,#sceneBoardOutline{background:transparent;color:#c7d0d8;}
#sceneBoardHeading{color:#eef2f4;} #sceneBoardSubheading,#sceneBoardSelection,#proofState,#wordCount{color:#75818c;}
)QSS") : QStringLiteral(R"QSS(
/* WBW-WRITING-LIGHT */
#writingPage,#writingTabs,#writingEditorTab,#sceneBoardTab,#editorPanel,#sceneBoardCanvas{background:#f4f1eb;color:#302d28;}
#writingCommandBar,#sceneBoardToolbar{background:#f8f6f1;border-bottom:1px solid #d7d0c5;}
#writingCommandBar QToolButton,#writingCommandBar QPushButton{color:#625c54;} #writingCommandBar QToolButton:hover,#writingCommandBar QPushButton:hover{background:#e9e3d9;color:#2c2925;}
#indexPanel,#sceneBoardOutlinePanel{background:#efebe4;border-right:1px solid #d5cec3;}
#metadataPanel{background:#efebe4;border-left:1px solid #d5cec3;}
#sceneEditor{background:#fffdf9;color:#302c27;selection-background-color:#d8c6ae;selection-color:#241f1a;}
#manuscriptTree,#sceneBoardOutline{background:transparent;color:#4d4841;} #manuscriptTree::item:hover,#sceneBoardOutline::item:hover{background:#e4ded5;}
#manuscriptTree::item:selected,#sceneBoardOutline::item:selected{background:#d9e6e8;color:#285d61;}
#sceneBoardHeading{color:#302b26;} #sceneBoardSubheading,#sceneBoardSelection,#proofState,#wordCount,#indexTitle{color:#777067;}
#metadataPanel QLineEdit,#metadataPanel QComboBox{background:#fffdf9;color:#312e29;border:1px solid #d2cbc0;}
#focusBar{background:#f6f2eb;border-bottom:1px solid #d5cec2;} #focusSceneName,#focusExit{color:#6f685f;}
)QSS");
}

QString worldSurface(bool dark) {
    return dark ? QStringLiteral(R"QSS(
/* WBW-WORLD-DARK */
#worldPage,#worldTabs,#atlasTab,#magicTab,#textsTab,#atlasCenterScroll,#atlasCenterEditorial,#atlasContentGrid{background:#0d1014;color:#e8ebef;}
#worldIndexPanel,#magicIndexPanel,#mapIndexPanel{background:#11161c;border-color:#252d37;}
#atlasContextScroll,#atlasContextRail{background:#11161c;} #atlasContextRail{border-left:1px solid #252d37;}
#atlasIdentityStrip,#atlasSummaryCard,#atlasEditorialSection,#atlasContextCard{background:#12171d;border:1px solid #28313b;}
#atlasEditorialTitle{color:#f2f3f5;} #atlasEditorialSubtitle,#atlasContextCopy{color:#7d8998;} #atlasEditorialKicker,#atlasContextKicker,#atlasSectionTitle{color:#c79b60;}
)QSS") : QStringLiteral(R"QSS(
/* WBW-WORLD-LIGHT */
#worldPage,#worldTabs,#atlasTab,#magicTab,#textsTab,#atlasCenterScroll,#atlasCenterEditorial,#atlasContentGrid{background:#f3f0ea;color:#302d28;}
#worldTabs::pane{background:#f3f0ea;} #worldTabs QTabBar::tab{color:#746d64;} #worldTabs QTabBar::tab:selected{color:#714f2e;border-bottom:2px solid #a67a4b;}
#worldIndexPanel,#magicIndexPanel,#mapIndexPanel{background:#ece8e0;border-color:#d5cec2;}
#worldIndexPanel QListWidget{color:#4c4741;} #worldIndexPanel QListWidget::item:hover{background:#e0dad0;} #worldIndexPanel QListWidget::item:selected{background:#d8e3e8;color:#315d72;}
#atlasContextScroll,#atlasContextRail{background:#ede9e1;} #atlasContextRail{border-left:1px solid #d5cec2;}
#atlasIdentityStrip,#atlasSummaryCard,#atlasEditorialSection,#atlasContextCard{background:#fbfaf7;border:1px solid #d6cfc4;}
#atlasEditorialTitle{color:#312c26;} #atlasEditorialSubtitle,#atlasContextCopy{color:#746e66;} #atlasEditorialKicker,#atlasContextKicker,#atlasSectionTitle{color:#8b653f;}
#atlasIdentityField QLineEdit,#atlasIdentityField QComboBox,#atlasSummaryField QTextEdit,#atlasEditorialSection QTextEdit,#atlasEditorialSection QLineEdit,#atlasEditorialSection QComboBox,#atlasContextCard QTextEdit,#atlasContextCard QLineEdit,#atlasContextCard QListWidget{background:#fffdf9;color:#312e29;border:1px solid #e1dbd1;}
#magicEditorCard,#mapEditorCard{background:#f5f2ec;border-color:#d6cfc4;}
)QSS");
}

QString reviewSurface(bool dark) {
    return dark ? QStringLiteral(R"QSS(
/* WBW-REVIEW-DARK */
#reviewPage,#reviewTabs,#reviewProjectTab,#reviewAnalysisTab,#reviewLayoutTab,#reviewMediaTab{background:#0b0f14;color:#dfe4ea;}
#reviewDiagnosis,#reviewStats,#reviewExportRail,#reviewIssueCard,#reviewCard{background:#10161d;border:1px solid #27323d;}
#reviewPageTitle,#reviewDiagnosisTitle,#reviewExportTitle,#reviewIssueTitle,#reviewCardTitle{color:#f2eee8;}
#reviewPageSubtitle,#reviewDiagnosisCopy,#reviewExportCopy{color:#8a949e;}
)QSS") : QStringLiteral(R"QSS(
/* WBW-REVIEW-LIGHT */
#reviewPage,#reviewTabs,#reviewProjectTab,#reviewAnalysisTab,#reviewLayoutTab,#reviewMediaTab{background:#f3f0ea;color:#302d28;}
#reviewSectionBar{background:#f8f5ef;border:1px solid #d6cfc4;} #reviewFilterButton{color:#746d64;} #reviewFilterButton:checked{background:#dce6ec;color:#365f78;border:1px solid #9cb6c8;}
#reviewDiagnosis,#reviewStats,#reviewExportRail,#reviewIssueCard,#reviewCard{background:#fbfaf7;border:1px solid #d6cfc4;}
#reviewPageTitle,#reviewDiagnosisTitle,#reviewExportTitle,#reviewIssueTitle,#reviewCardTitle{color:#312c26;}
#reviewPageSubtitle,#reviewDiagnosisCopy,#reviewExportCopy,#reviewSeverity{color:#777067;}
#reviewMetric{background:#f6f2eb;border:1px solid #ded6ca;} #reviewMetricValue{color:#342f29;} #reviewMetricLabel{color:#766f66;}
#reviewExportButton{background:#f3efe7;color:#4d463e;border:1px solid #d2c7b8;} #reviewIssueRow{background:#f5f1ea;color:#4c4740;}
#reviewAnalysisTab QListWidget{background:#fffdf9;color:#3a3630;border:1px solid #d7d0c5;}
)QSS");
}

QString settingsSurface(bool dark) {
    return dark ? QStringLiteral(R"QSS(
/* WBW-SETTINGS-DARK */
#settingsWorkspace,#settingsContent,#settingsWorkspacePages{background:#0b0f14;color:#d8e0e8;}
#settingsSide{background:#090d12;border-right:1px solid #202832;} #settingsCard{background:#0f151c;border:1px solid #26313d;}
#settingsWorkspaceTitle,#settingsCardTitle{color:#f1ede6;} #settingsWorkspaceCopy,#settingsCardCopy,#settingsOutputText{color:#8995a0;}
)QSS") : QStringLiteral(R"QSS(
/* WBW-SETTINGS-LIGHT */
#settingsWorkspace,#settingsContent,#settingsWorkspacePages{background:#f3f0ea;color:#302d28;}
#settingsSide{background:#e9e5dd;border-right:1px solid #d3ccc0;} #settingsWorkspaceNavigation{background:transparent;} #settingsWorkspaceNavigation::item{color:#716a61;} #settingsWorkspaceNavigation::item:selected{background:#dbe5eb;color:#315f78;border-left:2px solid #4b82aa;}
#settingsCard{background:#fbfaf7;border:1px solid #d6cfc4;} #settingsWorkspaceTitle,#settingsCardTitle{color:#302b25;} #settingsWorkspaceCopy,#settingsCardCopy,#settingsOutputText{color:#777067;}
)QSS");
}

QString mapSurface(bool dark) {
    return dark ? QStringLiteral(R"QSS(
/* WBW-MAP-DARK */
#pilinReyEditor,#pilinCanvasHost,#pilinQtViewport{background:#090d12;}
#pilinToolPalette,#pilinTopCommands,#pilinPopover,#pilinGeneratorPopover,#pilinTerrainPopover,#pilinBiomePopover,#pilinSelectionPopover,#pilinToolOptions{background:rgba(12,17,23,247);color:#d9e0e7;border-color:#2c3741;}
)QSS") : QStringLiteral(R"QSS(
/* WBW-MAP-LIGHT */
#pilinReyEditor,#pilinCanvasHost,#pilinQtViewport{background:#d8d2c8;}
#pilinToolPalette,#pilinTopCommands,#pilinPopover,#pilinGeneratorPopover,#pilinTerrainPopover,#pilinBiomePopover,#pilinSelectionPopover,#pilinToolOptions{background:rgba(250,248,243,248);color:#342f29;border:1px solid #cfc6b9;}
QToolButton#pilinMapTool{color:#625b52;} QToolButton#pilinMapTool:hover{background:#e8e2d8;color:#2d2924;} QToolButton#pilinMapTool:checked{background:#d9e5ec;color:#315e78;border:1px solid #90aec1;}
QToolButton#pilinCommand{color:#615a51;} QToolButton#pilinCommand:hover{background:#e9e3d9;color:#2e2a25;}
#pilinPopoverTitle,#pilinToolOptionsLabel{color:#312b25;} #pilinPopoverCopy{color:#746c62;}
#pilinPopover QListWidget,#pilinGeneratorPopover QListWidget{background:#fffdf9;color:#342f29;border:1px solid #d5cdc1;}
#pilinPopover QPushButton,#pilinGeneratorPopover QPushButton,#pilinTerrainPopover QPushButton,#pilinBiomePopover QPushButton{background:#f5f1ea;color:#494239;border:1px solid #d7cec1;}
#pilinStatus{background:rgba(250,248,243,235);color:#665f56;border:1px solid #d0c7bb;}
)QSS");
}

void appendSurface(const QString& objectName, const QString& block) {
    if (!qApp) return;
    for (QWidget* widget : QApplication::allWidgets()) {
        if (!widget || widget->objectName() != objectName) continue;
        QString sheet = widget->styleSheet();
        if (!sheet.endsWith(block)) widget->setStyleSheet(sheet + block);
    }
}

void applyProductSurfaces(bool dark) {
    if (!qApp) return;
    if (QWidget* root = [&]() -> QWidget* {
        for (QWidget* widget : QApplication::allWidgets())
            if (widget && widget->objectName() == QStringLiteral("appRoot")) return widget->window();
        return nullptr;
    }()) {
        const QString block = mainSurface(dark);
        QString sheet = root->styleSheet();
        if (!sheet.endsWith(block)) root->setStyleSheet(sheet + block);
    }
    appendSurface(QStringLiteral("appTitleBar"), titleSurface(dark));
    appendSurface(QStringLiteral("projectHubPage"), librarySurface(dark));
    appendSurface(QStringLiteral("writingPage"), writingSurface(dark));
    appendSurface(QStringLiteral("worldPage"), worldSurface(dark));
    appendSurface(QStringLiteral("reviewPage"), reviewSurface(dark));
    appendSurface(QStringLiteral("settingsWorkspace"), settingsSurface(dark));
    appendSurface(QStringLiteral("pilinReyEditor"), mapSurface(dark));
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
    applyProductSurfaces(dark);
}

void ThemeManager::saveAndApply(Mode mode) {
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
    settings.setValue(QStringLiteral("ui/theme"), mode == Mode::Dark ? QStringLiteral("dark") : QStringLiteral("light"));
    settings.sync();
    apply(mode);
}

} // namespace wbw
