#include "import/ManuscriptImporter.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextDocument>
#include <QXmlStreamReader>
#include <QUuid>

#include <miniz.h>

namespace wbw {
namespace {

QString uid(const QString& prefix) {
    return prefix + QLatin1Char('-') + QUuid::createUuid().toString(QUuid::Id128);
}

QJsonObject sceneObject(const QString& chapterId, const QString& title, const QString& plain, int order) {
    QTextDocument doc;
    doc.setPlainText(plain.trimmed());
    return QJsonObject{
        {QStringLiteral("id"), uid(QStringLiteral("scene"))},
        {QStringLiteral("chapterId"), chapterId},
        {QStringLiteral("order"), order},
        {QStringLiteral("title"), title.isEmpty() ? QStringLiteral("Escena 1") : title},
        {QStringLiteral("content"), doc.toHtml()},
        {QStringLiteral("pov"), QString()},
        {QStringLiteral("location"), QString()},
        {QStringLiteral("narrativeLayer"), QString()},
        {QStringLiteral("status"), QStringLiteral("Borrador")},
        {QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}
    };
}

QJsonObject chapterObject(const QString& heading, const QString& plain, int order) {
    const QString chapterId = uid(QStringLiteral("chapter"));
    QString label = heading.trimmed();
    QString title;
    if (label.isEmpty()) label = QStringLiteral("Capítulo %1").arg(order + 1);
    const QRegularExpression numbered(QStringLiteral("^(cap[ií]tulo|chapter)\\s+([\\divxlcdm]+)\\s*[:.\\-–—]?\\s*(.*)$"), QRegularExpression::CaseInsensitiveOption);
    const auto match = numbered.match(label);
    if (match.hasMatch()) {
        const QString number = match.captured(2);
        title = match.captured(3).trimmed();
        label = QStringLiteral("Capítulo %1").arg(number);
    }
    QJsonArray scenes;
    scenes.append(sceneObject(chapterId, QStringLiteral("Escena 1"), plain, 0));
    return QJsonObject{
        {QStringLiteral("id"), chapterId},
        {QStringLiteral("label"), label},
        {QStringLiteral("title"), title},
        {QStringLiteral("order"), order},
        {QStringLiteral("scenes"), scenes}
    };
}

QJsonArray parsePlain(const QString& input, bool markdown) {
    const QString normalized = QString(input).replace(QStringLiteral("\r\n"), QStringLiteral("\n")).replace(QLatin1Char('\r'), QLatin1Char('\n'));
    const QStringList lines = normalized.split(QLatin1Char('\n'));
    const QRegularExpression chapterLine(QStringLiteral("^\\s*(cap[ií]tulo|chapter)\\s+[\\divxlcdm]+(?:\\s*[:.\\-–—].*)?\\s*$"), QRegularExpression::CaseInsensitiveOption);

    QJsonArray chapters;
    QString heading;
    QStringList body;
    auto flush = [&]() {
        if (heading.trimmed().isEmpty() && body.join(QLatin1Char('\n')).trimmed().isEmpty()) return;
        chapters.append(chapterObject(heading, body.join(QLatin1Char('\n')), chapters.size()));
        heading.clear();
        body.clear();
    };

    for (const QString& raw : lines) {
        QString line = raw;
        QString candidate = line.trimmed();
        bool isHeading = chapterLine.match(candidate).hasMatch();
        if (markdown && candidate.startsWith(QStringLiteral("# "))) {
            candidate = candidate.mid(2).trimmed();
            isHeading = true;
        }
        if (isHeading) {
            flush();
            heading = candidate;
        } else {
            body.append(line);
        }
    }
    flush();
    if (chapters.isEmpty() && !normalized.trimmed().isEmpty()) chapters.append(chapterObject(QStringLiteral("Capítulo 1"), normalized, 0));
    return chapters;
}

bool extractDocxXml(const QByteArray& archive, QByteArray& xml) {
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, archive.constData(), static_cast<size_t>(archive.size()), 0)) return false;
    size_t size = 0;
    void* memory = mz_zip_reader_extract_file_to_heap(&zip, "word/document.xml", &size, 0);
    if (!memory) {
        mz_zip_reader_end(&zip);
        return false;
    }
    xml = QByteArray(static_cast<const char*>(memory), static_cast<qsizetype>(size));
    mz_free(memory);
    mz_zip_reader_end(&zip);
    return true;
}

QJsonArray parseDocx(const QByteArray& xmlBytes) {
    QXmlStreamReader xml(xmlBytes);
    struct Paragraph { QString text; QString style; };
    QList<Paragraph> paragraphs;
    Paragraph current;
    bool inParagraph = false;
    bool inText = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QStringView name = xml.name();
            if (name == u"p") {
                inParagraph = true;
                current = Paragraph{};
            } else if (inParagraph && name == u"pStyle") {
                current.style = xml.attributes().value(QStringLiteral("w:val")).toString();
                if (current.style.isEmpty()) current.style = xml.attributes().value(QStringLiteral("val")).toString();
            } else if (inParagraph && name == u"t") {
                inText = true;
            } else if (inParagraph && (name == u"tab")) {
                current.text += QLatin1Char('\t');
            } else if (inParagraph && (name == u"br" || name == u"cr")) {
                current.text += QLatin1Char('\n');
            }
        } else if (xml.isCharacters() && inParagraph && inText) {
            current.text += xml.text().toString();
        } else if (xml.isEndElement()) {
            const QStringView name = xml.name();
            if (name == u"t") inText = false;
            else if (name == u"p") {
                inParagraph = false;
                paragraphs.append(current);
            }
        }
    }

    QJsonArray chapters;
    QString heading;
    QStringList body;
    const QRegularExpression chapterLine(QStringLiteral("^(cap[ií]tulo|chapter)\\s+[\\divxlcdm]+(?:\\s*[:.\\-–—].*)?$"), QRegularExpression::CaseInsensitiveOption);
    auto flush = [&]() {
        if (heading.trimmed().isEmpty() && body.join(QLatin1Char('\n')).trimmed().isEmpty()) return;
        chapters.append(chapterObject(heading, body.join(QStringLiteral("\n\n")), chapters.size()));
        heading.clear();
        body.clear();
    };
    for (const Paragraph& paragraph : paragraphs) {
        const QString value = paragraph.text.trimmed();
        const QString lowerStyle = paragraph.style.toLower();
        const bool styleHeading = lowerStyle.contains(QStringLiteral("heading1")) || lowerStyle.contains(QStringLiteral("título1")) || lowerStyle.contains(QStringLiteral("titulo1"));
        const bool chapterHeading = chapterLine.match(value).hasMatch();
        if (!value.isEmpty() && (styleHeading || chapterHeading)) {
            flush();
            heading = value;
        } else if (!value.isEmpty()) {
            body.append(paragraph.text);
        }
    }
    flush();
    return chapters;
}

} // namespace

bool ManuscriptImporter::importFile(const QString& path, QJsonArray& chapters, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    const QByteArray bytes = file.readAll();
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QStringLiteral("txt") || suffix == QStringLiteral("md") || suffix == QStringLiteral("markdown")) {
        chapters = parsePlain(QString::fromUtf8(bytes), suffix != QStringLiteral("txt"));
    } else if (suffix == QStringLiteral("docx")) {
        QByteArray xml;
        if (!extractDocxXml(bytes, xml)) {
            if (error) *error = QStringLiteral("El DOCX no contiene word/document.xml o está dañado.");
            return false;
        }
        chapters = parseDocx(xml);
    } else {
        if (error) *error = QStringLiteral("Formato de manuscrito no compatible. Usa TXT, Markdown o DOCX.");
        return false;
    }
    if (chapters.isEmpty()) {
        if (error) *error = QStringLiteral("No se encontró texto importable en el manuscrito.");
        return false;
    }
    return true;
}

} // namespace wbw
