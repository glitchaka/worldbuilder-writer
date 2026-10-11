#include "ui/LibraryMockupController.h"

#include <QEvent>
#include <QGridLayout>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QPointer>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace wbw {
namespace {

class LibraryWorkspaceFilter final : public QObject {
public:
    explicit LibraryWorkspaceFilter(QMainWindow* window) : QObject(window), window_(window) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == window_ && (event->type() == QEvent::Show || event->type() == QEvent::Resize || event->type() == QEvent::LayoutRequest)) schedule();
        return QObject::eventFilter(watched, event);
    }

    void apply() {
        pending_ = false;
        if (!window_) return;

        QWidget* hub = window_->findChild<QWidget*>(QStringLiteral("projectHubPage"));
        QWidget* shell = window_->findChild<QWidget*>(QStringLiteral("hubShell"));
        QWidget* cardsHost = window_->findChild<QWidget*>(QStringLiteral("hubCardsHost"));
        auto* navigation = window_->findChild<QListWidget*>(QStringLiteral("sideNavigation"));
        QWidget* topShell = window_->findChild<QWidget*>(QStringLiteral("topShell"));
        if (!hub || !shell || !cardsHost) return;

        if (QWidget* duplicateBar = hub->findChild<QWidget*>(QStringLiteral("hubDesktopBar"))) duplicateBar->hide();
        if (QWidget* storage = hub->findChild<QWidget*>(QStringLiteral("hubStorage"))) storage->hide();

        auto syncChrome = [navigation, topShell]() {
            if (!navigation || !topShell) return;
            topShell->setVisible(navigation->currentRow() != 0);
        };
        syncChrome();
        if (navigation && !navigation->property("wbwLibraryChromeSyncV2").toBool()) {
            navigation->setProperty("wbwLibraryChromeSyncV2", true);
            QObject::connect(navigation, &QListWidget::currentRowChanged, this, [syncChrome](int) { syncChrome(); });
        }

        if (auto* root = qobject_cast<QVBoxLayout*>(shell->layout())) {
            root->setContentsMargins(38, 34, 38, 38);
            root->setSpacing(18);
        }
        if (auto* scroll = hub->findChild<QScrollArea*>()) {
            scroll->setFrameShape(QFrame::NoFrame);
            scroll->viewport()->setAutoFillBackground(false);
        }

        hub->setStyleSheet(QStringLiteral(R"QSS(
#projectHubPage,#hubShell,#hubCardsHost{background:#0d1117;color:#d8dee9;}
#projectHubPage QLabel{background:transparent;}
#hubDesktopBar,#hubStorage{background:transparent;border:0;}
#hubKicker{color:#70839a;font-size:7.5pt;font-weight:700;letter-spacing:1.4px;}
#hubTitle{font-family:'Georgia';font-size:32pt;font-weight:600;color:#f2f5f8;}
#hubDescription{font-size:10pt;color:#7f8b99;}
#hubSearch,#hubStatusFilter{background:#10161f;color:#dfe7ef;border:1px solid #263242;border-radius:7px;padding:9px 11px;selection-background-color:#274e73;}
#hubSearch:focus,#hubStatusFilter:focus{border-color:#4a83b6;}
#hubPrimary{background:#2f7ed8;color:white;border:0;border-radius:7px;padding:9px 14px;font-weight:700;}
#hubPrimary:hover{background:#3c8fe9;}
#hubSecondary{background:#111821;color:#c8d2dc;border:1px solid #2b3745;border-radius:7px;padding:9px 13px;}
#hubSecondary:hover{background:#17212c;color:white;border-color:#41546a;}
#projectDelete{background:transparent;color:#8b96a2;border:0;padding:7px 8px;}
#projectDelete:hover{color:#d7dde5;}
#projectCard{background:#111821;border:1px solid #25303d;border-radius:12px;}
#projectCard:hover,#projectCard:focus{background:#141d27;border-color:#456b8d;}
#projectCardCover{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #263443,stop:.52 #18232f,stop:1 #111821);color:#9fb4c9;border:0;border-radius:9px;font-family:'Georgia';font-size:8.5pt;font-weight:700;letter-spacing:1.1px;}
#projectCardGenre,#newProjectMeta{color:#71859a;font-size:7.2pt;font-weight:700;letter-spacing:1.15px;}
#projectCardState{background:#1a2b3b;color:#9bc8ed;border:1px solid #315673;border-radius:10px;padding:3px 8px;font-size:7.5pt;}
#projectCardTitle,#newProjectTitle{font-family:'Georgia';font-size:17pt;font-weight:700;color:#f3f5f7;}
#projectCardArchive,#newProjectCopy{color:#7f8a96;font-size:9pt;}
#projectStatValue{font-weight:700;color:#e5eaf0;font-size:10pt;}
#projectStatLabel,#projectSaved{color:#697684;font-size:8pt;}
QFrame#projectCardNew{background:#0f151d;border:1px dashed #354555;border-radius:12px;}
QFrame#projectCardNew:hover,QFrame#projectCardNew:focus{background:#121b25;border-color:#4f7da6;}
#newProjectMark{background:#14283b;color:#72baf7;border:1px solid #315d82;border-radius:23px;font-size:20pt;font-weight:300;}
#hubEmptyState{color:#7b8793;padding:26px;}
)QSS"));

        auto* grid = qobject_cast<QGridLayout*>(cardsHost->layout());
        if (!grid) return;
        grid->setContentsMargins(0, 6, 0, 26);
        grid->setHorizontalSpacing(18);
        grid->setVerticalSpacing(18);
        grid->setAlignment(Qt::AlignLeft | Qt::AlignTop);

        QList<QWidget*> cards;
        while (QLayoutItem* item = grid->takeAt(0)) {
            if (QWidget* widget = item->widget()) cards.append(widget);
            delete item;
        }

        const int available = qMax(320, cardsHost->width());
        const int cardWidth = 300;
        const int columns = qBound(1, available / (cardWidth + 18), 4);

        for (int i = 0; i < cards.size(); ++i) {
            QWidget* card = cards.at(i);
            card->setMinimumSize(cardWidth, 338);
            card->setMaximumSize(cardWidth, 338);
            if (QLabel* cover = card->findChild<QLabel*>(QStringLiteral("projectCardCover"))) {
                cover->setMinimumHeight(148);
                cover->setMaximumHeight(148);
            }
            grid->addWidget(card, i / columns, i % columns, Qt::AlignLeft | Qt::AlignTop);
        }

        for (int column = 0; column < 5; ++column) grid->setColumnStretch(column, 0);
        grid->setColumnStretch(columns, 1);
    }

private:
    void schedule() {
        if (pending_) return;
        pending_ = true;
        QTimer::singleShot(0, this, [this]() { apply(); });
    }

    QPointer<QMainWindow> window_;
    bool pending_ = false;
};

} // namespace

void installLibraryMockupController(QMainWindow* window) {
    if (!window || window->property("wbwLibraryMockupControllerV5").toBool()) return;
    window->setProperty("wbwLibraryMockupControllerV5", true);
    auto* filter = new LibraryWorkspaceFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
