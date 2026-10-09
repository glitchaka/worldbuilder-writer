#include "ui/ThemeManager.h"

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QSettings>
#include <QVariant>
#include <QWidget>

namespace wbw {
namespace {

QPalette lightPalette() {
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
    return palette;
}

QPalette darkPalette() {
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#12161d")));
    palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#e6eaf0")));
    palette.setColor(QPalette::Base, QColor(QStringLiteral("#171c24")));
    palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#1d2430")));
    palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#222a36")));
    palette.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#f4f6f8")));
    palette.setColor(QPalette::Text, QColor(QStringLiteral("#e6eaf0")));
    palette.setColor(QPalette::Button, QColor(QStringLiteral("#1d2430")));
    palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#e6eaf0")));
    palette.setColor(QPalette::BrightText, QColor(QStringLiteral("#ff8d82")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#234f83")));
    palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#ffffff")));
    return palette;
}

QString lightStyle() {
    return QStringLiteral(
        "QWidget{font-family:'Segoe UI';font-size:9.5pt;color:#1f2937;}"
        "QPushButton,QToolButton{background:#ffffff;color:#344054;border:1px solid #cfd6df;border-radius:2px;padding:6px 10px;min-height:18px;}"
        "QPushButton:hover,QToolButton:hover{background:#f7f9fc;border-color:#98a2b3;}QPushButton:pressed,QToolButton:pressed{background:#eef2f7;}QToolButton:checked{background:#1668d4;color:#ffffff;border-color:#1668d4;}"
        "QLineEdit,QTextEdit,QComboBox,QSpinBox,QDoubleSpinBox,QListWidget,QTreeWidget{background:#ffffff;color:#1f2937;border:1px solid #cfd6df;border-radius:2px;padding:6px 8px;}"
        "QComboBox{padding-right:10px;}QComboBox::drop-down{width:0;border:0;}QComboBox::down-arrow{image:none;width:0;height:0;}"
        "QSpinBox,QDoubleSpinBox{padding-right:8px;}QSpinBox::up-button,QSpinBox::down-button,QDoubleSpinBox::up-button,QDoubleSpinBox::down-button{width:0;height:0;border:0;background:transparent;}"
        "QSlider::groove:horizontal{height:4px;background:#d8dee8;border-radius:2px;}QSlider::handle:horizontal{width:14px;margin:-5px 0;background:#1668d4;border:1px solid #0f5fc8;border-radius:7px;}"
        "QCheckBox{spacing:7px;}QCheckBox::indicator{width:14px;height:14px;border:1px solid #aab4c0;background:#ffffff;border-radius:2px;}QCheckBox::indicator:checked{background:#1668d4;border-color:#1668d4;}"
        "#appTitleBar{background:#ffffff;border-bottom:1px solid #d8dee8;}"
        "#windowMinimize,#windowMaximize,#windowClose{border:0;border-radius:0;background:transparent;padding:0;min-height:0;color:#475467;font-size:12pt;}#windowMinimize:hover,#windowMaximize:hover{background:#eef2f7;}#windowClose:hover{background:#c42b1c;color:#ffffff;}"
        "#topShell{background:#ffffff;border-bottom:1px solid #d8dee8;}"
        "#appIdentity{background:transparent;}"
        "#appMark{background:#17233a;color:#ffffff;border-radius:3px;font-family:'Georgia';font-size:10.5pt;font-weight:700;}"
        "#appName{color:#243247;font-size:10pt;font-weight:700;}"
        "#appMode{color:#8b95a5;font-size:7.5pt;font-weight:600;}"
        "#projectTitle{color:#344054;font-weight:600;}#saveState{color:#2f855a;font-size:8pt;}"
        "QListWidget#topNavigation{background:transparent;border:0;padding:0;}"
        "QListWidget#topNavigation::item{background:transparent;color:#344054;border:0;border-bottom:3px solid transparent;padding:0 14px;}"
        "QListWidget#topNavigation::item:hover{background:#f7f9fc;}"
        "QListWidget#topNavigation::item:selected{background:#eef5ff;color:#1668d4;border-bottom:3px solid #1668d4;}"
    );
}

QString darkStyle() {
    return QStringLiteral(
        "QWidget{font-family:'Segoe UI';font-size:9.5pt;color:#e6eaf0;background:#12161d;}"
        "QPushButton,QToolButton{background:#1d2430;color:#e6eaf0;border:1px solid #394454;border-radius:2px;padding:6px 10px;min-height:18px;}"
        "QPushButton:hover,QToolButton:hover{background:#283241;border-color:#56657a;}QPushButton:pressed,QToolButton:pressed{background:#303b4c;}QToolButton:checked{background:#2670c9;color:#ffffff;border-color:#2670c9;}"
        "QLineEdit,QTextEdit,QComboBox,QSpinBox,QDoubleSpinBox,QListWidget,QTreeWidget{background:#171c24;color:#e6eaf0;border:1px solid #394454;border-radius:2px;padding:6px 8px;selection-background-color:#234f83;selection-color:#ffffff;}"
        "QComboBox::drop-down{width:0;border:0;}QComboBox::down-arrow{image:none;width:0;height:0;}QSpinBox::up-button,QSpinBox::down-button,QDoubleSpinBox::up-button,QDoubleSpinBox::down-button{width:0;height:0;border:0;background:transparent;}"
        "QTabWidget::pane{border:0;background:#12161d;}QTabBar::tab{background:#12161d;color:#aeb8c6;border:0;border-bottom:2px solid transparent;padding:10px 14px;}QTabBar::tab:selected{color:#72aef0;border-bottom-color:#4289d8;}"
        "QScrollArea,QAbstractScrollArea{background:#12161d;}QScrollBar{background:#161c25;}QScrollBar::handle{background:#455266;border-radius:4px;}"
        "QSlider::groove:horizontal{height:4px;background:#394454;border-radius:2px;}QSlider::handle:horizontal{width:14px;margin:-5px 0;background:#4d97e8;border-radius:7px;}"
        "QCheckBox::indicator{width:14px;height:14px;border:1px solid #5c697b;background:#171c24;border-radius:2px;}QCheckBox::indicator:checked{background:#2670c9;border-color:#2670c9;}"
        "#appRoot,#pageStack,#planningPage,#planningCharacters,#planningBoard,#planningTheories,#planningTimeline,#writingPage,#writingEditorTab,#sceneBoardTab,#worldPage,#reviewPage,#projectHubPage,#hubShell,#hubCardsHost,#atlasTab,#magicTab,#mapsTab{background:#12161d;color:#e6eaf0;}"
        "#topShell,#hubDesktopBar,#appTitleBar{background:#171c24;border-color:#303a48;}"
        "#writingHero,#indexPanel,#editorPanel,#metadataPanel,#worldIndexPanel,#worldEditorCard,#magicIndexPanel,#magicEditorCard,#mapIndexPanel,#mapEditorCard,#legacyMarkers,#projectCard,#hubStorage,#generatorBar,#planningIndex,#planningEditor,#planningRelationPanel,#planningHero,#planningCard{background:#171c24;border-color:#394454;color:#e6eaf0;}"
        "#sceneEditor{background:#171c24;color:#e6eaf0;}"
        "#appMark,#hubAppMark,#projectCardMark{background:#0d1728;color:#ffffff;border:1px solid #334155;border-radius:3px;font-family:'Georgia';font-weight:700;}"
        "#appName,#projectTitle,#hubAppName,#projectCardTitle,#newProjectTitle,#pageTitle,#dialogTitle,#planningTitle,#planningCardTitle,#writingTitle{color:#f2f4f7;}"
        "#appMode,#saveState,#hubAppMode,#hubKicker,#projectCardGenre,#newProjectMeta,#pageKicker,#fieldTitle,#panelTitle,#projectSaved,#projectStatLabel,#hubDescription,#pageDescription,#writingDescription,#proofState,#planningKicker,#planningField,#planningDescription,#planningIndexTitle{color:#9aa7b8;}"
        "QListWidget#topNavigation{background:transparent;border:0;padding:0;}QListWidget#topNavigation::item{background:transparent;color:#c5ced9;border:0;border-bottom:3px solid transparent;padding:0 14px;}QListWidget#topNavigation::item:hover{background:#202936;}QListWidget#topNavigation::item:selected{background:#1b2d44;color:#72aef0;border-bottom:3px solid #4289d8;}"
        "#appTitleBar{border-bottom:1px solid #303a48;}"
        "#windowMinimize,#windowMaximize,#windowClose{border:0;background:transparent;color:#c5ced9;}#windowMinimize:hover,#windowMaximize:hover{background:#283241;}#windowClose:hover{background:#c42b1c;color:#ffffff;}"
    );
}

void restyleExistingWidgets(bool dark) {
    const QString override = dark ? darkStyle() : QString();
    for (QWidget* widget : QApplication::allWidgets()) {
        if (!widget) continue;
        const char* key = "wbwBaseStyleSheet";
        if (!widget->property(key).isValid()) widget->setProperty(key, widget->styleSheet());
        const QString base = widget->property(key).toString();
        widget->setStyleSheet(dark ? base + override : base);
    }
}

} // namespace

ThemeManager::Mode ThemeManager::savedMode() {
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
    return settings.value(QStringLiteral("ui/theme"), QStringLiteral("light")).toString() == QStringLiteral("dark") ? Mode::Dark : Mode::Light;
}

void ThemeManager::applySaved() {
    apply(savedMode());
}

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
    apply(mode);
}

} // namespace wbw
