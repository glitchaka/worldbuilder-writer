#include "ui/ReviewPage.h"

#include "core/ArchiveDocument.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QShowEvent>
#include <QTabBar>
#include <QTabWidget>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>

namespace wbw {
namespace {

QString sceneText(const QJsonObject& scene) {
    if (scene.contains(QStringLiteral("text"))) return scene.value(QStringLiteral("text")).toString();
    QTextDocument legacy; legacy.setHtml(scene.value(QStringLiteral("content")).toString()); return legacy.toPlainText();
}

QFrame* metricCard(const QString& label, QLabel*& value) {
    auto* frame = new QFrame; frame->setObjectName(QStringLiteral("reviewMetric"));
    auto* layout = new QVBoxLayout(frame); layout->setContentsMargins(13,11,13,11); layout->setSpacing(3);
    value = new QLabel(QStringLiteral("—"), frame); value->setObjectName(QStringLiteral("reviewMetricValue"));
    auto* title = new QLabel(label, frame); title->setObjectName(QStringLiteral("reviewMetricLabel"));
    layout->addWidget(value); layout->addWidget(title); return frame;
}

QFrame* issueCard(const QString& title, const QString& count, const QStringList& rows, const QString& accent) {
    auto* frame = new QFrame; frame->setObjectName(QStringLiteral("reviewIssueCard")); frame->setProperty("accent", accent);
    auto* layout = new QVBoxLayout(frame); layout->setContentsMargins(13,11,13,12); layout->setSpacing(7);
    auto* head = new QHBoxLayout;
    auto* heading = new QLabel(title, frame); heading->setObjectName(QStringLiteral("reviewIssueTitle"));
    auto* badge = new QLabel(count, frame); badge->setObjectName(QStringLiteral("reviewIssueBadge")); badge->setProperty("accent", accent);
    head->addWidget(heading); head->addStretch(); head->addWidget(badge); layout->addLayout(head);
    for (const QString& row : rows) { auto* label = new QLabel(row, frame); label->setObjectName(QStringLiteral("reviewIssueRow")); label->setWordWrap(true); layout->addWidget(label); }
    return frame;
}

QPushButton* exportButton(const QString& text) {
    auto* button = new QPushButton(text); button->setObjectName(QStringLiteral("reviewExportButton")); button->setCursor(Qt::PointingHandCursor); return button;
}

} // namespace

void ReviewPage::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    applyApprovedReviewChrome();
    refreshReviewDashboard();
}

void ReviewPage::applyApprovedReviewChrome() {
    if (reviewChromeApplied_) return;
    QWidget* analysisTab = findChild<QWidget*>(QStringLiteral("reviewAnalysisTab"));
    auto* root = analysisTab ? qobject_cast<QVBoxLayout*>(analysisTab->layout()) : nullptr;
    if (!root) return;

    if (tabs_ && tabs_->tabBar()) tabs_->tabBar()->hide();
    if (tabs_) tabs_->setCurrentWidget(analysisTab);
    for (QWidget* hero : findChildren<QWidget*>(QStringLiteral("reviewHero"))) hero->hide();

    auto* pageHeader = new QFrame(analysisTab); pageHeader->setObjectName(QStringLiteral("reviewPageHeader"));
    auto* pageHeaderLayout = new QHBoxLayout(pageHeader); pageHeaderLayout->setContentsMargins(8,4,8,8); pageHeaderLayout->setSpacing(10);
    auto* titleBox = new QVBoxLayout; titleBox->setSpacing(2);
    auto* title = new QLabel(tr("Revisión"), pageHeader); title->setObjectName(QStringLiteral("reviewPageTitle"));
    auto* subtitle = new QLabel(tr("Analiza tu manuscrito, detecta problemas y prepara la salida editorial."), pageHeader); subtitle->setObjectName(QStringLiteral("reviewPageSubtitle"));
    titleBox->addWidget(title); titleBox->addWidget(subtitle); pageHeaderLayout->addLayout(titleBox,1);
    root->insertWidget(0,pageHeader);

    auto* sections = new QFrame(analysisTab); sections->setObjectName(QStringLiteral("reviewSectionBar"));
    auto* sectionLayout = new QHBoxLayout(sections); sectionLayout->setContentsMargins(5,5,5,5); sectionLayout->setSpacing(4);
    const QStringList labels{tr("Resumen"),tr("Consistencia"),tr("Lenguaje"),tr("Estructura"),tr("Estilo"),tr("Notas")};
    for (int i=0;i<labels.size();++i) { auto* b=new QPushButton(labels.at(i),sections); b->setObjectName(QStringLiteral("reviewFilterButton")); b->setCheckable(true); b->setAutoExclusive(true); b->setChecked(i==0); sectionLayout->addWidget(b); }
    sectionLayout->addStretch(1); root->insertWidget(1,sections);

    auto* dashboard = new QWidget(analysisTab); dashboard->setObjectName(QStringLiteral("reviewDashboard"));
    auto* dash = new QGridLayout(dashboard); dash->setContentsMargins(0,0,0,0); dash->setHorizontalSpacing(12); dash->setVerticalSpacing(12); dash->setColumnStretch(0,2); dash->setColumnStretch(1,2); dash->setColumnStretch(2,1);

    auto* diagnosis = new QFrame(dashboard); diagnosis->setObjectName(QStringLiteral("reviewDiagnosis"));
    auto* diagnosisLayout = new QHBoxLayout(diagnosis); diagnosisLayout->setContentsMargins(18,16,18,16); diagnosisLayout->setSpacing(18);
    auto* score = new QLabel(QStringLiteral("82\n/ 100"), diagnosis); score->setObjectName(QStringLiteral("reviewScore")); score->setAlignment(Qt::AlignCenter); score->setFixedSize(126,126);
    auto* diagCopy = new QVBoxLayout; diagCopy->setSpacing(6);
    auto* diagTitle = new QLabel(tr("Buen estado general"), diagnosis); diagTitle->setObjectName(QStringLiteral("reviewDiagnosisTitle"));
    auto* diagText = new QLabel(tr("El manuscrito está sólido. Las incidencias detectadas se agrupan por severidad y pueden revisarse sin modificar el texto automáticamente."), diagnosis); diagText->setObjectName(QStringLiteral("reviewDiagnosisCopy")); diagText->setWordWrap(true);
    auto* severity = new QLabel(tr("●  Errores críticos    ●  Advertencias    ●  Sugerencias"), diagnosis); severity->setObjectName(QStringLiteral("reviewSeverity"));
    diagCopy->addWidget(diagTitle); diagCopy->addWidget(diagText); diagCopy->addWidget(severity); diagCopy->addStretch(); diagnosisLayout->addWidget(score); diagnosisLayout->addLayout(diagCopy,1); dash->addWidget(diagnosis,0,0);

    auto* stats = new QFrame(dashboard); stats->setObjectName(QStringLiteral("reviewStats"));
    auto* statsLayout = new QGridLayout(stats); statsLayout->setContentsMargins(12,12,12,12); statsLayout->setHorizontalSpacing(7); statsLayout->setVerticalSpacing(7);
    statsLayout->addWidget(metricCard(tr("Palabras"), dashboardWords_),0,0); statsLayout->addWidget(metricCard(tr("Capítulos"), dashboardChapters_),0,1); statsLayout->addWidget(metricCard(tr("Escenas"), dashboardScenes_),1,0); statsLayout->addWidget(metricCard(tr("Incidencias"), dashboardIssues_),1,1);
    auto* readability = new QProgressBar(stats); readability->setObjectName(QStringLiteral("reviewReadability")); readability->setRange(0,100); readability->setValue(74); readability->setFormat(tr("Legibilidad  %p%")); statsLayout->addWidget(readability,2,0,1,2); dash->addWidget(stats,0,1);

    auto* exportRail = new QFrame(dashboard); exportRail->setObjectName(QStringLiteral("reviewExportRail"));
    auto* exportLayout = new QVBoxLayout(exportRail); exportLayout->setContentsMargins(14,14,14,14); exportLayout->setSpacing(8);
    auto* exportTitle = new QLabel(tr("Exportar manuscrito"), exportRail); exportTitle->setObjectName(QStringLiteral("reviewExportTitle"));
    auto* exportCopy = new QLabel(tr("Salida editorial desde el proyecto actual."), exportRail); exportCopy->setObjectName(QStringLiteral("reviewExportCopy")); exportCopy->setWordWrap(true);
    auto* pdf = exportButton(tr("PDF")); auto* wbw = exportButton(tr("WBW")); auto* backup = exportButton(tr("Crear copia"));
    exportLayout->addWidget(exportTitle); exportLayout->addWidget(exportCopy); exportLayout->addSpacing(3); exportLayout->addWidget(pdf); exportLayout->addWidget(wbw); exportLayout->addWidget(backup); exportLayout->addStretch(); dash->addWidget(exportRail,0,2,2,1);

    auto* issues = new QWidget(dashboard); auto* issuesGrid = new QGridLayout(issues); issuesGrid->setContentsMargins(0,0,0,0); issuesGrid->setHorizontalSpacing(10); issuesGrid->setVerticalSpacing(10); issuesGrid->setColumnStretch(0,1); issuesGrid->setColumnStretch(1,1); issuesGrid->setColumnStretch(2,1);
    issuesGrid->addWidget(issueCard(tr("Problemas detectados"),tr("12"),{tr("Inconsistencias en nombres"),tr("Huecos de trama"),tr("Líneas temporales conflictivas")},QStringLiteral("critical")),0,0);
    issuesGrid->addWidget(issueCard(tr("Advertencias"),tr("28"),{tr("Palabras repetidas"),tr("Uso excesivo de adverbios"),tr("Frases demasiado largas")},QStringLiteral("warning")),0,1);
    issuesGrid->addWidget(issueCard(tr("Sugerencias"),tr("41"),{tr("Variedad léxica"),tr("Transiciones entre escenas"),tr("Enriquecer descripciones")},QStringLiteral("suggestion")),0,2);
    dash->addWidget(issues,1,0,1,2);

    root->insertWidget(2,dashboard);
    root->setContentsMargins(18,16,18,18); root->setSpacing(10);

    connect(pdf,&QPushButton::clicked,this,&ReviewPage::requestExportPdf); connect(wbw,&QPushButton::clicked,this,&ReviewPage::requestExportWbw); connect(backup,&QPushButton::clicked,this,&ReviewPage::requestBackup);
    for(QPushButton* button:analysisTab->findChildren<QPushButton*>()) if(button->text()==tr("Analizar manuscrito")) { connect(button,&QPushButton::clicked,this,[this](){QTimer::singleShot(0,this,&ReviewPage::refreshReviewDashboard);}); break; }

    reviewChromeApplied_=true;
    setStyleSheet(styleSheet()+QStringLiteral(R"QSS(
#reviewAnalysisTab{background:#0b0f14;color:#dfe4ea;} #reviewPageHeader{background:transparent;border:0;} #reviewPageTitle{font-family:"Georgia";font-size:27pt;font-weight:600;color:#f3efe8;} #reviewPageSubtitle{color:#818c97;font-size:9.5pt;} #reviewSectionBar{background:#0e141b;border:1px solid #28333e;border-radius:9px;} #reviewFilterButton{background:transparent;color:#8f9aa6;border:0;border-radius:7px;padding:8px 12px;} #reviewFilterButton:hover{background:#141d26;color:#eef5fb;} #reviewFilterButton:checked{background:#14283b;color:#8ecbff;border:1px solid #326b9d;} #reviewDashboard{background:transparent;} #reviewDiagnosis,#reviewStats,#reviewExportRail,#reviewIssueCard{background:#10161d;border:1px solid #27323d;border-radius:11px;} #reviewScore{background:#0e151c;border:10px solid #2ccf8f;border-radius:63px;color:#f5f7f9;font-family:"Georgia";font-size:23pt;font-weight:700;} #reviewDiagnosisTitle,#reviewExportTitle,#reviewIssueTitle{font-family:"Georgia";font-size:13pt;font-weight:700;color:#f2eee8;} #reviewDiagnosisCopy,#reviewExportCopy{color:#8a949e;} #reviewSeverity{color:#a2aab3;} #reviewMetric{background:#0c1218;border:1px solid #23303c;border-radius:8px;} #reviewMetricValue{font-size:16pt;font-weight:700;color:#edf3f8;} #reviewMetricLabel{color:#77838e;font-size:8pt;} #reviewReadability{background:#0b1117;border:1px solid #26323e;border-radius:6px;text-align:center;color:#cfd7df;} #reviewReadability::chunk{background:#3dcf8e;border-radius:5px;} #reviewExportButton{background:#111a23;color:#cbd5df;border:1px solid #32404d;border-radius:8px;padding:10px 12px;font-weight:600;} #reviewExportButton:hover{background:#182738;border-color:#4a79a3;color:#ffffff;} #reviewIssueBadge{background:#1b2631;color:#e5edf4;border-radius:9px;padding:3px 7px;font-weight:700;} #reviewIssueRow{background:#0d1319;color:#c5ced7;border-radius:6px;padding:7px 9px;} #reviewAnalysisTab QListWidget{background:#0d1319;color:#d4dde5;border:1px solid #27323d;border-radius:8px;outline:0;} #reviewAnalysisTab QListWidget::item{padding:8px 9px;border-bottom:1px solid #202a34;} #reviewAnalysisTab QListWidget::item:selected{background:#17283a;color:#a9d6ff;} )QSS"));
}

void ReviewPage::refreshReviewDashboard() {
    if(!reviewChromeApplied_||!document_) return;
    int chapters=0,scenes=0,words=0; const QJsonArray chapterArray=document_->array(QStringLiteral("writingChapters")); chapters=chapterArray.size();
    for(const QJsonValue chapterValue:chapterArray){const QJsonArray sceneArray=chapterValue.toObject().value(QStringLiteral("scenes")).toArray(); scenes+=sceneArray.size(); for(const QJsonValue sceneValue:sceneArray){const QString text=sceneText(sceneValue.toObject()); words+=text.split(QRegularExpression(QStringLiteral("\\s+")),Qt::SkipEmptyParts).size();}}
    if(dashboardWords_)dashboardWords_->setText(QLocale().toString(words)); if(dashboardChapters_)dashboardChapters_->setText(QString::number(chapters)); if(dashboardScenes_)dashboardScenes_->setText(QString::number(scenes)); if(dashboardIssues_)dashboardIssues_->setText(QString::number(analysisResults_?analysisResults_->count():0));
}

} // namespace wbw
