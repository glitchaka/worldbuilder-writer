#include "ui/WritingPage.h"

#include <QApplication>
#include <QComboBox>
#include <QLineEdit>

namespace wbw {

void WritingPage::refreshProductChromeTheme() {
    if (auto* search = findChild<QLineEdit*>(QStringLiteral("sceneBoardSearch"))) {
        search->setMinimumWidth(0);
        search->setMaximumWidth(QWIDGETSIZE_MAX);
    }
    if (auto* status = findChild<QComboBox*>(QStringLiteral("sceneBoardStatus"))) {
        status->setMinimumWidth(0);
        status->setMaximumWidth(QWIDGETSIZE_MAX);
    }

    const bool dark = qApp && qApp->property("wbwDarkMode").toBool();
    const QString mode = dark ? QStringLiteral("dark") : QStringLiteral("light");
    if (property("wbwProductChromeTheme").toString() == mode) return;
    setProperty("wbwProductChromeTheme", mode);

    setStyleSheet(styleSheet() + (dark ? QStringLiteral(R"QSS(
#writingEditorTab,#sceneBoardTab,#editorPanel,#sceneBoardCanvas{background:#0b1016;color:#d8dee7;}
#writingCommandBar,#sceneBoardToolbar{background:#0d141c;border-bottom:1px solid #28313c;}
#indexPanel,#sceneBoardOutlinePanel{background:#0c1219;border-right:1px solid #25303b;}
#sceneInspector{background:#0c1219;border-left:1px solid #25303b;}
#manuscriptHeader{background:#0b1016;}
#manuscriptBreadcrumb,#sceneInspectorCopy,#planningFilterCopy,#sceneInspectorLabel{color:#75818c;}
#manuscriptSceneTitle,#sceneInspectorTitle,#planningFilterTitle{color:#f0ece5;}
#sceneEditor{background:#0b1016;color:#e6dfd5;selection-background-color:#315a70;selection-color:#ffffff;}
#sceneInspector QLineEdit,#sceneInspector QComboBox,#sceneBoardOutlinePanel QLineEdit,#sceneBoardOutlinePanel QComboBox{background:#101820;color:#dce4ea;border:1px solid #2b3742;}
#sceneInspectorHint{background:#101820;color:#85939f;border:1px solid #26333e;}
#writingFocusAction,#planningClearFilters{background:#111922;color:#aeb9c3;border:1px solid #2e3a46;}
)QSS") : QStringLiteral(R"QSS(
#writingEditorTab,#sceneBoardTab,#editorPanel,#sceneBoardCanvas{background:#f5f2ec;color:#302d28;}
#writingCommandBar,#sceneBoardToolbar{background:#faf8f3;border-bottom:1px solid #d8d0c5;}
#indexPanel,#sceneBoardOutlinePanel{background:#eeeae2;border-right:1px solid #d4ccc0;}
#sceneInspector{background:#eeeae2;border-left:1px solid #d4ccc0;}
#manuscriptHeader{background:#fffdf9;}
#manuscriptBreadcrumb,#sceneInspectorCopy,#planningFilterCopy,#sceneInspectorLabel{color:#756e65;}
#manuscriptSceneTitle,#sceneInspectorTitle,#planningFilterTitle{color:#312d28;}
#sceneEditor{background:#fffdf9;color:#302c27;selection-background-color:#d8c6ae;selection-color:#241f1a;}
#sceneInspector QLineEdit,#sceneInspector QComboBox,#sceneBoardOutlinePanel QLineEdit,#sceneBoardOutlinePanel QComboBox{background:#fffdf9;color:#312e29;border:1px solid #d2cbc0;}
#sceneInspectorHint{background:#f8f4ed;color:#6f685f;border:1px solid #d7cfc3;}
#writingFocusAction,#planningClearFilters{background:#f8f4ed;color:#625c54;border:1px solid #d2c9bd;}
#manuscriptTree{color:#4b4740;} #manuscriptTree::item:hover{background:#e4ded4;color:#2e2a25;} #manuscriptTree::item:selected{background:#d9e6e8;color:#285d61;border-left:2px solid #3a9598;}
)QSS")));
}

} // namespace wbw
