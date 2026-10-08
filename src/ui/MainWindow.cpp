#include "ui/MainWindow.h"

#include "storage/ProjectStore.h"

#include <QAction>
#include <QCloseEvent>
#include <QDateTime>
#include <QDialog>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPrinter>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTextDocument>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QUuid>

namespace wbw {
namespace {

QString uid(const QString& prefix) {
    return prefix + "-" + QUuid::createUuid().toString(QUuid::Id128);
}

QPushButton* button(const QString& text) {
    auto* b = new QPushButton(text);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

QWidget* header(const QString& section, const QString& title, const QString& note) {
    auto* widget = new QWidget;
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 10);
    auto* eyebrow = new QLabel(section.toUpper());
    eyebrow->setObjectName("Eyebrow");
    auto* heading = new QLabel(title);
    heading->setObjectName("PageTitle");
    auto* description = new QLabel(note);
    description->setObjectName("PageNote");
    description->setWordWrap(true);
    layout->addWidget(eyebrow);
    layout->addWidget(heading);
    layout->addWidget(description);
    return widget;
}

QString text(const QJsonObject& object, const char* key) {
    return object.value(QLatin1String(key)).toString();
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    resize(1420, 900);
    setMinimumSize(1050, 700);
    createMenus();
    createShell();
    setDocument(ArchiveDocument::empty());

    setStyleSheet(R"(
        QMainWindow, QWidget { background:#171815; color:#ebe6dc; font-size:14px; }
        QMenuBar { background:#11120f; border-bottom:1px solid #2f302a; }
        QMenuBar::item:selected, QMenu::item:selected { background:#34352e; }
        QMenu { background:#1d1e1a; border:1px solid #3c3d35; }
        QListWidget#Navigation { background:#11120f; border:0; outline:0; }
        QListWidget#Navigation::item { padding:13px 12px; margin:2px 0; border-radius:5px; color:#a8a69d; }
        QListWidget#Navigation::item:selected { background:#302f28; color:#f4efe4; }
        QLabel#ProjectName { font-size:16px; font-weight:700; }
        QLabel#Eyebrow { color:#ae9157; font-size:11px; font-weight:700; }
        QLabel#PageTitle { font-size:27px; font-weight:700; }
        QLabel#PageNote { color:#aaa89f; }
        QLineEdit, QTextEdit, QTreeWidget, QListWidget, QTabWidget::pane {
            background:#20211d; border:1px solid #393a32; border-radius:4px; selection-background-color:#715b34;
        }
        QLineEdit { padding:7px; }
        QTextEdit { padding:8px; }
        QPushButton { background:#302f28; border:1px solid #484940; border-radius:4px; padding:7px 11px; }
        QPushButton:hover { background:#3a3930; }
        QTabBar::tab { background:#1d1e1a; padding:9px 15px; margin-right:2px; }
        QTabBar::tab:selected { background:#302f28; color:#f2ecdc; }
        QSplitter::handle { background:#2d2e28; width:1px; }
    )");
}

void MainWindow::createMenus() {
    auto* file = menuBar()->addMenu(tr("Archivo"));
    auto* actionNew = file->addAction(tr("Nueva obra"));
    actionNew->setShortcut(QKeySequence::New);
    connect(actionNew, &QAction::triggered, this, &MainWindow::newProject);

    auto* actionOpen = file->addAction(tr("Abrir project.json…"));
    actionOpen->setShortcut(QKeySequence::Open);
    connect(actionOpen, &QAction::triggered, this, &MainWindow::openProject);

    file->addSeparator();
    auto* actionSave = file->addAction(tr("Guardar"));
    actionSave->setShortcut(QKeySequence::Save);
    connect(actionSave, &QAction::triggered, this, &MainWindow::saveProject);

    auto* actionSaveAs = file->addAction(tr("Guardar como…"));
    actionSaveAs->setShortcut(QKeySequence::SaveAs);
    connect(actionSaveAs, &QAction::triggered, this, &MainWindow::saveProjectAs);

    file->addSeparator();
    auto* actionPdf = file->addAction(tr("Exportar manuscrito a PDF…"));
    connect(actionPdf, &QAction::triggered, this, &MainWindow::exportManuscriptPdf);

    auto* view = menuBar()->addMenu(tr("Vista"));
    auto* focus = view->addAction(tr("Modo enfoque"));
    focus->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
    connect(focus, &QAction::triggered, this, &MainWindow::openFocusMode);
}

void MainWindow::createShell() {
    auto* central = new QWidget;
    auto* root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* sidebar = new QWidget;
    sidebar->setFixedWidth(225);
    sidebar->setStyleSheet("background:#11120f;border-right:1px solid #303129;");
    auto* side = new QVBoxLayout(sidebar);
    side->setContentsMargins(14, 18, 14, 14);

    auto* identity = new QLabel(QStringLiteral("WW  Worldbuilder Writer"));
    identity->setStyleSheet("font-weight:700;color:#c9ad71;padding:4px 4px 18px 4px;");
    side->addWidget(identity);

    navigation_ = new QListWidget;
    navigation_->setObjectName("Navigation");
    navigation_->addItems({tr("Planificación"), tr("Escritura"), tr("Mundo"), tr("Revisión y salida")});
    side->addWidget(navigation_, 1);

    projectLabel_ = new QLabel;
    projectLabel_->setObjectName("ProjectName");
    projectLabel_->setWordWrap(true);
    saveLabel_ = new QLabel;
    saveLabel_->setStyleSheet("color:#85857d;");
    side->addWidget(projectLabel_);
    side->addWidget(saveLabel_);

    pages_ = new QStackedWidget;
    pages_->addWidget(buildPlanningPage());
    pages_->addWidget(buildWritingPage());
    pages_->addWidget(buildWorldPage());
    pages_->addWidget(buildReviewPage());

    root->addWidget(sidebar);
    root->addWidget(pages_, 1);
    setCentralWidget(central);

    connect(navigation_, &QListWidget::currentRowChanged, pages_, &QStackedWidget::setCurrentIndex);
    navigation_->setCurrentRow(1);
}

QWidget* MainWindow::buildPlanningPage() {
    auto* page = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(26, 22, 26, 22);
    outer->addWidget(header("Planificación", "Personajes y estructura", "Tablero, relaciones, teorías y cronología quedan fuera del espacio de escritura."));

    auto* tabs = new QTabWidget;
    auto* characters = new QWidget;
    auto* split = new QSplitter;
    characterList_ = new QListWidget;
    split->addWidget(characterList_);

    auto* editor = new QWidget;
    auto* form = new QFormLayout(editor);
    characterName_ = new QLineEdit;
    characterRole_ = new QLineEdit;
    characterOrigin_ = new QLineEdit;
    characterSummary_ = new QTextEdit;
    form->addRow(tr("Nombre"), characterName_);
    form->addRow(tr("Rol"), characterRole_);
    form->addRow(tr("Origen"), characterOrigin_);
    form->addRow(tr("Resumen"), characterSummary_);
    auto* actions = new QHBoxLayout;
    auto* add = button(tr("+ Personaje"));
    auto* remove = button(tr("Eliminar"));
    actions->addWidget(add);
    actions->addWidget(remove);
    actions->addStretch();
    form->addRow(actions);
    split->addWidget(editor);
    split->setStretchFactor(1, 1);
    auto* cLayout = new QVBoxLayout(characters);
    cLayout->setContentsMargins(0, 0, 0, 0);
    cLayout->addWidget(split);
    tabs->addTab(characters, tr("Personajes"));

    auto placeholder = [tabs](const QString& title, const QString& copy) {
        auto* label = new QLabel(copy);
        label->setWordWrap(true);
        label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        label->setMargin(18);
        tabs->addTab(label, title);
    };
    placeholder(tr("Tablero"), tr("Lienzo nativo para fichas y relaciones. Se implementa separado del formulario de personaje para evitar la mezcla de controles de la edición anterior."));
    placeholder(tr("Hilos"), tr("Teorías: estado, confianza, tesis, evidencias, contraargumento, personajes y etiquetas."));
    placeholder(tr("Cronología"), tr("Eventos temporales con capítulo, fecha libre, resumen, personajes, intensidad y orden."));
    outer->addWidget(tabs, 1);

    connect(characterList_, &QListWidget::currentRowChanged, this, &MainWindow::selectCharacter);
    connect(characterName_, &QLineEdit::editingFinished, this, &MainWindow::applyCharacterEdits);
    connect(characterRole_, &QLineEdit::editingFinished, this, &MainWindow::applyCharacterEdits);
    connect(characterOrigin_, &QLineEdit::editingFinished, this, &MainWindow::applyCharacterEdits);
    connect(characterSummary_, &QTextEdit::textChanged, this, &MainWindow::applyCharacterEdits);
    connect(add, &QPushButton::clicked, this, &MainWindow::addCharacter);
    connect(remove, &QPushButton::clicked, this, &MainWindow::removeCharacter);
    return page;
}

QWidget* MainWindow::buildWritingPage() {
    auto* page = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(24, 20, 24, 20);
    outer->addWidget(header("Escritura", "Manuscrito", "El texto es la superficie principal. Estructura a la izquierda; metadatos arriba; edición en el centro."));

    auto* bar = new QHBoxLayout;
    auto* chapter = button(tr("+ Capítulo"));
    auto* scene = button(tr("+ Escena"));
    auto* remove = button(tr("Eliminar"));
    auto* focus = button(tr("Modo enfoque"));
    bar->addWidget(chapter);
    bar->addWidget(scene);
    bar->addWidget(remove);
    bar->addStretch();
    bar->addWidget(focus);
    outer->addLayout(bar);

    auto* split = new QSplitter;
    manuscriptTree_ = new QTreeWidget;
    manuscriptTree_->setHeaderHidden(true);
    manuscriptTree_->setMinimumWidth(230);
    manuscriptTree_->setMaximumWidth(390);
    split->addWidget(manuscriptTree_);

    auto* writing = new QWidget;
    auto* writingLayout = new QVBoxLayout(writing);
    writingLayout->setContentsMargins(12, 0, 0, 0);
    auto* meta = new QHBoxLayout;
    sceneTitle_ = new QLineEdit;
    sceneTitle_->setPlaceholderText(tr("Título de escena"));
    scenePov_ = new QLineEdit;
    scenePov_->setPlaceholderText(tr("POV"));
    sceneLocation_ = new QLineEdit;
    sceneLocation_->setPlaceholderText(tr("Ubicación"));
    sceneStatus_ = new QLineEdit;
    sceneStatus_->setPlaceholderText(tr("Estado"));
    meta->addWidget(sceneTitle_, 3);
    meta->addWidget(scenePov_, 1);
    meta->addWidget(sceneLocation_, 1);
    meta->addWidget(sceneStatus_, 1);
    writingLayout->addLayout(meta);
    sceneEditor_ = new QTextEdit;
    sceneEditor_->setAcceptRichText(true);
    sceneEditor_->setPlaceholderText(tr("Escribe aquí…"));
    writingLayout->addWidget(sceneEditor_, 1);
    split->addWidget(writing);
    split->setStretchFactor(1, 1);
    outer->addWidget(split, 1);

    connect(manuscriptTree_, &QTreeWidget::itemSelectionChanged, this, &MainWindow::selectScene);
    connect(sceneTitle_, &QLineEdit::editingFinished, this, &MainWindow::applySceneEdits);
    connect(scenePov_, &QLineEdit::editingFinished, this, &MainWindow::applySceneEdits);
    connect(sceneLocation_, &QLineEdit::editingFinished, this, &MainWindow::applySceneEdits);
    connect(sceneStatus_, &QLineEdit::editingFinished, this, &MainWindow::applySceneEdits);
    connect(sceneEditor_, &QTextEdit::textChanged, this, &MainWindow::applySceneEdits);
    connect(chapter, &QPushButton::clicked, this, &MainWindow::addChapter);
    connect(scene, &QPushButton::clicked, this, &MainWindow::addScene);
    connect(remove, &QPushButton::clicked, this, &MainWindow::removeWritingItem);
    connect(focus, &QPushButton::clicked, this, &MainWindow::openFocusMode);
    return page;
}

QWidget* MainWindow::buildWorldPage() {
    auto* page = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(26, 22, 26, 22);
    outer->addWidget(header("Mundo", "Atlas, mapas y magia", "Worldbuilding agrupado por dominio, no por controles dispersos."));

    auto* tabs = new QTabWidget;

    auto* atlas = new QWidget;
    auto* atlasSplit = new QSplitter;
    worldList_ = new QListWidget;
    atlasSplit->addWidget(worldList_);
    auto* atlasEditor = new QWidget;
    auto* atlasForm = new QFormLayout(atlasEditor);
    worldName_ = new QLineEdit;
    worldKind_ = new QLineEdit;
    worldSummary_ = new QTextEdit;
    worldNotes_ = new QTextEdit;
    atlasForm->addRow(tr("Nombre"), worldName_);
    atlasForm->addRow(tr("Tipo"), worldKind_);
    atlasForm->addRow(tr("Resumen"), worldSummary_);
    atlasForm->addRow(tr("Notas"), worldNotes_);
    auto* atlasActions = new QHBoxLayout;
    auto* addWorld = button(tr("+ Entrada"));
    auto* removeWorld = button(tr("Eliminar"));
    atlasActions->addWidget(addWorld);
    atlasActions->addWidget(removeWorld);
    atlasActions->addStretch();
    atlasForm->addRow(atlasActions);
    atlasSplit->addWidget(atlasEditor);
    atlasSplit->setStretchFactor(1, 1);
    auto* atlasLayout = new QVBoxLayout(atlas);
    atlasLayout->setContentsMargins(0, 0, 0, 0);
    atlasLayout->addWidget(atlasSplit);
    tabs->addTab(atlas, tr("Atlas"));

    auto* magic = new QWidget;
    auto* magicSplit = new QSplitter;
    magicList_ = new QListWidget;
    magicSplit->addWidget(magicList_);
    auto* magicEditor = new QWidget;
    auto* magicForm = new QFormLayout(magicEditor);
    magicName_ = new QLineEdit;
    magicCategory_ = new QLineEdit;
    magicPrinciple_ = new QTextEdit;
    magicLimits_ = new QTextEdit;
    magicForm->addRow(tr("Nombre"), magicName_);
    magicForm->addRow(tr("Categoría"), magicCategory_);
    magicForm->addRow(tr("Principio"), magicPrinciple_);
    magicForm->addRow(tr("Límites"), magicLimits_);
    auto* magicActions = new QHBoxLayout;
    auto* addMagic = button(tr("+ Sistema"));
    auto* removeMagic = button(tr("Eliminar"));
    magicActions->addWidget(addMagic);
    magicActions->addWidget(removeMagic);
    magicActions->addStretch();
    magicForm->addRow(magicActions);
    magicSplit->addWidget(magicEditor);
    magicSplit->setStretchFactor(1, 1);
    auto* magicLayout = new QVBoxLayout(magic);
    magicLayout->setContentsMargins(0, 0, 0, 0);
    magicLayout->addWidget(magicSplit);
    tabs->addTab(magic, tr("Magia"));

    auto* maps = new QLabel(tr("Mapas tendrán lienzo propio con imagen de fondo, semilla, estilos y marcadores; no reutilizarán recursos visuales de la edición anterior."));
    maps->setWordWrap(true);
    maps->setMargin(18);
    maps->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    tabs->addTab(maps, tr("Mapas"));
    outer->addWidget(tabs, 1);

    connect(worldList_, &QListWidget::currentRowChanged, this, &MainWindow::selectWorldRecord);
    connect(worldName_, &QLineEdit::editingFinished, this, &MainWindow::applyWorldEdits);
    connect(worldKind_, &QLineEdit::editingFinished, this, &MainWindow::applyWorldEdits);
    connect(worldSummary_, &QTextEdit::textChanged, this, &MainWindow::applyWorldEdits);
    connect(worldNotes_, &QTextEdit::textChanged, this, &MainWindow::applyWorldEdits);
    connect(addWorld, &QPushButton::clicked, this, &MainWindow::addWorldRecord);
    connect(removeWorld, &QPushButton::clicked, this, &MainWindow::removeWorldRecord);

    connect(magicList_, &QListWidget::currentRowChanged, this, &MainWindow::selectMagicRecord);
    connect(magicName_, &QLineEdit::editingFinished, this, &MainWindow::applyMagicEdits);
    connect(magicCategory_, &QLineEdit::editingFinished, this, &MainWindow::applyMagicEdits);
    connect(magicPrinciple_, &QTextEdit::textChanged, this, &MainWindow::applyMagicEdits);
    connect(magicLimits_, &QTextEdit::textChanged, this, &MainWindow::applyMagicEdits);
    connect(addMagic, &QPushButton::clicked, this, &MainWindow::addMagicRecord);
    connect(removeMagic, &QPushButton::clicked, this, &MainWindow::removeMagicRecord);
    return page;
}

QWidget* MainWindow::buildReviewPage() {
    auto* page = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(26, 22, 26, 22);
    outer->addWidget(header("Revisión y salida", "Revisión, maquetación y exportación", "Herramientas de salida apartadas del entorno de escritura para reducir distracciones."));
    reviewStats_ = new QLabel;
    reviewStats_->setWordWrap(true);
    reviewStats_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    reviewStats_->setStyleSheet("background:#20211d;border:1px solid #393a32;border-radius:4px;padding:18px;");
    outer->addWidget(reviewStats_);
    auto* actions = new QHBoxLayout;
    auto* pdf = button(tr("Exportar manuscrito a PDF"));
    actions->addWidget(pdf);
    actions->addStretch();
    outer->addLayout(actions);
    outer->addStretch();
    connect(pdf, &QPushButton::clicked, this, &MainWindow::exportManuscriptPdf);
    return page;
}

void MainWindow::newProject() {
    if (!confirmDiscard()) return;
    bool ok = false;
    const QString archive = QInputDialog::getText(this, tr("Nueva obra"), tr("Nombre del archivo:"), QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || archive.isEmpty()) return;
    const QString story = QInputDialog::getText(this, tr("Nueva obra"), tr("Título de la historia (opcional):"), QLineEdit::Normal, archive, &ok).trimmed();
    if (!ok) return;
    setDocument(ArchiveDocument::empty(archive, story));
}

void MainWindow::openProject() {
    if (!confirmDiscard()) return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Abrir proyecto"), QString(), tr("Worldbuilder project (project.json *.json)"));
    if (path.isEmpty()) return;
    ArchiveDocument candidate;
    QString error;
    if (!ProjectStore::loadJsonFile(path, candidate, &error)) {
        QMessageBox::critical(this, tr("No se pudo abrir"), error);
        return;
    }
    setDocument(std::move(candidate));
}

bool MainWindow::saveProject() {
    applyCharacterEdits();
    applySceneEdits();
    applyWorldEdits();
    applyMagicEdits();
    if (document_.sourcePath().isEmpty()) return saveProjectAs();
    QString error;
    if (!ProjectStore::saveJsonFile(document_.sourcePath(), document_, &error)) {
        QMessageBox::critical(this, tr("No se pudo guardar"), error);
        return false;
    }
    setSaveLabel();
    updateWindowTitle();
    return true;
}

bool MainWindow::saveProjectAs() {
    const QString path = QFileDialog::getSaveFileName(this, tr("Guardar proyecto"), document_.sourcePath().isEmpty() ? QStringLiteral("project.json") : document_.sourcePath(), tr("JSON (*.json)"));
    if (path.isEmpty()) return false;
    QString error;
    if (!ProjectStore::saveJsonFile(path, document_, &error)) {
        QMessageBox::critical(this, tr("No se pudo guardar"), error);
        return false;
    }
    setSaveLabel();
    updateWindowTitle();
    return true;
}

bool MainWindow::confirmDiscard() {
    if (!document_.isDirty()) return true;
    const auto answer = QMessageBox::question(this, tr("Cambios sin guardar"), tr("Hay cambios sin guardar. ¿Quieres guardarlos antes de continuar?"), QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Cancel) return false;
    if (answer == QMessageBox::Save) return saveProject();
    return true;
}

void MainWindow::setDocument(ArchiveDocument document) {
    document_ = std::move(document);
    refreshAll();
    updateWindowTitle();
    setSaveLabel();
}

void MainWindow::refreshAll() {
    refreshing_ = true;
    projectLabel_->setText(document_.storyTitle());
    refreshPlanning();
    refreshWriting();
    refreshWorld();
    refreshReview();
    refreshing_ = false;
}

void MainWindow::refreshPlanning() {
    characterList_->clear();
    for (const auto value : document_.array("characters")) characterList_->addItem(value.toObject().value("name").toString(tr("Personaje sin nombre")));
    if (characterList_->count()) characterList_->setCurrentRow(0);
}

void MainWindow::refreshWriting() {
    manuscriptTree_->clear();
    const QJsonArray chapters = document_.array("writingChapters");
    for (int c = 0; c < chapters.size(); ++c) {
        const QJsonObject chapter = chapters.at(c).toObject();
        auto* chapterItem = new QTreeWidgetItem({chapter.value("label").toString(tr("Capítulo")) + QStringLiteral(" — ") + chapter.value("title").toString()});
        chapterItem->setData(0, Qt::UserRole, QStringLiteral("chapter"));
        chapterItem->setData(0, Qt::UserRole + 1, c);
        const QJsonArray scenes = chapter.value("scenes").toArray();
        for (int s = 0; s < scenes.size(); ++s) {
            auto* sceneItem = new QTreeWidgetItem({scenes.at(s).toObject().value("title").toString(tr("Escena"))});
            sceneItem->setData(0, Qt::UserRole, QStringLiteral("scene"));
            sceneItem->setData(0, Qt::UserRole + 1, c);
            sceneItem->setData(0, Qt::UserRole + 2, s);
            chapterItem->addChild(sceneItem);
        }
        manuscriptTree_->addTopLevelItem(chapterItem);
        chapterItem->setExpanded(true);
    }
    if (manuscriptTree_->topLevelItemCount() && manuscriptTree_->topLevelItem(0)->childCount()) manuscriptTree_->setCurrentItem(manuscriptTree_->topLevelItem(0)->child(0));
}

void MainWindow::refreshWorld() {
    worldList_->clear();
    for (const auto value : document_.array("world")) worldList_->addItem(value.toObject().value("name").toString(tr("Entrada sin nombre")));
    magicList_->clear();
    for (const auto value : document_.array("magicSystems")) magicList_->addItem(value.toObject().value("name").toString(tr("Sistema sin nombre")));
    if (worldList_->count()) worldList_->setCurrentRow(0);
    if (magicList_->count()) magicList_->setCurrentRow(0);
}

void MainWindow::refreshReview() {
    const auto characters = document_.array("characters").size();
    const auto world = document_.array("world").size();
    const auto magic = document_.array("magicSystems").size();
    const auto chapters = document_.array("writingChapters");
    int scenes = 0;
    int words = 0;
    for (const auto chapterValue : chapters) {
        for (const auto sceneValue : chapterValue.toObject().value("scenes").toArray()) {
            ++scenes;
            const QString content = sceneValue.toObject().value("content").toString();
            words += content.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts).size();
        }
    }
    reviewStats_->setText(tr("Capítulos: %1\nEscenas: %2\nPalabras aproximadas: %3\nPersonajes: %4\nEntradas de mundo: %5\nSistemas de magia: %6").arg(chapters.size()).arg(scenes).arg(words).arg(characters).arg(world).arg(magic));
}

void MainWindow::updateWindowTitle() {
    setWindowTitle(QStringLiteral("%1%2 — Worldbuilder Writer").arg(document_.isDirty() ? QStringLiteral("• ") : QString(), document_.storyTitle()));
}

void MainWindow::setSaveLabel() {
    saveLabel_->setText(document_.isDirty() ? tr("Cambios sin guardar") : tr("Guardado"));
}

void MainWindow::selectCharacter(int row) {
    refreshing_ = true;
    const QJsonArray array = document_.array("characters");
    const QJsonObject object = row >= 0 && row < array.size() ? array.at(row).toObject() : QJsonObject();
    characterName_->setText(text(object, "name"));
    characterRole_->setText(text(object, "role"));
    characterOrigin_->setText(text(object, "origin"));
    characterSummary_->setPlainText(text(object, "summary"));
    refreshing_ = false;
}

void MainWindow::applyCharacterEdits() {
    if (refreshing_) return;
    const int row = characterList_->currentRow();
    QJsonArray array = document_.array("characters");
    if (row < 0 || row >= array.size()) return;
    QJsonObject object = array.at(row).toObject();
    object.insert("name", characterName_->text());
    object.insert("role", characterRole_->text());
    object.insert("origin", characterOrigin_->text());
    object.insert("summary", characterSummary_->toPlainText());
    array.replace(row, object);
    document_.setArray("characters", array);
    characterList_->item(row)->setText(characterName_->text().isEmpty() ? tr("Personaje sin nombre") : characterName_->text());
    setSaveLabel(); updateWindowTitle(); refreshReview();
}

void MainWindow::addCharacter() {
    QJsonArray array = document_.array("characters");
    QJsonObject object{{"id", uid("character")}, {"name", tr("Personaje sin nombre")}, {"aliases", QJsonArray()}, {"category", tr("Secundario")}, {"status", tr("Activo")}, {"role", QString()}, {"occupation", QString()}, {"origin", QString()}, {"affiliation", QString()}, {"summary", QString()}, {"background", QString()}, {"physical", QString()}, {"traits", QJsonArray()}, {"evidence", QJsonArray()}, {"presence", QJsonArray()}, {"color", QStringLiteral("amber")}, {"board", QJsonObject{{"x", 120}, {"y", 120}, {"visible", true}}}};
    array.append(object); document_.setArray("characters", array); refreshPlanning(); characterList_->setCurrentRow(array.size() - 1); setSaveLabel(); updateWindowTitle(); refreshReview();
}

void MainWindow::removeCharacter() {
    const int row = characterList_->currentRow(); if (row < 0) return;
    QJsonArray array = document_.array("characters"); array.removeAt(row); document_.setArray("characters", array); refreshPlanning(); setSaveLabel(); updateWindowTitle(); refreshReview();
}

void MainWindow::selectScene() {
    refreshing_ = true;
    auto* item = manuscriptTree_->currentItem();
    if (!item || item->data(0, Qt::UserRole).toString() != "scene") {
        sceneTitle_->clear(); scenePov_->clear(); sceneLocation_->clear(); sceneStatus_->clear(); sceneEditor_->clear(); refreshing_ = false; return;
    }
    const int c = item->data(0, Qt::UserRole + 1).toInt();
    const int s = item->data(0, Qt::UserRole + 2).toInt();
    const QJsonObject scene = document_.array("writingChapters").at(c).toObject().value("scenes").toArray().at(s).toObject();
    sceneTitle_->setText(text(scene, "title"));
    scenePov_->setText(text(scene, "pov"));
    sceneLocation_->setText(text(scene, "location"));
    sceneStatus_->setText(text(scene, "status"));
    sceneEditor_->setHtml(text(scene, "content"));
    refreshing_ = false;
}

void MainWindow::applySceneEdits() {
    if (refreshing_) return;
    auto* item = manuscriptTree_->currentItem();
    if (!item || item->data(0, Qt::UserRole).toString() != "scene") return;
    const int c = item->data(0, Qt::UserRole + 1).toInt();
    const int s = item->data(0, Qt::UserRole + 2).toInt();
    QJsonArray chapters = document_.array("writingChapters");
    QJsonObject chapter = chapters.at(c).toObject();
    QJsonArray scenes = chapter.value("scenes").toArray();
    QJsonObject scene = scenes.at(s).toObject();
    scene.insert("title", sceneTitle_->text()); scene.insert("pov", scenePov_->text()); scene.insert("location", sceneLocation_->text()); scene.insert("status", sceneStatus_->text()); scene.insert("content", sceneEditor_->toHtml()); scene.insert("updatedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    scenes.replace(s, scene); chapter.insert("scenes", scenes); chapters.replace(c, chapter); document_.setArray("writingChapters", chapters);
    item->setText(0, sceneTitle_->text().isEmpty() ? tr("Escena") : sceneTitle_->text()); setSaveLabel(); updateWindowTitle(); refreshReview();
}

void MainWindow::addChapter() {
    QJsonArray chapters = document_.array("writingChapters"); const int number = chapters.size() + 1;
    QJsonObject chapter{{"id", uid("chapter")}, {"label", tr("Capítulo %1").arg(number)}, {"title", tr("Sin título")}, {"order", chapters.size()}, {"scenes", QJsonArray()}};
    chapters.append(chapter); document_.setArray("writingChapters", chapters); refreshWriting(); setSaveLabel(); updateWindowTitle(); refreshReview();
}

void MainWindow::addScene() {
    auto* current = manuscriptTree_->currentItem(); int chapterIndex = -1;
    if (current) chapterIndex = current->data(0, Qt::UserRole + 1).toInt();
    QJsonArray chapters = document_.array("writingChapters"); if (chapters.isEmpty()) { addChapter(); chapters = document_.array("writingChapters"); chapterIndex = 0; }
    if (chapterIndex < 0 || chapterIndex >= chapters.size()) chapterIndex = 0;
    QJsonObject chapter = chapters.at(chapterIndex).toObject(); QJsonArray scenes = chapter.value("scenes").toArray();
    QJsonObject scene{{"id", uid("scene")}, {"chapterId", chapter.value("id")}, {"order", scenes.size()}, {"title", tr("Escena %1").arg(scenes.size() + 1)}, {"content", QString()}, {"pov", QString()}, {"location", QString()}, {"narrativeLayer", QString()}, {"status", tr("Borrador")}, {"updatedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
    scenes.append(scene); chapter.insert("scenes", scenes); chapters.replace(chapterIndex, chapter); document_.setArray("writingChapters", chapters); refreshWriting(); auto* ch = manuscriptTree_->topLevelItem(chapterIndex); if (ch && ch->childCount()) manuscriptTree_->setCurrentItem(ch->child(ch->childCount() - 1)); setSaveLabel(); updateWindowTitle(); refreshReview();
}

void MainWindow::removeWritingItem() {
    auto* item = manuscriptTree_->currentItem(); if (!item) return; QJsonArray chapters = document_.array("writingChapters"); const QString kind = item->data(0, Qt::UserRole).toString(); const int c = item->data(0, Qt::UserRole + 1).toInt();
    if (kind == "chapter") chapters.removeAt(c); else if (kind == "scene") { QJsonObject chapter = chapters.at(c).toObject(); QJsonArray scenes = chapter.value("scenes").toArray(); scenes.removeAt(item->data(0, Qt::UserRole + 2).toInt()); chapter.insert("scenes", scenes); chapters.replace(c, chapter); }
    document_.setArray("writingChapters", chapters); refreshWriting(); setSaveLabel(); updateWindowTitle(); refreshReview();
}

void MainWindow::openFocusMode() {
    auto* item = manuscriptTree_->currentItem(); if (!item || item->data(0, Qt::UserRole).toString() != "scene") return;
    QDialog dialog(this); dialog.setWindowTitle(tr("Modo enfoque")); dialog.setWindowState(Qt::WindowFullScreen); auto* layout = new QVBoxLayout(&dialog); layout->setContentsMargins(80, 40, 80, 40); auto* editor = new QTextEdit; editor->setAcceptRichText(true); editor->setHtml(sceneEditor_->toHtml()); editor->setStyleSheet("QTextEdit{font-size:18px;line-height:1.5;background:#181914;border:0;padding:30px;}"); layout->addWidget(editor); auto* close = button(tr("Cerrar enfoque")); layout->addWidget(close, 0, Qt::AlignRight); connect(close, &QPushButton::clicked, &dialog, &QDialog::accept); if (dialog.exec() == QDialog::Accepted) { sceneEditor_->setHtml(editor->toHtml()); applySceneEdits(); }
}

void MainWindow::selectWorldRecord(int row) {
    refreshing_ = true; const QJsonArray array = document_.array("world"); const QJsonObject object = row >= 0 && row < array.size() ? array.at(row).toObject() : QJsonObject(); worldName_->setText(text(object, "name")); worldKind_->setText(text(object, "kind")); worldSummary_->setPlainText(text(object, "summary")); worldNotes_->setPlainText(text(object, "notes")); refreshing_ = false;
}

void MainWindow::applyWorldEdits() {
    if (refreshing_) return; const int row = worldList_->currentRow(); QJsonArray array = document_.array("world"); if (row < 0 || row >= array.size()) return; QJsonObject object = array.at(row).toObject(); object.insert("name", worldName_->text()); object.insert("kind", worldKind_->text()); object.insert("summary", worldSummary_->toPlainText()); object.insert("notes", worldNotes_->toPlainText()); array.replace(row, object); document_.setArray("world", array); worldList_->item(row)->setText(worldName_->text().isEmpty() ? tr("Entrada sin nombre") : worldName_->text()); setSaveLabel(); updateWindowTitle(); refreshReview();
}

void MainWindow::addWorldRecord() {
    QJsonArray array = document_.array("world"); array.append(QJsonObject{{"id", uid("world")}, {"kind", tr("Otro")}, {"name", tr("Entrada sin nombre")}, {"aliases", QJsonArray()}, {"summary", QString()}, {"geography", QString()}, {"government", QString()}, {"peoples", QString()}, {"culture", QString()}, {"economy", QString()}, {"currency", QString()}, {"languages", QString()}, {"religions", QString()}, {"military", QString()}, {"history", QString()}, {"relations", QString()}, {"locations", QString()}, {"conflicts", QString()}, {"notes", QString()}, {"tags", QJsonArray()}, {"attachments", QJsonArray()}}); document_.setArray("world", array); refreshWorld(); worldList_->setCurrentRow(array.size() - 1); setSaveLabel(); updateWindowTitle(); refreshReview();
}

void MainWindow::removeWorldRecord() { const int row = worldList_->currentRow(); if (row < 0) return; QJsonArray array = document_.array("world"); array.removeAt(row); document_.setArray("world", array); refreshWorld(); setSaveLabel(); updateWindowTitle(); refreshReview(); }

void MainWindow::selectMagicRecord(int row) {
    refreshing_ = true; const QJsonArray array = document_.array("magicSystems"); const QJsonObject object = row >= 0 && row < array.size() ? array.at(row).toObject() : QJsonObject(); magicName_->setText(text(object, "name")); magicCategory_->setText(text(object, "category")); magicPrinciple_->setPlainText(text(object, "principle")); magicLimits_->setPlainText(text(object, "limits")); refreshing_ = false;
}

void MainWindow::applyMagicEdits() {
    if (refreshing_) return; const int row = magicList_->currentRow(); QJsonArray array = document_.array("magicSystems"); if (row < 0 || row >= array.size()) return; QJsonObject object = array.at(row).toObject(); object.insert("name", magicName_->text()); object.insert("category", magicCategory_->text()); object.insert("principle", magicPrinciple_->toPlainText()); object.insert("limits", magicLimits_->toPlainText()); array.replace(row, object); document_.setArray("magicSystems", array); magicList_->item(row)->setText(magicName_->text().isEmpty() ? tr("Sistema sin nombre") : magicName_->text()); setSaveLabel(); updateWindowTitle(); refreshReview();
}

void MainWindow::addMagicRecord() {
    QJsonArray array = document_.array("magicSystems"); array.append(QJsonObject{{"id", uid("magic")}, {"name", tr("Sistema sin nombre")}, {"category", QString()}, {"status", tr("En desarrollo")}, {"source", QString()}, {"principle", QString()}, {"access", QString()}, {"cost", QString()}, {"limits", QString()}, {"manifestations", QString()}, {"materials", QString()}, {"institutions", QString()}, {"users", QString()}, {"risks", QString()}, {"history", QString()}, {"notes", QString()}, {"evidence", QJsonArray()}, {"tags", QJsonArray()}, {"attachments", QJsonArray()}}); document_.setArray("magicSystems", array); refreshWorld(); magicList_->setCurrentRow(array.size() - 1); setSaveLabel(); updateWindowTitle(); refreshReview();
}

void MainWindow::removeMagicRecord() { const int row = magicList_->currentRow(); if (row < 0) return; QJsonArray array = document_.array("magicSystems"); array.removeAt(row); document_.setArray("magicSystems", array); refreshWorld(); setSaveLabel(); updateWindowTitle(); refreshReview(); }

void MainWindow::exportManuscriptPdf() {
    const QString path = QFileDialog::getSaveFileName(this, tr("Exportar manuscrito"), document_.storyTitle() + QStringLiteral(".pdf"), tr("PDF (*.pdf)")); if (path.isEmpty()) return;
    QString html = QStringLiteral("<h1>%1</h1>").arg(document_.storyTitle().toHtmlEscaped());
    for (const auto chapterValue : document_.array("writingChapters")) { const QJsonObject chapter = chapterValue.toObject(); html += QStringLiteral("<h2>%1 — %2</h2>").arg(chapter.value("label").toString().toHtmlEscaped(), chapter.value("title").toString().toHtmlEscaped()); for (const auto sceneValue : chapter.value("scenes").toArray()) { const QJsonObject scene = sceneValue.toObject(); html += QStringLiteral("<h3>%1</h3>").arg(scene.value("title").toString().toHtmlEscaped()); html += scene.value("content").toString(); } }
    QTextDocument doc; doc.setHtml(html); QPrinter printer(QPrinter::HighResolution); printer.setOutputFormat(QPrinter::PdfFormat); printer.setOutputFileName(path); doc.print(&printer);
}

void MainWindow::closeEvent(QCloseEvent* event) { if (confirmDiscard()) event->accept(); else event->ignore(); }

} // namespace wbw
