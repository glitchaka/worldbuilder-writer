#pragma once

#include <QString>

namespace wbw {

class ArchiveDocument;

class WbwPackage {
public:
    static constexpr qint64 MaxPackageBytes = 300LL * 1024LL * 1024LL;
    static constexpr qint64 MaxProjectBytes = 4LL * 1000LL * 1000LL;
    static constexpr int MaxAssets = 1500;

    static bool importPackage(const QString& path, ArchiveDocument& document, QString* error = nullptr);
    static bool exportPackage(const QString& path, const ArchiveDocument& document, QString* error = nullptr);
};

} // namespace wbw
