#include "ui/ReviewMockupController.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QPointer>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace wbw {
namespace {

void applyReviewStyle(QWidget* analysis) {
    if (!analysis) return;
    analysis->setStyleSheet(QStringLiteral(R"QSS(
#reviewAnalysisTab{background:palette(window);color:palette(window-text);}
#reviewSectionBar{background:palette(base);border:1px solid palette(mid);border-radius:9px;}
#reviewFilterButton{background:transparent;color:palette(window-text);border:0;border-radius:6px;padding:7px 10px;}
#reviewFilterButton:hover{background:palette(alternate-base);color:palette(text);}
#reviewFilterButton:checked{background:palette(alternate-base);color:#b98a53;border:1px solid #8b765d;}
#reviewAnalysisTab #reviewCard,#reviewDashboard{background:palette(base);border:1px solid palette(mid);border-radius:10px;}
#reviewAnalysisTab #reviewCardTitle{color:palette(text);font-family:'Georgia';font-size:13pt;}
#reviewAnalysisTab QListWidget{background:palette(base);color:palette(text);border:1px solid palette(mid);border-radius:8px;outline:0;}
#reviewAnalysisTab QListWidget::item{padding:8px;border-bottom:1px solid palette(mid);}
#reviewAnalysisTab QListWidget::item:selected{background:palette(highlight);color:palette(highlighted-text);}
#reviewAnalysisTab QTextEdit,#reviewAnalysisTab QSpinBox{background:palette(base);color:palette(text);border:1px solid palette(mid);border-radius:7px;padding:7px;}
)QSS"));
}

class ReviewWorkspaceFilter final : public QObject {
public:
    explicit ReviewWorkspaceFilter(QMainWindow* window) : QObject(window), window_(window) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        Q_UNUSED(watched);
        if (event->type() == QEvent::Show || event->type() == QEvent::LayoutRequest || event->type() == QEvent::PaletteChange)
            QTimer::singleShot(0, this, [this]() { apply(); });
        return false;
    }

    void apply() {
        if (!window_) return;
        auto* tabs = window_->findChild<QTabWidget*>(QStringLiteral("reviewTabs"));
        QWidget* analysis = window_->findChild<QWidget*>(QStringLiteral("reviewAnalysisTab"));
        if (!tabs || !analysis) return;
        auto* layout = qobject_cast<QVBoxLayout*>(analysis->layout());
        if (!layout) return;

        if (applied_) {
            applyReviewStyle(analysis);
            return;
        }

        applied_ = true;
        tabs->setTabText(0, QObject::tr("Resumen"));
        tabs->setTabText(1, QObject::tr("Consistencia / lenguaje"));
        tabs->setTabText(2, QObject::tr("Estructura / salida"));
        tabs->setTabText(3, QObject::tr("Notas / ambiente"));

        const auto heroes = window_->findChildren<QWidget*>(QStringLiteral("reviewHero"));
        for (QWidget* hero : heroes) hero->hide();

        auto* sectionBar = new QFrame(analysis);
        sectionBar->setObjectName(QStringLiteral("reviewSectionBar"));
        auto* bar = new QHBoxLayout(sectionBar);
        bar->setContentsMargins(8, 7, 8, 7);
        bar->setSpacing(5);

        auto addFilter = [this, sectionBar, bar](const QString& text, const QString& key) {
            auto* button = new QPushButton(text, sectionBar);
            button->setObjectName(QStringLiteral("reviewFilterButton"));
            button->setCheckable(true);
            button->setAutoExclusive(true);
            if (key.isEmpty()) button->setChecked(true);
            bar->addWidget(button);
            QObject::connect(button, &QPushButton::clicked, sectionBar, [this, key]() { filter(key); });
        };
        addFilter(QObject::tr("Resumen"), QString());
        addFilter(QObject::tr("Repeticiones"), QStringLiteral("Repetición cercana:"));
        addFilter(QObject::tr("Muletillas"), QStringLiteral("Muletilla:"));
        addFilter(QObject::tr("Mecánica"), QStringLiteral("mechanical"));
        bar->addStretch();
        layout->insertWidget(0, sectionBar);
        applyReviewStyle(analysis);
    }

private:
    void filter(const QString& key) {
        if (!window_) return;
        QListWidget* list = nullptr;
        const auto lists = window_->findChildren<QListWidget*>();
        for (QListWidget* candidate : lists) {
            QWidget* p = candidate->parentWidget();
            bool inAnalysis = false;
            while (p) {
                if (p->objectName() == QStringLiteral("reviewAnalysisTab")) { inAnalysis = true; break; }
                p = p->parentWidget();
            }
            if (inAnalysis) { list = candidate; break; }
        }
        if (!list) return;
        for (int i = 0; i < list->count(); ++i) {
            QListWidgetItem* item = list->item(i);
            if (!item) continue;
            const QString text = item->text();
            bool visible = key.isEmpty();
            if (key == QStringLiteral("mechanical"))
                visible = text.startsWith(QObject::tr("Palabra duplicada consecutiva:")) || text.startsWith(QObject::tr("Espacios dobles o múltiples:"));
            else if (!key.isEmpty()) visible = text.startsWith(key);
            item->setHidden(!visible);
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
    const auto widgets = window->findChildren<QWidget*>();
    for (QWidget* widget : widgets) widget->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
