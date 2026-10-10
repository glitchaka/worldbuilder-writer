#include "ui/ReviewMockupController.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QPointer>
#include <QPushButton>
#include <QTabBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace wbw {
namespace {

enum class ReviewSection {
    Summary,
    Consistency,
    Language,
    Structure,
    Style,
    Notes
};

void applyReviewStyle(QWidget* analysis) {
    if (!analysis) return;
    analysis->setStyleSheet(QStringLiteral(R"QSS(
#reviewAnalysisTab{background:#0e1116;color:#d9dee7;}
#reviewSectionBar{background:#12171d;border:1px solid #29313b;border-radius:10px;}
#reviewFilterButton{background:transparent;color:#8f99a7;border:0;border-radius:7px;padding:8px 11px;min-height:30px;}
#reviewFilterButton:hover{background:#1b222b;color:#eef1f5;}
#reviewFilterButton:checked{background:#282118;color:#d5a66c;border:1px solid #6d5438;}
#reviewAnalysisTab #reviewCard,#reviewDashboard{background:#12171d;border:1px solid #29313b;border-radius:10px;}
#reviewAnalysisTab #reviewCardTitle{color:#edf1f5;font-family:'Georgia';font-size:13pt;font-weight:700;}
#reviewAnalysisTab QListWidget{background:#0f141a;color:#d5dbe4;border:1px solid #29313b;border-radius:8px;outline:0;}
#reviewAnalysisTab QListWidget::item{padding:9px 10px;border-bottom:1px solid #222a34;}
#reviewAnalysisTab QListWidget::item:hover{background:#171e27;}
#reviewAnalysisTab QListWidget::item:selected{background:#232d38;color:#ffffff;}
#reviewAnalysisTab QTextEdit,#reviewAnalysisTab QSpinBox{background:#0f141a;color:#d8dde5;border:1px solid #2b3440;border-radius:7px;padding:8px;}
#reviewDashboardTitle{font-family:'Georgia';font-size:16pt;color:#f1f3f5;font-weight:700;}
#reviewDashboardCopy{color:#7e8997;}
#reviewMetric{background:#171d24;border:1px solid #2b3440;border-radius:9px;}
#reviewMetricValue{font-size:18pt;font-weight:700;color:#f2f4f6;}
#reviewMetricLabel{color:#808b98;font-size:8pt;}
#reviewAnalysisTab QScrollBar:vertical{background:#0e1116;width:9px;}
#reviewAnalysisTab QScrollBar::handle:vertical{background:#343c47;border-radius:4px;min-height:34px;}
)QSS"));
}

bool matchesSection(const QString& text, ReviewSection section) {
    const QString normalized = text.trimmed().toLower();
    switch (section) {
        case ReviewSection::Summary:
            return true;
        case ReviewSection::Consistency:
            return normalized.startsWith(QObject::tr("Repetición cercana:").toLower()) ||
                   normalized.contains(QObject::tr("consistencia").toLower()) ||
                   normalized.contains(QObject::tr("continuidad").toLower());
        case ReviewSection::Language:
            return normalized.startsWith(QObject::tr("Muletilla:").toLower()) ||
                   normalized.startsWith(QObject::tr("Palabra duplicada consecutiva:").toLower()) ||
                   normalized.startsWith(QObject::tr("Espacios dobles o múltiples:").toLower()) ||
                   normalized.contains(QObject::tr("ortografía").toLower()) ||
                   normalized.contains(QObject::tr("gramática").toLower());
        case ReviewSection::Structure:
            return normalized.contains(QObject::tr("estructura").toLower()) ||
                   normalized.contains(QObject::tr("capítulo").toLower()) ||
                   normalized.contains(QObject::tr("escena").toLower()) ||
                   normalized.contains(QObject::tr("ritmo").toLower());
        case ReviewSection::Style:
            return normalized.contains(QObject::tr("estilo").toLower()) ||
                   normalized.contains(QObject::tr("voz").toLower()) ||
                   normalized.contains(QObject::tr("pov").toLower()) ||
                   normalized.contains(QObject::tr("repetición").toLower()) ||
                   normalized.contains(QObject::tr("muletilla").toLower());
        case ReviewSection::Notes:
            return normalized.contains(QObject::tr("nota").toLower()) ||
                   normalized.contains(QObject::tr("pendiente").toLower()) ||
                   normalized.contains(QObject::tr("observación").toLower());
    }
    return true;
}

class ReviewWorkspaceFilter final : public QObject {
public:
    explicit ReviewWorkspaceFilter(QMainWindow* window) : QObject(window), window_(window) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == window_ && (event->type() == QEvent::Show || event->type() == QEvent::Resize))
            QTimer::singleShot(0, this, [this]() { apply(); });
        return QObject::eventFilter(watched, event);
    }

    void apply() {
        if (!window_) return;
        auto* tabs = window_->findChild<QTabWidget*>(QStringLiteral("reviewTabs"));
        QWidget* analysis = window_->findChild<QWidget*>(QStringLiteral("reviewAnalysisTab"));
        if (!tabs || !analysis) return;
        auto* layout = qobject_cast<QVBoxLayout*>(analysis->layout());
        if (!layout) return;

        tabs->setDocumentMode(true);
        if (tabs->tabBar()) tabs->tabBar()->hide();
        tabs->setCurrentWidget(analysis);

        const auto heroes = window_->findChildren<QWidget*>(QStringLiteral("reviewHero"));
        for (QWidget* hero : heroes) hero->hide();

        if (!applied_) {
            applied_ = true;
            auto* sectionBar = new QFrame(analysis);
            sectionBar->setObjectName(QStringLiteral("reviewSectionBar"));
            auto* bar = new QHBoxLayout(sectionBar);
            bar->setContentsMargins(8, 7, 8, 7);
            bar->setSpacing(5);

            addFilter(sectionBar, bar, QObject::tr("Resumen"), ReviewSection::Summary, true);
            addFilter(sectionBar, bar, QObject::tr("Consistencia"), ReviewSection::Consistency);
            addFilter(sectionBar, bar, QObject::tr("Lenguaje"), ReviewSection::Language);
            addFilter(sectionBar, bar, QObject::tr("Estructura"), ReviewSection::Structure);
            addFilter(sectionBar, bar, QObject::tr("Estilo"), ReviewSection::Style);
            addFilter(sectionBar, bar, QObject::tr("Notas"), ReviewSection::Notes);
            bar->addStretch();
            layout->insertWidget(0, sectionBar);
        }

        applyReviewStyle(analysis);
    }

private:
    void addFilter(QWidget* parent, QHBoxLayout* bar, const QString& text, ReviewSection section, bool checked = false) {
        auto* button = new QPushButton(text, parent);
        button->setObjectName(QStringLiteral("reviewFilterButton"));
        button->setCheckable(true);
        button->setAutoExclusive(true);
        button->setChecked(checked);
        bar->addWidget(button);
        QObject::connect(button, &QPushButton::clicked, parent, [this, section]() { filter(section); });
    }

    void filter(ReviewSection section) {
        if (!window_) return;
        const auto lists = window_->findChildren<QListWidget*>();
        for (QListWidget* list : lists) {
            QWidget* p = list->parentWidget();
            bool inAnalysis = false;
            while (p) {
                if (p->objectName() == QStringLiteral("reviewAnalysisTab")) { inAnalysis = true; break; }
                p = p->parentWidget();
            }
            if (!inAnalysis) continue;
            for (int i = 0; i < list->count(); ++i) {
                QListWidgetItem* item = list->item(i);
                if (item) item->setHidden(!matchesSection(item->text(), section));
            }
        }
    }

    QPointer<QMainWindow> window_;
    bool applied_ = false;
};

} // namespace

void installReviewMockupController(QMainWindow* window) {
    if (!window || window->property("wbwReviewMockupController").toBool()) return;
    window->setProperty("wbwReviewMockupController", true);
    auto* filter = new ReviewWorkspaceFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
