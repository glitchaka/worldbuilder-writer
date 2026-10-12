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
    for (QPushButton* button : root->findChildren<QPushButton*>())
        if (button->text() == text) return button;
    return nullptr;
}

void applyAtlasFilter(QListWidget* list, const QString& query, const QString& category) {
    if (!list) return;
    const QString needle = query.trimmed();
    for (int row = 0; row < list->count(); ++row) {
        QListWidgetItem* item = list->item(row);
        const QString text = item->text();
        const bool queryMatch = needle.isEmpty() || text.contains(needle, Qt::CaseInsensitive);
        const bool categoryMatch = category.isEmpty() || text.startsWith(QStringLiteral("[%1]").arg(category), Qt::CaseInsensitive);
        item->setHidden(!queryMatch || !categoryMatch);
    }
}

void installAtlasIndexNavigation(QWidget* atlas) {
    if (!atlas) return;
    auto* index = atlas->findChild<QWidget*>(QStringLiteral("worldIndexPanel"));
    if (!index || index->property("wbwAtlasIndexV4").toBool()) return;
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
    auto* copy = new QLabel(QObject::tr("Lugares, regiones, personajes y documentación conectada."), heading);
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
            applyAtlasFilter(list, search->text(), button->property("filter").toString());
        });
    }
    filterLayout->addStretch(1);
    layout->insertWidget(2, filters);

    QObject::connect(search, &QLineEdit::textChanged, list, [list, filters](const QString& query) {
        QString category;
        for (QToolButton* button : filters->findChildren<QToolButton*>()) {
            if (button->isChecked()) {
                category = button->property("filter").toString();
                break;
            }
        }
        applyAtlasFilter(list, query, category);
    });

    index->setProperty("wbwAtlasIndexV4", true);
}

void installReferenceActions(QWidget* atlas, QFrame* context) {
    if (!atlas || !context || context->findChild<QWidget*>(QStringLiteral("atlasReferenceActions"))) return;
    auto* contextLayout = qobject_cast<QVBoxLayout*>(context->layout());
    if (!contextLayout) return;

    QPushButton* oldAdd = buttonByText(atlas, QObject::tr("Añadir adjunto…"));
    QPushButton* oldRemove = buttonByText(atlas, QObject::tr("Quitar"));
    QPushButton* oldExport = buttonByText(atlas, QObject::tr("Exportar…"));
    if (!oldAdd || !oldRemove || !oldExport) return;
    oldAdd->hide(); oldRemove->hide(); oldExport->hide();

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
    box->addWidget(heading); box->addLayout(actions);
    contextLayout->insertWidget(2, card);
    QObject::connect(add, &QPushButton::clicked, oldAdd, &QPushButton::click);
    QObject::connect(remove, &QPushButton::clicked, oldRemove, &QPushButton::click);
    QObject::connect(exportButton, &QPushButton::clicked, oldExport, &QPushButton::click);
}

void styleAtlasOnce(QWidget* atlas) {
    if (!atlas || atlas->property("wbwAtlasStyledV4").toBool()) return;
    atlas->setProperty("wbwAtlasStyledV4", true);
    atlas->setStyleSheet(QStringLiteral(R"QSS(
#atlasTab{background:#0b1016;color:#dce4eb;}
#worldIndexPanel{background:#0c1219;border:0;border-right:1px solid #25303b;}
#atlasIndexHeading{background:transparent;border:0;}
#atlasIndexTitle{font-family:'Georgia';font-size:17pt;font-weight:600;color:#f2eee7;}
#atlasIndexCopy{color:#7d8994;font-size:8.5pt;}
#atlasSearch{background:#0d151d;color:#dce4ea;border:1px solid #2d3945;border-radius:7px;padding:8px 9px;margin:3px 0 5px 0;}
#atlasSearch:focus{border-color:#4383b1;}
#atlasIndexFilters{background:transparent;border:0;}
#atlasFilterButton{background:#101820;color:#8895a0;border:1px solid #293641;border-radius:6px;padding:5px 7px;font-size:7.5pt;}
#atlasFilterButton:hover{background:#17232e;color:#e8eef3;}
#atlasFilterButton:checked{background:#17364b;color:#80d0f7;border-color:#397aa4;}
#worldIndexPanel QListWidget{background:transparent;color:#c5ced7;border:0;outline:0;}
#worldIndexPanel QListWidget::item{padding:9px 8px;border-radius:7px;margin:1px 0;}
#worldIndexPanel QListWidget::item:hover{background:#131d27;color:#eef4f8;}
#worldIndexPanel QListWidget::item:selected{background:#12343a;color:#83d8d2;border-left:2px solid #31c6c4;}
#atlasContextRail{background:#0c1219;border:0;border-left:1px solid #25303b;}
#atlasReferenceActions{background:#101820;border:1px solid #293641;border-radius:10px;}
#atlasReferenceActions QPushButton{background:transparent;color:#c8d2da;border:1px solid #33424f;border-radius:7px;padding:7px 9px;}
#atlasReferenceActions QPushButton:hover{background:#192735;color:#ffffff;border-color:#48677e;}
#atlasSummaryField,#atlasEditorialSection,#atlasContextCard{background:#101720;border:1px solid #27333f;border-radius:10px;}
#atlasSummaryField QTextEdit,#atlasEditorialSection QTextEdit,#atlasContextCard QTextEdit{background:transparent;color:#d8e0e7;border:0;padding:4px 2px;font-family:'Georgia';}
#atlasTab QLabel{color:#cbd4dc;}
)QSS"));
}

class AtlasWorkspaceFilter final : public QObject {
public:
    explicit AtlasWorkspaceFilter(QMainWindow* window) : QObject(window), window_(window) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        if ((watched == window_ || watched == atlas_) && (event->type() == QEvent::Show || event->type() == QEvent::Resize))
            schedule();
        return false;
    }

    void apply() {
        pending_ = false;
        if (!window_) return;
        QWidget* atlas = window_->findChild<QWidget*>(QStringLiteral("atlasTab"));
        if (!atlas) return;
        atlas_ = atlas;

        if (!initialized_) {
            installAtlasIndexNavigation(atlas);
            QFrame* context = window_->findChild<QFrame*>(QStringLiteral("atlasContextRail"));
            if (context) installReferenceActions(atlas, context);
            styleAtlasOnce(atlas);

            for (QTextEdit* edit : atlas->findChildren<QTextEdit*>()) {
                edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                QWidget* parent = edit->parentWidget();
                const QString role = parent ? parent->objectName() : QString();
                if (role == QStringLiteral("atlasSummaryField")) { edit->setMinimumHeight(74); edit->setMaximumHeight(112); }
                else if (role == QStringLiteral("atlasEditorialSection")) { edit->setMinimumHeight(54); edit->setMaximumHeight(84); }
                else if (role == QStringLiteral("atlasContextCard")) { edit->setMinimumHeight(48); edit->setMaximumHeight(74); }
            }
            atlas->installEventFilter(this);
            initialized_ = true;
        }

        if (auto* index = atlas->findChild<QWidget*>(QStringLiteral("worldIndexPanel"))) {
            const int w = atlas->width();
            index->setMinimumWidth(w < 1180 ? 250 : 286);
            index->setMaximumWidth(w < 1180 ? 284 : 330);
        }
        if (QFrame* context = window_->findChild<QFrame*>(QStringLiteral("atlasContextRail"))) {
            context->setMinimumWidth(atlas->width() >= 1180 ? 280 : 240);
            context->setMaximumWidth(atlas->width() >= 1180 ? 330 : 282);
        }
    }

private:
    void schedule() {
        if (pending_) return;
        pending_ = true;
        QTimer::singleShot(45, this, [this]() { apply(); });
    }

    QPointer<QMainWindow> window_;
    QPointer<QWidget> atlas_;
    bool initialized_ = false;
    bool pending_ = false;
};

} // namespace

void installAtlasWorkspaceController(QMainWindow* window) {
    if (!window || window->property("wbwAtlasWorkspaceControllerV4").toBool()) return;
    window->setProperty("wbwAtlasWorkspaceControllerV4", true);
    auto* filter = new AtlasWorkspaceFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
