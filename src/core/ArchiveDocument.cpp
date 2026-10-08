#include "core/ArchiveDocument.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonParseError>

namespace wbw {

ArchiveDocument::ArchiveDocument() {
    normalizeMinimumStructure();
    dirty_ = false;
}

ArchiveDocument ArchiveDocument::empty(const QString& archiveTitle, const QString& storyTitle) {
    ArchiveDocument document;
    QJsonObject profile = document.object("profile");
    const QString archive = archiveTitle.trimmed().isEmpty() ? QStringLiteral("Nueva obra") : archiveTitle.trimmed();
    const QString story = storyTitle.trimmed().isEmpty() ? archive : storyTitle.trimmed();
    profile.insert("archiveTitle", archive);
    profile.insert("storyTitle", story);
    profile.insert("subtitle", QStringLiteral("Biblia narrativa editable"));
    profile.insert("projectLabel", QStringLiteral("Proyecto narrativo"));
    profile.insert("homeHeading", story);
    profile.insert("location", QString());
    profile.insert("author", QString());
    profile.insert("genre", QString());
    profile.insert("status", QStringLiteral("Planificación"));
    profile.insert("synopsis", QString());
    profile.insert("chapterLabels", QJsonArray());
    profile.insert("theme", QStringLiteral("grim"));
    document.setObject("profile", profile);
    document.root_.insert("title", archive);
    document.root_.insert("dataVersion", 1);
    document.dirty_ = true;
    return document;
}

bool ArchiveDocument::loadJson(const QByteArray& bytes, QString* error) {
    QJsonParseError parseError;
    const QJsonDocument parsed = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !parsed.isObject()) {
        if (error) *error = parseError.error != QJsonParseError::NoError
            ? parseError.errorString()
            : QStringLiteral("El archivo no contiene un objeto JSON de proyecto.");
        return false;
    }

    root_ = parsed.object();
    normalizeMinimumStructure();
    dirty_ = false;
    return true;
}

QByteArray ArchiveDocument::toJson() const {
    return QJsonDocument(root_).toJson(QJsonDocument::Indented);
}

void ArchiveDocument::replaceRoot(QJsonObject root) {
    root_ = std::move(root);
    normalizeMinimumStructure();
    dirty_ = true;
}

QJsonArray ArchiveDocument::array(const QString& key) const {
    return root_.value(key).toArray();
}

void ArchiveDocument::setArray(const QString& key, const QJsonArray& value) {
    root_.insert(key, value);
    dirty_ = true;
}

QJsonObject ArchiveDocument::object(const QString& key) const {
    return root_.value(key).toObject();
}

void ArchiveDocument::setObject(const QString& key, const QJsonObject& value) {
    root_.insert(key, value);
    dirty_ = true;
}

QString ArchiveDocument::title() const {
    const QString rootTitle = root_.value("title").toString().trimmed();
    if (!rootTitle.isEmpty()) return rootTitle;
    return object("profile").value("archiveTitle").toString(QStringLiteral("Nueva obra"));
}

QString ArchiveDocument::storyTitle() const {
    return object("profile").value("storyTitle").toString(title());
}

void ArchiveDocument::setTitle(const QString& value) {
    root_.insert("title", value);
    QJsonObject profile = object("profile");
    profile.insert("archiveTitle", value);
    root_.insert("profile", profile);
    dirty_ = true;
}

void ArchiveDocument::setStoryTitle(const QString& value) {
    QJsonObject profile = object("profile");
    profile.insert("storyTitle", value);
    root_.insert("profile", profile);
    dirty_ = true;
}

void ArchiveDocument::normalizeMinimumStructure() {
    if (!root_.contains("title")) root_.insert("title", QStringLiteral("Nueva obra"));

    QJsonObject profile = root_.value("profile").toObject();
    if (!profile.contains("archiveTitle")) profile.insert("archiveTitle", root_.value("title").toString(QStringLiteral("Nueva obra")));
    if (!profile.contains("storyTitle")) profile.insert("storyTitle", profile.value("archiveTitle"));
    if (!profile.contains("status")) profile.insert("status", QStringLiteral("Planificación"));
    if (!profile.contains("theme")) profile.insert("theme", QStringLiteral("grim"));
    root_.insert("profile", profile);

    const QStringList arrayKeys{
        "characters", "relationships", "theories", "timeline", "world", "magicSystems",
        "worldTexts", "magicTexts", "writingChapters", "maps", "narrativeHeat"
    };
    for (const QString& key : arrayKeys) {
        if (!root_.value(key).isArray()) root_.insert(key, QJsonArray());
    }

    if (!root_.value("manuscript").isObject()) {
        root_.insert("manuscript", QJsonObject{
            {"fileName", QString()}, {"words", 0}, {"chapters", 0}, {"updatedLabel", QString()}
        });
    }
}

} // namespace wbw
