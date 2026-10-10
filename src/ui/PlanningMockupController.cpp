#include "ui/PlanningMockupController.h"

#include <QEvent>
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QPointer>
#include <QSplitter>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace wbw {
namespace {

void applyPlanningStyle(QWidget* tab) {
    if (!tab) return;
    tab->setStyleSheet(QStringLiteral(R"QSS(
#planningCharacters{background:#0e1116;color:#d8dde5;}
#planningIndex{background:#11161d;border:0;border-right:1px solid #252d38;}
#planningEditor{background:#0e1116;border:0;}
#planningContextRail{background:#11161d;border:0;border-left:1px solid #252d38;}
#planningContextKicker{color:#c49a62;font-size:8pt;font-weight:700;letter-spacing:1px;}
#planningEditorialTitle{color:#f0f2f4;font-family:'Georgia';font-size:23pt;font-weight:600;padding:6px 0 10px 0;}
#planningContextCard,#planningCard{background:#151b22;border:1px solid #2a333f;border-radius:10px;padding:10px;}
#planningContextCard #planningField,#planningCard #planningField{color:#8e99a7;font-size:8pt;font-weight:700;letter-spacing:.7px;}
#planningIndex QListWidget{background:transparent;color:#cbd2dc;border:0;outline:0;padding:6px;}
#planningIndex QListWidget::item{padding:9px 8px;border-radius:7px;margin:1px 0;}
#planningIndex QListWidget::item:hover{background:#18202a;color:#f0f3f6;}
#planningIndex QListWidget::item:selected{background:#222c38;color:#ffffff;border-left:2px solid #c49a62;}
#characterImage{background:#12171d;border:1px solid #2b3440;border-radius:10px;color:#7f8a97;}
#planningCharacters QLineEdit,#planningCharacters QTextEdit,#planningCharacters QComboBox,#planningCharacters QSpinBox{background:#10151b;color:#dbe0e7;border:1px solid #2c3541;border-radius:8px;padding:8px 9px;}
#planningCharacters QLineEdit:focus,#planningCharacters QTextEdit:focus,#planningCharacters QComboBox:focus,#planningCharacters QSpinBox:focus{border-color:#8b6843;}
#planningCharacters QPushButton{background:#171e27;color:#cfd6df;border:1px solid #303a46;border-radius:7px;padding:7px 10px;}
#planningCharacters QPushButton:hover{background:#202a35;color:#ffffff;}
#planningCharacters QScrollBar:vertical{background:#0e1116;width:9px;}
#planningCharacters QScrollBar::handle:vertical{background:#343c47;border-radius:4px;min-height:34px;}
QSplitter::handle{background:#252d38;}
)QSS"));
}

class PlanningWorkspaceFilter final : public QObject {
public:
    explicit PlanningWorkspaceFilter(QMainWindow* window) : QObject(window), window_(window) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == window_ && (event->type() == QEvent::Show || event->type() == QEvent::Resize))
            QTimer::singleShot(0, this, [this]() { apply(); });
        return QObject::eventFilter(watched, event);
    }

    void apply() {
        if (!window_) return;
        QWidget* tab = window_->findChild<QWidget*>(QStringLiteral("planningCharacters"));
        QWidget* editor = window_->findChild<QWidget*>(QStringLiteral("planningEditor"));
        QWidget* index = window_->findChild<QWidget*>(QStringLiteral("planningIndex"));
        if (!tab || !editor || !index) return;

        QSplitter* split = nullptr;
        const auto splitters = tab->findChildren<QSplitter*>(QString(), Qt::FindDirectChildrenOnly);
        if (!splitters.isEmpty()) split = splitters.first();
        if (!split || split->count() < 2) return;

        if (!applied_) {
            applied_ = true;
            if (QWidget* hero = window_->findChild<QWidget*>(QStringLiteral("planningHero"))) hero->hide();
            if (auto* outer = qobject_cast<QVBoxLayout*>(tab->layout())) {
                outer->setContentsMargins(0, 0, 0, 0);
                outer->setSpacing(0);
            }

            context_ = new QFrame(split);
            context_->setObjectName(QStringLiteral("planningContextRail"));
            auto* contextLayout = new QVBoxLayout(context_);
            contextLayout->setContentsMargins(14, 16, 14, 18);
            contextLayout->setSpacing(9);
            auto* kicker = new QLabel(QObject::tr("DATOS RÁPIDOS"), context_);
            kicker->setObjectName(QStringLiteral("planningContextKicker"));
            contextLayout->addWidget(kicker);

            if (auto* editorLayout = qobject_cast<QVBoxLayout*>(editor->layout())) {
                auto* title = new QLabel(QObject::tr("Ficha de personaje"), editor);
                title->setObjectName(QStringLiteral("planningEditorialTitle"));
                editorLayout->insertWidget(0, title);
            }

            const QStringList contextTitles{
                QObject::tr("Categoría"), QObject::tr("Estado"), QObject::tr("Rol"), QObject::tr("Ocupación"),
                QObject::tr("Origen"), QObject::tr("Afiliación"), QObject::tr("Alias"),
                QObject::tr("Presencia por capítulo"), QObject::tr("Color")
            };
            QList<QWidget*> moved;
            const auto labels = editor->findChildren<QLabel*>();
            for (QLabel* label : labels) {
                if (!label || label->objectName() != QStringLiteral("planningField") || !contextTitles.contains(label->text())) continue;
                QWidget* block = label->parentWidget();
                if (!block || block == editor || moved.contains(block)) continue;
                block->setParent(context_);
                block->setObjectName(QStringLiteral("planningContextCard"));
                contextLayout->addWidget(block);
                moved.append(block);
            }
            contextLayout->addStretch(1);
            split->addWidget(context_);
            split->setStretchFactor(0, 0);
            split->setStretchFactor(1, 1);
            split->setStretchFactor(2, 0);
        }

        const int width = tab->width();
        index->setMinimumWidth(width < 1050 ? 190 : 220);
        index->setMaximumWidth(width < 1050 ? 230 : 285);
        if (context_) {
            const bool showContext = width >= 980;
            context_->setVisible(showContext);
            context_->setMinimumWidth(showContext ? 240 : 0);
            context_->setMaximumWidth(showContext ? 315 : 0);
        }
        if (width >= 1280) split->setSizes({250, 880, 285});
        else if (width >= 980) split->setSizes({220, 690, 255});
        else split->setSizes({205, 760, 0});

        applyPlanningStyle(tab);
    }

private:
    QPointer<QMainWindow> window_;
    QPointer<QFrame> context_;
    bool applied_ = false;
};

} // namespace

void installPlanningMockupController(QMainWindow* window) {
    if (!window || window->property("wbwPlanningMockupController").toBool()) return;
    window->setProperty("wbwPlanningMockupController", true);
    auto* filter = new PlanningWorkspaceFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
