#pragma once

#include <QJsonArray>
#include <QList>
#include <QString>

namespace wbw {

class ManuscriptImporter {
public:
    static bool importFile(const QString& path, QJsonArray& chapters, QString* error = nullptr);
};

} // namespace wbw
