#include "ui/PlanningMockupController.h"

#include <QEvent>
#include <QFrame>
#include <QLabel>
#include <QMainWindow>
#include <QPointer>
#include <QScrollArea>
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace wbw {
namespace {

class PlanningWorkspaceFilter final : public QObject {
public:
    explicit PlanningWorkspaceFilter(QMainWindow* window) : QObject(window), window_(window) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        Q_UNUSED(watched);
        if (event->type() == QEvent::Show || event->type() == QEvent::Resize || event->type() == QEvent::LayoutRequest)
            QTimer::singleShot(0, this, [this]() { apply(); });
        return false;
    }

    void apply() {
        if (!window_ || applied_) return;
        QWidget* tab = window_->findChild<QWidget*>(QStringLiteral("planningCharacters"));
        QWidget* editor = window_->findChild<QWidget*>(QStringLiteral("planningEditor"));
        QWidget* index = window_->findChild<QWidget*>(QStringLiteral("planningIndex"));
        if (!tab || !editor || !index) return;

        QSplitter* split = nullptr;
        const auto splitters = tab->findChildren<QSplitter*>(QString(), Qt::FindDirectChildrenOnly);
        if (!splitters.isEmpty()) split = splitters.first();
        if (!split || split->count() < 2) return;

        applied_ = true;

        if (QWidget* hero = window_->findChild<QWidget*>(QStringLiteral("planningHero"))) hero->hide();
        if (auto* outer = qobject_cast<QVBoxLayout*>(tab->layout())) {
            outer->setContentsMargins(0, 0, 0, 0);
            outer->setSpacing(0);
        }

        index->setMinimumWidth(220);
        index->setMaximumWidth(280);

        auto* context = new QFrame(split);
        context->setObjectName(QStringLiteral("planningContextRail"));
        context->setMinimumWidth(250);
        context->setMaximumWidth(320);
        auto* contextLayout = new QVBoxLayout(context);
        contextLayout->setContentsMargins(14, 16, 14, 18);
        contextLayout->setSpacing(9);
        auto* kicker = new QLabel(QObject::tr("DATOS RÁPIDOS"), context);
        kicker->setObjectName(QStringLiteral("planningContextKicker"));
        contextLayout->addWidget(kicker);

        auto* editorLayout = qobject_cast<QVBoxLayout*>(editor->layout());
        if (editorLayout) {
            auto* title = new QLabel(QObject::tr("Ficha de personaje"), editor);
            title->setObjectName(QStringLiteral("planningEditorialTitle"));
            editorLayout->insertWidget(0, title);
        }

        const QStringList contextTitles{
            QObject::tr("Categoría"), QObject::tr("Estado"), QObject::tr("Rol"),
            QObject::tr("Ocupación"), QObject::tr("Origen"), QObject::tr("Afiliación"),
            QObject::tr("Alias"), QObject::tr("Presencia por capítulo"), QObject::tr("Color")
        };

        QList<QWidget*> moved;
        const auto labels = editor->findChildren<QLabel*>();
        for (QLabel* label : labels) {
            if (!label || label->objectName() != QStringLiteral("planningField")) continue;
            if (!contextTitles.contains(label->text())) continue;
            QWidget* block = label->parentWidget();
            if (!block || block == editor || moved.contains(block)) continue;
            block->setParent(context);
            block->setObjectName(QStringLiteral("planningContextCard"));
            contextLayout->addWidget(block);
            moved.append(block);
        }
        contextLayout->addStretch(1);
        split->addWidget(context);
        split->setStretchFactor(0, 0);
        split->setStretchFactor(1, 1);
        split->setStretchFactor(2, 0);
        split->setSizes({250, 860, 285});

        tab->setStyleSheet(tab->styleSheet() + QStringLiteral(R"QSS(
#planningCharacters{background:#0f1115;color:#e6e9ed;}
#planningIndex{background:#14171c;border:0;border-right:1px solid #2a2f36;}
#planningEditor{background:#0f1115;border:0;}
#planningContextRail{background:#14171c;border:0;border-left:1px solid #2a2f36;}
#planningContextKicker{color:#c59a5d;font-size:8pt;font-weight:700;letter-spacing:1px;}
#planningEditorialTitle{color:#f1f2f4;font-family:'Georgia';font-size:22pt;padding:4px 0 8px 0;}
#planningContextCard,#planningCard{background:#171a20;border:1px solid #2b3037;border-radius:10px;padding:10px;}
#planningContextCard #planningField,#planningCard #planningField{color:#9ba3ae;font-size:8pt;font-weight:700;letter-spacing:.7px;}
#planningIndex QListWidget{background:transparent;color:#d7dce2;border:0;outline:0;}
#planningIndex QListWidget::item{padding:8px;border-radius:6px;}
#planningIndex QListWidget::item:selected{background:#262c33;color:#fff;}
#characterImage{background:#15191e;border:1px solid #30363e;border-radius:10px;color:#7f8791;}
#planningCharacters QLineEdit,#planningCharacters QTextEdit,#planningCharacters QComboBox,#planningCharacters QSpinBox{background:#111419;color:#e7eaee;border:1px solid #30363d;border-radius:7px;padding:7px;}
)QSS"));
    }

private:
    QPointer<QMainWindow> window_;
    bool applied_ = false;
};

} // namespace

void installPlanningMockupController(QMainWindow* window) {
    if (!window || window->property("wbwPlanningMockupController").toBool()) return;
    window->setProperty("wbwPlanningMockupController", true);
    auto* filter = new PlanningWorkspaceFilter(window);
    window->installEventFilter(filter);
    const auto widgets = window->findChildren<QWidget*>();
    for (QWidget* widget : widgets) widget->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
