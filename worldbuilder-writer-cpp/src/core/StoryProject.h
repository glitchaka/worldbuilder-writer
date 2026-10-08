#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace wbw {

struct ProjectProfile {
    QString archiveTitle = "Nueva obra";
    QString storyTitle = "Nueva obra";
    QString subtitle = "Biblia narrativa editable";
    QString author;
    QString genre;
    QString status = "Planificación";
    QString synopsis;
    QString theme = "grim";
};

struct CharacterRecord {
    QString id;
    QString name = "Personaje sin nombre";
    QString role;
    QString occupation;
    QString summary;
    QString background;
    QString notes;
    QString photoPath;
    double x = 120.0;
    double y = 120.0;
};

struct RelationshipRecord {
    QString id;
    QString sourceId;
    QString targetId;
    QString type = "Interacción";
    QString label;
    QString details;
    int strength = 1;
};

struct WritingScene {
    QString id;
    QString chapterId;
    QString title = "Escena 1";
    int order = 0;
    QString content;
    QString pov;
    QString location;
    QString narrativeLayer;
    QString status = "Borrador";
};

struct WritingChapter {
    QString id;
    QString label = "Capítulo 1";
    QString title = "Sin título";
    int order = 0;
    QVector<WritingScene> scenes;
};

struct WorldEntry {
    QString id;
    QString category = "País o reino";
    QString name = "Entrada sin nombre";
    QString summary;
    QString details;
};

struct MagicSystem {
    QString id;
    QString name = "Sistema sin nombre";
    QString source;
    QString rules;
    QString costs;
    QString limits;
};

struct TimelineEvent {
    QString id;
    QString when = "Fecha sin definir";
    QString title = "Evento sin título";
    QString summary;
    QString chapter;
    int order = 0;
};

struct StoryProject {
    int formatVersion = 1;
    QString id;
    QString createdAt;
    QString updatedAt;
    ProjectProfile profile;
    QVector<CharacterRecord> characters;
    QVector<RelationshipRecord> relationships;
    QVector<WritingChapter> chapters;
    QVector<WorldEntry> world;
    QVector<MagicSystem> magicSystems;
    QVector<TimelineEvent> timeline;

    static StoryProject create(const QString& title, const QString& theme = "grim");
    static QString newId(const QString& prefix);
    static QString normalizeTheme(const QString& theme);

    QJsonObject toJson() const;
    static StoryProject fromJson(const QJsonObject& object);
};

} // namespace wbw
