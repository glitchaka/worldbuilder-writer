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
    explicit LibraryWorkspaceFilter(QMainWindow* window)
        : QObject(window), window_(window) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == window_ && (event->type() == QEvent::Show || event->type() == QEvent::Resize || event->type() == QEvent::PaletteChange))
            QTimer::singleShot(0, this, [this]() { apply(); });
        return QObject::eventFilter(watched, event);
    }

    void apply() {
        if (!window_) return;
        QWidget* hub = window_->findChild<QWidget*>(QStringLiteral("projectHubPage"));
        QWidget* cardsHost = window_->findChild<QWidget*>(QStringLiteral("hubCardsHost"));
        if (!hub || !cardsHost) return;

        hub->setStyleSheet(QStringLiteral(R"QSS(
#projectHubPage,#hubShell,#hubCardsHost{background:palette(window);color:palette(window-text);}
#hubDesktopBar{background:palette(base);border-bottom:1px solid palette(mid);}
#hubAppMark,#projectCardMark{background:#c59a5d;color:#111315;border-radius:8px;font-family:'Georgia';font-weight:700;}
#hubAppName{font-size:10.5pt;font-weight:700;color:palette(text);}
#hubAppMode{font-size:8pt;color:palette(window-text);}
#hubKicker,#projectCardGenre,#newProjectMeta{color:#c59a5d;font-size:8pt;font-weight:700;letter-spacing:.9px;}
#hubTitle{font-family:'Georgia';font-size:29pt;font-weight:500;color:palette(text);}
#hubDescription{font-family:'Georgia';font-size:10pt;color:palette(window-text);}
#hubStorage{background:palette(alternate-base);border:1px solid palette(mid);border-radius:9px;}
#hubStorageDot{color:#55c88a;}#hubStorageTitle{font-weight:700;color:palette(text);}#hubStorageDetail{color:palette(window-text);font-size:8.5pt;}
#projectCard{background:palette(base);border:1px solid palette(mid);border-radius:11px;}
#projectCard:hover,#projectCard:focus{background:palette(alternate-base);border-color:#8b6843;}
#projectCardCover{background:palette(alternate-base);color:palette(text);border:0;border-radius:9px;font-size:8pt;font-weight:700;letter-spacing:1px;min-height:128px;}
#projectCardState{background:palette(alternate-base);color:palette(text);border:1px solid palette(mid);border-radius:9px;padding:3px 7px;font-size:8pt;}
#projectCardTitle,#newProjectTitle{font-family:'Georgia';font-size:18pt;font-weight:700;color:palette(text);}
#projectCardArchive,#newProjectCopy{color:palette(window-text);font-size:9pt;}
#projectStatValue{font-weight:700;color:palette(text);font-size:10pt;}#projectStatLabel,#projectSaved{color:palette(window-text);font-size:8pt;}
#hubEmptyState{color:palette(window-text);padding:28px;}
QFrame#projectCardNew{background:palette(alternate-base);border:1px dashed palette(mid);border-radius:11px;}
QFrame#projectCardNew:hover,QFrame#projectCardNew:focus{border-color:#c59a5d;}
#newProjectMark{background:palette(base);color:#c59a5d;border:1px solid #8b6843;border-radius:23px;font-size:20pt;font-weight:300;}
#hubSearch,#hubStatusFilter{background:palette(base);color:palette(text);border:1px solid palette(mid);border-radius:8px;padding:8px 9px;}
#hubSearch:focus,#hubStatusFilter:focus{border-color:#8b6843;}
#hubPrimary{background:#c59a5d;color:#111315;border:0;border-radius:8px;padding:8px 12px;font-weight:700;}
#hubPrimary:hover{background:#d3a86c;}
#hubSecondary{background:palette(alternate-base);color:palette(button-text);border:1px solid palette(mid);border-radius:8px;padding:8px 12px;}
#hubSecondary:hover{background:palette(highlight);color:palette(highlighted-text);}
)QSS"));

        auto* grid = qobject_cast<QGridLayout*>(cardsHost->layout());
        if (!grid) return;
        grid->setHorizontalSpacing(14);
        grid->setVerticalSpacing(14);

        QList<QWidget*> cards;
        while (QLayoutItem* item = grid->takeAt(0)) {
            if (QWidget* w = item->widget()) cards.append(w);
            delete item;
        }

        const int available = qMax(1, cardsHost->width());
        int columns = 1;
        if (available >= 1400) columns = 4;
        else if (available >= 980) columns = 3;
        else if (available >= 650) columns = 2;

        for (int i = 0; i < cards.size(); ++i) {
            QWidget* card = cards.at(i);
            card->setMinimumWidth(0);
            card->setMaximumWidth(QWIDGETSIZE_MAX);
            card->setMinimumHeight(270);
            card->setMaximumHeight(360);
            grid->addWidget(card, i / columns, i % columns);
        }
        for (int c = 0; c < 4; ++c) grid->setColumnStretch(c, c < columns ? 1 : 0);
        grid->setRowStretch((cards.size() + columns - 1) / columns, 1);
    }

private:
    QPointer<QMainWindow> window_;
};

} // namespace

void installLibraryMockupController(QMainWindow* window) {
    if (!window || window->property("wbwLibraryMockupController").toBool()) return;
    window->setProperty("wbwLibraryMockupController", true);
    auto* filter = new LibraryWorkspaceFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
