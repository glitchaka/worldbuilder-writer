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

void styleCharacterWorkspace(QWidget* tab) {
    if (!tab) return;
    tab->setStyleSheet(QStringLiteral(R"QSS(
#planningCharacters{background:#0a0c0f;color:#d9d4ca;}
#planningIndex{background:#0d1013;border:0;border-right:1px solid #292821;}
#planningEditor{background:#0a0c0f;border:0;}
#planningContextRail{background:#0d1013;border:0;border-left:1px solid #292821;}
#planningContextKicker{color:#b88a52;font-size:7.5pt;font-weight:700;letter-spacing:1.3px;}
#planningEditorialTitle{color:#f1ede4;font-family:'Georgia';font-size:23pt;font-weight:600;padding:6px 0 10px 0;}
#planningContextCard,#planningCard{background:#121518;border:1px solid #2b2b27;border-radius:10px;padding:10px;}
#planningContextCard #planningField,#planningCard #planningField{color:#8f918c;font-size:7.5pt;font-weight:700;letter-spacing:.8px;}
#planningIndex QListWidget{background:transparent;color:#c8c4bb;border:0;outline:0;padding:6px;}
#planningIndex QListWidget::item{padding:9px 8px;border-radius:7px;margin:1px 0;}
#planningIndex QListWidget::item:hover{background:#171a1d;color:#f1ede4;}
#planningIndex QListWidget::item:selected{background:#28251f;color:#e7c28a;border-left:2px solid #b88a52;}
#characterImage{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #2c241c,stop:1 #111315);border:1px solid #493b2d;border-radius:10px;color:#8f887d;}
#planningCharacters QLineEdit,#planningCharacters QTextEdit,#planningCharacters QComboBox,#planningCharacters QSpinBox{background:#0d1013;color:#ddd8cf;border:1px solid #33342f;border-radius:8px;padding:8px 9px;}
#planningCharacters QLineEdit:focus,#planningCharacters QTextEdit:focus,#planningCharacters QComboBox:focus,#planningCharacters QSpinBox:focus{border-color:#8d6841;}
#planningCharacters QPushButton{background:#151819;color:#cac6be;border:1px solid #34352f;border-radius:7px;padding:7px 10px;}
#planningCharacters QPushButton:hover{background:#20211f;color:#fff9ee;}
QSplitter::handle{background:#292821;}
)QSS"));
}

void styleAtlasWorkspace(QWidget* world) {
    if (!world) return;
    world->setStyleSheet(world->styleSheet() + QStringLiteral(R"QSS(
#worldPage,#atlasTab,#worldEditorCard,#atlasCenterEditorial,#atlasContentGrid{background:#0a0c0f;color:#d9d4ca;}
#worldTabs::pane{border:0;background:#0a0c0f;}
#worldTabs QTabBar::tab{background:transparent;color:#817f79;border:0;border-bottom:2px solid transparent;padding:10px 13px;}
#worldTabs QTabBar::tab:hover{color:#d7d3ca;}
#worldTabs QTabBar::tab:selected{color:#e5bd83;border-bottom:2px solid #b88a52;}
#worldIndexPanel{background:#0d1013;border:0;border-right:1px solid #292821;}
#worldIndexPanel QListWidget{background:transparent;color:#c7c3ba;border:0;outline:0;padding:7px;}
#worldIndexPanel QListWidget::item{padding:9px 8px;border-radius:7px;margin:1px 0;}
#worldIndexPanel QListWidget::item:hover{background:#171a1d;color:#f1ede4;}
#worldIndexPanel QListWidget::item:selected{background:#28251f;color:#e7c28a;border-left:2px solid #b88a52;}
#atlasContextRail,#atlasContextScroll{background:#0d1013;border:0;}
#atlasContextRail{border-left:1px solid #292821;}
#atlasEditorialKicker,#atlasContextKicker,#atlasSectionTitle{color:#b88a52;font-size:7.5pt;font-weight:700;letter-spacing:1.3px;}
#atlasEditorialTitle{color:#f1ede4;font-family:'Georgia';font-size:25pt;font-weight:500;}
#atlasEditorialSubtitle,#atlasContextCopy{color:#797b76;font-family:'Georgia';font-size:9pt;}
#atlasIdentityStrip,#atlasSummaryCard,#atlasEditorialSection,#atlasContextCard{background:#121518;border:1px solid #2b2b27;border-radius:10px;}
#atlasIdentityField{background:transparent;border:0;}
#atlasSummaryField{background:transparent;border:0;padding:0;}
#atlasIdentityField #fieldTitle,#atlasEditorialSection #fieldTitle,#atlasContextCard #fieldTitle{color:#8d8f8a;font-size:7.5pt;font-weight:700;letter-spacing:.8px;}
#atlasIdentityField QLineEdit,#atlasIdentityField QComboBox,#atlasSummaryField QTextEdit,#atlasEditorialSection QTextEdit,#atlasEditorialSection QLineEdit,#atlasEditorialSection QComboBox,#atlasContextCard QTextEdit,#atlasContextCard QLineEdit,#atlasContextCard QListWidget{background:#0d1013;color:#ddd8cf;border:1px solid #32332e;border-radius:7px;padding:8px;selection-background-color:#5a4630;}
#atlasSummaryField QTextEdit{font-family:'Georgia';font-size:11pt;min-height:112px;}
#atlasEditorialSection QTextEdit{min-height:98px;}
#atlasContextCard QTextEdit{min-height:82px;}
#atlasEditorialSplit::handle{background:#292821;}
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

private:
    void apply() {
        if (!window_) return;
        QWidget* tab = window_->findChild<QWidget*>(QStringLiteral("planningCharacters"));
        QWidget* editor = window_->findChild<QWidget*>(QStringLiteral("planningEditor"));
        QWidget* index = window_->findChild<QWidget*>(QStringLiteral("planningIndex"));

        if (tab && editor && index) {
            QSplitter* split = nullptr;
            const auto direct = tab->findChildren<QSplitter*>(QString(), Qt::FindDirectChildrenOnly);
            if (!direct.isEmpty()) split = direct.first();
            if (split && split->count() >= 2 && !applied_) {
                applied_ = true;
                if (QWidget* hero = window_->findChild<QWidget*>(QStringLiteral("planningHero"))) hero->hide();
                if (auto* outer = qobject_cast<QVBoxLayout*>(tab->layout())) { outer->setContentsMargins(0, 0, 0, 0); outer->setSpacing(0); }

                context_ = new QFrame(split);
                context_->setObjectName(QStringLiteral("planningContextRail"));
                auto* contextLayout = new QVBoxLayout(context_);
                contextLayout->setContentsMargins(14, 18, 14, 18);
                contextLayout->setSpacing(9);
                auto* kicker = new QLabel(QObject::tr("DATOS RÁPIDOS"), context_);
                kicker->setObjectName(QStringLiteral("planningContextKicker"));
                contextLayout->addWidget(kicker);

                if (auto* editorLayout = qobject_cast<QVBoxLayout*>(editor->layout())) {
                    auto* title = new QLabel(QObject::tr("Ficha de personaje"), editor);
                    title->setObjectName(QStringLiteral("planningEditorialTitle"));
                    editorLayout->insertWidget(0, title);
                }

                const QStringList contextTitles{QObject::tr("Categoría"), QObject::tr("Estado"), QObject::tr("Rol"), QObject::tr("Ocupación"), QObject::tr("Origen"), QObject::tr("Afiliación"), QObject::tr("Alias"), QObject::tr("Presencia por capítulo"), QObject::tr("Color")};
                QList<QWidget*> moved;
                for (QLabel* label : editor->findChildren<QLabel*>()) {
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

            if (split) {
                const int width = tab->width();
                index->setMinimumWidth(width < 1080 ? 200 : 228);
                index->setMaximumWidth(width < 1080 ? 230 : 285);
                if (context_) {
                    const bool show = width >= 980;
                    context_->setVisible(show);
                    context_->setMinimumWidth(show ? 248 : 0);
                    context_->setMaximumWidth(show ? 320 : 0);
                }
                split->setSizes(width >= 1280 ? QList<int>{246, 880, 286} : QList<int>{210, 720, width >= 980 ? 260 : 0});
            }
            styleCharacterWorkspace(tab);
        }

        if (QWidget* world = window_->findChild<QWidget*>(QStringLiteral("worldPage"))) styleAtlasWorkspace(world);
    }

    QPointer<QMainWindow> window_;
    QPointer<QFrame> context_;
    bool applied_ = false;
};

} // namespace

void installPlanningMockupController(QMainWindow* window) {
    if (!window || window->property("wbwPlanningMockupControllerV3").toBool()) return;
    window->setProperty("wbwPlanningMockupControllerV3", true);
    auto* filter = new PlanningWorkspaceFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
