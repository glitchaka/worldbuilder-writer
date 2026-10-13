#pragma once

#include <QJsonArray>
#include <QString>

namespace wbw {

class NativeSpellChecker final {
public:
    // Returns true when a native spelling engine was available and the result is valid.
    // Matches use LanguageTool-compatible offset/length/message/replacements fields so the
    // editor can consume local and network proofing through the same presentation model.
    static bool check(const QString& text, const QString& language, QJsonArray* matches);
};

} // namespace wbw
