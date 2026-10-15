#include "ui/ReviewPage.h"

#include "core/ArchiveDocument.h"

#include <QJsonObject>
#include <QVariant>

namespace wbw {

void ReviewPage::installProfileCompatibilityGuard() {
    if (!document_) return;

    const QJsonObject profile = document_->object(QStringLiteral("profile"));
    const bool hadTheme = profile.contains(QStringLiteral("theme"));
    setProperty("wbwLegacyProfileThemePresent", hadTheme);
    setProperty("wbwLegacyProfileThemeValue", hadTheme
        ? profile.value(QStringLiteral("theme")).toVariant()
        : QVariant());

    if (property("wbwLegacyProfileGuardInstalled").toBool()) return;
    setProperty("wbwLegacyProfileGuardInstalled", true);

    connect(this, &ReviewPage::changed, this, [this]() {
        if (!document_ || property("wbwRestoringLegacyProfile").toBool()) return;

        QJsonObject current = document_->object(QStringLiteral("profile"));
        const bool shouldHaveTheme = property("wbwLegacyProfileThemePresent").toBool();
        const QJsonValue preservedTheme = QJsonValue::fromVariant(property("wbwLegacyProfileThemeValue"));

        const bool changedTheme = shouldHaveTheme
            ? current.value(QStringLiteral("theme")) != preservedTheme
            : current.contains(QStringLiteral("theme"));
        if (!changedTheme) return;

        setProperty("wbwRestoringLegacyProfile", true);
        if (shouldHaveTheme) current.insert(QStringLiteral("theme"), preservedTheme);
        else current.remove(QStringLiteral("theme"));
        document_->setObject(QStringLiteral("profile"), current);
        setProperty("wbwRestoringLegacyProfile", false);
    });
}

} // namespace wbw
