#include "ui/PilinReyEditor.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPixmap>
#include <QTimer>
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

QByteArray assetBytes(const QString& value) {
    if (!value.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive)) return {};
    const qsizetype comma = value.indexOf(QLatin1Char(','));
    if (comma < 0) return {};
    const QString meta = value.mid(5, comma - 5);
    const QByteArray payload = value.mid(comma + 1).toLatin1();
    return meta.contains(QStringLiteral(";base64"), Qt::CaseInsensitive)
        ? QByteArray::fromBase64(payload)
        : QByteArray::fromPercentEncoding(payload);
}

QString assetId() {
    return QStringLiteral("asset-") + QUuid::createUuid().toString(QUuid::Id128);
}

} // namespace

void PilinReyEditor::decorateAssetLibrary() {
    if (!assets_) return;

    assets_->setIconSize(QSize(74, 48));
    assets_->setGridSize(QSize(106, 88));
    assets_->setSpacing(4);

    const QJsonArray values = map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("assets")).toArray();
    const int rows = qMin(assets_->count(), values.size());
    for (int row = 0; row < rows; ++row) {
        QListWidgetItem* item = assets_->item(row);
        if (!item) continue;
        const QJsonObject asset = values.at(row).toObject();
        const QString name = asset.value(QStringLiteral("name")).toString(tr("Símbolo"));
        const QString category = asset.value(QStringLiteral("category")).toString(tr("Biblioteca"));
        const QString source = asset.value(QStringLiteral("source")).toString(QStringLiteral("user-import"));
        item->setText(name);
        item->setToolTip(tr("%1\nCategoría: %2\nOrigen: %3").arg(name, category, source));

        QImage image;
        const QByteArray bytes = assetBytes(asset.value(QStringLiteral("dataUrl")).toString());
        if (!bytes.isEmpty()) image.loadFromData(bytes);
        if (!image.isNull()) {
            const QPixmap preview = QPixmap::fromImage(image).scaled(
                QSize(74, 48), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            item->setIcon(QIcon(preview));
        }
    }

    if (!property("wbwAssetThumbnailRefresh").toBool()) {
        setProperty("wbwAssetThumbnailRefresh", true);
        connect(this, &PilinReyEditor::mapEdited, this, [this](const QJsonObject&) {
            QTimer::singleShot(0, this, &PilinReyEditor::decorateAssetLibrary);
        });
    }
}

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
    decorateAssetLibrary();
}

} // namespace wbw
