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
#include <QPushButton>
#include <QRegularExpression>
#include <QShowEvent>
#include <QTabWidget>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>

namespace wbw {
namespace {

QString sceneText(const QJsonObject& scene) {
    if (scene.contains(QStringLiteral("text"))) return scene.value(QStringLiteral("text")).toString();
    QTextDocument legacy;
    legacy.setHtml(scene.value(QStringLiteral("content")).toString());
    return legacy.toPlainText();
}

QFrame* metricCard(const QString& label, QLabel*& value) {
    auto* frame = new QFrame;
    frame->setObjectName(QStringLiteral("reviewMetric"));
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(13, 11, 13, 11);
    layout->setSpacing(2);
    value = new QLabel(QStringLiteral("—"), frame);
    value->setObjectName(QStringLiteral("reviewMetricValue"));
    auto* title = new QLabel(label, frame);
    title->setObjectName(QStringLiteral("reviewMetricLabel"));
    layout->addWidget(value);
    layout->addWidget(title);
    return frame;
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

    auto* dashboard = new QFrame(analysisTab);
    dashboard->setObjectName(QStringLiteral("reviewDashboard"));
    auto* dashboardLayout = new QVBoxLayout(dashboard);
    dashboardLayout->setContentsMargins(14, 12, 14, 14);
    dashboardLayout->setSpacing(10);

    auto* headingRow = new QHBoxLayout;
    auto* heading = new QLabel(tr("Estado del manuscrito"), dashboard);
    heading->setObjectName(QStringLiteral("reviewDashboardTitle"));
    auto* copy = new QLabel(tr("Métricas reales del documento y acceso directo a la salida editorial."), dashboard);
    copy->setObjectName(QStringLiteral("reviewDashboardCopy"));
    headingRow->addWidget(heading);
    headingRow->addSpacing(8);
    headingRow->addWidget(copy);
    headingRow->addStretch();

    auto* pdf = new QPushButton(tr("Exportar PDF"), dashboard);
    pdf->setObjectName(QStringLiteral("reviewPrimary"));
    auto* wbw = new QPushButton(tr("Proyecto .wbw"), dashboard);
    auto* backup = new QPushButton(tr("Copia"), dashboard);
    headingRow->addWidget(pdf);
    headingRow->addWidget(wbw);
    headingRow->addWidget(backup);
    dashboardLayout->addLayout(headingRow);

    auto* metrics = new QGridLayout;
    metrics->setHorizontalSpacing(8);
    metrics->setVerticalSpacing(8);
    metrics->addWidget(metricCard(tr("Palabras"), dashboardWords_), 0, 0);
    metrics->addWidget(metricCard(tr("Capítulos"), dashboardChapters_), 0, 1);
    metrics->addWidget(metricCard(tr("Escenas"), dashboardScenes_), 0, 2);
    metrics->addWidget(metricCard(tr("Incidencias"), dashboardIssues_), 0, 3);
    for (int column = 0; column < 4; ++column) metrics->setColumnStretch(column, 1);
    dashboardLayout->addLayout(metrics);

    root->insertWidget(qMin(1, root->count()), dashboard);
    root->setContentsMargins(16, 12, 16, 16);
    root->setSpacing(9);

    connect(pdf, &QPushButton::clicked, this, &ReviewPage::requestExportPdf);
    connect(wbw, &QPushButton::clicked, this, &ReviewPage::requestExportWbw);
    connect(backup, &QPushButton::clicked, this, &ReviewPage::requestBackup);

    for (QPushButton* button : analysisTab->findChildren<QPushButton*>()) {
        if (button->text() == tr("Analizar manuscrito")) {
            connect(button, &QPushButton::clicked, this, [this]() {
                QTimer::singleShot(0, this, &ReviewPage::refreshReviewDashboard);
            });
            break;
        }
    }

    reviewChromeApplied_ = true;
    setStyleSheet(styleSheet() + QStringLiteral(
        "#reviewDashboard{background:#141a22;border:1px solid #2a3440;border-radius:7px;}"
        "#reviewDashboardTitle{font-size:12pt;font-weight:700;color:#e8edf4;}"
        "#reviewDashboardCopy{color:#7f8c9d;}"
        "#reviewMetric{background:#11161d;border:1px solid #27313d;border-radius:6px;}"
        "#reviewMetricValue{font-size:16pt;font-weight:700;color:#f0f5fb;}"
        "#reviewMetricLabel{color:#788698;font-size:8pt;}"
    ));
}

void ReviewPage::refreshReviewDashboard() {
    if (!reviewChromeApplied_ || !document_) return;

    int chapters = 0;
    int scenes = 0;
    int words = 0;
    const QJsonArray chapterArray = document_->array(QStringLiteral("writingChapters"));
    chapters = chapterArray.size();
    for (const QJsonValue chapterValue : chapterArray) {
        const QJsonArray sceneArray = chapterValue.toObject().value(QStringLiteral("scenes")).toArray();
        scenes += sceneArray.size();
        for (const QJsonValue sceneValue : sceneArray) {
            const QString text = sceneText(sceneValue.toObject());
            words += text.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts).size();
        }
    }

    if (dashboardWords_) dashboardWords_->setText(QLocale().toString(words));
    if (dashboardChapters_) dashboardChapters_->setText(QString::number(chapters));
    if (dashboardScenes_) dashboardScenes_->setText(QString::number(scenes));
    if (dashboardIssues_) dashboardIssues_->setText(QString::number(analysisResults_ ? analysisResults_->count() : 0));
}

} // namespace wbw
