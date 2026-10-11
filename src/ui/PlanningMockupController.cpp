#include "ui/PlanningMockupController.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPointer>
#include <QPushButton>
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
#planningCharacters{background:#0b1016;color:#d9e1e8;}
#planningIndex{background:#0c1219;border:0;border-right:1px solid #25303b;}
#planningEditor{background:#0b1016;border:0;}
#planningContextRail{background:#0c1219;border:0;border-left:1px solid #25303b;}
#planningContextKicker,#planningIndexKicker{color:#6fa9d8;font-size:7.5pt;font-weight:700;letter-spacing:1.3px;}
#planningEditorialTitle{color:#f1ede4;font-family:'Georgia';font-size:24pt;font-weight:600;padding:5px 0 10px 0;}
#planningEditorialSubtitle{color:#7f8b96;font-family:'Georgia';font-size:9pt;padding-bottom:8px;}
#planningContextCard,#planningCard{background:#101820;border:1px solid #293641;border-radius:10px;padding:10px;}
#planningContextCard #planningField,#planningCard #planningField{color:#8e9aa5;font-size:7.5pt;font-weight:700;letter-spacing:.8px;}
#planningIndex QListWidget{background:transparent;color:#c8d0d8;border:0;outline:0;padding:5px;}
#planningIndex QListWidget::item{padding:9px 8px;border-radius:7px;margin:1px 0;}
#planningIndex QListWidget::item:hover{background:#131e28;color:#f2f6f9;}
#planningIndex QListWidget::item:selected{background:#143441;color:#81d5dc;border-left:2px solid #2dcad0;}
#planningCharacterSearch{background:#0e161f;color:#dce4eb;border:1px solid #2b3744;border-radius:7px;padding:8px 10px;margin:5px 0 7px 0;}
#planningCharacterSearch:focus{border-color:#3f83b5;}
#characterImage{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #243242,stop:1 #10161d);border:1px solid #334657;border-radius:10px;color:#8493a0;}
#planningCharacters QLineEdit,#planningCharacters QTextEdit,#planningCharacters QComboBox,#planningCharacters QSpinBox{background:#0d151d;color:#dce4ea;border:1px solid #2d3945;border-radius:8px;padding:8px 9px;}
#planningCharacters QLineEdit:focus,#planningCharacters QTextEdit:focus,#planningCharacters QComboBox:focus,#planningCharacters QSpinBox:focus{border-color:#4383b1;}
#planningCharacters QPushButton{background:#111a23;color:#cad3db;border:1px solid #303f4c;border-radius:7px;padding:7px 10px;}
#planningCharacters QPushButton:hover{background:#192836;color:#ffffff;border-color:#45637a;}
QSplitter::handle{background:#25303b;}
)QSS"));
}

void styleAtlasWorkspace(QWidget* world) {
    if (!world) return;
    world->setStyleSheet(world->styleSheet() + QStringLiteral(R"QSS(
#worldPage,#atlasTab,#worldEditorCard,#atlasCenterEditorial,#atlasContentGrid{background:#0b1016;color:#d9e1e8;}
#worldTabs::pane{border:0;background:#0b1016;}
#worldTabs QTabBar::tab{background:transparent;color:#7e8a95;border:0;border-bottom:2px solid transparent;padding:10px 13px;}
#worldTabs QTabBar::tab:hover{color:#dbe3e9;}
#worldTabs QTabBar::tab:selected{color:#82cef5;border-bottom:2px solid #3698d2;}
#worldIndexPanel{background:#0c1219;border:0;border-right:1px solid #25303b;}
#worldIndexPanel QListWidget{background:transparent;color:#c7d0d8;border:0;outline:0;padding:7px;}
#worldIndexPanel QListWidget::item{padding:9px 8px;border-radius:7px;margin:1px 0;}
#worldIndexPanel QListWidget::item:hover{background:#131e28;color:#f1f5f8;}
#worldIndexPanel QListWidget::item:selected{background:#143441;color:#83d8df;border-left:2px solid #2dcad0;}
#atlasContextRail,#atlasContextScroll{background:#0c1219;border:0;}
#atlasContextRail{border-left:1px solid #25303b;}
#atlasEditorialKicker,#atlasContextKicker,#atlasSectionTitle{color:#6fa9d8;font-size:7.5pt;font-weight:700;letter-spacing:1.3px;}
#atlasEditorialTitle{color:#f1ede4;font-family:'Georgia';font-size:25pt;font-weight:500;}
#atlasEditorialSubtitle,#atlasContextCopy{color:#7d8994;font-family:'Georgia';font-size:9pt;}
#atlasIdentityStrip,#atlasSummaryCard,#atlasEditorialSection,#atlasContextCard,#atlasAttachmentActions{background:#101820;border:1px solid #293641;border-radius:10px;}
#atlasIdentityField{background:transparent;border:0;}
#atlasSummaryField{background:transparent;border:0;padding:0;}
#atlasIdentityField #fieldTitle,#atlasEditorialSection #fieldTitle,#atlasContextCard #fieldTitle{color:#8e9aa5;font-size:7.5pt;font-weight:700;letter-spacing:.8px;}
#atlasIdentityField QLineEdit,#atlasIdentityField QComboBox{background:#0d151d;color:#dce4ea;border:1px solid #2d3945;border-radius:7px;padding:8px;selection-background-color:#315f7b;}
#atlasSummaryField QTextEdit,#atlasEditorialSection QTextEdit,#atlasContextCard QTextEdit{background:transparent;color:#d9d4ca;border:0;border-radius:0;padding:2px 0;selection-background-color:#315f7b;font-family:'Georgia';}
#atlasEditorialSection QLineEdit,#atlasEditorialSection QComboBox,#atlasContextCard QLineEdit,#atlasContextCard QListWidget{background:#0d151d;color:#dce4ea;border:1px solid #2d3945;border-radius:7px;padding:7px;selection-background-color:#315f7b;}
#atlasSummaryField QTextEdit{font-size:11pt;min-height:76px;max-height:126px;}
#atlasEditorialSection QTextEdit{font-size:10pt;min-height:56px;max-height:96px;}
#atlasContextCard QTextEdit{font-size:9.5pt;min-height:54px;max-height:90px;}
#atlasContextCard QListWidget{min-height:56px;max-height:92px;}
#atlasAttachmentActions QPushButton{background:transparent;color:#c8d2da;border:1px solid #33424f;border-radius:7px;padding:7px 9px;}
#atlasAttachmentActions QPushButton:hover{background:#192735;color:#fff;border-color:#48677e;}
#atlasEditorialSplit::handle{background:#25303b;}
)QSS"));
}

QPushButton* buttonByText(QWidget* root, const QString& text) {
    if (!root) return nullptr;
    for (QPushButton* button : root->findChildren<QPushButton*>()) if (button->text() == text) return button;
    return nullptr;
}

void installAtlasAttachmentActions(QWidget* world) {
    if (!world || world->property("wbwAtlasAttachmentActions").toBool()) return;
    auto* context = world->findChild<QFrame*>(QStringLiteral("atlasContextRail"));
    if (!context) return;
    auto* contextLayout = qobject_cast<QVBoxLayout*>(context->layout());
    if (!contextLayout) return;
    QPushButton* oldAdd = buttonByText(world, QObject::tr("Añadir adjunto…"));
    QPushButton* oldRemove = buttonByText(world, QObject::tr("Quitar"));
    QPushButton* oldExport = buttonByText(world, QObject::tr("Exportar…"));
    if (!oldAdd || !oldRemove || !oldExport) return;
    oldAdd->hide(); oldRemove->hide(); oldExport->hide();
    auto* card = new QFrame(context); card->setObjectName(QStringLiteral("atlasAttachmentActions"));
    auto* box = new QVBoxLayout(card); box->setContentsMargins(10, 9, 10, 10); box->setSpacing(7);
    auto* heading = new QLabel(QObject::tr("MATERIAL DE REFERENCIA"), card); heading->setObjectName(QStringLiteral("atlasSectionTitle"));
    auto* actions = new QHBoxLayout; actions->setSpacing(5);
    auto* add = new QPushButton(QObject::tr("+ Adjuntar"), card); auto* remove = new QPushButton(QObject::tr("Quitar"), card); auto* exportButton = new QPushButton(QObject::tr("Exportar"), card);
    actions->addWidget(add); actions->addWidget(remove); actions->addWidget(exportButton); box->addWidget(heading); box->addLayout(actions); contextLayout->insertWidget(2, card);
    QObject::connect(add, &QPushButton::clicked, oldAdd, &QPushButton::click); QObject::connect(remove, &QPushButton::clicked, oldRemove, &QPushButton::click); QObject::connect(exportButton, &QPushButton::clicked, oldExport, &QPushButton::click);
    world->setProperty("wbwAtlasAttachmentActions", true);
}

class PlanningWorkspaceFilter final : public QObject {
public:
    explicit PlanningWorkspaceFilter(QMainWindow* window) : QObject(window), window_(window) {}
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == window_ && (event->type() == QEvent::Show || event->type() == QEvent::Resize || event->type() == QEvent::LayoutRequest)) schedule();
        return QObject::eventFilter(watched, event);
    }

    void apply() {
        pending_ = false;
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

                if (auto* indexLayout = qobject_cast<QVBoxLayout*>(index->layout())) {
                    auto* kicker = new QLabel(QObject::tr("PERSONAJES"), index); kicker->setObjectName(QStringLiteral("planningIndexKicker")); indexLayout->insertWidget(0, kicker);
                    auto* search = new QLineEdit(index); search->setObjectName(QStringLiteral("planningCharacterSearch")); search->setPlaceholderText(QObject::tr("Buscar personajes…")); search->setClearButtonEnabled(true); indexLayout->insertWidget(2, search);
                    if (auto* list = index->findChild<QListWidget*>()) {
                        QObject::connect(search, &QLineEdit::textChanged, list, [list](const QString& query) {
                            const QString needle = query.trimmed();
                            for (int i=0;i<list->count();++i) list->item(i)->setHidden(!needle.isEmpty() && !list->item(i)->text().contains(needle, Qt::CaseInsensitive));
                        });
                    }
                }

                context_ = new QFrame(split); context_->setObjectName(QStringLiteral("planningContextRail"));
                auto* contextLayout = new QVBoxLayout(context_); contextLayout->setContentsMargins(14, 18, 14, 18); contextLayout->setSpacing(9);
                auto* kicker = new QLabel(QObject::tr("DATOS RÁPIDOS"), context_); kicker->setObjectName(QStringLiteral("planningContextKicker")); contextLayout->addWidget(kicker);

                QLabel* editorialTitle = nullptr;
                if (auto* editorLayout = qobject_cast<QVBoxLayout*>(editor->layout())) {
                    editorialTitle = new QLabel(QObject::tr("Ficha de personaje"), editor); editorialTitle->setObjectName(QStringLiteral("planningEditorialTitle"));
                    auto* subtitle = new QLabel(QObject::tr("Perfil narrativo, relaciones, presencia y notas del personaje."), editor); subtitle->setObjectName(QStringLiteral("planningEditorialSubtitle")); subtitle->setWordWrap(true);
                    editorLayout->insertWidget(0, subtitle); editorLayout->insertWidget(0, editorialTitle);
                }

                QLineEdit* nameField = nullptr;
                for (QLabel* label : editor->findChildren<QLabel*>()) {
                    if (label->objectName() == QStringLiteral("planningField") && label->text() == QObject::tr("Nombre")) { nameField = label->parentWidget()->findChild<QLineEdit*>(); break; }
                }
                if (editorialTitle && nameField) {
                    QObject::connect(nameField, &QLineEdit::textChanged, editorialTitle, [editorialTitle](const QString& text){ editorialTitle->setText(text.trimmed().isEmpty()?QObject::tr("Ficha de personaje"):text.trimmed()); });
                }

                const QStringList contextTitles{QObject::tr("Categoría"), QObject::tr("Estado"), QObject::tr("Rol"), QObject::tr("Ocupación"), QObject::tr("Origen"), QObject::tr("Afiliación"), QObject::tr("Alias"), QObject::tr("Rasgos"), QObject::tr("Presencia por capítulo"), QObject::tr("Color")};
                QList<QWidget*> moved;
                for (QLabel* label : editor->findChildren<QLabel*>()) {
                    if (!label || label->objectName() != QStringLiteral("planningField") || !contextTitles.contains(label->text())) continue;
                    QWidget* block = label->parentWidget();
                    if (!block || block == editor || moved.contains(block)) continue;
                    block->setParent(context_); block->setObjectName(QStringLiteral("planningContextCard")); contextLayout->addWidget(block); moved.append(block);
                }
                contextLayout->addStretch(1); split->addWidget(context_); split->setStretchFactor(0,0); split->setStretchFactor(1,1); split->setStretchFactor(2,0);
            }

            if (split) {
                const int width = tab->width(); index->setMinimumWidth(width < 1080 ? 210 : 242); index->setMaximumWidth(width < 1080 ? 244 : 292);
                if (context_) { const bool show = width >= 1040; context_->setVisible(show); context_->setMinimumWidth(show ? 250 : 0); context_->setMaximumWidth(show ? 320 : 0); }
                split->setSizes(width >= 1280 ? QList<int>{258, 880, 286} : QList<int>{220, 720, width >= 1040 ? 260 : 0});
            }
            styleCharacterWorkspace(tab);
        }

        if (QWidget* world = window_->findChild<QWidget*>(QStringLiteral("worldPage"))) { styleAtlasWorkspace(world); installAtlasAttachmentActions(world); }
    }

private:
    void schedule(){if(pending_)return;pending_=true;QTimer::singleShot(0,this,[this](){apply();});}
    QPointer<QMainWindow> window_;
    QPointer<QFrame> context_;
    bool applied_ = false;
    bool pending_ = false;
};

} // namespace

void installPlanningMockupController(QMainWindow* window) {
    if (!window || window->property("wbwPlanningMockupControllerV5").toBool()) return;
    window->setProperty("wbwPlanningMockupControllerV5", true);
    auto* filter = new PlanningWorkspaceFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
