#pragma once

#include <QString>

namespace wbw {

class ArchiveDocument;

class ManuscriptExporter final {
public:
    enum class Format {
        Txt,
        Docx,
        Odt,
        Epub
    };

    static bool exportDocument(Format format,
                               const QString& path,
                               const ArchiveDocument& document,
                               QString* error = nullptr);
};

} // namespace wbw
