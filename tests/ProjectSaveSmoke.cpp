#include "core/ArchiveDocument.h"
#include "storage/ProjectStore.h"
#include "ui/MainWindow.h"
#include "ui/NavigationConsistencyController.h"
#include "ui/ProductReorganizer.h"
#include "ui/ProjectHubPage.h"
#include "ui/WritingPage.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTimer>
#include <QtTest/QTest>

#include <iostream>

namespace {

bool require(bool condition, const char* message) {
    if (!condition) std::cerr << "FAILED: " << message << '\n';
    return condition;
}

void settleUi() {
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 30);
}

wbw::ArchiveDocument fixture(const QString& title, const QString& text) {
    wbw::ArchiveDocument document = wbw::ArchiveDocument::empty(title, title);
    QJsonObject root = document.root();
    root.insert(QStringLiteral("futureField"), QJsonObject{{QStringLiteral("keepMe"), 42}});
    root.insert(QStringLiteral("writingChapters"), QJsonArray{QJsonObject{
        {QStringLiteral("id"), QStringLiteral("chapter-one")},
        {QStringLiteral("label"), QStringLiteral("Capítulo 1")},
        {QStringLiteral("scenes"), QJsonArray{QJsonObject{
            {QStringLiteral("id"), QStringLiteral("scene-one")},
            {QStringLiteral("title"), QStringLiteral("Escena original")},
            {QStringLiteral("text"), text},
            {QStringLiteral("formatting"), QJsonArray{QJsonObject{
                {QStringLiteral("start"), 0}, {QStringLiteral("length"), 4},
                {QStringLiteral("bold"), true}
            }}}
        }}}
    }});
    document.replaceRoot(root);
    return document;
}

QJsonObject savedScene(const QString& path) {
    wbw::ArchiveDocument document;
    if (!wbw::ProjectStore::loadJsonFile(path, document)) return {};
    const QJsonArray chapters = document.array(QStringLiteral("writingChapters"));
    if (chapters.isEmpty()) return {};
    const QJsonArray scenes = chapters.first().toObject().value(QStringLiteral("scenes")).toArray();
    return scenes.isEmpty() ? QJsonObject() : scenes.first().toObject();
}

void openProject(wbw::ProjectHubPage* hub, const QString& path) {
    std::cout << "Opening " << path.toStdString() << std::endl;
    QMetaObject::invokeMethod(hub, "openProjectRequested", Qt::DirectConnection, Q_ARG(QString, path));
    settleUi();
    std::cout << "Open request finished" << std::endl;
}

void answerNextPrompt(QMessageBox::StandardButton answer, bool* seen) {
    QTimer::singleShot(0, qApp, [answer, seen]() {
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            auto* box = qobject_cast<QMessageBox*>(widget);
            if (!box || !box->isVisible()) continue;
            *seen = true;
            if (auto* button = box->button(answer)) button->click();
            return;
        }
    });
}

} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    int unexpectedPrompts = 0;
    QTimer promptWatchdog;
    promptWatchdog.setInterval(200);
    QObject::connect(&promptWatchdog, &QTimer::timeout, &app, [&unexpectedPrompts]() {
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            auto* box = qobject_cast<QMessageBox*>(widget);
            if (!box || !box->isVisible()) continue;
            ++unexpectedPrompts;
            std::cerr << "Unexpected prompt: " << box->text().toStdString() << std::endl;
            if (auto* cancel = box->button(QMessageBox::Cancel)) cancel->click();
            else box->accept();
        }
    });
    promptWatchdog.start();
    QTemporaryDir temp;
    if (!require(temp.isValid(), "temporary fixture directory")) return 1;

    const QString firstDirectory = QDir(temp.path()).filePath(QStringLiteral("existing-one"));
    const QString secondDirectory = QDir(temp.path()).filePath(QStringLiteral("existing-two"));
    const QString firstPath = QDir(firstDirectory).filePath(QStringLiteral("project.json"));
    const QString secondPath = QDir(secondDirectory).filePath(QStringLiteral("project.json"));
    wbw::ArchiveDocument first = fixture(QStringLiteral("Primera obra"), QStringLiteral("Texto original."));
    wbw::ArchiveDocument second = fixture(QStringLiteral("Segunda obra"), QStringLiteral("Otra historia."));
    QString error;
    if (!require(wbw::ProjectStore::saveIntoProjectDirectory(firstDirectory, first, &error), "create first existing project") ||
        !require(wbw::ProjectStore::saveIntoProjectDirectory(secondDirectory, second, &error), "create second existing project")) return 1;

    std::cout << "Creating project window" << std::endl;
    wbw::MainWindow window;
    wbw::applyProductReorganization(&window);
    wbw::installNavigationConsistencyController(&window);
    window.show();
    window.activateWindow();
    QApplication::setActiveWindow(&window);
    settleUi();
    std::cout << "Project window ready" << std::endl;

    auto* hub = window.findChild<wbw::ProjectHubPage*>();
    auto* editor = window.findChild<QTextEdit*>(QStringLiteral("sceneEditor"));
    auto* save = window.findChild<QPushButton*>(QStringLiteral("primarySave"));
    auto* shortcut = window.findChild<QShortcut*>(QStringLiteral("wbwSaveShortcut"));
    auto* state = window.findChild<QLabel*>(QStringLiteral("saveState"));
    auto* navigation = window.findChild<QListWidget*>(QStringLiteral("sideNavigation"));
    QLineEdit* title = nullptr;
    for (QLineEdit* field : window.findChildren<QLineEdit*>()) {
        if (field->placeholderText() == QStringLiteral("Título de escena")) title = field;
    }
    if (!require(hub && editor && save && shortcut && state && navigation && title, "project save UI controls")) return 1;

    bool ok = true;
    ok &= require(shortcut->key() == QKeySequence(QKeySequence::Save) && shortcut->context() == Qt::WindowShortcut,
                  "Ctrl+S works at window scope");
    openProject(hub, firstPath);
    ok &= require(editor->toPlainText() == QStringLiteral("Texto original."), "open existing project's scene");
    ok &= require(!window.windowTitle().startsWith(QStringLiteral("* ")), "opening existing project keeps it clean");

    editor->append(QStringLiteral("Cambios guardados con Ctrl+S."));
    const QString editedText = editor->toPlainText();
    title->setFocus();
    title->selectAll();
    QTest::keyClicks(title, "Titulo sin salir del campo");
    QTest::keyClick(title, Qt::Key_S, Qt::ControlModifier);
    settleUi();
    const QJsonObject manuallySaved = savedScene(firstPath);
    ok &= require(manuallySaved.value(QStringLiteral("text")).toString() == editedText, "shortcut writes to existing project file");
    ok &= require(manuallySaved.value(QStringLiteral("title")).toString() == title->text(), "save includes focused pending field");
    ok &= require(!manuallySaved.value(QStringLiteral("formatting")).toArray().isEmpty(), "save preserves scene formatting");
    ok &= require(state->text() == QStringLiteral("Guardado local"), "navigation preserves successful save status");

    wbw::ArchiveDocument reopened;
    ok &= require(wbw::ProjectStore::loadJsonFile(firstPath, reopened, &error), "saved project reopens");
    ok &= require(reopened.root().value(QStringLiteral("futureField")).toObject().value(QStringLiteral("keepMe")).toInt() == 42,
                  "save preserves compatible existing fields");
    openProject(hub, firstPath);
    ok &= require(editor->toPlainText() == editedText, "reopening in UI preserves saved text");

    title->setFocus();
    title->selectAll();
    QTest::keyClicks(title, "Titulo guardado automaticamente");
    QMetaObject::invokeMethod(window.autosaveTimer(), "timeout", Qt::DirectConnection);
    settleUi();
    ok &= require(savedScene(firstPath).value(QStringLiteral("title")).toString() == title->text(),
                  "autosave commits title-only edits while focused");

    editor->append(QStringLiteral("Cambios antes de abrir otra obra."));
    const QString beforeSwitch = editor->toPlainText();
    navigation->setCurrentRow(0);
    settleUi();
    bool cancelPromptSeen = false;
    answerNextPrompt(QMessageBox::Cancel, &cancelPromptSeen);
    openProject(hub, secondPath);
    ok &= require(cancelPromptSeen, "opening another project protects unsaved changes");
    ok &= require(navigation->currentRow() == 0 && editor->toPlainText() == beforeSwitch,
                  "cancelled open preserves current project and library route");

    bool savePromptSeen = false;
    answerNextPrompt(QMessageBox::Save, &savePromptSeen);
    openProject(hub, secondPath);
    ok &= require(savePromptSeen, "switch allows saving previous project");
    ok &= require(savedScene(firstPath).value(QStringLiteral("text")).toString() == beforeSwitch,
                  "switch saves to previous project's own file");
    ok &= require(editor->toPlainText() == QStringLiteral("Otra historia."), "switch opens second project after saving");
    ok &= require(!window.windowTitle().startsWith(QStringLiteral("* ")), "loading another project's formatting does not create edits");

    openProject(hub, firstPath);
    editor->append(QStringLiteral("Cambios guardados con el botón."));
    const QString buttonText = editor->toPlainText();
    save->click();
    settleUi();
    ok &= require(savedScene(firstPath).value(QStringLiteral("text")).toString() == buttonText,
                  "Guardar button updates the existing project");
    ok &= require(savedScene(secondPath).value(QStringLiteral("text")).toString() == QStringLiteral("Otra historia."),
                  "saving reopened project does not overwrite another project");

    const QString movedDirectory = firstDirectory + QStringLiteral("-preserved");
    if (!require(QDir().rename(firstDirectory, movedDirectory), "move fixture to simulate unavailable project folder")) return 1;
    QFile blocker(firstDirectory);
    if (!require(blocker.open(QIODevice::WriteOnly), "block original folder path")) return 1;
    blocker.write("blocked");
    blocker.close();
    editor->append(QStringLiteral("Cambios que deben conservarse tras un fallo."));
    const QString pendingText = editor->toPlainText();
    QMetaObject::invokeMethod(window.autosaveTimer(), "timeout", Qt::DirectConnection);
    settleUi();
    ok &= require(state->text() == QStringLiteral("No se pudo guardar") && !state->toolTip().isEmpty(),
                  "failed autosave stays visible after navigation updates");
    ok &= require(editor->toPlainText() == pendingText, "failed save preserves edits in memory");
    ok &= require(savedScene(QDir(movedDirectory).filePath(QStringLiteral("project.json"))).value(QStringLiteral("text")).toString() == buttonText,
                  "failed save leaves previously saved project intact");

    if (!require(QFile::remove(firstDirectory) && QDir().rename(movedDirectory, firstDirectory), "restore writable project folder")) return 1;
    save->click();
    settleUi();
    ok &= require(savedScene(firstPath).value(QStringLiteral("text")).toString() == pendingText,
                  "retry persists edits after storage becomes available");
    ok &= require(state->text() == QStringLiteral("Guardado local") && state->toolTip().isEmpty(),
                  "successful retry clears save error");

    auto* writing = window.findChild<wbw::WritingPage*>();
    if (!require(writing != nullptr, "writing page for focus-mode save")) return 1;
    writing->openFocusMode();
    settleUi();
    editor->setFocus();
    editor->append(QStringLiteral("Cambios guardados en modo enfoque."));
    const QString focusedText = editor->toPlainText();
    QTest::keyClick(editor, Qt::Key_S, Qt::ControlModifier);
    settleUi();
    ok &= require(!save->isVisible() && savedScene(firstPath).value(QStringLiteral("text")).toString() == focusedText,
                  "Ctrl+S saves in focus mode while Guardar is hidden");
    QTest::keyClick(editor, Qt::Key_Escape);
    settleUi();
    ok &= require(unexpectedPrompts == 0, "normal save and reopen never show unexpected prompts");
    std::cout << (ok ? "Project save smoke passed\n" : "Project save smoke failed\n");
    return ok ? 0 : 1;
}
