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
        if (event->type() == QEvent::Show || event->type() == QEvent::Resize || event->type() == QEvent::LayoutRequest)
            QTimer::singleShot(0, this, [this]() { apply(); });
        return QObject::eventFilter(watched, event);
    }

    void apply() {
        if (!window_) return;
        QWidget* hub = window_->findChild<QWidget*>(QStringLiteral("projectHubPage"));
        QWidget* cardsHost = window_->findChild<QWidget*>(QStringLiteral("hubCardsHost"));
        if (!hub || !cardsHost) return;

        hub->setStyleSheet(QStringLiteral(
            "#projectHubPage,#hubShell,#hubCardsHost{background:#0f1115;color:#e8ebef;}"
            "#hubDesktopBar{background:#14171c;border-bottom:1px solid #2a2f36;}"
            "#hubAppMark,#projectCardMark{background:#c59a5d;color:#111315;border-radius:7px;font-family:'Georgia';font-weight:700;}"
            "#hubAppName{font-size:10.5pt;font-weight:700;color:#f0f2f5;}"
            "#hubAppMode{font-size:8pt;color:#8f97a3;}"
            "#hubKicker,#projectCardGenre,#newProjectMeta{color:#c59a5d;font-size:8pt;font-weight:700;letter-spacing:.8px;}"
            "#hubTitle{font-family:'Georgia';font-size:28pt;font-weight:500;color:#f3f4f6;}"
            "#hubDescription{font-family:'Georgia';font-size:10pt;color:#9ca3ad;}"
            "#hubStorage{background:#151a18;border:1px solid #26352d;border-radius:8px;}"
            "#hubStorageDot{color:#56c18b;}#hubStorageTitle{font-weight:700;color:#e8ebef;}#hubStorageDetail{color:#89929d;font-size:8.5pt;}"
            "#projectCard{background:#171a20;border:1px solid #2b3038;border-radius:10px;}"
            "#projectCard:hover,#projectCard:focus{background:#1b1f26;border-color:#4a515d;}"
            "#projectCardCover{background:#202631;color:#aab4c1;border:0;border-radius:8px;font-size:8pt;font-weight:700;letter-spacing:1px;}"
            "#projectCardState{background:#20242b;color:#cbd1d8;border:1px solid #303640;border-radius:8px;padding:3px 7px;font-size:8pt;}"
            "#projectCardTitle,#newProjectTitle{font-family:'Georgia';font-size:17pt;font-weight:700;color:#f1f3f5;}"
            "#projectCardArchive,#newProjectCopy{color:#929ba6;font-size:9pt;}"
            "#projectStatValue{font-weight:700;color:#e7eaee;font-size:10pt;}#projectStatLabel,#projectSaved{color:#7e8792;font-size:8pt;}"
            "#hubEmptyState{color:#7f8994;padding:28px;}"
            "QFrame#projectCardNew{background:#14171c;border:1px dashed #3a414b;border-radius:10px;}"
            "QFrame#projectCardNew:hover,QFrame#projectCardNew:focus{background:#191d23;border-color:#c59a5d;}"
            "#newProjectMark{background:#221f19;color:#d7ae68;border:1px solid #5a4a33;border-radius:23px;font-size:20pt;font-weight:300;}"
            "#hubSearch,#hubStatusFilter{background:#15181d;color:#e6e9ed;border:1px solid #2d333c;border-radius:8px;padding:7px;}"
            "#hubPrimary{background:#c59a5d;color:#111315;border:0;border-radius:8px;padding:8px 12px;font-weight:700;}"
            "#hubSecondary{background:#181b20;color:#d9dde2;border:1px solid #303640;border-radius:8px;padding:8px 12px;}"
        ));

        auto* grid = qobject_cast<QGridLayout*>(cardsHost->layout());
        if (!grid) return;
        QList<QWidget*> cards;
        while (QLayoutItem* item = grid->takeAt(0)) {
            if (QWidget* w = item->widget()) cards.append(w);
            delete item;
        }

        const int available = qMax(1, cardsHost->width());
        int columns = 1;
        if (available >= 1220) columns = 4;
        else if (available >= 900) columns = 3;
        else if (available >= 600) columns = 2;

        for (int i = 0; i < cards.size(); ++i) {
            QWidget* card = cards.at(i);
            card->setMinimumWidth(0);
            card->setMaximumWidth(QWIDGETSIZE_MAX);
            grid->addWidget(card, i / columns, i % columns);
        }
        for (int c = 0; c < 4; ++c) grid->setColumnStretch(c, c < columns ? 1 : 0);
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
    for (QWidget* widget : window->findChildren<QWidget*>()) widget->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
