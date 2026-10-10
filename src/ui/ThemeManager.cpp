#include "ui/ThemeManager.h"

#include <QApplication>
#include <QColor>
#include <QListWidget>
#include <QPalette>
#include <QSettings>
#include <QSize>
#include <QVariant>
#include <QWidget>

namespace wbw {
namespace {

QPalette lightPalette() {
    QPalette p;
    p.setColor(QPalette::Window, QColor(QStringLiteral("#eef1f5")));
    p.setColor(QPalette::WindowText, QColor(QStringLiteral("#202630")));
    p.setColor(QPalette::Base, QColor(QStringLiteral("#ffffff")));
    p.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#f7f8fa")));
    p.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#ffffff")));
    p.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#202630")));
    p.setColor(QPalette::Text, QColor(QStringLiteral("#202630")));
    p.setColor(QPalette::Button, QColor(QStringLiteral("#ffffff")));
    p.setColor(QPalette::ButtonText, QColor(QStringLiteral("#344054")));
    p.setColor(QPalette::BrightText, QColor(QStringLiteral("#b42318")));
    p.setColor(QPalette::Highlight, QColor(QStringLiteral("#dbeafe")));
    p.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#175cd3")));
    p.setColor(QPalette::Mid, QColor(QStringLiteral("#d6dce5")));
    return p;
}

QPalette darkPalette() {
    QPalette p;
    p.setColor(QPalette::Window, QColor(QStringLiteral("#0f1218")));
    p.setColor(QPalette::WindowText, QColor(QStringLiteral("#e8edf4")));
    p.setColor(QPalette::Base, QColor(QStringLiteral("#151922")));
    p.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#1a202b")));
    p.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#1b202a")));
    p.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#f7f9fc")));
    p.setColor(QPalette::Text, QColor(QStringLiteral("#e8edf4")));
    p.setColor(QPalette::Button, QColor(QStringLiteral("#171c25")));
    p.setColor(QPalette::ButtonText, QColor(QStringLiteral("#dce3ed")));
    p.setColor(QPalette::BrightText, QColor(QStringLiteral("#ff8a80")));
    p.setColor(QPalette::Highlight, QColor(QStringLiteral("#1b3b59")));
    p.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#ffffff")));
    p.setColor(QPalette::Mid, QColor(QStringLiteral("#2b3441")));
    return p;
}

QString lightStyle() {
    return QStringLiteral(
        "QWidget{font-family:'Segoe UI';font-size:9pt;color:#202630;}"
        "QPushButton{background:transparent;color:#344054;border:0;border-radius:5px;padding:6px 9px;min-height:20px;}"
        "QPushButton:hover{background:#e9edf3;}QPushButton:pressed{background:#dde3eb;}QPushButton:disabled{color:#9aa4b2;}"
        "QToolButton{background:transparent;color:#344054;border:0;border-radius:5px;padding:5px;min-height:20px;}"
        "QToolButton:hover{background:#e9edf3;}QToolButton:checked{background:#dcecff;color:#175cd3;}"
        "QLineEdit,QTextEdit,QPlainTextEdit{background:#ffffff;color:#202630;border:1px solid #d3dae4;border-radius:5px;padding:6px 8px;selection-background-color:#dbeafe;selection-color:#175cd3;}"
        "QLineEdit:focus,QTextEdit:focus,QPlainTextEdit:focus{border-color:#7caee8;}"
        "QComboBox,QSpinBox,QDoubleSpinBox{background:#ffffff;color:#202630;border:1px solid #d3dae4;border-radius:5px;padding:5px 9px;}"
        "QComboBox::drop-down{width:0;border:0;}QComboBox::down-arrow{image:none;width:0;height:0;}"
        "QSpinBox::up-button,QSpinBox::down-button,QDoubleSpinBox::up-button,QDoubleSpinBox::down-button{width:0;height:0;border:0;}"
        "QAbstractItemView{background:#ffffff;color:#202630;border:1px solid #d3dae4;outline:0;selection-background-color:#e4effc;selection-color:#175cd3;}"
        "QListWidget,QTreeWidget{background:#ffffff;color:#202630;border:0;padding:2px;outline:0;}"
        "QListWidget::item,QTreeWidget::item{border:0;border-radius:4px;padding:6px 8px;}QListWidget::item:hover,QTreeWidget::item:hover{background:#eef2f6;}QListWidget::item:selected,QTreeWidget::item:selected{background:#e2edfb;color:#175cd3;}"
        "QTabWidget::pane{border:0;background:transparent;}QTabBar::tab{background:transparent;color:#667085;border:0;border-bottom:2px solid transparent;padding:9px 12px;margin-right:3px;}QTabBar::tab:selected{color:#175cd3;border-bottom:2px solid #2f75c8;}QTabBar::tab:hover{color:#202630;}"
        "QSlider::groove:horizontal{height:3px;background:#d5dbe4;border-radius:1px;}QSlider::handle:horizontal{width:12px;margin:-5px 0;background:#2f75c8;border:0;border-radius:6px;}"
        "QCheckBox{spacing:6px;}QCheckBox::indicator{width:13px;height:13px;border:1px solid #aeb7c3;background:#ffffff;border-radius:3px;}QCheckBox::indicator:checked{background:#2f75c8;border-color:#2f75c8;}"
        "QScrollBar:vertical{width:8px;background:transparent;}QScrollBar::handle:vertical{background:#c4ccd7;border-radius:4px;min-height:28px;}QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
        "QScrollBar:horizontal{height:8px;background:transparent;}QScrollBar::handle:horizontal{background:#c4ccd7;border-radius:4px;min-width:28px;}QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{width:0;}"
        "#appTitleBar{background:#f8fafc;border-bottom:1px solid #dce2e9;}"
        "#workspaceShell,#contentShell,#appRoot,#pageStack{background:#eef1f5;}"
        "#sideRail{background:#f8fafc;border-right:1px solid #dce2e9;}#topShell{background:#ffffff;border-bottom:1px solid #dce2e9;}"
        "#appMark{background:#1c293d;color:#ffffff;border-radius:7px;font-family:'Georgia';font-size:10pt;font-weight:700;}#appName{color:#243247;font-weight:700;}#appMode{color:#8d98a7;font-size:7pt;font-weight:700;letter-spacing:1px;}"
        "QListWidget#sideNavigation{background:transparent;border:0;padding:0;}QListWidget#sideNavigation::item{background:transparent;color:#596579;border:0;border-left:3px solid transparent;border-radius:5px;padding:9px 10px;margin:1px 0;}QListWidget#sideNavigation::item:hover{background:#edf2f7;color:#243247;}QListWidget#sideNavigation::item:selected{background:#e4effb;color:#175cd3;border-left:3px solid #2f75c8;font-weight:600;}"
        "#projectTitle{color:#263244;font-weight:700;font-size:9.5pt;}#saveState{color:#7d8998;font-size:8pt;}"
        "#windowMinimize,#windowMaximize,#windowClose{border:0;border-radius:0;background:transparent;padding:0;min-height:0;color:#596273;font-size:11pt;}#windowMinimize:hover,#windowMaximize:hover{background:#e9edf3;}#windowClose:hover{background:#c42b1c;color:#ffffff;}"
        "QPushButton#primarySave,QPushButton#primaryAction,QPushButton#writingPrimary,QPushButton#reviewPrimary,QPushButton#planningPrimary,QPushButton#hubPrimary{background:#1769c2;color:#ffffff;border:0;border-radius:5px;padding:7px 12px;font-weight:600;}"
    );
}

QString darkStyle() {
    return QStringLiteral(
        "QWidget{font-family:'Segoe UI';font-size:9pt;color:#e7edf5;background:#0f1218;}"
        "QPushButton{background:transparent;color:#cfd7e2;border:0;border-radius:5px;padding:6px 9px;min-height:20px;}QPushButton:hover{background:#222936;color:#ffffff;}QPushButton:pressed{background:#2a3341;}QPushButton:disabled{color:#626d7d;}"
        "QToolButton{background:transparent;color:#cfd7e2;border:0;border-radius:5px;padding:5px;min-height:20px;}QToolButton:hover{background:#222936;color:#ffffff;}QToolButton:pressed{background:#2a3341;}QToolButton:checked{background:#193956;color:#7bc0ff;}"
        "QLineEdit,QTextEdit,QPlainTextEdit{background:#151922;color:#e7edf5;border:1px solid #2a3240;border-radius:5px;padding:6px 8px;selection-background-color:#245074;selection-color:#ffffff;}"
        "QLineEdit:focus,QTextEdit:focus,QPlainTextEdit:focus{border-color:#4d91cf;}"
        "QComboBox,QSpinBox,QDoubleSpinBox{background:#151922;color:#e7edf5;border:1px solid #2a3240;border-radius:5px;padding:5px 9px;}"
        "QComboBox::drop-down{width:0;border:0;}QComboBox::down-arrow{image:none;width:0;height:0;}"
        "QSpinBox::up-button,QSpinBox::down-button,QDoubleSpinBox::up-button,QDoubleSpinBox::down-button{width:0;height:0;border:0;background:transparent;}"
        "QAbstractItemView{background:#151922;color:#e7edf5;border:1px solid #29323e;outline:0;selection-background-color:#1a3852;selection-color:#82c3ff;}"
        "QListWidget,QTreeWidget{background:#151922;color:#dce3ed;border:0;padding:2px;outline:0;}QListWidget::item,QTreeWidget::item{border:0;border-radius:4px;padding:6px 8px;}QListWidget::item:hover,QTreeWidget::item:hover{background:#1d2430;}QListWidget::item:selected,QTreeWidget::item:selected{background:#1a3852;color:#82c3ff;}"
        "QTabWidget::pane{border:0;background:transparent;}QTabBar::tab{background:transparent;color:#8f9bab;border:0;border-bottom:2px solid transparent;padding:9px 12px;margin-right:3px;}QTabBar::tab:hover{color:#f5f7fb;}QTabBar::tab:selected{color:#78bdff;border-bottom:2px solid #4d98e8;}"
        "QSlider::groove:horizontal{height:3px;background:#303844;border-radius:1px;}QSlider::handle:horizontal{width:12px;margin:-5px 0;background:#4d98e8;border:0;border-radius:6px;}"
        "QCheckBox{spacing:6px;}QCheckBox::indicator{width:13px;height:13px;border:1px solid #4b5564;background:#151922;border-radius:3px;}QCheckBox::indicator:checked{background:#397fc8;border-color:#397fc8;}"
        "QScrollBar:vertical{width:8px;background:transparent;}QScrollBar::handle:vertical{background:#3b4553;border-radius:4px;min-height:28px;}QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
        "QScrollBar:horizontal{height:8px;background:transparent;}QScrollBar::handle:horizontal{background:#3b4553;border-radius:4px;min-width:28px;}QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{width:0;}"

        "#appRoot,#workspaceShell,#contentShell,#pageStack,#planningPage,#planningCharacters,#planningBoard,#planningTheories,#planningTimeline,#writingPage,#writingEditorTab,#sceneBoardTab,#worldPage,#reviewPage,#projectHubPage,#hubShell,#hubCardsHost,#atlasTab,#magicTab,#mapsTab,#reviewProjectTab,#reviewAnalysisTab,#reviewLayoutTab,#reviewMediaTab{background:#0f1218;color:#e7edf5;}"
        "#appTitleBar{background:#0c0f14;border-bottom:1px solid #202631;}"
        "#sideRail{background:#0c1016;border-right:1px solid #202631;}"
        "#topShell{background:#11151c;border-bottom:1px solid #242b36;}"
        "#appIdentity{background:transparent;}#appMark{background:#17324d;color:#eaf5ff;border:1px solid #27577e;border-radius:7px;font-family:'Georgia';font-size:10pt;font-weight:700;}"
        "#appName{color:#f3f6fa;font-size:9.5pt;font-weight:700;}#appMode{color:#6f7b8b;font-size:7pt;font-weight:700;letter-spacing:1px;}"
        "QListWidget#sideNavigation{background:transparent;border:0;padding:0;}QListWidget#sideNavigation::item{background:transparent;color:#929eae;border:0;border-left:3px solid transparent;border-radius:5px;padding:9px 10px;margin:1px 0;}QListWidget#sideNavigation::item:hover{background:#171d27;color:#e7edf5;}QListWidget#sideNavigation::item:selected{background:#18293a;color:#8ac8ff;border-left:3px solid #4d98e8;font-weight:600;}"
        "QPushButton#settingsAction{background:transparent;color:#929eae;text-align:left;padding-left:13px;}QPushButton#settingsAction:hover{background:#171d27;color:#ffffff;}"
        "#projectTitle{color:#e7edf5;font-weight:700;font-size:9.5pt;}#saveState{color:#718096;font-size:8pt;}"
        "#windowMinimize,#windowMaximize,#windowClose{border:0;background:transparent;color:#aeb7c4;}#windowMinimize:hover,#windowMaximize:hover{background:#202632;}#windowClose:hover{background:#c42b1c;color:#ffffff;}"

        "QPushButton#primarySave,QPushButton#primaryAction,QPushButton#writingPrimary,QPushButton#reviewPrimary,QPushButton#planningPrimary,QPushButton#hubPrimary{background:#2d7fd0;color:#ffffff;border:0;border-radius:5px;padding:7px 12px;font-weight:600;}"
        "QPushButton#primarySave:hover,QPushButton#primaryAction:hover,QPushButton#writingPrimary:hover,QPushButton#reviewPrimary:hover,QPushButton#planningPrimary:hover,QPushButton#hubPrimary:hover{background:#378bdc;}"
        "QPushButton#secondaryAction,QPushButton#writingSubtle,QPushButton#hubSecondary{background:transparent;color:#aeb8c7;border:1px solid #2b3441;border-radius:5px;}QPushButton#secondaryAction:hover,QPushButton#writingSubtle:hover,QPushButton#hubSecondary:hover{background:#1c232e;color:#ffffff;border-color:#3b4657;}"

        "#hubDesktopBar{background:#11151c;border-bottom:1px solid #242b36;}#hubAppMark,#projectCardMark{background:#17324d;color:#eaf5ff;border:1px solid #27577e;border-radius:6px;font-family:'Georgia';font-weight:700;}"
        "#hubTitle{font-family:'Georgia';font-size:27pt;font-weight:500;color:#f4f7fb;}#hubDescription{font-family:'Georgia';font-size:10pt;color:#8e9aaa;}#hubKicker,#projectCardGenre,#newProjectMeta{color:#6f7d90;font-size:7.5pt;font-weight:700;letter-spacing:1px;}"
        "#hubStorage{background:#141b22;border:1px solid #27323d;border-radius:7px;}#hubStorageDot{color:#57c58b;}#hubStorageTitle{color:#dce5ef;font-weight:600;}#hubStorageDetail{color:#7f8b9b;}"
        "#projectCard{background:#151a22;border:1px solid #29323d;border-radius:8px;}#projectCard:hover{border-color:#3a4a5e;background:#171e28;}#projectCardState{background:#1c2530;color:#9fb0c4;border:1px solid #2d3947;border-radius:9px;padding:3px 7px;font-size:7.5pt;}"
        "#projectCardTitle,#newProjectTitle{font-family:'Georgia';font-size:16pt;font-weight:700;color:#f2f5f9;}#projectCardArchive,#newProjectCopy{color:#8f9bab;}#projectStatValue{color:#dce5ef;font-weight:700;}#projectStatLabel,#projectSaved{color:#697789;font-size:8pt;}"
        "QFrame#projectCardNew{background:#11161d;border:1px dashed #344354;border-radius:8px;}QFrame#projectCardNew:hover,QFrame#projectCardNew:focus{background:#151d27;border-color:#4d98e8;}#newProjectMark{background:#193956;color:#8ac8ff;border:1px solid #28577c;border-radius:23px;font-size:19pt;}"

        "#writingHero,#planningHero,#reviewHero{background:#121720;border:1px solid #252e39;border-radius:7px;}"
        "#writingKicker,#sectionLabel,#planningKicker,#planningField,#planningIndexTitle,#reviewKicker,#reviewFieldTitle,#pageKicker,#fieldTitle,#panelTitle{color:#6f7d90;font-size:7.5pt;font-weight:700;letter-spacing:.8px;}"
        "#writingTitle,#planningTitle,#reviewTitle,#pageTitle{font-family:'Georgia';color:#eef2f7;}#writingTitle{font-size:18pt;}#planningTitle,#reviewTitle,#pageTitle{font-size:20pt;}"
        "#writingDescription,#planningDescription,#reviewDescription,#pageDescription{color:#8491a2;font-size:9pt;}"
        "#indexPanel,#editorPanel,#metadataPanel,#sceneBoardToolbar,#sceneBoardOutlinePanel,#planningIndex,#planningEditor,#planningRelationPanel,#planningCard,#reviewCard,#worldIndexPanel,#worldEditorCard,#magicIndexPanel,#magicEditorCard,#mapIndexPanel,#mapEditorCard,#legacyMarkers,#generatorBar,#settingsCard{background:#151922;border:1px solid #29323d;border-radius:7px;color:#e7edf5;}"
        "#sceneEditor{background:#12161d;color:#e6e3dc;border:0;padding:34px 54px;font-family:'Georgia';font-size:12pt;selection-background-color:#244d6d;}"
        "#indexTitle,#editorPanelTitle,#planningCardTitle,#reviewCardTitle{color:#dfe6ef;font-weight:700;}#planningCardTitle,#reviewCardTitle{font-family:'Georgia';font-size:13pt;}"
        "#proofState,#sceneBoardSubheading,#sceneBoardSelection{color:#748295;}#sceneBoardHeading{color:#e7edf5;font-size:14pt;font-weight:700;}#sceneBoardCanvas{background:#10141b;border:1px solid #252e39;}"
        "#focusBar{background:#0e1218;border-bottom:1px solid #202833;}#focusSceneName{color:#8996a8;}#focusExit{color:#9aa8b9;}"

        "#worldEditorCard QTextEdit,#planningEditor QTextEdit,#reviewPage QTextEdit{background:#131820;border:1px solid #28323e;}"
        "#worldIndexPanel QListWidget,#planningIndex QListWidget,#sceneBoardOutline{background:#121720;}"
        "#reviewImage,#characterImage{background:#10141b;border:1px solid #2a3340;color:#788698;border-radius:6px;}"

        "#pilinReyEditor,#pilinCanvasHost{background:#0b0f14;}#pilinToolPalette,#pilinTopCommands,#pilinPopover,#pilinSelectionPopover{background:#121821;border:1px solid #2b3542;border-radius:7px;}"
        "QToolButton#pilinMapTool{min-width:30px;min-height:30px;border-radius:5px;font-size:11pt;}QToolButton#pilinMapTool:checked{background:#1c4668;color:#9bd3ff;}QToolButton#pilinCommand{min-width:26px;min-height:26px;}"
        "#pilinToolOptionsLabel,#pilinPopoverTitle{color:#edf3fa;font-weight:700;}#pilinTinyLabel{color:#77879a;font-size:7.5pt;}#pilinStatus{background:rgba(12,16,22,210);color:#9dacbd;border:1px solid #27313d;border-radius:5px;padding:5px 8px;}"

        "QDialog{background:#10141a;}#dialogTitle{font-family:'Georgia';font-size:20pt;color:#f3f6fa;}#dialogKicker,#fieldLabel{color:#718095;font-size:8pt;font-weight:700;}#dialogDescription,#driveStatus{color:#8c99aa;}"
    );
}

void compactApplicationChrome() {
    for (QWidget* widget : QApplication::allWidgets()) {
        if (!widget) continue;
        const QString name = widget->objectName();
        if (name == QStringLiteral("topShell")) {
            widget->setMinimumHeight(48);
            widget->setMaximumHeight(48);
        } else if (name == QStringLiteral("sideRail")) {
            widget->setMinimumWidth(184);
            widget->setMaximumWidth(184);
        } else if (name == QStringLiteral("appMark")) {
            widget->setFixedSize(34, 34);
        } else if (name == QStringLiteral("writingHero")) {
            widget->setMaximumHeight(78);
        } else if (name == QStringLiteral("planningHero") || name == QStringLiteral("reviewHero")) {
            widget->setMaximumHeight(96);
        }
    }
}

void restyleExistingWidgets(bool dark) {
    const QString override = dark ? darkStyle() : lightStyle();
    for (QWidget* widget : QApplication::allWidgets()) {
        if (!widget) continue;
        const char* key = "wbwBaseStyleSheet";
        if (!widget->property(key).isValid()) widget->setProperty(key, widget->styleSheet());
        const QString base = widget->property(key).toString();
        widget->setStyleSheet(base + override);
    }
    compactApplicationChrome();
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
    restyleExistingWidgets(dark);
}

void ThemeManager::saveAndApply(Mode mode) {
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
    settings.setValue(QStringLiteral("ui/theme"), mode == Mode::Dark ? QStringLiteral("dark") : QStringLiteral("light"));
    settings.sync();
    apply(mode);
}

} // namespace wbw
