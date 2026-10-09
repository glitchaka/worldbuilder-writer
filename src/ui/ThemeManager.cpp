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
    p.setColor(QPalette::Window, QColor(QStringLiteral("#f4f6f8")));
    p.setColor(QPalette::WindowText, QColor(QStringLiteral("#20242b")));
    p.setColor(QPalette::Base, QColor(QStringLiteral("#ffffff")));
    p.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#f7f8fa")));
    p.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#ffffff")));
    p.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#20242b")));
    p.setColor(QPalette::Text, QColor(QStringLiteral("#20242b")));
    p.setColor(QPalette::Button, QColor(QStringLiteral("#ffffff")));
    p.setColor(QPalette::ButtonText, QColor(QStringLiteral("#303640")));
    p.setColor(QPalette::BrightText, QColor(QStringLiteral("#b42318")));
    p.setColor(QPalette::Highlight, QColor(QStringLiteral("#d9e8fb")));
    p.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#145fb8")));
    p.setColor(QPalette::Mid, QColor(QStringLiteral("#d9dee5")));
    return p;
}

QPalette darkPalette() {
    QPalette p;
    p.setColor(QPalette::Window, QColor(QStringLiteral("#111214")));
    p.setColor(QPalette::WindowText, QColor(QStringLiteral("#e5e7eb")));
    p.setColor(QPalette::Base, QColor(QStringLiteral("#17181b")));
    p.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#202226")));
    p.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#202226")));
    p.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#f3f4f6")));
    p.setColor(QPalette::Text, QColor(QStringLiteral("#e5e7eb")));
    p.setColor(QPalette::Button, QColor(QStringLiteral("#1b1d21")));
    p.setColor(QPalette::ButtonText, QColor(QStringLiteral("#e5e7eb")));
    p.setColor(QPalette::BrightText, QColor(QStringLiteral("#ff8a80")));
    p.setColor(QPalette::Highlight, QColor(QStringLiteral("#244c6b")));
    p.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#ffffff")));
    p.setColor(QPalette::Mid, QColor(QStringLiteral("#34373d")));
    return p;
}

QString lightStyle() {
    return QStringLiteral(
        "QWidget{font-family:'Segoe UI';font-size:9pt;color:#20242b;}"
        "QPushButton{background:transparent;color:#303640;border:0;border-radius:4px;padding:6px 9px;min-height:18px;}"
        "QPushButton:hover{background:#edf1f5;}QPushButton:pressed{background:#e2e7ed;}QPushButton:disabled{color:#a3aab4;}"
        "QToolButton{background:transparent;color:#303640;border:0;border-radius:4px;padding:4px;min-height:20px;}"
        "QToolButton:hover{background:#edf1f5;}QToolButton:pressed{background:#e2e7ed;}QToolButton:checked{background:#dceafb;color:#145fb8;}"
        "QLineEdit,QTextEdit,QPlainTextEdit{background:#ffffff;color:#20242b;border:1px solid #d9dee5;border-radius:4px;padding:6px 8px;selection-background-color:#d9e8fb;selection-color:#145fb8;}"
        "QLineEdit:focus,QTextEdit:focus,QPlainTextEdit:focus{border-color:#8fb5df;}"
        "QComboBox,QSpinBox,QDoubleSpinBox{background:#ffffff;color:#20242b;border:1px solid #d9dee5;border-radius:4px;padding:5px 9px;}"
        "QComboBox:focus,QSpinBox:focus,QDoubleSpinBox:focus{border-color:#8fb5df;}"
        "QComboBox::drop-down{width:0;border:0;}QComboBox::down-arrow{image:none;width:0;height:0;}"
        "QSpinBox::up-button,QSpinBox::down-button,QDoubleSpinBox::up-button,QDoubleSpinBox::down-button{width:0;height:0;border:0;background:transparent;}"
        "QAbstractItemView{background:#ffffff;color:#20242b;border:1px solid #d9dee5;outline:0;selection-background-color:#dfeafb;selection-color:#145fb8;}"
        "QListWidget,QTreeWidget{background:#ffffff;color:#20242b;border:0;padding:2px;outline:0;}"
        "QListWidget::item,QTreeWidget::item{border:0;border-radius:3px;padding:5px 7px;}"
        "QListWidget::item:hover,QTreeWidget::item:hover{background:#f0f3f7;}QListWidget::item:selected,QTreeWidget::item:selected{background:#dfeafb;color:#145fb8;}"
        "QTabWidget::pane{border:0;background:transparent;}"
        "QTabBar::tab{background:transparent;color:#667085;border:0;border-bottom:2px solid transparent;padding:8px 11px;margin:0 3px 0 0;}"
        "QTabBar::tab:hover{color:#20242b;}QTabBar::tab:selected{color:#145fb8;border-bottom:2px solid #2f75c8;}"
        "QSlider::groove:horizontal{height:3px;background:#d8dde5;border-radius:1px;}QSlider::handle:horizontal{width:12px;margin:-5px 0;background:#2f75c8;border:0;border-radius:6px;}"
        "QCheckBox{spacing:6px;}QCheckBox::indicator{width:13px;height:13px;border:1px solid #aeb7c3;background:#ffffff;border-radius:3px;}QCheckBox::indicator:checked{background:#2f75c8;border-color:#2f75c8;}"
        "QScrollBar:vertical{width:9px;background:transparent;margin:0;}QScrollBar::handle:vertical{background:#c3cbd5;border-radius:4px;min-height:28px;}QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
        "QScrollBar:horizontal{height:9px;background:transparent;margin:0;}QScrollBar::handle:horizontal{background:#c3cbd5;border-radius:4px;min-width:28px;}QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{width:0;}"
        "#appTitleBar{background:#ffffff;border-bottom:1px solid #e2e5e9;}"
        "#windowMinimize,#windowMaximize,#windowClose{border:0;border-radius:0;background:transparent;padding:0;min-height:0;color:#596273;font-size:11pt;}#windowMinimize:hover,#windowMaximize:hover{background:#eceff3;}#windowClose:hover{background:#c42b1c;color:#ffffff;}"
        "#topShell{background:#ffffff;border-bottom:1px solid #dde2e8;}#appIdentity{background:transparent;}"
        "#appMark{background:#17233a;color:#ffffff;border:0;border-radius:4px;font-family:'Georgia';font-size:10pt;font-weight:700;}"
        "#appName{color:#222b38;font-size:9.5pt;font-weight:700;}#appMode{color:#8b95a5;font-size:7pt;font-weight:600;}"
        "#projectTitle{color:#303640;font-weight:600;}#saveState{color:#39805a;font-size:8pt;}"
        "QPushButton#primarySave,QPushButton#primaryAction,QPushButton#writingPrimary{background:#1769c2;color:#ffffff;border:0;border-radius:4px;padding:6px 11px;font-weight:600;}"
        "QPushButton#primarySave:hover,QPushButton#primaryAction:hover,QPushButton#writingPrimary:hover{background:#105eaf;}"
        "QListWidget#topNavigation{background:transparent;border:0;padding:0;}"
        "QListWidget#topNavigation::item{background:transparent;color:#4b5563;border:0;border-bottom:2px solid transparent;padding:0 10px;margin:0 2px;}"
        "QListWidget#topNavigation::item:hover{background:transparent;color:#20242b;}QListWidget#topNavigation::item:selected{background:transparent;color:#1769c2;border-bottom:2px solid #1769c2;}"
    );
}

QString darkStyle() {
    return QStringLiteral(
        "QWidget{font-family:'Segoe UI';font-size:9pt;color:#e5e7eb;background:#111214;}"
        "QPushButton{background:transparent;color:#d1d5db;border:0;border-radius:4px;padding:6px 9px;min-height:18px;}QPushButton:hover{background:#24262b;}QPushButton:pressed{background:#2d3036;}QPushButton:disabled{color:#6b7280;}"
        "QToolButton{background:transparent;color:#d1d5db;border:0;border-radius:4px;padding:4px;min-height:20px;}QToolButton:hover{background:#24262b;}QToolButton:pressed{background:#2d3036;}QToolButton:checked{background:#173754;color:#74b8ff;}"
        "QLineEdit,QTextEdit,QPlainTextEdit{background:#17181b;color:#e5e7eb;border:1px solid #34373d;border-radius:4px;padding:6px 8px;selection-background-color:#244c6b;selection-color:#ffffff;}"
        "QLineEdit:focus,QTextEdit:focus,QPlainTextEdit:focus{border-color:#4f7ea8;}"
        "QComboBox,QSpinBox,QDoubleSpinBox{background:#17181b;color:#e5e7eb;border:1px solid #34373d;border-radius:4px;padding:5px 9px;}"
        "QComboBox::drop-down{width:0;border:0;}QComboBox::down-arrow{image:none;width:0;height:0;}"
        "QSpinBox::up-button,QSpinBox::down-button,QDoubleSpinBox::up-button,QDoubleSpinBox::down-button{width:0;height:0;border:0;background:transparent;}"
        "QAbstractItemView{background:#17181b;color:#e5e7eb;border:1px solid #32353a;outline:0;selection-background-color:#1c3954;selection-color:#79baff;}"
        "QListWidget,QTreeWidget{background:#17181b;color:#e5e7eb;border:0;padding:2px;outline:0;}QListWidget::item,QTreeWidget::item{border:0;border-radius:3px;padding:5px 7px;}QListWidget::item:hover,QTreeWidget::item:hover{background:#222429;}QListWidget::item:selected,QTreeWidget::item:selected{background:#1c3954;color:#79baff;}"
        "QTabWidget::pane{border:0;background:transparent;}QTabBar::tab{background:transparent;color:#a8afb9;border:0;border-bottom:2px solid transparent;padding:8px 11px;margin-right:3px;}QTabBar::tab:hover{color:#f3f4f6;}QTabBar::tab:selected{color:#76b8ff;border-bottom:2px solid #4d98e8;}"
        "QSlider::groove:horizontal{height:3px;background:#373a40;border-radius:1px;}QSlider::handle:horizontal{width:12px;margin:-5px 0;background:#4d98e8;border:0;border-radius:6px;}"
        "QCheckBox::indicator{width:13px;height:13px;border:1px solid #555b64;background:#17181b;border-radius:3px;}QCheckBox::indicator:checked{background:#397fc8;border-color:#397fc8;}"
        "QScrollBar:vertical{width:9px;background:transparent;}QScrollBar::handle:vertical{background:#454951;border-radius:4px;min-height:28px;}QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
        "QScrollBar:horizontal{height:9px;background:transparent;}QScrollBar::handle:horizontal{background:#454951;border-radius:4px;min-width:28px;}QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{width:0;}"
        "#appRoot,#pageStack,#planningPage,#planningCharacters,#planningBoard,#planningTheories,#planningTimeline,#writingPage,#writingEditorTab,#sceneBoardTab,#worldPage,#reviewPage,#projectHubPage,#hubShell,#hubCardsHost,#atlasTab,#magicTab,#mapsTab{background:#111214;color:#e5e7eb;}"
        "#topShell,#hubDesktopBar,#appTitleBar{background:#17181b;border-color:#2b2e33;}#appIdentity{background:transparent;}"
        "#appMark,#hubAppMark,#projectCardMark{background:#0d1728;color:#ffffff;border:0;border-radius:4px;font-family:'Georgia';font-weight:700;}"
        "#appName,#projectTitle,#hubAppName,#projectCardTitle,#newProjectTitle,#pageTitle,#dialogTitle,#planningTitle,#planningCardTitle,#writingTitle{color:#f3f4f6;}"
        "#appMode,#saveState,#hubAppMode,#hubKicker,#projectCardGenre,#newProjectMeta,#pageKicker,#fieldTitle,#panelTitle,#projectSaved,#projectStatLabel,#hubDescription,#pageDescription,#writingDescription,#proofState,#planningKicker,#planningField,#planningDescription,#planningIndexTitle{color:#9299a4;}"
        "#writingHero,#indexPanel,#editorPanel,#metadataPanel,#worldIndexPanel,#worldEditorCard,#magicIndexPanel,#magicEditorCard,#mapIndexPanel,#mapEditorCard,#legacyMarkers,#projectCard,#hubStorage,#generatorBar,#planningIndex,#planningEditor,#planningRelationPanel,#planningHero,#planningCard,#mapGeneratorPopup{background:#17181b;border-color:#30343a;color:#e5e7eb;}#sceneEditor{background:#17181b;color:#e5e7eb;}"
        "QPushButton#primarySave,QPushButton#primaryAction,QPushButton#writingPrimary{background:#2b78c8;color:#ffffff;border:0;border-radius:4px;font-weight:600;}QPushButton#primarySave:hover,QPushButton#primaryAction:hover,QPushButton#writingPrimary:hover{background:#3687da;}"
        "QListWidget#topNavigation{background:transparent;border:0;padding:0;}QListWidget#topNavigation::item{background:transparent;color:#b9c0ca;border:0;border-bottom:2px solid transparent;padding:0 10px;margin:0 2px;}QListWidget#topNavigation::item:hover{background:transparent;color:#ffffff;}QListWidget#topNavigation::item:selected{background:transparent;color:#78b9ff;border-bottom:2px solid #4d98e8;}"
        "#windowMinimize,#windowMaximize,#windowClose{border:0;background:transparent;color:#c5cbd4;}#windowMinimize:hover,#windowMaximize:hover{background:#27292e;}#windowClose:hover{background:#c42b1c;color:#ffffff;}"
    );
}

void compactApplicationChrome() {
    for (QWidget* widget : QApplication::allWidgets()) {
        if (!widget) continue;
        const QString name = widget->objectName();
        if (name == QStringLiteral("topShell")) {
            widget->setMinimumHeight(48);
            widget->setMaximumHeight(48);
        } else if (name == QStringLiteral("topNavigation")) {
            widget->setMinimumHeight(47);
            widget->setMaximumHeight(47);
            if (auto* list = qobject_cast<QListWidget*>(widget)) {
                list->setMinimumWidth(420);
                for (int i = 0; i < list->count(); ++i) list->item(i)->setSizeHint(QSize(i == 0 ? 108 : 92, 46));
            }
        } else if (name == QStringLiteral("appMark")) {
            widget->setFixedSize(32, 32);
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
    return settings.value(QStringLiteral("ui/theme"), QStringLiteral("light")).toString() == QStringLiteral("dark") ? Mode::Dark : Mode::Light;
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
