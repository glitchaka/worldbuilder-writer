#include "ui/PilinReyEditor.h"

#include <QBuffer>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSvgRenderer>
#include <QTimer>
#include <QUuid>

namespace wbw {
namespace {

QString rasterizedSvgDataUrl(const QByteArray& svg) {
    QSvgRenderer renderer(svg);
    if (!renderer.isValid()) return {};

    QSize source = renderer.defaultSize();
    if (!source.isValid() || source.isEmpty()) source = QSize(512, 512);
    source.scale(QSize(512, 512), Qt::KeepAspectRatio);
    source.setWidth(qMax(32, source.width()));
    source.setHeight(qMax(32, source.height()));

    QImage image(source, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    renderer.render(&painter, image.rect());
    painter.end();

    QByteArray png;
    QBuffer buffer(&png);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) return {};
    return QStringLiteral("data:image/png;base64,%1").arg(QString::fromLatin1(png.toBase64()));
}

QString assetDataUrl(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const QByteArray bytes = file.readAll();
    const QString lower = path.toLower();
    if (lower.endsWith(QStringLiteral(".svg"))) return rasterizedSvgDataUrl(bytes);

    QString mime = QStringLiteral("image/png");
    if (lower.endsWith(QStringLiteral(".jpg")) || lower.endsWith(QStringLiteral(".jpeg"))) mime = QStringLiteral("image/jpeg");
    else if (lower.endsWith(QStringLiteral(".webp"))) mime = QStringLiteral("image/webp");
    return QStringLiteral("data:%1;base64,%2").arg(mime, QString::fromLatin1(bytes.toBase64()));
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

    if (assetsPopover_) {
        for (QLabel* label : assetsPopover_->findChildren<QLabel*>()) {
            if (label->objectName() == QStringLiteral("pilinPopoverCopy") && label->text().contains(tr("símbolos cartográficos"), Qt::CaseInsensitive)) {
                label->setText(tr("Importa símbolos cartográficos propios, históricos o con licencia compatible en PNG, JPG, WebP o SVG. La aplicación no genera assets mediante IA."));
            }
        }
        if (auto* folder = assetsPopover_->findChild<QPushButton*>(QStringLiteral("pilinImportAssetFolder"))) {
            folder->setToolTip(tr("Importa una biblioteca de PNG, JPG, WebP o SVG. Los SVG se rasterizan localmente para mantener el proyecto portable."));
        }
    }

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
                    {QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"),
                     QStringLiteral("*.webp"), QStringLiteral("*.svg")},
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
