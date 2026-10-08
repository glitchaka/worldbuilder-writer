#include "storage/ProjectStore.h"

#include "core/ArchiveDocument.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
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

QStringList ProjectStore::projectFiles(const QString& root) {
    const QString base = root.trimmed().isEmpty() ? defaultRoot() : root;
    QDir().mkpath(base);
    QStringList files;
    QDirIterator iterator(base, {QStringLiteral("project.json")}, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) files.append(QFileInfo(iterator.next()).absoluteFilePath());
    std::sort(files.begin(), files.end(), [](const QString& a, const QString& b) {
        return QFileInfo(a).lastModified() > QFileInfo(b).lastModified();
    });
    return files;
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

bool ProjectStore::removeProject(const QString& projectFile, QString* error) {
    const QFileInfo info(projectFile);
    if (!info.exists()) return true;
    const QString root = QFileInfo(defaultRoot()).absoluteFilePath();
    const QString absolute = info.absoluteFilePath();
    if (!absolute.startsWith(root, Qt::CaseInsensitive) || info.fileName() != QStringLiteral("project.json")) {
        if (error) *error = QStringLiteral("Solo se pueden eliminar proyectos de la biblioteca local.");
        return false;
    }
    QDir directory(info.absolutePath());
    if (!directory.removeRecursively()) {
        if (error) *error = QStringLiteral("No se pudo eliminar la carpeta del proyecto.");
        return false;
    }
    return true;
}

bool ProjectStore::backupProject(const QString& projectFile, const QString& destinationDirectory, QString* createdPath, QString* error) {
    QFileInfo sourceInfo(projectFile);
    if (!sourceInfo.exists()) {
        if (error) *error = QStringLiteral("Guarda el proyecto antes de crear una copia de seguridad.");
        return false;
    }
    QDir().mkpath(destinationDirectory);
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString projectName = sourceInfo.dir().dirName().isEmpty() ? QStringLiteral("worldbuilder-writer") : sourceInfo.dir().dirName();
    const QString target = QDir(destinationDirectory).filePath(QStringLiteral("%1-%2.json").arg(projectName, stamp));
    if (!QFile::copy(sourceInfo.absoluteFilePath(), target)) {
        if (error) *error = QStringLiteral("No se pudo crear la copia de seguridad en %1.").arg(target);
        return false;
    }
    if (createdPath) *createdPath = target;
    return true;
}

} // namespace wbw
