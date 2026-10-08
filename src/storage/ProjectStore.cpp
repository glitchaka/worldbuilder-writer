#include "storage/ProjectStore.h"

#include "core/ArchiveDocument.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

namespace wbw {

QString ProjectStore::defaultRoot() {
    QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (documents.isEmpty()) documents = QDir::homePath();
    return QDir(documents).filePath(QStringLiteral("Worldbuilder Writer/Projects"));
}

QString ProjectStore::createProjectDirectory(const QString& root) {
    const QString base = root.trimmed().isEmpty() ? defaultRoot() : root;
    QDir().mkpath(base);
    const QString id = QStringLiteral("project-") + QUuid::createUuid().toString(QUuid::Id128);
    const QString directory = QDir(base).filePath(id);
    QDir().mkpath(directory);
    return directory;
}

bool ProjectStore::loadJsonFile(const QString& path, ArchiveDocument& document, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    if (!document.loadJson(file.readAll(), error)) return false;
    document.setSourcePath(QFileInfo(path).absoluteFilePath());
    document.markClean();
    return true;
}

bool ProjectStore::saveJsonFile(const QString& path, ArchiveDocument& document, QString* error) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    if (file.write(document.toJson()) < 0) {
        if (error) *error = file.errorString();
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    document.setSourcePath(QFileInfo(path).absoluteFilePath());
    document.markClean();
    return true;
}

bool ProjectStore::saveIntoProjectDirectory(const QString& directory, ArchiveDocument& document, QString* error) {
    return saveJsonFile(QDir(directory).filePath(QStringLiteral("project.json")), document, error);
}

} // namespace wbw
