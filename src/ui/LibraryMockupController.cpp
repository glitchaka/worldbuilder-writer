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
            "#projectHubPage,#hubShell,#hubCardsHost{background:palette(window);color:palette(window-text);}"
            "#hubDesktopBar{background:palette(base);border-bottom:1px solid palette(mid);}"
            "#hubAppMark,#projectCardMark{background:#c59a5d;color:#111315;border-radius:7px;font-family:'Georgia';font-weight:700;}"
            "#hubAppName{font-size:10.5pt;font-weight:700;color:palette(text);}"
            "#hubAppMode{font-size:8pt;color:palette(mid); }"
            "#hubKicker,#projectCardGenre,#newProjectMeta{color:#b98a53;font-size:8pt;font-weight:700;letter-spacing:.8px;}"
            "#hubTitle{font-family:'Georgia';font-size:28pt;font-weight:500;color:palette(text);}"
            "#hubDescription{font-family:'Georgia';font-size:10pt;color:palette(window-text);}"
            "#hubStorage{background:palette(alternate-base);border:1px solid palette(mid);border-radius:8px;}"
            "#hubStorageDot{color:#3ba66f;}#hubStorageTitle{font-weight:700;color:palette(text);}#hubStorageDetail{color:palette(window-text);font-size:8.5pt;}"
            "#projectCard{background:palette(base);border:1px solid palette(mid);border-radius:10px;}"
            "#projectCard:hover,#projectCard:focus{background:palette(alternate-base);border-color:#8b765d;}"
            "#projectCardCover{background:#202631;color:#d2d8df;border:0;border-radius:8px;font-size:8pt;font-weight:700;letter-spacing:1px;}"
            "#projectCardState{background:palette(alternate-base);color:palette(text);border:1px solid palette(mid);border-radius:8px;padding:3px 7px;font-size:8pt;}"
            "#projectCardTitle,#newProjectTitle{font-family:'Georgia';font-size:17pt;font-weight:700;color:palette(text);}"
            "#projectCardArchive,#newProjectCopy{color:palette(window-text);font-size:9pt;}"
            "#projectStatValue{font-weight:700;color:palette(text);font-size:10pt;}#projectStatLabel,#projectSaved{color:palette(mid);font-size:8pt;}"
            "#hubEmptyState{color:palette(window-text);padding:28px;}"
            "QFrame#projectCardNew{background:palette(alternate-base);border:1px dashed palette(mid);border-radius:10px;}"
            "QFrame#projectCardNew:hover,QFrame#projectCardNew:focus{border-color:#c59a5d;}"
            "#newProjectMark{background:palette(alternate-base);color:#b98a53;border:1px solid #8b765d;border-radius:23px;font-size:20pt;font-weight:300;}"
            "#hubSearch,#hubStatusFilter{background:palette(base);color:palette(text);border:1px solid palette(mid);border-radius:8px;padding:7px;}"
            "#hubPrimary{background:#c59a5d;color:#111315;border:0;border-radius:8px;padding:8px 12px;font-weight:700;}"
            "#hubSecondary{background:palette(base);color:palette(text);border:1px solid palette(mid);border-radius:8px;padding:8px 12px;}"
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
