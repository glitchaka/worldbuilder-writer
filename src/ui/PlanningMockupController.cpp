#include "ui/PlanningMockupController.h"

#include <QEvent>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
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
    if (!tab || tab->property("wbwCharacterWorkspaceStyledV1").toBool()) return;
    tab->setProperty("wbwCharacterWorkspaceStyledV1", true);
    tab->setStyleSheet(QStringLiteral(R"QSS(
#planningCharacters{background:#0b1016;color:#d9e1e8;}
#planningIndex{background:#0c1219;border:0;border-right:1px solid #25303b;}
#planningEditor{background:#0b1016;border:0;}
#planningContextRail{background:#0c1219;border:0;border-left:1px solid #25303b;}
#planningContextKicker,#planningIndexKicker{color:#6fa9d8;font-size:7.5pt;font-weight:700;letter-spacing:1.3px;}
#planningEditorialTitle{color:#f1ede4;font-family:'Georgia';font-size:24pt;font-weight:600;padding:5px 0 3px 0;}
#planningEditorialSubtitle{color:#7f8b96;font-family:'Georgia';font-size:9pt;padding-bottom:10px;}
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

class PlanningWorkspaceFilter final : public QObject {
public:
    explicit PlanningWorkspaceFilter(QMainWindow* window) : QObject(window), window_(window) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        if ((watched == window_ || watched == tab_) && (event->type() == QEvent::Show || event->type() == QEvent::Resize))
            schedule();
        return false;
    }

    void apply() {
        pending_ = false;
        if (!window_) return;

        QWidget* tab = window_->findChild<QWidget*>(QStringLiteral("planningCharacters"));
        QWidget* editor = window_->findChild<QWidget*>(QStringLiteral("planningEditor"));
        QWidget* index = window_->findChild<QWidget*>(QStringLiteral("planningIndex"));
        if (!tab || !editor || !index) return;
        tab_ = tab;

        QSplitter* split = nullptr;
        const auto splitters = tab->findChildren<QSplitter*>(QString(), Qt::FindDirectChildrenOnly);
        if (!splitters.isEmpty()) split = splitters.first();
        if (!split) return;

        if (!initialized_) {
            if (QWidget* hero = window_->findChild<QWidget*>(QStringLiteral("planningHero"))) hero->hide();
            if (auto* outer = qobject_cast<QVBoxLayout*>(tab->layout())) {
                outer->setContentsMargins(0, 0, 0, 0);
                outer->setSpacing(0);
            }

            if (auto* indexLayout = qobject_cast<QVBoxLayout*>(index->layout())) {
                auto* kicker = new QLabel(QObject::tr("PERSONAJES Y ENTIDADES"), index);
                kicker->setObjectName(QStringLiteral("planningIndexKicker"));
                indexLayout->insertWidget(0, kicker);
                auto* search = new QLineEdit(index);
                search->setObjectName(QStringLiteral("planningCharacterSearch"));
                search->setPlaceholderText(QObject::tr("Buscar personajes, grupos o conceptos…"));
                search->setClearButtonEnabled(true);
                indexLayout->insertWidget(2, search);
                if (auto* list = index->findChild<QListWidget*>()) {
                    QObject::connect(search, &QLineEdit::textChanged, list, [list](const QString& query) {
                        const QString needle = query.trimmed();
                        for (int i = 0; i < list->count(); ++i)
                            list->item(i)->setHidden(!needle.isEmpty() && !list->item(i)->text().contains(needle, Qt::CaseInsensitive));
                    });
                }
            }

            context_ = new QFrame(split);
            context_->setObjectName(QStringLiteral("planningContextRail"));
            auto* contextLayout = new QVBoxLayout(context_);
            contextLayout->setContentsMargins(14, 18, 14, 18);
            contextLayout->setSpacing(9);
            auto* kicker = new QLabel(QObject::tr("DATOS RÁPIDOS"), context_);
            kicker->setObjectName(QStringLiteral("planningContextKicker"));
            contextLayout->addWidget(kicker);

            QLabel* editorialTitle = nullptr;
            if (auto* editorLayout = qobject_cast<QVBoxLayout*>(editor->layout())) {
                editorialTitle = new QLabel(QObject::tr("Ficha de personaje"), editor);
                editorialTitle->setObjectName(QStringLiteral("planningEditorialTitle"));
                auto* subtitle = new QLabel(QObject::tr("Perfil narrativo, relaciones, notas, apariciones y vínculos con el mundo."), editor);
                subtitle->setObjectName(QStringLiteral("planningEditorialSubtitle"));
                subtitle->setWordWrap(true);
                editorLayout->insertWidget(0, subtitle);
                editorLayout->insertWidget(0, editorialTitle);
            }

            QLineEdit* nameField = nullptr;
            for (QLabel* fieldLabel : editor->findChildren<QLabel*>()) {
                if (fieldLabel->objectName() == QStringLiteral("planningField") && fieldLabel->text() == QObject::tr("Nombre")) {
                    nameField = fieldLabel->parentWidget()->findChild<QLineEdit*>();
                    break;
                }
            }
            if (editorialTitle && nameField) {
                QObject::connect(nameField, &QLineEdit::textChanged, editorialTitle, [editorialTitle](const QString& text) {
                    editorialTitle->setText(text.trimmed().isEmpty() ? QObject::tr("Ficha de personaje") : text.trimmed());
                });
                if (!nameField->text().trimmed().isEmpty()) editorialTitle->setText(nameField->text().trimmed());
            }

            const QStringList quickFields{
                QObject::tr("Categoría"), QObject::tr("Estado"), QObject::tr("Rol"), QObject::tr("Ocupación"),
                QObject::tr("Origen"), QObject::tr("Afiliación"), QObject::tr("Alias"), QObject::tr("Rasgos"),
                QObject::tr("Presencia por capítulo"), QObject::tr("Color")
            };
            QList<QWidget*> moved;
            for (QLabel* fieldLabel : editor->findChildren<QLabel*>()) {
                if (!fieldLabel || fieldLabel->objectName() != QStringLiteral("planningField") || !quickFields.contains(fieldLabel->text())) continue;
                QWidget* block = fieldLabel->parentWidget();
                if (!block || block == editor || moved.contains(block)) continue;
                block->setParent(context_);
                block->setObjectName(QStringLiteral("planningContextCard"));
                contextLayout->addWidget(block);
                moved.append(block);
            }
            contextLayout->addStretch(1);
            split->addWidget(context_);
            split->setHandleWidth(1);
            split->setStretchFactor(0, 0);
            split->setStretchFactor(1, 1);
            split->setStretchFactor(2, 0);

            styleCharacterWorkspace(tab);
            tab->installEventFilter(this);
            initialized_ = true;
        }

        const int width = tab->width();
        index->setMinimumWidth(width < 1080 ? 210 : 242);
        index->setMaximumWidth(width < 1080 ? 244 : 292);
        if (context_) {
            const bool visible = width >= 1040;
            context_->setVisible(visible);
            context_->setMinimumWidth(visible ? 250 : 0);
            context_->setMaximumWidth(visible ? 320 : 0);
        }
        split->setSizes(width >= 1280 ? QList<int>{258, 880, 286} : QList<int>{220, 720, width >= 1040 ? 260 : 0});
    }

private:
    void schedule() {
        if (pending_) return;
        pending_ = true;
        QTimer::singleShot(45, this, [this]() { apply(); });
    }

    QPointer<QMainWindow> window_;
    QPointer<QWidget> tab_;
    QPointer<QFrame> context_;
    bool initialized_ = false;
    bool pending_ = false;
};

} // namespace

void installPlanningMockupController(QMainWindow* window) {
    if (!window || window->property("wbwPlanningWorkspaceStableV1").toBool()) return;
    window->setProperty("wbwPlanningWorkspaceStableV1", true);
    auto* filter = new PlanningWorkspaceFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
