#include "ui/LibraryMockupController.h"

#include <QEvent>
#include <QGridLayout>
#include <QMainWindow>
#include <QPointer>
#include <QTimer>
#include <QWidget>

namespace wbw {
namespace {

class LibraryWorkspaceFilter final : public QObject {
public:
    explicit LibraryWorkspaceFilter(QMainWindow* window) : QObject(window), window_(window) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == window_ && (event->type() == QEvent::Show || event->type() == QEvent::Resize))
            QTimer::singleShot(0, this, [this]() { apply(); });
        return QObject::eventFilter(watched, event);
    }

    void apply() {
        if (!window_) return;
        QWidget* hub = window_->findChild<QWidget*>(QStringLiteral("projectHubPage"));
        QWidget* cardsHost = window_->findChild<QWidget*>(QStringLiteral("hubCardsHost"));
        if (!hub || !cardsHost) return;

        hub->setStyleSheet(QStringLiteral(R"QSS(
#projectHubPage,#hubShell,#hubCardsHost{background:#0a0c0f;color:#d9d4ca;}
#hubDesktopBar{background:#0b0e11;border-bottom:1px solid #25251f;}
#hubAppMark,#projectCardMark{background:#30261d;color:#edc68d;border:1px solid #6d5236;border-radius:8px;font-family:'Georgia';font-weight:700;}
#hubAppName{font-size:10.5pt;font-weight:700;color:#f1ede4;}
#hubAppMode{font-size:7pt;color:#766f65;font-weight:700;letter-spacing:1.1px;}
#hubKicker,#projectCardGenre,#newProjectMeta{color:#b88a52;font-size:7.5pt;font-weight:700;letter-spacing:1.2px;}
#hubTitle{font-family:'Georgia';font-size:30pt;font-weight:500;color:#f3efe6;}
#hubDescription{font-family:'Georgia';font-size:10pt;color:#7b7d78;}
#hubStorage{background:#111416;border:1px solid #2b2b26;border-radius:10px;}
#hubStorageDot{color:#68b887;}#hubStorageTitle{font-weight:700;color:#ded9d0;}#hubStorageDetail{color:#7c7f79;font-size:8.5pt;}
#projectCard{background:#111416;border:1px solid #2a2b27;border-radius:12px;}
#projectCard:hover,#projectCard:focus{background:#171918;border-color:#76573a;}
#projectCardCover{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #30261d,stop:.46 #181817,stop:1 #0f1112);color:#e5c18b;border:1px solid #493b2d;border-radius:10px;font-family:'Georgia';font-size:9pt;font-weight:700;letter-spacing:1.2px;min-height:156px;}
#projectCardState{background:#211d18;color:#d7b27e;border:1px solid #56412c;border-radius:10px;padding:3px 8px;font-size:7.5pt;}
#projectCardTitle,#newProjectTitle{font-family:'Georgia';font-size:18pt;font-weight:700;color:#f1ede4;}
#projectCardArchive,#newProjectCopy{color:#7f817c;font-size:9pt;}
#projectStatValue{font-weight:700;color:#e5e0d7;font-size:10pt;}#projectStatLabel,#projectSaved{color:#70736e;font-size:8pt;}
#hubEmptyState{color:#858780;padding:30px;}
QFrame#projectCardNew{background:#0d1012;border:1px dashed #4a4034;border-radius:12px;}
QFrame#projectCardNew:hover,QFrame#projectCardNew:focus{background:#151615;border-color:#b88a52;}
#newProjectMark{background:#211d18;color:#d4ab72;border:1px solid #6c5135;border-radius:24px;font-size:20pt;font-weight:300;}
#hubSearch,#hubStatusFilter{background:#0e1113;color:#ddd8cf;border:1px solid #33342f;border-radius:8px;padding:8px 10px;}
#hubSearch:focus,#hubStatusFilter:focus{border-color:#8d6841;}
#hubPrimary{background:#b88a52;color:#11120f;border:0;border-radius:8px;padding:8px 13px;font-weight:700;}
#hubPrimary:hover{background:#c79a61;}
#hubSecondary{background:#141719;color:#c9c5bd;border:1px solid #34352f;border-radius:8px;padding:8px 12px;}
#hubSecondary:hover{background:#1e201e;color:#fff9ee;}
)QSS"));

        auto* grid = qobject_cast<QGridLayout*>(cardsHost->layout());
        if (!grid) return;
        grid->setContentsMargins(0, 0, 0, 24);
        grid->setHorizontalSpacing(18);
        grid->setVerticalSpacing(18);

        QList<QWidget*> cards;
        while (QLayoutItem* item = grid->takeAt(0)) {
            if (QWidget* widget = item->widget()) cards.append(widget);
            delete item;
        }

        const int available = qMax(1, cardsHost->width());
        int columns = 1;
        if (available >= 1380) columns = 4;
        else if (available >= 980) columns = 3;
        else if (available >= 640) columns = 2;

        for (int i = 0; i < cards.size(); ++i) {
            QWidget* card = cards.at(i);
            card->setMinimumWidth(0);
            card->setMaximumWidth(QWIDGETSIZE_MAX);
            card->setMinimumHeight(305);
            card->setMaximumHeight(390);
            grid->addWidget(card, i / columns, i % columns);
        }
        for (int column = 0; column < 4; ++column) grid->setColumnStretch(column, column < columns ? 1 : 0);
        grid->setRowStretch((cards.size() + columns - 1) / columns, 1);
    }

private:
    QPointer<QMainWindow> window_;
};

} // namespace

void installLibraryMockupController(QMainWindow* window) {
    if (!window || window->property("wbwLibraryMockupControllerV3").toBool()) return;
    window->setProperty("wbwLibraryMockupControllerV3", true);
    auto* filter = new LibraryWorkspaceFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
