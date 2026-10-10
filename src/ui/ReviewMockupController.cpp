#include "ui/ReviewMockupController.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMainWindow>
#include <QPointer>
#include <QPushButton>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace wbw {
namespace {

enum class ReviewSection { Summary, Consistency, Language, Structure, Style, Notes };

bool matchesSection(const QString& text, ReviewSection section) {
    const QString value = text.trimmed().toLower();
    switch (section) {
        case ReviewSection::Summary: return true;
        case ReviewSection::Consistency: return value.contains(QObject::tr("consistencia").toLower()) || value.contains(QObject::tr("continuidad").toLower()) || value.startsWith(QObject::tr("Repetición cercana:").toLower());
        case ReviewSection::Language: return value.contains(QObject::tr("ortografía").toLower()) || value.contains(QObject::tr("gramática").toLower()) || value.startsWith(QObject::tr("Muletilla:").toLower()) || value.startsWith(QObject::tr("Palabra duplicada consecutiva:").toLower()) || value.startsWith(QObject::tr("Espacios dobles o múltiples:").toLower());
        case ReviewSection::Structure: return value.contains(QObject::tr("estructura").toLower()) || value.contains(QObject::tr("capítulo").toLower()) || value.contains(QObject::tr("escena").toLower()) || value.contains(QObject::tr("ritmo").toLower());
        case ReviewSection::Style: return value.contains(QObject::tr("estilo").toLower()) || value.contains(QObject::tr("voz").toLower()) || value.contains(QObject::tr("pov").toLower()) || value.contains(QObject::tr("muletilla").toLower());
        case ReviewSection::Notes: return value.contains(QObject::tr("nota").toLower()) || value.contains(QObject::tr("pendiente").toLower()) || value.contains(QObject::tr("observación").toLower());
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
        for (QWidget* hero : window_->findChildren<QWidget*>(QStringLiteral("reviewHero"))) hero->hide();

        if (!applied_) {
            applied_ = true;
            auto* barFrame = new QFrame(analysis);
            barFrame->setObjectName(QStringLiteral("reviewSectionBar"));
            auto* bar = new QHBoxLayout(barFrame);
            bar->setContentsMargins(8, 7, 8, 7);
            bar->setSpacing(5);
            addFilter(barFrame, bar, QObject::tr("Resumen"), ReviewSection::Summary, true);
            addFilter(barFrame, bar, QObject::tr("Consistencia"), ReviewSection::Consistency);
            addFilter(barFrame, bar, QObject::tr("Lenguaje"), ReviewSection::Language);
            addFilter(barFrame, bar, QObject::tr("Estructura"), ReviewSection::Structure);
            addFilter(barFrame, bar, QObject::tr("Estilo"), ReviewSection::Style);
            addFilter(barFrame, bar, QObject::tr("Notas"), ReviewSection::Notes);
            bar->addStretch(1);
            layout->insertWidget(0, barFrame);
        }

        analysis->setStyleSheet(QStringLiteral(R"QSS(
#reviewAnalysisTab{background:#0a0c0f;color:#d9d4ca;}
#reviewSectionBar{background:#0d1013;border:1px solid #2b2b27;border-radius:10px;}
#reviewFilterButton{background:transparent;color:#888a84;border:0;border-radius:7px;padding:8px 11px;min-height:30px;}
#reviewFilterButton:hover{background:#191b1b;color:#f1ede4;}
#reviewFilterButton:checked{background:#30271f;color:#e8c48d;border:1px solid #6a5136;}
#reviewAnalysisTab #reviewCard,#reviewDashboard{background:#111416;border:1px solid #2a2b27;border-radius:10px;}
#reviewAnalysisTab #reviewCardTitle{color:#f1ede4;font-family:'Georgia';font-size:13pt;font-weight:700;}
#reviewAnalysisTab QListWidget{background:#0d1013;color:#d3cfc6;border:1px solid #2b2b27;border-radius:8px;outline:0;}
#reviewAnalysisTab QListWidget::item{padding:9px 10px;border-bottom:1px solid #23241f;}
#reviewAnalysisTab QListWidget::item:hover{background:#171a1d;}
#reviewAnalysisTab QListWidget::item:selected{background:#28251f;color:#e7c28a;}
#reviewAnalysisTab QTextEdit,#reviewAnalysisTab QSpinBox{background:#0d1013;color:#d9d4ca;border:1px solid #30312c;border-radius:7px;padding:8px;}
#reviewDashboardTitle{font-family:'Georgia';font-size:17pt;color:#f2eee5;font-weight:700;}
#reviewDashboardCopy{color:#7d7f79;}
#reviewMetric{background:#151817;border:1px solid #2b2b27;border-radius:9px;}
#reviewMetricValue{font-size:18pt;font-weight:700;color:#f1ede4;}
#reviewMetricLabel{color:#7f817c;font-size:8pt;}
)QSS"));
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
        for (QListWidget* list : window_->findChildren<QListWidget*>()) {
            QWidget* parent = list->parentWidget();
            bool inAnalysis = false;
            while (parent) {
                if (parent->objectName() == QStringLiteral("reviewAnalysisTab")) { inAnalysis = true; break; }
                parent = parent->parentWidget();
            }
            if (!inAnalysis) continue;
            for (int i = 0; i < list->count(); ++i)
                if (QListWidgetItem* item = list->item(i)) item->setHidden(!matchesSection(item->text(), section));
        }
    }

    QPointer<QMainWindow> window_;
    bool applied_ = false;
};

} // namespace

void installReviewMockupController(QMainWindow* window) {
    if (!window || window->property("wbwReviewMockupControllerV3").toBool()) return;
    window->setProperty("wbwReviewMockupControllerV3", true);
    auto* filter = new ReviewWorkspaceFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
