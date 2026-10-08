#include "storage/WbwPackage.h"

#include "core/ArchiveDocument.h"

#include <QByteArray>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QSaveFile>

#include <miniz.h>

namespace wbw {
namespace {

constexpr auto Format = "worldbuilder-writer-project";
constexpr int Version = 1;

QString cleanSegment(QString value) {
    value = value.normalized(QString::NormalizationForm_D);
    value.remove(QRegularExpression(QStringLiteral("[\\x{0300}-\\x{036f}]")));
    value.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")), QStringLiteral("-"));
    value.remove(QRegularExpression(QStringLiteral("^-+|-+$")));
    if (value.size() > 100) value.truncate(100);
    return value.isEmpty() ? QStringLiteral("asset") : value;
}

QString extensionFor(const QString& mimeType, const QString& name) {
    const QFileInfo info(name);
    if (!info.suffix().isEmpty() && info.suffix().size() <= 8) return info.suffix().toLower();
    if (mimeType == QStringLiteral("image/jpeg")) return QStringLiteral("jpg");
    if (mimeType == QStringLiteral("image/png")) return QStringLiteral("png");
    if (mimeType == QStringLiteral("image/webp")) return QStringLiteral("webp");
    if (mimeType == QStringLiteral("image/gif")) return QStringLiteral("gif");
    if (mimeType == QStringLiteral("audio/mpeg")) return QStringLiteral("mp3");
    if (mimeType == QStringLiteral("audio/ogg")) return QStringLiteral("ogg");
    if (mimeType == QStringLiteral("video/mp4")) return QStringLiteral("mp4");
    return QStringLiteral("bin");
}

bool decodeDataUrl(const QString& source, QByteArray& bytes, QString& mimeType) {
    if (!source.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive)) return false;
    const qsizetype comma = source.indexOf(QLatin1Char(','));
    if (comma < 0) return false;
    const QString meta = source.mid(5, comma - 5);
    const bool base64 = meta.contains(QStringLiteral(";base64"), Qt::CaseInsensitive);
    mimeType = meta.section(QLatin1Char(';'), 0, 0).trimmed();
    if (mimeType.isEmpty()) mimeType = QStringLiteral("application/octet-stream");
    const QByteArray payload = source.mid(comma + 1).toUtf8();
    bytes = base64 ? QByteArray::fromBase64(payload) : QByteArray::fromPercentEncoding(payload);
    return true;
}

bool loadAsset(const QString& source, QByteArray& bytes, QString& mimeType, QString* error) {
    if (decodeDataUrl(source, bytes, mimeType)) return true;

    QString path = source;
    if (path.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive)) path = QUrl(path).toLocalFile();
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("No se pudo leer el recurso: %1").arg(source);
        return false;
    }
    bytes = file.readAll();
    mimeType = QMimeDatabase().mimeTypeForFile(file.fileName(), QMimeDatabase::MatchContent).name();
    if (mimeType.isEmpty()) mimeType = QStringLiteral("application/octet-stream");
    return true;
}

QString toDataUrl(const QByteArray& bytes, const QString& mimeType) {
    return QStringLiteral("data:%1;base64,%2").arg(
        mimeType.isEmpty() ? QStringLiteral("application/octet-stream") : mimeType,
        QString::fromLatin1(bytes.toBase64()));
}

bool safeAssetPath(const QString& path) {
    return path.startsWith(QStringLiteral("media/")) && !path.contains(QStringLiteral("..")) && !path.startsWith(QLatin1Char('/'));
}

bool addZipBytes(mz_zip_archive& zip, const QString& name, const QByteArray& bytes, int level = MZ_BEST_COMPRESSION) {
    const QByteArray utf8Name = name.toUtf8();
    return mz_zip_writer_add_mem(&zip, utf8Name.constData(), bytes.constData(), static_cast<size_t>(bytes.size()), level) == MZ_TRUE;
}

bool extractZipBytes(mz_zip_archive& zip, const QString& name, QByteArray& out) {
    const QByteArray utf8Name = name.toUtf8();
    size_t size = 0;
    void* memory = mz_zip_reader_extract_file_to_heap(&zip, utf8Name.constData(), &size, 0);
    if (!memory) return false;
    out = QByteArray(static_cast<const char*>(memory), static_cast<qsizetype>(size));
    mz_free(memory);
    return true;
}

void replaceAttachmentData(QJsonArray& records, const QString& recordId, const QString& attachmentId, const QString& dataUrl, qint64 size, const QString& mimeType, const QString& name) {
    for (int i = 0; i < records.size(); ++i) {
        QJsonObject record = records.at(i).toObject();
        if (record.value(QStringLiteral("id")).toString() != recordId) continue;
        QJsonArray attachments = record.value(QStringLiteral("attachments")).toArray();
        bool found = false;
        for (int a = 0; a < attachments.size(); ++a) {
            QJsonObject attachment = attachments.at(a).toObject();
            if (attachment.value(QStringLiteral("id")).toString() != attachmentId) continue;
            attachment.insert(QStringLiteral("dataUrl"), dataUrl);
            attachment.insert(QStringLiteral("size"), static_cast<double>(size));
            attachment.insert(QStringLiteral("mimeType"), mimeType);
            attachment.insert(QStringLiteral("name"), name);
            attachments.replace(a, attachment);
            found = true;
            break;
        }
        if (!found) {
            attachments.append(QJsonObject{
                {QStringLiteral("id"), attachmentId},
                {QStringLiteral("name"), name},
                {QStringLiteral("mimeType"), mimeType},
                {QStringLiteral("size"), static_cast<double>(size)},
                {QStringLiteral("kind"), mimeType.startsWith(QStringLiteral("image/")) ? QStringLiteral("image") : QStringLiteral("document")},
                {QStringLiteral("dataUrl"), dataUrl}
            });
        }
        record.insert(QStringLiteral("attachments"), attachments);
        records.replace(i, record);
        return;
    }
}

} // namespace

bool WbwPackage::exportPackage(const QString& path, const ArchiveDocument& document, QString* error) {
    QJsonObject root = document.root();
    QJsonArray assets;

    mz_zip_archive zip{};
    if (!mz_zip_writer_init_heap(&zip, 0, 0)) {
        if (error) *error = QStringLiteral("No se pudo iniciar el paquete .wbw.");
        return false;
    }

    auto fail = [&](const QString& message) {
        if (error) *error = message;
        mz_zip_writer_end(&zip);
        return false;
    };

    QJsonArray characters = root.value(QStringLiteral("characters")).toArray();
    for (int i = 0; i < characters.size(); ++i) {
        QJsonObject character = characters.at(i).toObject();
        const QString source = character.value(QStringLiteral("imageUrl")).toString();
        if (source.isEmpty()) continue;
        QByteArray bytes;
        QString mimeType;
        QString assetError;
        if (!loadAsset(source, bytes, mimeType, &assetError)) return fail(assetError);
        const QString id = character.value(QStringLiteral("id")).toString();
        const QString ext = extensionFor(mimeType, QString());
        const QString assetPath = QStringLiteral("media/personajes/%1.%2").arg(cleanSegment(id), ext);
        if (!addZipBytes(zip, assetPath, bytes)) return fail(QStringLiteral("No se pudo empaquetar la imagen de %1.").arg(character.value(QStringLiteral("name")).toString()));
        assets.append(QJsonObject{
            {QStringLiteral("kind"), QStringLiteral("character-photo")},
            {QStringLiteral("path"), assetPath},
            {QStringLiteral("characterId"), id},
            {QStringLiteral("name"), character.value(QStringLiteral("name")).toString() + QStringLiteral(".") + ext},
            {QStringLiteral("mimeType"), mimeType}
        });
        character.remove(QStringLiteral("imageUrl"));
        characters.replace(i, character);
    }
    root.insert(QStringLiteral("characters"), characters);

    for (const QString scope : {QStringLiteral("world"), QStringLiteral("magic")}) {
        const QString key = scope == QStringLiteral("world") ? QStringLiteral("world") : QStringLiteral("magicSystems");
        QJsonArray records = root.value(key).toArray();
        for (int i = 0; i < records.size(); ++i) {
            QJsonObject record = records.at(i).toObject();
            QJsonArray attachments = record.value(QStringLiteral("attachments")).toArray();
            for (int a = 0; a < attachments.size(); ++a) {
                QJsonObject attachment = attachments.at(a).toObject();
                const QString source = attachment.value(QStringLiteral("dataUrl")).toString();
                if (source.isEmpty()) continue;
                QByteArray bytes;
                QString mimeType = attachment.value(QStringLiteral("mimeType")).toString();
                QString detected;
                QString assetError;
                if (!loadAsset(source, bytes, detected, &assetError)) return fail(assetError);
                if (mimeType.isEmpty()) mimeType = detected;
                const QString recordId = record.value(QStringLiteral("id")).toString();
                const QString attachmentId = attachment.value(QStringLiteral("id")).toString();
                const QString name = attachment.value(QStringLiteral("name")).toString(QStringLiteral("adjunto"));
                const QString ext = extensionFor(mimeType, name);
                const QString assetPath = QStringLiteral("media/%1/%2/%3-%4.%5")
                    .arg(scope, cleanSegment(recordId), cleanSegment(attachmentId), cleanSegment(QFileInfo(name).completeBaseName()), ext);
                if (!addZipBytes(zip, assetPath, bytes)) return fail(QStringLiteral("No se pudo empaquetar el adjunto %1.").arg(name));
                assets.append(QJsonObject{
                    {QStringLiteral("kind"), QStringLiteral("attachment")},
                    {QStringLiteral("path"), assetPath},
                    {QStringLiteral("scope"), scope},
                    {QStringLiteral("recordId"), recordId},
                    {QStringLiteral("attachmentId"), attachmentId},
                    {QStringLiteral("name"), name},
                    {QStringLiteral("mimeType"), mimeType}
                });
                attachment.remove(QStringLiteral("dataUrl"));
                attachments.replace(a, attachment);
            }
            record.insert(QStringLiteral("attachments"), attachments);
            records.replace(i, record);
        }
        root.insert(key, records);
    }

    QJsonObject manifest{
        {QStringLiteral("format"), QStringLiteral(Format)},
        {QStringLiteral("version"), Version},
        {QStringLiteral("application"), QStringLiteral("Worldbuilder Writer")},
        {QStringLiteral("createdAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("projectFile"), QStringLiteral("project.json")},
        {QStringLiteral("assets"), assets}
    };

    const QByteArray manifestBytes = QJsonDocument(manifest).toJson(QJsonDocument::Indented);
    const QByteArray projectBytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    const QByteArray readme = QByteArrayLiteral("Paquete de proyecto de Worldbuilder Writer. Contiene el proyecto editable y sus recursos.\n");
    if (projectBytes.size() > MaxProjectBytes) return fail(QStringLiteral("Los datos editables del proyecto superan el límite de 4 MB."));
    if (!addZipBytes(zip, QStringLiteral("manifest.json"), manifestBytes) ||
        !addZipBytes(zip, QStringLiteral("project.json"), projectBytes) ||
        !addZipBytes(zip, QStringLiteral("LEEME.txt"), readme)) {
        return fail(QStringLiteral("No se pudo completar el paquete .wbw."));
    }

    void* archiveMemory = nullptr;
    size_t archiveSize = 0;
    if (!mz_zip_writer_finalize_heap_archive(&zip, &archiveMemory, &archiveSize)) return fail(QStringLiteral("No se pudo finalizar el paquete .wbw."));
    mz_zip_writer_end(&zip);

    if (static_cast<qint64>(archiveSize) > MaxPackageBytes) {
        mz_free(archiveMemory);
        if (error) *error = QStringLiteral("El paquete supera el límite de 300 MB.");
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        mz_free(archiveMemory);
        if (error) *error = file.errorString();
        return false;
    }
    const qint64 written = file.write(static_cast<const char*>(archiveMemory), static_cast<qint64>(archiveSize));
    mz_free(archiveMemory);
    if (written != static_cast<qint64>(archiveSize) || !file.commit()) {
        if (error) *error = file.errorString().isEmpty() ? QStringLiteral("No se pudo escribir el paquete .wbw.") : file.errorString();
        return false;
    }
    return true;
}

bool WbwPackage::importPackage(const QString& path, ArchiveDocument& document, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    if (file.size() > MaxPackageBytes) {
        if (error) *error = QStringLiteral("El paquete supera el límite de 300 MB.");
        return false;
    }
    const QByteArray archive = file.readAll();

    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, archive.constData(), static_cast<size_t>(archive.size()), 0)) {
        if (error) *error = QStringLiteral("El archivo no es un paquete ZIP/.wbw válido.");
        return false;
    }

    auto finish = [&]() { mz_zip_reader_end(&zip); };
    QByteArray manifestBytes;
    QByteArray projectBytes;
    if (!extractZipBytes(zip, QStringLiteral("manifest.json"), manifestBytes) || !extractZipBytes(zip, QStringLiteral("project.json"), projectBytes)) {
        finish();
        if (error) *error = QStringLiteral("El paquete .wbw está incompleto.");
        return false;
    }
    if (projectBytes.size() > MaxProjectBytes) {
        finish();
        if (error) *error = QStringLiteral("Los datos editables del proyecto superan el límite admitido.");
        return false;
    }

    const QJsonDocument manifestDoc = QJsonDocument::fromJson(manifestBytes);
    if (!manifestDoc.isObject()) {
        finish();
        if (error) *error = QStringLiteral("El manifiesto .wbw no es válido.");
        return false;
    }
    const QJsonObject manifest = manifestDoc.object();
    if (manifest.value(QStringLiteral("format")).toString() != QStringLiteral(Format) ||
        manifest.value(QStringLiteral("version")).toInt() != Version ||
        manifest.value(QStringLiteral("projectFile")).toString() != QStringLiteral("project.json")) {
        finish();
        if (error) *error = QStringLiteral("El archivo no es un proyecto compatible de Worldbuilder Writer.");
        return false;
    }
    const QJsonArray assets = manifest.value(QStringLiteral("assets")).toArray();
    if (assets.size() > MaxAssets) {
        finish();
        if (error) *error = QStringLiteral("La lista de recursos del paquete supera el límite admitido.");
        return false;
    }

    ArchiveDocument candidate;
    QString jsonError;
    if (!candidate.loadJson(projectBytes, &jsonError)) {
        finish();
        if (error) *error = jsonError;
        return false;
    }
    QJsonObject root = candidate.root();
    QJsonArray characters = root.value(QStringLiteral("characters")).toArray();
    QJsonArray world = root.value(QStringLiteral("world")).toArray();
    QJsonArray magic = root.value(QStringLiteral("magicSystems")).toArray();

    for (const QJsonValue value : assets) {
        const QJsonObject descriptor = value.toObject();
        const QString kind = descriptor.value(QStringLiteral("kind")).toString();
        const QString assetPath = descriptor.value(QStringLiteral("path")).toString();
        const QString name = descriptor.value(QStringLiteral("name")).toString();
        const QString mimeType = descriptor.value(QStringLiteral("mimeType")).toString(QStringLiteral("application/octet-stream"));
        if (!safeAssetPath(assetPath) || name.isEmpty()) {
            finish();
            if (error) *error = QStringLiteral("La lista de recursos del paquete contiene una ruta no válida.");
            return false;
        }
        QByteArray bytes;
        if (!extractZipBytes(zip, assetPath, bytes)) {
            finish();
            if (error) *error = QStringLiteral("Falta el recurso '%1' dentro del paquete.").arg(name);
            return false;
        }
        const QString dataUrl = toDataUrl(bytes, mimeType);
        if (kind == QStringLiteral("character-photo")) {
            const QString characterId = descriptor.value(QStringLiteral("characterId")).toString();
            for (int i = 0; i < characters.size(); ++i) {
                QJsonObject character = characters.at(i).toObject();
                if (character.value(QStringLiteral("id")).toString() != characterId) continue;
                character.insert(QStringLiteral("imageUrl"), dataUrl);
                characters.replace(i, character);
                break;
            }
        } else if (kind == QStringLiteral("attachment")) {
            const QString scope = descriptor.value(QStringLiteral("scope")).toString();
            const QString recordId = descriptor.value(QStringLiteral("recordId")).toString();
            const QString attachmentId = descriptor.value(QStringLiteral("attachmentId")).toString();
            if (scope != QStringLiteral("world") && scope != QStringLiteral("magic")) {
                finish();
                if (error) *error = QStringLiteral("El paquete contiene un ámbito de adjunto no válido.");
                return false;
            }
            replaceAttachmentData(scope == QStringLiteral("world") ? world : magic, recordId, attachmentId, dataUrl, bytes.size(), mimeType, name);
        } else {
            finish();
            if (error) *error = QStringLiteral("El paquete contiene un tipo de recurso desconocido.");
            return false;
        }
    }
    finish();

    root.insert(QStringLiteral("characters"), characters);
    root.insert(QStringLiteral("world"), world);
    root.insert(QStringLiteral("magicSystems"), magic);
    candidate.replaceRoot(root);
    candidate.markClean();
    candidate.setSourcePath(QString());
    document = std::move(candidate);
    return true;
}

} // namespace wbw
