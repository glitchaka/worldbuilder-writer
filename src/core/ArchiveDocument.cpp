#include "core/ArchiveDocument.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QStringList>

namespace wbw {
namespace {

QJsonObject defaultManuscriptLayout() {
    return QJsonObject{
        {QStringLiteral("preset"), QStringLiteral("editorial")},
        {QStringLiteral("pageWidthMm"), 152.4},
        {QStringLiteral("pageHeightMm"), 228.6},
        {QStringLiteral("marginTopMm"), 20.0},
        {QStringLiteral("marginRightMm"), 19.0},
        {QStringLiteral("marginBottomMm"), 22.0},
        {QStringLiteral("marginLeftMm"), 19.0},
        {QStringLiteral("fontFamily"), QStringLiteral("Garamond")},
        {QStringLiteral("fontSizePt"), 11.0},
        {QStringLiteral("lineHeight"), 1.35},
        {QStringLiteral("paragraphIndentMm"), 5.0},
        {QStringLiteral("chapterOpening"), QStringLiteral("Página nueva")},
        {QStringLiteral("sceneSeparator"), QStringLiteral("⁂")},
        {QStringLiteral("headerText"), QStringLiteral("{título}")},
        {QStringLiteral("footerText"), QStringLiteral("{página}")}
    };
}

QJsonObject defaultWritingAnalysis() {
    return QJsonObject{
        {QStringLiteral("repetitionWindow"), 40},
        {QStringLiteral("fillerPhrases"), QJsonArray{
            QStringLiteral("de repente"),
            QStringLiteral("entonces"),
            QStringLiteral("bueno"),
            QStringLiteral("en realidad"),
            QStringLiteral("de alguna manera"),
            QStringLiteral("era"),
            QStringLiteral("estaba"),
            QStringLiteral("había")
        }}
    };
}

} // namespace

ArchiveDocument::ArchiveDocument() {
    normalizeMinimumStructure();
    dirty_ = false;
}

ArchiveDocument ArchiveDocument::empty(const QString& archiveTitle, const QString& storyTitle) {
    ArchiveDocument document;
    QJsonObject profile = document.object(QStringLiteral("profile"));
    const QString archive = archiveTitle.trimmed().isEmpty() ? QStringLiteral("Nueva obra") : archiveTitle.trimmed();
    const QString story = storyTitle.trimmed().isEmpty() ? archive : storyTitle.trimmed();
    profile.insert(QStringLiteral("archiveTitle"), archive);
    profile.insert(QStringLiteral("storyTitle"), story);
    profile.insert(QStringLiteral("subtitle"), QStringLiteral("Biblia narrativa editable"));
    profile.insert(QStringLiteral("projectLabel"), QStringLiteral("Proyecto narrativo"));
    profile.insert(QStringLiteral("homeHeading"), story);
    profile.insert(QStringLiteral("location"), QString());
    profile.insert(QStringLiteral("author"), QString());
    profile.insert(QStringLiteral("genre"), QString());
    profile.insert(QStringLiteral("status"), QStringLiteral("Planificación"));
    profile.insert(QStringLiteral("synopsis"), QString());
    profile.insert(QStringLiteral("chapterLabels"), QJsonArray());
    profile.insert(QStringLiteral("theme"), QStringLiteral("grim"));
    profile.insert(QStringLiteral("manuscriptLayout"), defaultManuscriptLayout());
    profile.insert(QStringLiteral("writingAnalysis"), defaultWritingAnalysis());
    document.setObject(QStringLiteral("profile"), profile);
    document.root_.insert(QStringLiteral("title"), archive);
    document.root_.insert(QStringLiteral("dataVersion"), 1);
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
    const QString rootTitle = root_.value(QStringLiteral("title")).toString().trimmed();
    if (!rootTitle.isEmpty()) return rootTitle;
    return object(QStringLiteral("profile")).value(QStringLiteral("archiveTitle")).toString(QStringLiteral("Nueva obra"));
}

QString ArchiveDocument::storyTitle() const {
    return object(QStringLiteral("profile")).value(QStringLiteral("storyTitle")).toString(title());
}

void ArchiveDocument::setTitle(const QString& value) {
    root_.insert(QStringLiteral("title"), value);
    QJsonObject profile = object(QStringLiteral("profile"));
    profile.insert(QStringLiteral("archiveTitle"), value);
    root_.insert(QStringLiteral("profile"), profile);
    dirty_ = true;
}

void ArchiveDocument::setStoryTitle(const QString& value) {
    QJsonObject profile = object(QStringLiteral("profile"));
    profile.insert(QStringLiteral("storyTitle"), value);
    root_.insert(QStringLiteral("profile"), profile);
    dirty_ = true;
}

void ArchiveDocument::normalizeMinimumStructure() {
    if (!root_.contains(QStringLiteral("title"))) root_.insert(QStringLiteral("title"), QStringLiteral("Nueva obra"));
    if (!root_.contains(QStringLiteral("dataVersion"))) root_.insert(QStringLiteral("dataVersion"), 1);

    QJsonObject profile = root_.value(QStringLiteral("profile")).toObject();
    if (!profile.contains(QStringLiteral("archiveTitle"))) profile.insert(QStringLiteral("archiveTitle"), root_.value(QStringLiteral("title")).toString(QStringLiteral("Nueva obra")));
    if (!profile.contains(QStringLiteral("storyTitle"))) profile.insert(QStringLiteral("storyTitle"), profile.value(QStringLiteral("archiveTitle")));
    if (!profile.contains(QStringLiteral("subtitle"))) profile.insert(QStringLiteral("subtitle"), QString());
    if (!profile.contains(QStringLiteral("projectLabel"))) profile.insert(QStringLiteral("projectLabel"), QStringLiteral("Proyecto narrativo"));
    if (!profile.contains(QStringLiteral("homeHeading"))) profile.insert(QStringLiteral("homeHeading"), profile.value(QStringLiteral("storyTitle")));
    if (!profile.contains(QStringLiteral("location"))) profile.insert(QStringLiteral("location"), QString());
    if (!profile.contains(QStringLiteral("author"))) profile.insert(QStringLiteral("author"), QString());
    if (!profile.contains(QStringLiteral("genre"))) profile.insert(QStringLiteral("genre"), QString());
    if (!profile.contains(QStringLiteral("status"))) profile.insert(QStringLiteral("status"), QStringLiteral("Planificación"));
    if (!profile.contains(QStringLiteral("synopsis"))) profile.insert(QStringLiteral("synopsis"), QString());
    if (!profile.value(QStringLiteral("chapterLabels")).isArray()) profile.insert(QStringLiteral("chapterLabels"), QJsonArray());
    if (!profile.contains(QStringLiteral("theme"))) profile.insert(QStringLiteral("theme"), QStringLiteral("grim"));
    if (!profile.value(QStringLiteral("manuscriptLayout")).isObject()) profile.insert(QStringLiteral("manuscriptLayout"), defaultManuscriptLayout());
    if (!profile.value(QStringLiteral("writingAnalysis")).isObject()) profile.insert(QStringLiteral("writingAnalysis"), defaultWritingAnalysis());
    root_.insert(QStringLiteral("profile"), profile);

    const QStringList arrayKeys{
        QStringLiteral("characters"), QStringLiteral("relationships"), QStringLiteral("theories"),
        QStringLiteral("timeline"), QStringLiteral("world"), QStringLiteral("magicSystems"),
        QStringLiteral("worldTexts"), QStringLiteral("magicTexts"), QStringLiteral("writingChapters"),
        QStringLiteral("maps"), QStringLiteral("narrativeHeat"), QStringLiteral("mediaTracks")
    };
    for (const QString& key : arrayKeys) {
        if (!root_.value(key).isArray()) root_.insert(key, QJsonArray());
    }

    if (!root_.value(QStringLiteral("manuscript")).isObject()) {
        root_.insert(QStringLiteral("manuscript"), QJsonObject{
            {QStringLiteral("fileName"), QString()},
            {QStringLiteral("words"), 0},
            {QStringLiteral("chapters"), 0},
            {QStringLiteral("updatedLabel"), QString()}
        });
    }
}

} // namespace wbw
