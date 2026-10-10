#include "ui/AtlasWorkspaceController.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPointer>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace wbw {
namespace {

QPushButton* buttonByText(QWidget* root, const QString& text) {
    if (!root) return nullptr;
    for (QPushButton* button : root->findChildren<QPushButton*>())
        if (button->text() == text) return button;
    return nullptr;
}

class AtlasWorkspaceFilter final : public QObject {
public:
    explicit AtlasWorkspaceFilter(QMainWindow* window) : QObject(window), window_(window) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        Q_UNUSED(watched);
        switch (event->type()) {
            case QEvent::Show:
            case QEvent::Resize:
            case QEvent::LayoutRequest:
            case QEvent::ParentChange:
                schedule();
                break;
            default:
                break;
        }
        return QObject::eventFilter(watched, event);
    }

    void apply() {
        pending_ = false;
        if (applying_ || !window_) return;
        applying_ = true;

        QWidget* world = window_->findChild<QWidget*>(QStringLiteral("worldPage"));
        QWidget* atlas = window_->findChild<QWidget*>(QStringLiteral("atlasTab"));
        QFrame* context = window_->findChild<QFrame*>(QStringLiteral("atlasContextRail"));
        if (!world || !atlas || !context) {
            applying_ = false;
            return;
        }

        if (!world->property("wbwAtlasObserved").toBool()) {
            world->setProperty("wbwAtlasObserved", true);
            world->installEventFilter(this);
            atlas->installEventFilter(this);
            context->installEventFilter(this);
        }

        QPushButton* oldAdd = buttonByText(atlas, QObject::tr("Añadir adjunto…"));
        QPushButton* oldRemove = buttonByText(atlas, QObject::tr("Quitar"));
        QPushButton* oldExport = buttonByText(atlas, QObject::tr("Exportar…"));
        if (oldAdd) oldAdd->hide();
        if (oldRemove) oldRemove->hide();
        if (oldExport) oldExport->hide();

        auto* contextLayout = qobject_cast<QVBoxLayout*>(context->layout());
        if (contextLayout && !context->findChild<QWidget*>(QStringLiteral("atlasReferenceActions")) && oldAdd && oldRemove && oldExport) {
            auto* card = new QFrame(context);
            card->setObjectName(QStringLiteral("atlasReferenceActions"));
            auto* box = new QVBoxLayout(card);
            box->setContentsMargins(10, 9, 10, 10);
            box->setSpacing(7);
            auto* heading = new QLabel(QObject::tr("MATERIAL DE REFERENCIA"), card);
            heading->setObjectName(QStringLiteral("atlasSectionTitle"));
            auto* actions = new QHBoxLayout;
            actions->setSpacing(5);
            auto* add = new QPushButton(QObject::tr("+ Adjuntar"), card);
            auto* remove = new QPushButton(QObject::tr("Quitar"), card);
            auto* exportButton = new QPushButton(QObject::tr("Exportar"), card);
            actions->addWidget(add);
            actions->addWidget(remove);
            actions->addWidget(exportButton);
            box->addWidget(heading);
            box->addLayout(actions);
            contextLayout->insertWidget(2, card);
            QObject::connect(add, &QPushButton::clicked, oldAdd, &QPushButton::click);
            QObject::connect(remove, &QPushButton::clicked, oldRemove, &QPushButton::click);
            QObject::connect(exportButton, &QPushButton::clicked, oldExport, &QPushButton::click);
        }

        for (QTextEdit* edit : atlas->findChildren<QTextEdit*>()) {
            QWidget* parent = edit->parentWidget();
            const QString role = parent ? parent->objectName() : QString();
            if (role == QStringLiteral("atlasSummaryField")) {
                edit->setMinimumHeight(76);
                edit->setMaximumHeight(118);
            } else if (role == QStringLiteral("atlasEditorialSection")) {
                edit->setMinimumHeight(54);
                edit->setMaximumHeight(88);
            } else if (role == QStringLiteral("atlasContextCard")) {
                edit->setMinimumHeight(52);
                edit->setMaximumHeight(82);
            }
        }

        atlas->setStyleSheet(atlas->styleSheet() + QStringLiteral(R"QSS(
#atlasReferenceActions{background:#121518;border:1px solid #2b2b27;border-radius:10px;}
#atlasReferenceActions QPushButton{background:transparent;color:#c8c3ba;border:1px solid #373730;border-radius:7px;padding:7px 9px;}
#atlasReferenceActions QPushButton:hover{background:#24241f;color:#fff8eb;border-color:#76573a;}
#atlasSummaryField QTextEdit,#atlasEditorialSection QTextEdit,#atlasContextCard QTextEdit{background:transparent;border:0;padding:2px 0;font-family:'Georgia';}
)QSS"));

        applying_ = false;
    }

private:
    void schedule() {
        if (pending_) return;
        pending_ = true;
        QTimer::singleShot(0, this, [this]() { apply(); });
    }

    QPointer<QMainWindow> window_;
    bool pending_ = false;
    bool applying_ = false;
};

} // namespace

void installAtlasWorkspaceController(QMainWindow* window) {
    if (!window || window->property("wbwAtlasWorkspaceControllerV1").toBool()) return;
    window->setProperty("wbwAtlasWorkspaceControllerV1", true);
    auto* filter = new AtlasWorkspaceFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
