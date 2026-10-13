#include "ui/WritingPage.h"

#include <QComboBox>
#include <QLineEdit>
#include <QStringList>

namespace wbw {

void WritingPage::activatePlanningFilters() {
    if (property("wbwPlanningFiltersV1").toBool()) return;

    auto* search = findChild<QLineEdit*>(QStringLiteral("sceneBoardSearch"));
    auto* pov = findChild<QComboBox*>(QStringLiteral("sceneBoardPovFilter"));
    auto* location = findChild<QComboBox*>(QStringLiteral("sceneBoardLocationFilter"));
    if (!search || !pov || !location) return;

    setProperty("wbwPlanningFiltersV1", true);

    QObject::disconnect(pov, nullptr, this, nullptr);
    QObject::disconnect(location, nullptr, this, nullptr);

    const auto apply = [search, pov, location]() {
        QStringList terms;
        if (pov->currentIndex() > 0) terms.append(pov->currentText().trimmed());
        if (location->currentIndex() > 0) terms.append(location->currentText().trimmed());
        search->setText(terms.join(QLatin1Char(' ')));
    };

    connect(pov, &QComboBox::currentTextChanged, this, [apply](const QString&) { apply(); });
    connect(location, &QComboBox::currentTextChanged, this, [apply](const QString&) { apply(); });
}

} // namespace wbw
