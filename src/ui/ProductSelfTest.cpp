#include "ui/ProductSelfTest.h"

#include "core/ArchiveDocument.h"
#include "export/ManuscriptExporter.h"
#include "storage/ProjectStore.h"
#include "storage/WbwPackage.h"
#include "ui/SemanticTextEdit.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QGraphicsView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QStringList>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QWidget>

namespace wbw {
namespace {

class ProductSelfTest final : public QObject {
public:
    explicit ProductSelfTest(QMainWindow* window) : QObject(window), window_(window) {}

    void run() {
        testDataPipeline();
        testSemanticReferences();
        testProductSurface();

        if (failures_.isEmpty()) {
            qInfo().noquote() << QStringLiteral("PRODUCT_SELF_TEST_OK");
            QCoreApplication::exit(0);
            return;
        }

        qCritical().noquote() << QStringLiteral("PRODUCT_SELF_TEST_FAIL (%1)").arg(failures_.size());
        for (const QString& failure : failures_) qCritical().noquote() << QStringLiteral(" - ") + failure;
        QCoreApplication::exit(2);
    }

private:
    void expect(bool condition, const QString& message) {
        if (!condition) failures_.append(message);
    }

    ArchiveDocument sampleDocument() const {
        ArchiveDocument document = ArchiveDocument::empty(QStringLiteral("Autoprueba"), QStringLiteral("Autoprueba narrativa"));
        document.setArray(QStringLiteral("world"), QJsonArray{
            QJsonObject{
                {QStringLiteral("id"), QStringLiteral("world-selftest-amber")},
                {QStringLiteral("name"), QStringLiteral("Puerto Ámbar")},
                {QStringLiteral("kind"), QStringLiteral("Ciudad/Lugar")},
                {QStringLiteral("summary"), QStringLiteral("Entrada semántica de autoprueba.")}
            }
        });

        const QJsonObject scene{
            {QStringLiteral("id"), QStringLiteral("scene-selftest")},
            {QStringLiteral("title"), QStringLiteral("Escena de autoprueba")},
            {QStringLiteral("text"), QStringLiteral("Diego llegó a Puerto Ámbar al caer la tarde.")},
            {QStringLiteral("pov"), QStringLiteral("Diego")},
            {QStringLiteral("location"), QStringLiteral("Puerto Ámbar")},
            {QStringLiteral("status"), QStringLiteral("Borrador")},
            {QStringLiteral("order"), 0}
        };
        const QJsonObject chapter{
            {QStringLiteral("id"), QStringLiteral("chapter-selftest")},
            {QStringLiteral("label"), QStringLiteral("Capítulo 1")},
            {QStringLiteral("title"), QStringLiteral("Autoprueba")},
            {QStringLiteral("order"), 0},
            {QStringLiteral("scenes"), QJsonArray{scene}}
        };
        document.setArray(QStringLiteral("writingChapters"), QJsonArray{chapter});
        return document;
    }

    void testDataPipeline() {
        QTemporaryDir temporary;
        expect(temporary.isValid(), QStringLiteral("No se pudo crear el directorio temporal de validación."));
        if (!temporary.isValid()) return;

        ArchiveDocument document = sampleDocument();
        QString error;

        const QString projectDirectory = ProjectStore::createProjectDirectory(temporary.path());
        expect(!projectDirectory.isEmpty(), QStringLiteral("La biblioteca no pudo crear una carpeta de proyecto."));
        if (!projectDirectory.isEmpty()) {
            ArchiveDocument stored = document;
            expect(ProjectStore::saveIntoProjectDirectory(projectDirectory, stored, &error),
                   QStringLiteral("Falló el guardado local del proyecto: %1").arg(error));
            const QString savedPath = stored.sourcePath();
            expect(QFileInfo::exists(savedPath), QStringLiteral("El proyecto guardado no existe en disco."));
            expect(ProjectStore::projectFiles(temporary.path()).contains(QFileInfo(savedPath).absoluteFilePath()),
                   QStringLiteral("La biblioteca no vuelve a descubrir un proyecto ya guardado."));

            ArchiveDocument loaded;
            error.clear();
            expect(ProjectStore::loadJsonFile(savedPath, loaded, &error),
                   QStringLiteral("Falló la reapertura del proyecto guardado: %1").arg(error));
            expect(loaded.storyTitle() == document.storyTitle(),
                   QStringLiteral("El round-trip JSON alteró el título de la obra."));
            expect(loaded.array(QStringLiteral("writingChapters")).size() == 1,
                   QStringLiteral("El round-trip JSON perdió capítulos del manuscrito."));
        }

        const QString wbwPath = temporary.filePath(QStringLiteral("roundtrip.wbw"));
        error.clear();
        expect(WbwPackage::exportPackage(wbwPath, document, &error),
               QStringLiteral("Falló la exportación WBW: %1").arg(error));
        expect(QFileInfo(wbwPath).size() > 0, QStringLiteral("La exportación WBW quedó vacía."));
        ArchiveDocument imported;
        error.clear();
        expect(WbwPackage::importPackage(wbwPath, imported, &error),
               QStringLiteral("Falló la reimportación WBW: %1").arg(error));
        expect(imported.array(QStringLiteral("world")).size() == 1,
               QStringLiteral("El round-trip WBW perdió entradas del atlas."));

        struct ExportCase {
            ManuscriptExporter::Format format;
            const char* extension;
        };
        const ExportCase cases[] = {
            {ManuscriptExporter::Format::Txt, "txt"},
            {ManuscriptExporter::Format::Docx, "docx"},
            {ManuscriptExporter::Format::Odt, "odt"},
            {ManuscriptExporter::Format::Epub, "epub"}
        };
        for (const ExportCase& item : cases) {
            const QString path = temporary.filePath(QStringLiteral("manuscript.%1").arg(QString::fromLatin1(item.extension)));
            error.clear();
            const bool exported = ManuscriptExporter::exportDocument(item.format, path, document, &error);
            expect(exported, QStringLiteral("Falló la exportación %1: %2").arg(QString::fromLatin1(item.extension).toUpper(), error));
            if (exported) {
                expect(QFileInfo(path).size() > 0,
                       QStringLiteral("La exportación %1 quedó vacía.").arg(QString::fromLatin1(item.extension).toUpper()));
            }
        }
    }

    void testSemanticReferences() {
        ArchiveDocument document = sampleDocument();
        SemanticTextEdit probe;
        probe.setPlainText(QStringLiteral("Una nota sobre Puerto Ámbar dentro del manuscrito."));
        QJsonArray referenceIndex;
        connect(&probe, &SemanticTextEdit::referenceIndexChanged, &probe,
                [&referenceIndex](const QJsonArray& references) { referenceIndex = references; });
        probe.setArchiveDocument(&document);
        probe.refreshSemanticReferences();

        bool found = false;
        for (const QJsonValue& value : referenceIndex) {
            const QJsonObject ref = value.toObject();
            if (ref.value(QStringLiteral("kind")).toString() == QStringLiteral("world") &&
                ref.value(QStringLiteral("id")).toString() == QStringLiteral("world-selftest-amber")) {
                found = true;
                break;
            }
        }
        expect(found, QStringLiteral("El editor no detectó una referencia semántica in-text del atlas."));
    }

    void route(QListWidget* navigation, int row) {
        if (!navigation) return;
        navigation->setCurrentRow(row);
        QApplication::processEvents();
    }

    void testProductSurface() {
        expect(window_ != nullptr, QStringLiteral("La ventana principal no existe."));
        if (!window_) return;

        expect(window_->windowFlags().testFlag(Qt::FramelessWindowHint),
               QStringLiteral("La ventana recuperó decoración nativa en vez del chrome propio."));
        bool visibleNativeMenu = false;
        for (QMenuBar* menu : window_->findChildren<QMenuBar*>()) {
            if (menu && menu->isVisible()) {
                visibleNativeMenu = true;
                break;
            }
        }
        expect(!visibleNativeMenu, QStringLiteral("Hay una barra de menú nativa visible."));

        auto* navigation = window_->findChild<QListWidget*>(QStringLiteral("productNavigation"));
        auto* pages = window_->findChild<QStackedWidget*>(QStringLiteral("pageStack"));
        expect(navigation != nullptr, QStringLiteral("No existe la navegación principal del producto."));
        expect(pages != nullptr, QStringLiteral("No existe el stack principal de módulos."));
        if (!navigation || !pages) return;
        expect(navigation->count() == 7, QStringLiteral("La navegación principal no contiene los siete módulos aprobados."));

        const QStringList expectedLabels{
            QStringLiteral("Biblioteca"), QStringLiteral("Escritura"), QStringLiteral("Tablero"),
            QStringLiteral("Atlas"), QStringLiteral("Mapas"), QStringLiteral("Revisión"), QStringLiteral("Configuración")
        };
        const int labelCount = qMin(navigation->count(), static_cast<int>(expectedLabels.size()));
        for (int i = 0; i < labelCount; ++i) {
            expect(navigation->item(i)->text() == expectedLabels.at(i),
                   QStringLiteral("Etiqueta de navegación incorrecta en el módulo %1.").arg(i));
        }

        route(navigation, 0);
        expect(pages->currentWidget() && pages->currentWidget()->objectName() == QStringLiteral("projectHubPage"),
               QStringLiteral("Biblioteca no abre como superficie principal."));
        expect(window_->findChild<QWidget*>(QStringLiteral("hubCardsHost")) != nullptr,
               QStringLiteral("Biblioteca no tiene superficie de tarjetas de proyectos."));
        expect(window_->findChild<QWidget*>(QStringLiteral("projectCardNew")) != nullptr,
               QStringLiteral("Biblioteca no ofrece una tarjeta real para crear una obra."));

        route(navigation, 1);
        expect(pages->currentWidget() && pages->currentWidget()->objectName() == QStringLiteral("writingPage"),
               QStringLiteral("Escritura no abre el manuscrito."));
        expect(window_->findChild<SemanticTextEdit*>(QStringLiteral("sceneEditor")) != nullptr,
               QStringLiteral("No existe el editor semántico del manuscrito."));
        expect(window_->findChild<QTreeWidget*>(QStringLiteral("manuscriptTree")) != nullptr,
               QStringLiteral("No existe el índice de capítulos y escenas."));

        route(navigation, 2);
        expect(pages->currentWidget() && pages->currentWidget()->objectName() == QStringLiteral("writingPage"),
               QStringLiteral("Tablero no abre dentro del módulo de escritura."));
        expect(window_->findChild<QGraphicsView*>(QStringLiteral("sceneBoardCanvas")) != nullptr,
               QStringLiteral("No existe el lienzo visual del tablero de escenas."));

        route(navigation, 3);
        expect(pages->currentWidget() && pages->currentWidget()->objectName() == QStringLiteral("worldPage"),
               QStringLiteral("Atlas no abre su espacio editorial."));
        expect(window_->findChild<QWidget*>(QStringLiteral("atlasTab")) != nullptr,
               QStringLiteral("No existe la superficie editorial del atlas."));
        expect(window_->findChild<QLineEdit*>(QStringLiteral("atlasSearch")) != nullptr,
               QStringLiteral("Atlas no tiene búsqueda contextual."));

        route(navigation, 4);
        expect(pages->currentWidget() && pages->currentWidget()->objectName() == QStringLiteral("mapWorkspace"),
               QStringLiteral("Mapas no abre el editor cartográfico dedicado."));
        expect(window_->findChild<QWidget*>(QStringLiteral("pilinReyEditor")) != nullptr,
               QStringLiteral("No existe el editor cartográfico nativo."));

        route(navigation, 5);
        expect(pages->currentWidget() && pages->currentWidget()->objectName() == QStringLiteral("reviewPage"),
               QStringLiteral("Revisión no abre su workspace."));
        auto* reviewTabs = window_->findChild<QTabWidget*>(QStringLiteral("reviewTabs"));
        const auto reviewSections = window_->findChildren<QPushButton*>(QStringLiteral("reviewWorkspaceSection"));
        expect(reviewTabs != nullptr, QStringLiteral("Revisión perdió sus superficies funcionales internas."));
        expect(reviewSections.size() == 4,
               QStringLiteral("Revisión no expone Diagnóstico, Maquetación, Proyecto y Ambientación."));
        if (reviewTabs) {
            for (QPushButton* button : reviewSections) {
                if (!button) continue;
                const int target = button->property("targetTab").toInt();
                button->click();
                QApplication::processEvents();
                expect(reviewTabs->currentIndex() == target,
                       QStringLiteral("Una sección de Revisión no navega a su superficie real."));
            }
        }

        const QStringList exportLabels{QStringLiteral("PDF"), QStringLiteral("TXT"), QStringLiteral("DOCX"), QStringLiteral("ODT"), QStringLiteral("EPUB")};
        for (const QString& label : exportLabels) {
            bool present = false;
            for (QPushButton* button : window_->findChildren<QPushButton*>()) {
                if (button && button->text() == label) {
                    present = true;
                    break;
                }
            }
            expect(present, QStringLiteral("No está expuesta la salida %1 en Revisión.").arg(label));
        }

        route(navigation, 6);
        expect(pages->currentWidget() && pages->currentWidget()->objectName() == QStringLiteral("settingsWorkspace"),
               QStringLiteral("Configuración no abre su workspace dedicado."));
        if (auto* output = window_->findChild<QLabel*>(QStringLiteral("settingsOutputText"))) {
            expect(output->text().contains(QStringLiteral("DOCX")) && output->text().contains(QStringLiteral("SVG")),
                   QStringLiteral("Configuración no informa las salidas que el producto realmente implementa."));
        } else {
            expect(false, QStringLiteral("Configuración perdió el resumen de formatos de salida."));
        }
    }

    QMainWindow* window_ = nullptr;
    QStringList failures_;
};

} // namespace

void scheduleProductSelfTest(QMainWindow* window) {
    if (!window) {
        qCritical().noquote() << QStringLiteral("PRODUCT_SELF_TEST_FAIL: ventana nula");
        QCoreApplication::exit(2);
        return;
    }
    auto* test = new ProductSelfTest(window);
    QTimer::singleShot(200, test, [test]() { test->run(); });
}

} // namespace wbw
