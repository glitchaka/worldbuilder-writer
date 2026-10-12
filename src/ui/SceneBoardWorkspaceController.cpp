#include "ui/SceneBoardWorkspaceController.h"

#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPointer>
#include <QSignalBlocker>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace wbw {
namespace {

QLabel* section(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("sceneBoardFilterLabel"));
    return label;
}

class SceneBoardWorkspace final : public QObject {
public:
    explicit SceneBoardWorkspace(QMainWindow* window) : QObject(window), window_(window) {
        refreshTimer_.setSingleShot(true);
        refreshTimer_.setInterval(160);
        connect(&refreshTimer_, &QTimer::timeout, this, [this]() { refreshFacets(); });
    }

    bool eventFilter(QObject* watched, QEvent* event) override {
        if ((watched == window_ || watched == board_) && (event->type() == QEvent::Show || event->type() == QEvent::Resize)) scheduleApply();
        return false;
    }

    void apply() {
        applyPending_ = false;
        if (!window_) return;
        QWidget* board = window_->findChild<QWidget*>(QStringLiteral("sceneBoardTab"));
        QWidget* outline = board ? board->findChild<QWidget*>(QStringLiteral("sceneBoardOutlinePanel")) : nullptr;
        auto* search = board ? board->findChild<QLineEdit*>(QStringLiteral("sceneBoardSearch")) : nullptr;
        auto* status = board ? board->findChild<QComboBox*>(QStringLiteral("sceneBoardStatus")) : nullptr;
        auto* view = board ? board->findChild<QGraphicsView*>(QStringLiteral("sceneBoardCanvas")) : nullptr;
        if (!board || !outline || !search || !status || !view) return;
        board_ = board; view_ = view; search_ = search; status_ = status;

        if (!initialized_) {
            auto* layout = qobject_cast<QVBoxLayout*>(outline->layout());
            if (!layout) return;

            auto* filters = new QFrame(outline);
            filters->setObjectName(QStringLiteral("sceneBoardFilters"));
            auto* box = new QVBoxLayout(filters);
            box->setContentsMargins(10, 10, 10, 12);
            box->setSpacing(7);
            auto* kicker = new QLabel(QObject::tr("FILTROS"), filters);
            kicker->setObjectName(QStringLiteral("sceneBoardFilterKicker"));
            box->addWidget(kicker);

            if (search->parentWidget() && search->parentWidget()->layout()) search->parentWidget()->layout()->removeWidget(search);
            search->setParent(filters);
            search->setFixedWidth(QWIDGETSIZE_MAX);
            search->setPlaceholderText(QObject::tr("Buscar escenas…"));
            box->addWidget(search);

            if (status->parentWidget() && status->parentWidget()->layout()) status->parentWidget()->layout()->removeWidget(status);
            status->setParent(filters);
            status->setFixedWidth(QWIDGETSIZE_MAX);
            box->addWidget(section(QObject::tr("ESTADO"), filters));
            box->addWidget(status);

            pov_ = new QComboBox(filters);
            pov_->setObjectName(QStringLiteral("sceneBoardPovFilter"));
            pov_->addItem(QObject::tr("Todos los POV"));
            box->addWidget(section(QObject::tr("PUNTO DE VISTA"), filters));
            box->addWidget(pov_);

            location_ = new QComboBox(filters);
            location_->setObjectName(QStringLiteral("sceneBoardLocationFilter"));
            location_->addItem(QObject::tr("Todas las ubicaciones"));
            box->addWidget(section(QObject::tr("UBICACIÓN"), filters));
            box->addWidget(location_);

            layout->insertWidget(0, filters);

            connect(pov_, &QComboBox::currentTextChanged, search, [this](const QString& value) {
                if (!search_ || !location_) return;
                if (value == QObject::tr("Todos los POV")) {
                    if (location_->currentIndex() == 0) search_->clear();
                    return;
                }
                { QSignalBlocker block(location_); location_->setCurrentIndex(0); }
                search_->setText(value);
            });
            connect(location_, &QComboBox::currentTextChanged, search, [this](const QString& value) {
                if (!search_ || !pov_) return;
                if (value == QObject::tr("Todas las ubicaciones")) {
                    if (pov_->currentIndex() == 0) search_->clear();
                    return;
                }
                { QSignalBlocker block(pov_); pov_->setCurrentIndex(0); }
                search_->setText(value);
            });
            connect(search, &QLineEdit::textChanged, this, [this]() { refreshTimer_.start(); });
            connect(status, &QComboBox::currentTextChanged, this, [this]() { refreshTimer_.start(); });
            if (view->scene()) connect(view->scene(), &QGraphicsScene::changed, this, [this]() { refreshTimer_.start(); });

            board->setStyleSheet(board->styleSheet() + QStringLiteral(R"QSS(
#sceneBoardFilters{background:#101720;border:1px solid #27333f;border-radius:9px;margin:4px 0 8px 0;}
#sceneBoardFilterKicker{color:#7f8c99;font-size:7.5pt;font-weight:700;letter-spacing:1px;}
#sceneBoardFilterLabel{color:#8d98a3;font-size:7.5pt;font-weight:700;letter-spacing:.5px;margin-top:3px;}
#sceneBoardFilters QLineEdit,#sceneBoardFilters QComboBox{background:#0c131a;color:#d9e2e9;border:1px solid #2b3945;border-radius:6px;padding:7px 8px;min-height:20px;}
#sceneBoardFilters QLineEdit:focus,#sceneBoardFilters QComboBox:focus{border-color:#3d7fae;}
)QSS"));

            board->installEventFilter(this);
            initialized_ = true;
            refreshFacets();
        }

        outline->setMinimumWidth(board->width() >= 1280 ? 286 : 250);
        outline->setMaximumWidth(board->width() >= 1280 ? 320 : 280);
    }

private:
    void refreshFacets() {
        if (!view_ || !view_->scene() || !pov_ || !location_) return;
        QStringList povs, locations;
        for (QGraphicsItem* item : view_->scene()->items()) {
            if (item->data(0).toString() != QStringLiteral("scene-card")) continue;
            for (QGraphicsItem* child : item->childItems()) {
                auto* text = dynamic_cast<QGraphicsSimpleTextItem*>(child);
                if (!text) continue;
                const QString value = text->text().trimmed();
                if (!value.startsWith(QStringLiteral("POV:"), Qt::CaseInsensitive)) continue;
                const QStringList parts = value.mid(4).trimmed().split(QStringLiteral(" · "), Qt::SkipEmptyParts);
                if (!parts.isEmpty() && !povs.contains(parts.at(0), Qt::CaseInsensitive)) povs.append(parts.at(0));
                if (parts.size() > 1 && !locations.contains(parts.at(1), Qt::CaseInsensitive)) locations.append(parts.at(1));
            }
        }
        povs.sort(Qt::CaseInsensitive); locations.sort(Qt::CaseInsensitive);
        const QString currentPov = pov_->currentText(), currentLocation = location_->currentText();
        { QSignalBlocker block(pov_); pov_->clear(); pov_->addItem(QObject::tr("Todos los POV")); pov_->addItems(povs); const int i = pov_->findText(currentPov); if (i >= 0) pov_->setCurrentIndex(i); }
        { QSignalBlocker block(location_); location_->clear(); location_->addItem(QObject::tr("Todas las ubicaciones")); location_->addItems(locations); const int i = location_->findText(currentLocation); if (i >= 0) location_->setCurrentIndex(i); }
    }

    void scheduleApply() {
        if (applyPending_) return;
        applyPending_ = true;
        QTimer::singleShot(45, this, [this]() { apply(); });
    }

    QPointer<QMainWindow> window_;
    QPointer<QWidget> board_;
    QPointer<QGraphicsView> view_;
    QPointer<QLineEdit> search_;
    QPointer<QComboBox> status_;
    QPointer<QComboBox> pov_;
    QPointer<QComboBox> location_;
    QTimer refreshTimer_;
    bool initialized_ = false;
    bool applyPending_ = false;
};

} // namespace

void installSceneBoardWorkspaceController(QMainWindow* window) {
    if (!window || window->property("wbwSceneBoardWorkspaceV1").toBool()) return;
    window->setProperty("wbwSceneBoardWorkspaceV1", true);
    auto* controller = new SceneBoardWorkspace(window);
    window->installEventFilter(controller);
    QTimer::singleShot(0, controller, [controller]() { controller->apply(); });
}

} // namespace wbw
