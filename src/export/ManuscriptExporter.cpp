#include "export/ManuscriptExporter.h"

#include "core/ArchiveDocument.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QTextDocument>
#include <QUuid>

#include <miniz.h>

namespace wbw {
namespace {

struct ZipEntry {
    QString name;
    QByteArray bytes;
    int compression = MZ_BEST_COMPRESSION;
};

QString xmlEscape(QString value) {
    value.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    value.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    value.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    value.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    value.replace(QLatin1Char('\''), QStringLiteral("&apos;"));
    return value;
}

QString sceneText(const QJsonObject& scene) {
    if (scene.contains(QStringLiteral("text"))) return scene.value(QStringLiteral("text")).toString();
    QTextDocument legacy;
    legacy.setHtml(scene.value(QStringLiteral("content")).toString());
    return legacy.toPlainText();
}

QString chapterHeading(const QJsonObject& chapter, int index) {
    const QString label = chapter.value(QStringLiteral("label")).toString(QObject::tr("Capítulo %1").arg(index + 1)).trimmed();
    const QString title = chapter.value(QStringLiteral("title")).toString().trimmed();
    if (title.isEmpty() || title.compare(QStringLiteral("Sin título"), Qt::CaseInsensitive) == 0) return label;
    return label.isEmpty() ? title : label + QStringLiteral(" — ") + title;
}

QStringList paragraphs(QString text) {
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
}

QByteArray manuscriptTxt(const ArchiveDocument& document) {
    QStringList out;
    const QString title = document.storyTitle().trimmed();
    if (!title.isEmpty()) {
        out.append(title);
        out.append(QString());
    }

    const QJsonArray chapters = document.array(QStringLiteral("writingChapters"));
    for (int c = 0; c < chapters.size(); ++c) {
        const QJsonObject chapter = chapters.at(c).toObject();
        out.append(chapterHeading(chapter, c));
        out.append(QString());
        const QJsonArray scenes = chapter.value(QStringLiteral("scenes")).toArray();
        for (int s = 0; s < scenes.size(); ++s) {
            const QJsonObject scene = scenes.at(s).toObject();
            const QString sceneTitle = scene.value(QStringLiteral("title")).toString().trimmed();
            if (!sceneTitle.isEmpty()) {
                out.append(sceneTitle);
                out.append(QString());
            }
            out.append(sceneText(scene));
            if (s + 1 < scenes.size()) {
                out.append(QString());
                out.append(QStringLiteral("⁂"));
                out.append(QString());
            }
        }
        if (c + 1 < chapters.size()) {
            out.append(QString());
            out.append(QString());
        }
    }
    return out.join(QLatin1Char('\n')).toUtf8();
}

bool writeFile(const QString& path, const QByteArray& bytes, QString* error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        if (error) *error = file.errorString().isEmpty() ? QObject::tr("No se pudo completar el archivo de salida.") : file.errorString();
        return false;
    }
    return true;
}

bool writeZip(const QString& path, const QList<ZipEntry>& entries, QString* error) {
    mz_zip_archive zip{};
    if (!mz_zip_writer_init_heap(&zip, 0, 0)) {
        if (error) *error = QObject::tr("No se pudo iniciar el archivo comprimido.");
        return false;
    }

    for (const ZipEntry& entry : entries) {
        const QByteArray name = entry.name.toUtf8();
        if (mz_zip_writer_add_mem(&zip, name.constData(), entry.bytes.constData(),
                                  static_cast<size_t>(entry.bytes.size()), entry.compression) != MZ_TRUE) {
            if (error) *error = QObject::tr("No se pudo escribir %1 dentro del archivo.").arg(entry.name);
            mz_zip_writer_end(&zip);
            return false;
        }
    }

    void* memory = nullptr;
    size_t size = 0;
    if (!mz_zip_writer_finalize_heap_archive(&zip, &memory, &size)) {
        if (error) *error = QObject::tr("No se pudo finalizar el archivo comprimido.");
        mz_zip_writer_end(&zip);
        return false;
    }
    mz_zip_writer_end(&zip);

    const QByteArray bytes(static_cast<const char*>(memory), static_cast<qsizetype>(size));
    mz_free(memory);
    return writeFile(path, bytes, error);
}

QString wmlParagraph(const QString& text, const QString& style = QString()) {
    QString xml = QStringLiteral("<w:p>");
    if (!style.isEmpty()) xml += QStringLiteral("<w:pPr><w:pStyle w:val=\"") + style + QStringLiteral("\"/></w:pPr>");
    if (text.isEmpty()) return xml + QStringLiteral("</w:p>");
    xml += QStringLiteral("<w:r><w:t xml:space=\"preserve\">") + xmlEscape(text) + QStringLiteral("</w:t></w:r></w:p>");
    return xml;
}

QByteArray docxDocumentXml(const ArchiveDocument& document) {
    QString body;
    if (!document.storyTitle().trimmed().isEmpty()) body += wmlParagraph(document.storyTitle().trimmed(), QStringLiteral("Title"));

    const QJsonArray chapters = document.array(QStringLiteral("writingChapters"));
    for (int c = 0; c < chapters.size(); ++c) {
        const QJsonObject chapter = chapters.at(c).toObject();
        body += wmlParagraph(chapterHeading(chapter, c), QStringLiteral("Heading1"));
        const QJsonArray scenes = chapter.value(QStringLiteral("scenes")).toArray();
        for (int s = 0; s < scenes.size(); ++s) {
            const QJsonObject scene = scenes.at(s).toObject();
            const QString sceneTitle = scene.value(QStringLiteral("title")).toString().trimmed();
            if (!sceneTitle.isEmpty()) body += wmlParagraph(sceneTitle, QStringLiteral("Heading2"));
            for (const QString& paragraph : paragraphs(sceneText(scene))) body += wmlParagraph(paragraph);
            if (s + 1 < scenes.size()) body += wmlParagraph(QStringLiteral("⁂"), QStringLiteral("SceneBreak"));
        }
    }

    body += QStringLiteral("<w:sectPr><w:pgSz w:w=\"8640\" w:h=\"12960\"/><w:pgMar w:top=\"1134\" w:right=\"1077\" w:bottom=\"1247\" w:left=\"1077\"/></w:sectPr>");
    return (QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>")
        + QStringLiteral("<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"><w:body>")
        + body + QStringLiteral("</w:body></w:document>")).toUtf8();
}

bool exportDocx(const QString& path, const ArchiveDocument& document, QString* error) {
    const QByteArray contentTypes = QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
        "<Override PartName=\"/word/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml\"/>"
        "</Types>");
    const QByteArray rootRels = QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/>"
        "</Relationships>");
    const QByteArray documentRels = QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>"
        "</Relationships>");
    const QByteArray styles = QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:styles xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:style w:type=\"paragraph\" w:default=\"1\" w:styleId=\"Normal\"><w:name w:val=\"Normal\"/><w:rPr><w:rFonts w:ascii=\"Garamond\" w:hAnsi=\"Garamond\"/><w:sz w:val=\"22\"/></w:rPr></w:style>"
        "<w:style w:type=\"paragraph\" w:styleId=\"Title\"><w:name w:val=\"Title\"/><w:basedOn w:val=\"Normal\"/><w:rPr><w:b/><w:sz w:val=\"36\"/></w:rPr></w:style>"
        "<w:style w:type=\"paragraph\" w:styleId=\"Heading1\"><w:name w:val=\"heading 1\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:pageBreakBefore/></w:pPr><w:rPr><w:b/><w:sz w:val=\"30\"/></w:rPr></w:style>"
        "<w:style w:type=\"paragraph\" w:styleId=\"Heading2\"><w:name w:val=\"heading 2\"/><w:basedOn w:val=\"Normal\"/><w:rPr><w:i/><w:sz w:val=\"24\"/></w:rPr></w:style>"
        "<w:style w:type=\"paragraph\" w:styleId=\"SceneBreak\"><w:name w:val=\"Scene Break\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:jc w:val=\"center\"/></w:pPr></w:style>"
        "</w:styles>");

    return writeZip(path, {
        {QStringLiteral("[Content_Types].xml"), contentTypes},
        {QStringLiteral("_rels/.rels"), rootRels},
        {QStringLiteral("word/document.xml"), docxDocumentXml(document)},
        {QStringLiteral("word/styles.xml"), styles},
        {QStringLiteral("word/_rels/document.xml.rels"), documentRels}
    }, error);
}

QByteArray odtContent(const ArchiveDocument& document) {
    QString content;
    if (!document.storyTitle().trimmed().isEmpty()) content += QStringLiteral("<text:h text:outline-level=\"1\" text:style-name=\"Title\">") + xmlEscape(document.storyTitle().trimmed()) + QStringLiteral("</text:h>");
    const QJsonArray chapters = document.array(QStringLiteral("writingChapters"));
    for (int c = 0; c < chapters.size(); ++c) {
        const QJsonObject chapter = chapters.at(c).toObject();
        content += QStringLiteral("<text:h text:outline-level=\"1\">") + xmlEscape(chapterHeading(chapter, c)) + QStringLiteral("</text:h>");
        const QJsonArray scenes = chapter.value(QStringLiteral("scenes")).toArray();
        for (int s = 0; s < scenes.size(); ++s) {
            const QJsonObject scene = scenes.at(s).toObject();
            const QString sceneTitle = scene.value(QStringLiteral("title")).toString().trimmed();
            if (!sceneTitle.isEmpty()) content += QStringLiteral("<text:h text:outline-level=\"2\">") + xmlEscape(sceneTitle) + QStringLiteral("</text:h>");
            for (const QString& paragraph : paragraphs(sceneText(scene)))
                content += QStringLiteral("<text:p>") + xmlEscape(paragraph) + QStringLiteral("</text:p>");
            if (s + 1 < scenes.size()) content += QStringLiteral("<text:p text:style-name=\"SceneBreak\">⁂</text:p>");
        }
    }
    return (QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>")
        + QStringLiteral("<office:document-content xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\" office:version=\"1.3\"><office:body><office:text>")
        + content + QStringLiteral("</office:text></office:body></office:document-content>")).toUtf8();
}

bool exportOdt(const QString& path, const ArchiveDocument& document, QString* error) {
    const QByteArray styles = QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<office:document-styles xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" xmlns:style=\"urn:oasis:names:tc:opendocument:xmlns:style:1.0\" xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\" xmlns:fo=\"urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0\" office:version=\"1.3\"><office:styles><style:style style:name=\"Title\" style:family=\"paragraph\"><style:text-properties fo:font-size=\"18pt\" fo:font-weight=\"bold\"/></style:style><style:style style:name=\"SceneBreak\" style:family=\"paragraph\"><style:paragraph-properties fo:text-align=\"center\"/></style:style></office:styles></office:document-styles>");
    const QByteArray manifest = QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<manifest:manifest xmlns:manifest=\"urn:oasis:names:tc:opendocument:xmlns:manifest:1.0\" manifest:version=\"1.3\">"
        "<manifest:file-entry manifest:full-path=\"/\" manifest:media-type=\"application/vnd.oasis.opendocument.text\"/>"
        "<manifest:file-entry manifest:full-path=\"content.xml\" manifest:media-type=\"text/xml\"/>"
        "<manifest:file-entry manifest:full-path=\"styles.xml\" manifest:media-type=\"text/xml\"/>"
        "</manifest:manifest>");
    return writeZip(path, {
        {QStringLiteral("mimetype"), QByteArrayLiteral("application/vnd.oasis.opendocument.text"), MZ_NO_COMPRESSION},
        {QStringLiteral("content.xml"), odtContent(document)},
        {QStringLiteral("styles.xml"), styles},
        {QStringLiteral("META-INF/manifest.xml"), manifest}
    }, error);
}

QByteArray epubBody(const ArchiveDocument& document) {
    QString body;
    if (!document.storyTitle().trimmed().isEmpty()) body += QStringLiteral("<h1 class=\"book-title\">") + xmlEscape(document.storyTitle().trimmed()) + QStringLiteral("</h1>");
    const QJsonArray chapters = document.array(QStringLiteral("writingChapters"));
    for (int c = 0; c < chapters.size(); ++c) {
        const QJsonObject chapter = chapters.at(c).toObject();
        body += QStringLiteral("<section id=\"chapter-%1\"><h1>").arg(c + 1) + xmlEscape(chapterHeading(chapter, c)) + QStringLiteral("</h1>");
        const QJsonArray scenes = chapter.value(QStringLiteral("scenes")).toArray();
        for (int s = 0; s < scenes.size(); ++s) {
            const QJsonObject scene = scenes.at(s).toObject();
            const QString sceneTitle = scene.value(QStringLiteral("title")).toString().trimmed();
            if (!sceneTitle.isEmpty()) body += QStringLiteral("<h2>") + xmlEscape(sceneTitle) + QStringLiteral("</h2>");
            for (const QString& paragraph : paragraphs(sceneText(scene))) {
                if (paragraph.isEmpty()) body += QStringLiteral("<p>&#160;</p>");
                else body += QStringLiteral("<p>") + xmlEscape(paragraph) + QStringLiteral("</p>");
            }
            if (s + 1 < scenes.size()) body += QStringLiteral("<p class=\"scene-break\">⁂</p>");
        }
        body += QStringLiteral("</section>");
    }
    return (QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>")
        + QStringLiteral("<!DOCTYPE html><html xmlns=\"http://www.w3.org/1999/xhtml\" lang=\"es\"><head><meta charset=\"utf-8\"/><title>")
        + xmlEscape(document.storyTitle()) + QStringLiteral("</title><link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\"/></head><body>")
        + body + QStringLiteral("</body></html>")).toUtf8();
}

QByteArray epubNav(const ArchiveDocument& document) {
    QString items;
    const QJsonArray chapters = document.array(QStringLiteral("writingChapters"));
    for (int c = 0; c < chapters.size(); ++c)
        items += QStringLiteral("<li><a href=\"manuscript.xhtml#chapter-%1\">").arg(c + 1)
            + xmlEscape(chapterHeading(chapters.at(c).toObject(), c)) + QStringLiteral("</a></li>");
    return (QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>")
        + QStringLiteral("<!DOCTYPE html><html xmlns=\"http://www.w3.org/1999/xhtml\" xmlns:epub=\"http://www.idpf.org/2007/ops\" lang=\"es\"><head><title>Índice</title></head><body><nav epub:type=\"toc\" id=\"toc\"><h1>Índice</h1><ol>")
        + items + QStringLiteral("</ol></nav></body></html>")).toUtf8();
}

bool exportEpub(const QString& path, const ArchiveDocument& document, QString* error) {
    const QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString modified = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-dd'T'HH:mm:ss'Z'"));
    const QByteArray container = QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\"><rootfiles>"
        "<rootfile full-path=\"OEBPS/package.opf\" media-type=\"application/oebps-package+xml\"/>"
        "</rootfiles></container>");
    const QByteArray package = (QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>")
        + QStringLiteral("<package xmlns=\"http://www.idpf.org/2007/opf\" version=\"3.0\" unique-identifier=\"book-id\"><metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\"><dc:identifier id=\"book-id\">urn:uuid:%1</dc:identifier><dc:title>%2</dc:title><dc:language>es</dc:language><meta property=\"dcterms:modified\">%3</meta></metadata><manifest><item id=\"nav\" href=\"nav.xhtml\" media-type=\"application/xhtml+xml\" properties=\"nav\"/><item id=\"book\" href=\"manuscript.xhtml\" media-type=\"application/xhtml+xml\"/><item id=\"css\" href=\"style.css\" media-type=\"text/css\"/></manifest><spine><itemref idref=\"book\"/></spine></package>")
            .arg(xmlEscape(uuid), xmlEscape(document.storyTitle()), xmlEscape(modified))).toUtf8();
    const QByteArray css = QByteArrayLiteral(
        "body{font-family:serif;line-height:1.45;margin:6%;}h1{page-break-before:always;}"
        ".book-title{page-break-before:auto;text-align:center;margin:30% 0 3em;}h2{font-style:italic;margin-top:2em;}"
        "p{text-indent:1.5em;margin:0 0 .35em}.scene-break{text-indent:0;text-align:center;margin:1.5em 0}");

    return writeZip(path, {
        {QStringLiteral("mimetype"), QByteArrayLiteral("application/epub+zip"), MZ_NO_COMPRESSION},
        {QStringLiteral("META-INF/container.xml"), container},
        {QStringLiteral("OEBPS/package.opf"), package},
        {QStringLiteral("OEBPS/nav.xhtml"), epubNav(document)},
        {QStringLiteral("OEBPS/manuscript.xhtml"), epubBody(document)},
        {QStringLiteral("OEBPS/style.css"), css}
    }, error);
}

} // namespace

bool ManuscriptExporter::exportDocument(Format format,
                                        const QString& path,
                                        const ArchiveDocument& document,
                                        QString* error) {
    if (path.trimmed().isEmpty()) {
        if (error) *error = QObject::tr("No se indicó un archivo de salida.");
        return false;
    }
    switch (format) {
        case Format::Txt:
            return writeFile(path, manuscriptTxt(document), error);
        case Format::Docx:
            return exportDocx(path, document, error);
        case Format::Odt:
            return exportOdt(path, document, error);
        case Format::Epub:
            return exportEpub(path, document, error);
    }
    if (error) *error = QObject::tr("Formato de exportación no compatible.");
    return false;
}

} // namespace wbw
