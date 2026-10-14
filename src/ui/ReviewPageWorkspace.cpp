#include "ui/ReviewPage.h"

#include <QApplication>
#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

namespace wbw {
namespace {

QString sectionTitle(int index) {
    switch (index) {
        case 0: return QObject::tr("Proyecto");
        case 1: return QObject::tr("Revisión");
        case 2: return QObject::tr("Maquetación y salida");
        case 3: return QObject::tr("Ambientación");
        default: return QObject::tr("Revisión");
    }
}

QString sectionCopy(int index) {
    switch (index) {
        case 0: return QObject::tr("Identidad, metadatos, portada, banner y presentación de la obra.");
        case 1: return QObject::tr("Diagnóstico editorial, repeticiones, mecánica y salida del manuscrito.");
        case 2: return QObject::tr("Página, márgenes, tipografía, estructura y preparación editorial.");
        case 3: return QObject::tr("Spotify, YouTube y medios locales para acompañar la escritura.");
        default: return {};
    }
}

} // namespace

void ReviewPage::installWorkspaceNavigation() {
    applyApprovedReviewChrome();
    if (!tabs_ || property("wbwReviewWorkspaceV1").toBool()) return;

    auto* root = qobject_cast<QVBoxLayout*>(layout());
    if (!root) return;

    setProperty("wbwReviewWorkspaceV1", true);
    if (tabs_->tabBar()) tabs_->tabBar()->hide();
    if (auto* oldHeader = findChild<QWidget*>(QStringLiteral("reviewPageHeader"))) oldHeader->hide();

    auto* header = new QFrame(this);
    header->setObjectName(QStringLiteral("reviewWorkspaceHeader"));
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(22, 12, 22, 12);
    headerLayout->setSpacing(18);

    auto* copy = new QVBoxLayout;
    copy->setSpacing(2);
    auto* title = new QLabel(sectionTitle(1), header);
    title->setObjectName(QStringLiteral("reviewWorkspaceTitle"));
    auto* subtitle = new QLabel(sectionCopy(1), header);
    subtitle->setObjectName(QStringLiteral("reviewWorkspaceSubtitle"));
    subtitle->setWordWrap(true);
    copy->addWidget(title);
    copy->addWidget(subtitle);
    headerLayout->addLayout(copy, 1);

    auto* nav = new QFrame(header);
    nav->setObjectName(QStringLiteral("reviewWorkspaceNav"));
    auto* navLayout = new QHBoxLayout(nav);
    navLayout->setContentsMargins(4, 4, 4, 4);
    navLayout->setSpacing(3);
    auto* group = new QButtonGroup(nav);
    group->setExclusive(true);

    struct Item { const char* text; int target; };
    const Item items[] = {
        {"Diagnóstico", 1},
        {"Maquetación", 2},
        {"Proyecto", 0},
        {"Ambientación", 3}
    };

    for (const Item& item : items) {
        auto* button = new QPushButton(tr(item.text), nav);
        button->setObjectName(QStringLiteral("reviewWorkspaceSection"));
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setProperty("targetTab", item.target);
        button->setChecked(item.target == 1);
        group->addButton(button);
        navLayout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, item]() {
            if (!tabs_ || item.target < 0 || item.target >= tabs_->count()) return;
            tabs_->setCurrentIndex(item.target);
            if (item.target == 1) refreshReviewDashboard();
        });
    }
    headerLayout->addWidget(nav, 0, Qt::AlignVCenter);
    root->insertWidget(0, header);

    connect(tabs_, &QTabWidget::currentChanged, this, [this, title, subtitle, group](int index) {
        title->setText(sectionTitle(index));
        subtitle->setText(sectionCopy(index));
        for (QAbstractButton* abstract : group->buttons()) {
            auto* button = qobject_cast<QPushButton*>(abstract);
            if (button) button->setChecked(button->property("targetTab").toInt() == index);
        }
    });

    tabs_->setCurrentIndex(1);

    setStyleSheet(styleSheet() + QStringLiteral(R"QSS(
/* Review workspace: these rules deliberately override the legacy light-only tab surfaces. */
#reviewWorkspaceHeader[reviewTheme="dark"]{background:#0b1016;border:0;border-bottom:1px solid #26313b;}
#reviewWorkspaceHeader[reviewTheme="light"]{background:#f7f5f0;border:0;border-bottom:1px solid #d8d0c4;}
#reviewWorkspaceTitle[reviewTheme="dark"]{color:#f3efe8;font-family:'Georgia';font-size:20pt;font-weight:600;}
#reviewWorkspaceTitle[reviewTheme="light"]{color:#302d28;font-family:'Georgia';font-size:20pt;font-weight:600;}
#reviewWorkspaceSubtitle[reviewTheme="dark"]{color:#7f8b96;font-size:8.8pt;}
#reviewWorkspaceSubtitle[reviewTheme="light"]{color:#756f66;font-size:8.8pt;}
#reviewWorkspaceNav[reviewTheme="dark"]{background:#101720;border:1px solid #293541;border-radius:9px;}
#reviewWorkspaceNav[reviewTheme="light"]{background:#eee9e0;border:1px solid #d4ccbf;border-radius:9px;}
#reviewWorkspaceSection[reviewTheme="dark"]{background:transparent;color:#8f9ba6;border:0;border-radius:6px;padding:8px 11px;}
#reviewWorkspaceSection[reviewTheme="dark"]:hover{background:#18232d;color:#ffffff;}
#reviewWorkspaceSection[reviewTheme="dark"]:checked{background:#173447;color:#89d0e3;border:1px solid #326b7c;}
#reviewWorkspaceSection[reviewTheme="light"]{background:transparent;color:#6e675e;border:0;border-radius:6px;padding:8px 11px;}
#reviewWorkspaceSection[reviewTheme="light"]:hover{background:#e2dbd0;color:#2e2a25;}
#reviewWorkspaceSection[reviewTheme="light"]:checked{background:#e0ecec;color:#23696d;border:1px solid #8ab7b8;}

#reviewPage[reviewTheme="dark"],#reviewTabs[reviewTheme="dark"]::pane,#reviewProjectTab[reviewTheme="dark"],#reviewAnalysisTab[reviewTheme="dark"],#reviewLayoutTab[reviewTheme="dark"],#reviewMediaTab[reviewTheme="dark"]{background:#0b0f14;color:#dfe4ea;border:0;}
#reviewPage[reviewTheme="light"],#reviewTabs[reviewTheme="light"]::pane,#reviewProjectTab[reviewTheme="light"],#reviewAnalysisTab[reviewTheme="light"],#reviewLayoutTab[reviewTheme="light"],#reviewMediaTab[reviewTheme="light"]{background:#f3f1ec;color:#302d28;border:0;}

#reviewCard[reviewTheme="dark"],#reviewDiagnosis[reviewTheme="dark"],#reviewStats[reviewTheme="dark"],#reviewExportRail[reviewTheme="dark"],#reviewIssueCard[reviewTheme="dark"]{background:#10161d;border:1px solid #27323d;border-radius:10px;}
#reviewCard[reviewTheme="light"],#reviewDiagnosis[reviewTheme="light"],#reviewStats[reviewTheme="light"],#reviewExportRail[reviewTheme="light"],#reviewIssueCard[reviewTheme="light"]{background:#fffdf8;border:1px solid #d8d1c6;border-radius:10px;}
#reviewCardTitle[reviewTheme="dark"],#reviewFieldTitle[reviewTheme="dark"],#reviewExportTitle[reviewTheme="dark"],#reviewIssueTitle[reviewTheme="dark"],#reviewDiagnosisTitle[reviewTheme="dark"]{color:#eee9e1;}
#reviewCardTitle[reviewTheme="light"],#reviewFieldTitle[reviewTheme="light"],#reviewExportTitle[reviewTheme="light"],#reviewIssueTitle[reviewTheme="light"],#reviewDiagnosisTitle[reviewTheme="light"]{color:#37322c;}
#reviewDescription[reviewTheme="dark"],#reviewExportCopy[reviewTheme="dark"],#reviewDiagnosisCopy[reviewTheme="dark"]{color:#87939e;}
#reviewDescription[reviewTheme="light"],#reviewExportCopy[reviewTheme="light"],#reviewDiagnosisCopy[reviewTheme="light"]{color:#746d64;}

QLineEdit[reviewTheme="dark"],QTextEdit[reviewTheme="dark"],QComboBox[reviewTheme="dark"],QSpinBox[reviewTheme="dark"],QDoubleSpinBox[reviewTheme="dark"],QListWidget[reviewTheme="dark"]{background:#0d1319;color:#dce4ea;border:1px solid #2b3742;border-radius:7px;selection-background-color:#254b63;}
QLineEdit[reviewTheme="light"],QTextEdit[reviewTheme="light"],QComboBox[reviewTheme="light"],QSpinBox[reviewTheme="light"],QDoubleSpinBox[reviewTheme="light"],QListWidget[reviewTheme="light"]{background:#fffefb;color:#302d28;border:1px solid #d4cdc2;border-radius:7px;selection-background-color:#cce0e1;selection-color:#203336;}
#reviewImage[reviewTheme="dark"]{background:#0d1319;color:#77838e;border:1px solid #2b3742;}
#reviewImage[reviewTheme="light"]{background:#f7f3ec;color:#81786e;border:1px solid #d4cdc2;}
#reviewScore[reviewTheme="light"]{background:#fffdf8;color:#302d28;border-color:#399b72;}
#reviewMetric[reviewTheme="light"]{background:#f8f5ef;border-color:#ded6ca;}
#reviewMetricValue[reviewTheme="light"]{color:#312e29;}
#reviewMetricLabel[reviewTheme="light"]{color:#756e65;}
#reviewIssueRow[reviewTheme="light"]{background:#f5f1ea;color:#514b44;}
#reviewAnalysisTab[reviewTheme="light"] QListWidget{background:#fffdf8;color:#37322d;border-color:#d8d1c6;}
#reviewAnalysisTab[reviewTheme="light"] QListWidget::item{border-bottom:1px solid #e5ded4;}
#reviewAnalysisTab[reviewTheme="light"] QListWidget::item:selected{background:#e0ecec;color:#285e62;}
QScrollArea[reviewTheme="dark"]>QWidget>QWidget{background:#0b0f14;}
QScrollArea[reviewTheme="light"]>QWidget>QWidget{background:#f3f1ec;}
)QSS"));

    refreshProductTheme();
}

void ReviewPage::refreshProductTheme() {
    const QString theme = (qApp && qApp->property("wbwDarkMode").toBool()) ? QStringLiteral("dark") : QStringLiteral("light");
    setProperty("reviewTheme", theme);
    const auto widgets = findChildren<QWidget*>();
    for (QWidget* widget : widgets) {
        if (!widget) continue;
        widget->setProperty("reviewTheme", theme);
        if (widget->style()) {
            widget->style()->unpolish(widget);
            widget->style()->polish(widget);
        }
        widget->update();
    }
    if (style()) {
        style()->unpolish(this);
        style()->polish(this);
    }
    update();
}

} // namespace wbw
