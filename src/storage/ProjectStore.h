#pragma once

#include <algorithm>
#include <QString>
#include <QStringList>

namespace wbw {

class ArchiveDocument;

class ProjectStore {
public:
    static QString defaultRoot();
    static QString createProjectDirectory(const QString& root = {});
    static QStringList projectFiles(const QString& root = {});
    static bool loadJsonFile(const QString& path, ArchiveDocument& document, QString* error = nullptr);
    static bool saveJsonFile(const QString& path, ArchiveDocument& document, QString* error = nullptr);
    static bool saveIntoProjectDirectory(const QString& directory, ArchiveDocument& document, QString* error = nullptr);
    static bool removeProject(const QString& projectFile, QString* error = nullptr);
    static bool backupProject(const QString& projectFile, const QString& destinationDirectory, QString* createdPath = nullptr, QString* error = nullptr);
};

} // namespace wbw
