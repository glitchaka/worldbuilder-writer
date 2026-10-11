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
        if (watched == window_ && (event->type() == QEvent::Show || event->type() == QEvent::Resize)) schedule();
        return QObject::eventFilter(watched, event);
    }

    void apply() {
        pending_ = false;
        if (!window_) return;

        QWidget* hub = window_->findChild<QWidget*>(QStringLiteral("projectHubPage"));
        QWidget* shell = window_->findChild<QWidget*>(QStringLiteral("hubShell"));
        QWidget* cardsHost = window_->findChild<QWidget*>(QStringLiteral("hubCardsHost"));
        auto* navigation = window_->findChild<QListWidget*>(QStringLiteral("sideNavigation"));
        if (!hub || !shell || !cardsHost) return;

        if (QWidget* duplicateBar = hub->findChild<QWidget*>(QStringLiteral("hubDesktopBar"))) duplicateBar->hide();
        if (QWidget* storage = hub->findChild<QWidget*>(QStringLiteral("hubStorage"))) storage->hide();

        // La biblioteca es una pantalla completa; el pequeño encabezado heredado de MainWindow
        // sólo duplica "Biblioteca / Tus historias" y rompe la composición del mockup.
        QLabel* chromeTitle = window_->findChild<QLabel*>(QStringLiteral("projectTitle"));
        QWidget* chromeHeader = chromeTitle ? chromeTitle->parentWidget() : nullptr;
        auto syncChrome = [navigation, chromeHeader]() {
            if (!chromeHeader || !navigation) return;
            chromeHeader->setVisible(navigation->currentRow() != 0);
        };
        syncChrome();
        if (navigation && !navigation->property("wbwLibraryChromeSync").toBool()) {
            navigation->setProperty("wbwLibraryChromeSync", true);
            QObject::connect(navigation, &QListWidget::currentRowChanged, this, [syncChrome](int) { syncChrome(); });
        }

        if (auto* root = qobject_cast<QVBoxLayout*>(shell->layout())) {
            root->setContentsMargins(44, 34, 44, 44);
            root->setSpacing(22);
        }

        if (auto* scroll = hub->findChild<QScrollArea*>()) {
            scroll->setFrameShape(QFrame::NoFrame);
            scroll->viewport()->setAutoFillBackground(false);
        }

        hub->setStyleSheet(QStringLiteral(R"QSS(
#projectHubPage,#hubShell,#hubCardsHost{background:#0b0e12;color:#d9d4ca;}
#projectHubPage QLabel{background:transparent;}
#hubDesktopBar{background:transparent;border:0;}
#hubKicker,#projectCardGenre,#newProjectMeta{color:#8b7760;font-size:7.5pt;font-weight:700;letter-spacing:1.4px;}
#hubTitle{font-family:'Georgia';font-size:31pt;font-weight:500;color:#f3efe7;}
#hubDescription{font-family:'Georgia';font-size:10.5pt;color:#7d817d;}
#projectCard{background:#12161b;border:1px solid #292f35;border-radius:13px;}
#projectCard:hover,#projectCard:focus{background:#151a20;border-color:#4f6578;}
#projectCardCover{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #263443,stop:.48 #17202a,stop:1 #10151b);color:#b8c7d5;border:0;border-bottom:1px solid #2c343d;border-radius:11px;font-family:'Georgia';font-size:9pt;font-weight:700;letter-spacing:1.1px;}
#projectCardState{background:#192634;color:#9fc4e5;border:1px solid #31516d;border-radius:10px;padding:3px 8px;font-size:7.5pt;}
#projectCardTitle,#newProjectTitle{font-family:'Georgia';font-size:17pt;font-weight:700;color:#f2eee6;}
#projectCardArchive,#newProjectCopy{color:#7f858b;font-size:9pt;}
#projectStatValue{font-weight:700;color:#e4e8ec;font-size:10pt;}
#projectStatLabel,#projectSaved{color:#6f7881;font-size:8pt;}
#hubEmptyState{color:#858d94;padding:30px;}
QFrame#projectCardNew{background:#0f1318;border:1px dashed #3d4a55;border-radius:13px;}
QFrame#projectCardNew:hover,QFrame#projectCardNew:focus{background:#131a21;border-color:#5d88ad;}
#newProjectMark{background:#14283b;color:#7fc4ff;border:1px solid #315e83;border-radius:25px;font-size:21pt;font-weight:300;}
#hubSearch,#hubStatusFilter{background:#10151b;color:#dfe5ea;border:1px solid #303842;border-radius:8px;padding:9px 11px;selection-background-color:#284761;}
#hubSearch:focus,#hubStatusFilter:focus{border-color:#527d9f;}
#hubPrimary{background:#2b78b9;color:#f5f9fc;border:0;border-radius:8px;padding:9px 14px;font-weight:700;}
#hubPrimary:hover{background:#3688ca;}
#hubSecondary{background:#11161c;color:#c4ccd3;border:1px solid #333b44;border-radius:8px;padding:9px 13px;}
#hubSecondary:hover{background:#192028;color:#f4f7fa;border-color:#4b5965;}
)QSS"));

        auto* grid = qobject_cast<QGridLayout*>(cardsHost->layout());
        if (!grid) return;
        grid->setContentsMargins(0, 4, 0, 30);
        grid->setHorizontalSpacing(20);
        grid->setVerticalSpacing(20);
        grid->setAlignment(Qt::AlignLeft | Qt::AlignTop);

        QList<QWidget*> cards;
        while (QLayoutItem* item = grid->takeAt(0)) {
            if (QWidget* widget = item->widget()) cards.append(widget);
            delete item;
        }

        const int available = qMax(320, cardsHost->width());
        const int cardWidth = 310;
        const int columns = qBound(1, available / (cardWidth + 20), 4);

        for (int i = 0; i < cards.size(); ++i) {
            QWidget* card = cards.at(i);
            card->setMinimumSize(cardWidth, 382);
            card->setMaximumSize(cardWidth, 382);
            if (QLabel* cover = card->findChild<QLabel*>(QStringLiteral("projectCardCover"))) {
                cover->setMinimumHeight(176);
                cover->setMaximumHeight(176);
            }
            grid->addWidget(card, i / columns, i % columns, Qt::AlignLeft | Qt::AlignTop);
        }

        for (int column = 0; column < 4; ++column) grid->setColumnStretch(column, 0);
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
    if (!window || window->property("wbwLibraryMockupControllerV4").toBool()) return;
    window->setProperty("wbwLibraryMockupControllerV4", true);
    auto* filter = new LibraryWorkspaceFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
