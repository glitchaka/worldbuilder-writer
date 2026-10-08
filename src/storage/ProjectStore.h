#pragma once

#include <QString>

namespace wbw {

class ArchiveDocument;

class ProjectStore {
public:
    static QString defaultRoot();
    static QString createProjectDirectory(const QString& root = {});
    static bool loadJsonFile(const QString& path, ArchiveDocument& document, QString* error = nullptr);
    static bool saveJsonFile(const QString& path, ArchiveDocument& document, QString* error = nullptr);
    static bool saveIntoProjectDirectory(const QString& directory, ArchiveDocument& document, QString* error = nullptr);
};

} // namespace wbw
