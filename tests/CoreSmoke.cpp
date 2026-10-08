#include "core/ArchiveDocument.h"
#include "import/ManuscriptImporter.h"
#include "storage/WbwPackage.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextDocument>

#include <cstdlib>
#include <iostream>

namespace {

bool require(bool condition, const char* message) {
    if (!condition) std::cerr << "FAILED: " << message << '\n';
    return condition;
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    bool ok = true;

    wbw::ArchiveDocument original = wbw::ArchiveDocument::empty(QStringLiteral("Archivo de prueba"), QStringLiteral("Historia de prueba"));
    QJsonObject root = original.root();
    root.insert(QStringLiteral("futureField"), QJsonObject{{QStringLiteral("keepMe"), 42}});
    QJsonArray characters;
    characters.append(QJsonObject{
        {QStringLiteral("id"), QStringLiteral("character-one")},
        {QStringLiteral("name"), QStringLiteral("Ada")},
        {QStringLiteral("imageUrl"), QStringLiteral("data:image/png;base64,iVBORw0KGgo=")}
    });
    root.insert(QStringLiteral("characters"), characters);
    QJsonArray world;
    world.append(QJsonObject{
        {QStringLiteral("id"), QStringLiteral("world-one")},
        {QStringLiteral("name"), QStringLiteral("Puerto de prueba")},
        {QStringLiteral("attachments"), QJsonArray{
            QJsonObject{
                {QStringLiteral("id"), QStringLiteral("attachment-one")},
                {QStringLiteral("name"), QStringLiteral("nota.txt")},
                {QStringLiteral("mimeType"), QStringLiteral("text/plain")},
                {QStringLiteral("kind"), QStringLiteral("text")},
                {QStringLiteral("dataUrl"), QStringLiteral("data:text/plain;base64,aG9sYQ==")}
            }
        }}
    });
    root.insert(QStringLiteral("world"), world);
    original.replaceRoot(root);

    wbw::ArchiveDocument jsonRoundTrip;
    QString error;
    ok &= require(jsonRoundTrip.loadJson(original.toJson(), &error), "JSON round trip loads");
    ok &= require(jsonRoundTrip.root().value(QStringLiteral("futureField")).toObject().value(QStringLiteral("keepMe")).toInt() == 42,
                  "unknown compatible fields survive JSON round trip");

    QTemporaryDir temp;
    ok &= require(temp.isValid(), "temporary directory created");
    const QString packagePath = temp.filePath(QStringLiteral("roundtrip.wbw"));
    ok &= require(wbw::WbwPackage::exportPackage(packagePath, original, &error), "WBW export succeeds");
    wbw::ArchiveDocument imported;
    ok &= require(wbw::WbwPackage::importPackage(packagePath, imported, &error), "WBW import succeeds");
    ok &= require(imported.storyTitle() == QStringLiteral("Historia de prueba"), "WBW preserves project title");
    ok &= require(imported.root().value(QStringLiteral("futureField")).toObject().value(QStringLiteral("keepMe")).toInt() == 42,
                  "WBW preserves unknown compatible fields");
    ok &= require(imported.array(QStringLiteral("characters")).at(0).toObject().value(QStringLiteral("imageUrl")).toString().startsWith(QStringLiteral("data:image/png;base64,")),
                  "WBW restores character image");
    ok &= require(imported.array(QStringLiteral("world")).at(0).toObject().value(QStringLiteral("attachments")).toArray().at(0).toObject().value(QStringLiteral("dataUrl")).toString().contains(QStringLiteral("aG9sYQ==")),
                  "WBW restores world attachment");

    const QString txtPath = temp.filePath(QStringLiteral("manuscrito.txt"));
    QFile txt(txtPath);
    ok &= require(txt.open(QIODevice::WriteOnly | QIODevice::Text), "test manuscript file opens");
    if (txt.isOpen()) {
        txt.write("Capítulo 1 — Llegada\nPrimer párrafo.\n\nSegundo párrafo.\nCapítulo 2 — Partida\nTercer párrafo.\n");
        txt.close();
    }
    QJsonArray chapters;
    ok &= require(wbw::ManuscriptImporter::importFile(txtPath, chapters, &error), "TXT manuscript import succeeds");
    ok &= require(chapters.size() == 2, "TXT manuscript reconstructs two chapters");
    if (chapters.size() >= 2) {
        const QJsonArray scenes = chapters.at(0).toObject().value(QStringLiteral("scenes")).toArray();
        ok &= require(!scenes.isEmpty(), "imported chapter contains a scene");
        if (!scenes.isEmpty()) {
            QTextDocument doc;
            doc.setHtml(scenes.at(0).toObject().value(QStringLiteral("content")).toString());
            ok &= require(doc.toPlainText().contains(QStringLiteral("Primer párrafo")), "imported scene retains manuscript text");
        }
    }

    if (!ok) {
        std::cerr << "Core smoke tests failed. Last error: " << error.toStdString() << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "Worldbuilder Writer core smoke tests passed.\n";
    return EXIT_SUCCESS;
}
