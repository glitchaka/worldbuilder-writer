#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace wbw {

class ArchiveDocument {
public:
    ArchiveDocument();

    static ArchiveDocument empty(const QString& archiveTitle = {}, const QString& storyTitle = {});

    bool loadJson(const QByteArray& bytes, QString* error = nullptr);
    QByteArray toJson() const;

    const QJsonObject& root() const { return root_; }
    void replaceRoot(QJsonObject root);

    QJsonArray array(const QString& key) const;
    void setArray(const QString& key, const QJsonArray& value);

    QJsonObject object(const QString& key) const;
    void setObject(const QString& key, const QJsonObject& value);

    QString title() const;
    QString storyTitle() const;
    void setTitle(const QString& value);
    void setStoryTitle(const QString& value);

    bool isDirty() const { return dirty_; }
    void markClean() { dirty_ = false; }
    void markDirty() { dirty_ = true; }

    QString sourcePath() const { return sourcePath_; }
    void setSourcePath(QString path) { sourcePath_ = std::move(path); }

private:
    void normalizeMinimumStructure();

    QJsonObject root_;
    QString sourcePath_;
    bool dirty_ = false;
};

} // namespace wbw
