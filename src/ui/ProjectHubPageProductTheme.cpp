#include "ui/ProjectHubPage.h"

#include <QApplication>

namespace wbw {

void ProjectHubPage::refreshProductTheme() {
    const bool dark = qApp && qApp->property("wbwDarkMode").toBool();
    if (dark) {
        setStyleSheet(QStringLiteral(R"QSS(
#projectHubPage,#hubScroll,#hubScroll>QWidget>QWidget,#hubShell,#hubCardsHost{background:#0b0f14;color:#dfe4ea;}
#hubTitle{font-family:'Georgia';font-size:30pt;font-weight:600;color:#f3efe8;}
#hubDescription{font-size:10.5pt;color:#7f8995;}
#hubSort,#hubGenreFilter,#hubStatusFilter,#hubSearch{background:#0f151c;color:#dfe5eb;border:1px solid #2b3440;border-radius:8px;padding:8px 11px;min-height:20px;}
#hubSort:hover,#hubGenreFilter:hover,#hubStatusFilter:hover,#hubSearch:focus{border-color:#456786;}
#hubViewButton{background:#10161d;color:#9da9b7;border:1px solid #2a333d;border-radius:8px;font-size:15pt;}
#hubViewButton:checked,#hubViewButton:hover{background:#162334;color:#79b9f2;border-color:#3a6c98;}
#projectCard,#projectCardNew{background:#11171e;border:1px solid #2b343d;border-radius:12px;}
#projectCard:hover,#projectCard:focus{background:#141b23;border-color:#426b8d;}
#projectCardNew{border-style:dashed;border-color:#3b4651;}
#projectCardNew:hover,#projectCardNew:focus{background:#111a24;border-color:#6096c3;}
#projectCardCover{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #263646,stop:.5 #151e28,stop:1 #0d1218);border:0;border-radius:9px;color:#a9bac9;font-size:9pt;font-weight:700;letter-spacing:1.2px;}
#projectCardTitle,#newProjectTitle{font-family:'Georgia';font-size:17pt;font-weight:700;color:#f3efe8;}
#projectCardArchive,#newProjectCopy,#projectCardGenre{color:#89939e;}
#projectStatValue{font-weight:700;color:#edf2f7;} #projectStatLabel,#projectSaved{color:#727d87;}
#newProjectMark{background:#10161e;color:#dfe9f3;border:1px solid #5e9ee0;border-radius:34px;font-size:24pt;}
#newProjectOption{background:transparent;color:#8ebff1;border:0;padding:6px 8px;text-align:left;}
#projectDelete{background:transparent;color:#7f8b96;border:0;padding:5px 6px;}
#hubEmptyState{color:#77828d;padding:36px;}
)QSS"));
    } else {
        setStyleSheet(QStringLiteral(R"QSS(
#projectHubPage,#hubScroll,#hubScroll>QWidget>QWidget,#hubShell,#hubCardsHost{background:#ece8e0;color:#342e28;}
#hubTitle{font-family:'Georgia';font-size:30pt;font-weight:600;color:#302a24;}
#hubDescription{font-size:10.5pt;color:#786f66;}
#hubSort,#hubGenreFilter,#hubStatusFilter,#hubSearch{background:#fffdf9;color:#332e29;border:1px solid #d1c6b8;border-radius:8px;padding:8px 11px;min-height:20px;}
#hubSort:hover,#hubGenreFilter:hover,#hubStatusFilter:hover,#hubSearch:focus{border-color:#789ba4;}
#hubViewButton{background:#f4efe7;color:#6c635a;border:1px solid #d4cabc;border-radius:8px;font-size:15pt;}
#hubViewButton:checked,#hubViewButton:hover{background:#dce8e9;color:#315f68;border-color:#91aeb3;}
#projectCard,#projectCardNew{background:#fbf8f2;border:1px solid #d4c9bb;border-radius:12px;}
#projectCard:hover,#projectCard:focus{background:#fffdf9;border-color:#89aab1;}
#projectCardNew{border-style:dashed;border-color:#c6b8a8;}
#projectCardNew:hover,#projectCardNew:focus{background:#f7f3ec;border-color:#789ba4;}
#projectCardCover{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #d7ddd9,stop:.5 #eee7dd,stop:1 #d6cbbb);border:0;border-radius:9px;color:#665d54;font-size:9pt;font-weight:700;letter-spacing:1.2px;}
#projectCardTitle,#newProjectTitle{font-family:'Georgia';font-size:17pt;font-weight:700;color:#342e28;}
#projectCardArchive,#newProjectCopy,#projectCardGenre{color:#786f66;}
#projectStatValue{font-weight:700;color:#342e28;} #projectStatLabel,#projectSaved{color:#80766d;}
#newProjectMark{background:#f4efe7;color:#315f68;border:1px solid #789ba4;border-radius:34px;font-size:24pt;}
#newProjectOption{background:transparent;color:#426f7c;border:0;padding:6px 8px;text-align:left;}
#projectDelete{background:transparent;color:#80766d;border:0;padding:5px 6px;}
#hubEmptyState{color:#80766d;padding:36px;}
)QSS"));
    }
}

} // namespace wbw
