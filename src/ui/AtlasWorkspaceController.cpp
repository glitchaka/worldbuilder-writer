#include "ui/AtlasWorkspaceController.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPointer>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

namespace wbw {
namespace {

QPushButton* buttonByText(QWidget* root, const QString& text) {
    if (!root) return nullptr;
    for (QPushButton* button : root->findChildren<QPushButton*>()) if (button->text() == text) return button;
    return nullptr;
}

void installAtlasIndexNavigation(QWidget* atlas) {
    if (!atlas) return;
    auto* index = atlas->findChild<QWidget*>(QStringLiteral("worldIndexPanel"));
    if (!index || index->property("wbwAtlasIndexV3").toBool()) return;
    auto* layout = qobject_cast<QVBoxLayout*>(index->layout());
    auto* list = index->findChild<QListWidget*>();
    if (!layout || !list) return;

    auto* heading = new QWidget(index);
    heading->setObjectName(QStringLiteral("atlasIndexHeading"));
    auto* headingLayout = new QVBoxLayout(heading);
    headingLayout->setContentsMargins(4, 4, 4, 8);
    headingLayout->setSpacing(2);
    auto* title = new QLabel(QObject::tr("Mundo / Atlas"), heading);
    title->setObjectName(QStringLiteral("atlasIndexTitle"));
    auto* copy = new QLabel(QObject::tr("Lugares, regiones, organizaciones y material de referencia."), heading);
    copy->setObjectName(QStringLiteral("atlasIndexCopy"));
    copy->setWordWrap(true);
    headingLayout->addWidget(title);
    headingLayout->addWidget(copy);
    layout->insertWidget(0, heading);

    auto* search = new QLineEdit(index);
    search->setObjectName(QStringLiteral("atlasSearch"));
    search->setPlaceholderText(QObject::tr("Buscar en el atlas…"));
    search->setClearButtonEnabled(true);
    layout->insertWidget(1, search);

    auto* filters = new QWidget(index);
    filters->setObjectName(QStringLiteral("atlasIndexFilters"));
    auto* filterLayout = new QHBoxLayout(filters);
    filterLayout->setContentsMargins(0, 0, 0, 5);
    filterLayout->setSpacing(4);
    const QStringList names{QObject::tr("Todos"), QObject::tr("Lugares"), QObject::tr("Naciones"), QObject::tr("Organizaciones")};
    for (int i = 0; i < names.size(); ++i) {
        auto* button = new QToolButton(filters);
        button->setObjectName(QStringLiteral("atlasFilterButton"));
        button->setText(names.at(i));
        button->setCheckable(true);
        button->setAutoExclusive(true);
        button->setChecked(i == 0);
        button->setProperty("filter", i == 0 ? QString() : names.at(i));
        filterLayout->addWidget(button);
        QObject::connect(button, &QToolButton::clicked, list, [list, search, button]() {
            const QString query = search->text().trimmed();
            const QString category = button->property("filter").toString();
            for (int row = 0; row < list->count(); ++row) {
                QListWidgetItem* item = list->item(row);
                const QString text = item->text();
                const bool queryMatch = query.isEmpty() || text.contains(query, Qt::CaseInsensitive);
                const bool categoryMatch = category.isEmpty() || text.startsWith(QStringLiteral("[%1]").arg(category), Qt::CaseInsensitive);
                item->setHidden(!queryMatch || !categoryMatch);
            }
        });
    }
    filterLayout->addStretch(1);
    layout->insertWidget(2, filters);

    QObject::connect(search, &QLineEdit::textChanged, list, [list, filters](const QString& query) {
        QString category;
        for (QToolButton* button : filters->findChildren<QToolButton*>()) if (button->isChecked()) { category = button->property("filter").toString(); break; }
        const QString needle = query.trimmed();
        for (int row = 0; row < list->count(); ++row) {
            QListWidgetItem* item = list->item(row);
            const QString text = item->text();
            const bool queryMatch = needle.isEmpty() || text.contains(needle, Qt::CaseInsensitive);
            const bool categoryMatch = category.isEmpty() || text.startsWith(QStringLiteral("[%1]").arg(category), Qt::CaseInsensitive);
            item->setHidden(!queryMatch || !categoryMatch);
        }
    });

    index->setProperty("wbwAtlasIndexV3", true);
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
        if (!world || !atlas) { applying_ = false; return; }

        if (!world->property("wbwAtlasObserved").toBool()) {
            world->setProperty("wbwAtlasObserved", true);
            world->installEventFilter(this);
            atlas->installEventFilter(this);
        }

        installAtlasIndexNavigation(atlas);

        QFrame* context = window_->findChild<QFrame*>(QStringLiteral("atlasContextRail"));
        if (!context) { applying_ = false; return; }
        if (!context->property("wbwAtlasContextObserved").toBool()) {
            context->setProperty("wbwAtlasContextObserved", true);
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
            actions->addWidget(add); actions->addWidget(remove); actions->addWidget(exportButton);
            box->addWidget(heading); box->addLayout(actions); contextLayout->insertWidget(2, card);
            QObject::connect(add, &QPushButton::clicked, oldAdd, &QPushButton::click);
            QObject::connect(remove, &QPushButton::clicked, oldRemove, &QPushButton::click);
            QObject::connect(exportButton, &QPushButton::clicked, oldExport, &QPushButton::click);
        }

        for (QTextEdit* edit : atlas->findChildren<QTextEdit*>()) {
            QWidget* parent = edit->parentWidget();
            const QString role = parent ? parent->objectName() : QString();
            edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            if (role == QStringLiteral("atlasSummaryField")) { edit->setMinimumHeight(72); edit->setMaximumHeight(108); }
            else if (role == QStringLiteral("atlasEditorialSection")) { edit->setMinimumHeight(50); edit->setMaximumHeight(78); }
            else if (role == QStringLiteral("atlasContextCard")) { edit->setMinimumHeight(48); edit->setMaximumHeight(72); }
        }

        if (auto* index = atlas->findChild<QWidget*>(QStringLiteral("worldIndexPanel"))) {
            const int w = atlas->width();
            index->setMinimumWidth(w < 1180 ? 250 : 286);
            index->setMaximumWidth(w < 1180 ? 284 : 330);
        }
        context->setMinimumWidth(atlas->width() >= 1180 ? 280 : 240);
        context->setMaximumWidth(atlas->width() >= 1180 ? 330 : 282);

        atlas->setStyleSheet(atlas->styleSheet() + QStringLiteral(R"QSS(
#atlasIndexHeading{background:transparent;border:0;}
#atlasIndexTitle{font-family:'Georgia';font-size:16pt;font-weight:600;color:#f0ece4;}
#atlasIndexCopy{color:#7d8994;font-size:8.5pt;}
#atlasSearch{background:#0d151d;color:#dce4ea;border:1px solid #2d3945;border-radius:7px;padding:8px 9px;margin:3px 0 5px 0;}
#atlasSearch:focus{border-color:#4383b1;}
#atlasIndexFilters{background:transparent;border:0;}
#atlasFilterButton{background:#101820;color:#8895a0;border:1px solid #293641;border-radius:6px;padding:5px 7px;font-size:7.5pt;}
#atlasFilterButton:hover{background:#17232e;color:#e8eef3;}
#atlasFilterButton:checked{background:#17364b;color:#80d0f7;border-color:#397aa4;}
#atlasReferenceActions{background:#101820;border:1px solid #293641;border-radius:10px;}
#atlasReferenceActions QPushButton{background:transparent;color:#c8d2da;border:1px solid #33424f;border-radius:7px;padding:7px 9px;}
#atlasReferenceActions QPushButton:hover{background:#192735;color:#ffffff;border-color:#48677e;}
#atlasSummaryField QTextEdit,#atlasEditorialSection QTextEdit,#atlasContextCard QTextEdit{background:transparent;border:0;padding:2px 0;font-family:'Georgia';}
)QSS"));

        applying_ = false;
    }

private:
    void schedule() { if (pending_) return; pending_ = true; QTimer::singleShot(0, this, [this]() { apply(); }); }
    QPointer<QMainWindow> window_;
    bool pending_ = false;
    bool applying_ = false;
};

} // namespace

void installAtlasWorkspaceController(QMainWindow* window) {
    if (!window || window->property("wbwAtlasWorkspaceControllerV3").toBool()) return;
    window->setProperty("wbwAtlasWorkspaceControllerV3", true);
    auto* filter = new AtlasWorkspaceFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
