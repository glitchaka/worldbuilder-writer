#include "ui/PilinReyEditor.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>

namespace wbw {
namespace {

QString assetDataUrl(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QString mime = QStringLiteral("image/png");
    const QString lower = path.toLower();
    if (lower.endsWith(QStringLiteral(".jpg")) || lower.endsWith(QStringLiteral(".jpeg"))) mime = QStringLiteral("image/jpeg");
    else if (lower.endsWith(QStringLiteral(".webp"))) mime = QStringLiteral("image/webp");
    return QStringLiteral("data:%1;base64,%2").arg(mime, QString::fromLatin1(file.readAll().toBase64()));
}

QString assetId() {
    return QStringLiteral("asset-") + QUuid::createUuid().toString(QUuid::Id128);
}

} // namespace

void PilinReyEditor::importAssetDirectory() {
    const QString root = QFileDialog::getExistingDirectory(this, tr("Importar biblioteca cartográfica"));
    if (root.isEmpty()) return;

    QJsonArray imported;
    QDirIterator it(root,
                    {QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.webp")},
                    QDir::Files, QDirIterator::Subdirectories);
    const QDir rootDir(root);
    while (it.hasNext() && imported.size() < 500) {
        const QString path = it.next();
        const QString data = assetDataUrl(path);
        if (data.isEmpty()) continue;
        const QFileInfo info(path);
        const QString relativeDir = rootDir.relativeFilePath(info.absolutePath());
        QString category = relativeDir.section(QLatin1Char('/'), 0, 0);
        if (category == QStringLiteral(".") || category.isEmpty()) category = tr("Biblioteca");
        imported.append(QJsonObject{{QStringLiteral("id"), assetId()},
                                    {QStringLiteral("name"), info.completeBaseName()},
                                    {QStringLiteral("category"), category},
                                    {QStringLiteral("source"), QStringLiteral("user-import")},
                                    {QStringLiteral("dataUrl"), data}});
    }
    if (imported.isEmpty()) return;

    mutateDocument([&](QJsonObject& pilin) {
        QJsonArray assets = pilin.value(QStringLiteral("assets")).toArray();
        for (const QJsonValue& value : imported) assets.append(value);
        pilin.insert(QStringLiteral("assets"), assets);
    });
}

} // namespace wbw
