#include "ui/MainWindow.h"

#include "storage/ProjectStore.h"
#include "storage/WbwPackage.h"
#include "ui/PlanningPage.h"
#include "ui/ReviewPage.h"
#include "ui/WorldPage.h"
#include "ui/WritingPage.h"

#include <QAction>
#include <QCloseEvent>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMarginsF>
#include <QMenuBar>
#include <QMessageBox>
#include <QPageLayout>
#include <QPageSize>
#include <QPrinter>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSizeF>
#include <QStackedWidget>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>

namespace wbw {
namespace {

QPushButton* makeButton(const QString& text) {
    auto* button = new QPushButton(text);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

QString safeFileName(QString value) {
    value = value.trimmed();
    value.replace(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*]+")), QStringLiteral("-"));
    value.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    if (value.isEmpty()) value = QStringLiteral("worldbuilder-writer");
    return value.left(100);
}

QString sceneBodyHtml(const QString& content) {
    if (content.trimmed().isEmpty()) return QString();
    const QRegularExpression body(QStringLiteral("<body[^>]*>([\\s\\S]*)</body>"), QRegularExpression::CaseInsensitiveOption);
    const auto match = body.match(content);
    if (match.hasMatch()) return match.captured(1);
    return content;
}

QJsonObject effectiveLayout(const ArchiveDocument& document) {
    const QJsonObject profile = document.object(QStringLiteral("profile"));
    QJsonObject layout = profile.value(QStringLiteral("manuscriptLayout")).toObject();
    if (!layout.contains(QStringLiteral("pageWidthMm"))) layout.insert(QStringLiteral("pageWidthMm"), 152.4);
    if (!layout.contains(QStringLiteral("pageHeightMm"))) layout.insert(QStringLiteral("pageHeightMm"), 228.6);
    if (!layout.contains(QStringLiteral("marginTopMm"))) layout.insert(QStringLiteral("marginTopMm"), 20.0);
    if (!layout.contains(QStringLiteral("marginRightMm"))) layout.insert(QStringLiteral("marginRightMm"), 19.0);
    if (!layout.contains(QStringLiteral("marginBottomMm"))) layout.insert(QStringLiteral("marginBottomMm"), 22.0);
    if (!layout.contains(QStringLiteral("marginLeftMm"))) layout.insert(QStringLiteral("marginLeftMm"), 19.0);
    if (!layout.contains(QStringLiteral("fontFamily"))) layout.insert(QStringLiteral("fontFamily"), QStringLiteral("Garamond"));
    if (!layout.contains(QStringLiteral("fontSizePt"))) layout.insert(QStringLiteral("fontSizePt"), 11.0);
    if (!layout.contains(QStringLiteral("lineHeight"))) layout.insert(QStringLiteral("lineHeight"), 1.35);
    if (!layout.contains(QStringLiteral("paragraphIndentMm"))) layout.insert(QStringLiteral("paragraphIndentMm"), 5.0);
    if (!layout.contains(QStringLiteral("chapterOpening"))) layout.insert(QStringLiteral("chapterOpening"), QStringLiteral("Página nueva"));
    if (!layout.contains(QStringLiteral("sceneSeparator"))) layout.insert(QStringLiteral("sceneSeparator"), QStringLiteral("⁂"));
    if (!layout.contains(QStringLiteral("headerText"))) layout.insert(QStringLiteral("headerText"), QStringLiteral("{título}"));
    if (!layout.contains(QStringLiteral("footerText"))) layout.insert(QStringLiteral("footerText"), QStringLiteral("{página}"));
    return layout;
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    resize(1480, 900);
    setMinimumSize(1040, 680);
    createShell();
    createMenus();

    autosaveTimer_ = new QTimer(this);
    autosaveTimer_->setInterval(15000);
    connect(autosaveTimer_, &QTimer::timeout, this, [this]() {
        if (document_.isDirty() && !document_.sourcePath().isEmpty()) saveProject(true);
    });
    autosaveTimer_->start();

    loadStartupProject();
}

void MainWindow::createShell() {
    auto* central = new QWidget;
    auto* root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* sidebar = new QWidget;
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(235);
    auto* sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(18, 20, 18, 18);
    sideLayout->setSpacing(12);

    auto* appName = new QLabel(QStringLiteral("WORLDBUILDER\nWRITER"));
    appName->setObjectName(QStringLiteral("appName"));
    projectTitle_ = new QLabel;
    projectTitle_->setObjectName(QStringLiteral("projectTitle"));
    projectTitle_->setWordWrap(true);
    saveState_ = new QLabel;
    saveState_->setObjectName(QStringLiteral("saveState"));
    saveState_->setWordWrap(true);
    navigation_ = new QListWidget;
    navigation_->setObjectName(QStringLiteral("navigation"));
    navigation_->addItems({tr("Planificación"), tr("Escritura"), tr("Mundo"), tr("Revisión y salida")});
    navigation_->setCurrentRow(1);
    sideLayout->addWidget(appName);
    sideLayout->addWidget(projectTitle_);
    sideLayout->addWidget(saveState_);
    sideLayout->addSpacing(8);
    sideLayout->addWidget(navigation_, 1);

    auto* quickSave = makeButton(tr("Guardar"));
    auto* focus = makeButton(tr("Modo enfoque"));
    sideLayout->addWidget(quickSave);
    sideLayout->addWidget(focus);
    root->addWidget(sidebar);

    pages_ = new QStackedWidget;
    planningPage_ = new PlanningPage;
    writingPage_ = new WritingPage;
    worldPage_ = new WorldPage;
    reviewPage_ = new ReviewPage;
    pages_->addWidget(planningPage_);
    pages_->addWidget(writingPage_);
    pages_->addWidget(worldPage_);
    pages_->addWidget(reviewPage_);
    pages_->setCurrentIndex(1);
    root->addWidget(pages_, 1);
    setCentralWidget(central);

    connect(navigation_, &QListWidget::currentRowChanged, pages_, &QStackedWidget::setCurrentIndex);
    connect(quickSave, &QPushButton::clicked, this, [this]() { saveProject(); });
    connect(focus, &QPushButton::clicked, writingPage_, &WritingPage::openFocusMode);
    connect(planningPage_, &PlanningPage::changed, this, &MainWindow::onDocumentChanged);
    connect(writingPage_, &WritingPage::changed, this, &MainWindow::onDocumentChanged);
    connect(worldPage_, &WorldPage::changed, this, &MainWindow::onDocumentChanged);
    connect(reviewPage_, &ReviewPage::changed, this, &MainWindow::onDocumentChanged);
    connect(writingPage_, &WritingPage::referenceActivated, this, &MainWindow::handleReference);
    connect(reviewPage_, &ReviewPage::requestExportPdf, this, &MainWindow::exportPdf);
    connect(reviewPage_, &ReviewPage::requestExportWbw, this, &MainWindow::exportWbw);
    connect(reviewPage_, &ReviewPage::requestBackup, this, &MainWindow::createBackup);
}

void MainWindow::createMenus() {
    auto* fileMenu = menuBar()->addMenu(tr("Archivo"));
    QAction* library = fileMenu->addAction(tr("Biblioteca local…"));
    QAction* create = fileMenu->addAction(tr("Nuevo proyecto"));
    QAction* open = fileMenu->addAction(tr("Abrir proyecto…"));
    QAction* import = fileMenu->addAction(tr("Importar .wbw…"));
    fileMenu->addSeparator();
    QAction* save = fileMenu->addAction(tr("Guardar"));
    QAction* saveAs = fileMenu->addAction(tr("Guardar JSON como…"));
    fileMenu->addSeparator();
    QAction* exportProject = fileMenu->addAction(tr("Exportar proyecto .wbw…"));
    QAction* pdf = fileMenu->addAction(tr("Exportar manuscrito PDF…"));
    QAction* backup = fileMenu->addAction(tr("Crear copia de seguridad…"));
    fileMenu->addSeparator();
    QAction* exit = fileMenu->addAction(tr("Salir"));

    create->setShortcut(QKeySequence::New);
    open->setShortcut(QKeySequence::Open);
    save->setShortcut(QKeySequence::Save);
    saveAs->setShortcut(QKeySequence::SaveAs);

    connect(library, &QAction::triggered, this, &MainWindow::openLibrary);
    connect(create, &QAction::triggered, this, &MainWindow::newProject);
    connect(open, &QAction::triggered, this, &MainWindow::openProject);
    connect(import, &QAction::triggered, this, &MainWindow::importWbw);
    connect(save, &QAction::triggered, this, [this]() { saveProject(); });
    connect(saveAs, &QAction::triggered, this, &MainWindow::saveProjectAs);
    connect(exportProject, &QAction::triggered, this, &MainWindow::exportWbw);
    connect(pdf, &QAction::triggered, this, &MainWindow::exportPdf);
    connect(backup, &QAction::triggered, this, &MainWindow::createBackup);
    connect(exit, &QAction::triggered, this, &QWidget::close);

    auto* viewMenu = menuBar()->addMenu(tr("Vista"));
    QAction* planning = viewMenu->addAction(tr("Planificación"));
    QAction* writing = viewMenu->addAction(tr("Escritura"));
    QAction* world = viewMenu->addAction(tr("Mundo"));
    QAction* review = viewMenu->addAction(tr("Revisión y salida"));
    viewMenu->addSeparator();
    QAction* focus = viewMenu->addAction(tr("Modo enfoque"));
    connect(planning, &QAction::triggered, this, [this]() { navigation_->setCurrentRow(0); });
    connect(writing, &QAction::triggered, this, [this]() { navigation_->setCurrentRow(1); });
    connect(world, &QAction::triggered, this, [this]() { navigation_->setCurrentRow(2); });
    connect(review, &QAction::triggered, this, [this]() { navigation_->setCurrentRow(3); });
    connect(focus, &QAction::triggered, writingPage_, &WritingPage::openFocusMode);
}

void MainWindow::loadStartupProject() {
    const QStringList files = ProjectStore::projectFiles();
    if (!files.isEmpty()) {
        ArchiveDocument document;
        QString error;
        if (ProjectStore::loadJsonFile(files.first(), document, &error)) {
            setDocument(std::move(document));
            return;
        }
    }
    ArchiveDocument document = ArchiveDocument::empty(tr("Nuevo proyecto"), tr("Nueva historia"));
    const QString directory = ProjectStore::createProjectDirectory();
    QString error;
    ProjectStore::saveIntoProjectDirectory(directory, document, &error);
    setDocument(std::move(document));
}

void MainWindow::newProject() {
    if (!confirmDiscard()) return;
    bool ok = false;
    const QString title = QInputDialog::getText(this, tr("Nuevo proyecto"), tr("Título de la historia:"), QLineEdit::Normal, tr("Nueva historia"), &ok).trimmed();
    if (!ok) return;
    ArchiveDocument document = ArchiveDocument::empty(title.isEmpty() ? tr("Nuevo proyecto") : title, title.isEmpty() ? tr("Nueva historia") : title);
    const QString directory = ProjectStore::createProjectDirectory();
    QString error;
    if (!ProjectStore::saveIntoProjectDirectory(directory, document, &error)) {
        QMessageBox::critical(this, tr("No se pudo crear el proyecto"), error);
        return;
    }
    setDocument(std::move(document));
    navigation_->setCurrentRow(1);
}

void MainWindow::openLibrary() {
    if (!confirmDiscard()) return;
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Biblioteca local"));
    dialog.resize(760, 520);
    auto* layout = new QVBoxLayout(&dialog);
    auto* description = new QLabel(tr("Proyectos guardados en Documentos/Worldbuilder Writer/Projects. La biblioteca no mezcla archivos externos con los proyectos administrados por la aplicación."));
    description->setWordWrap(true);
    layout->addWidget(description);
    auto* list = new QListWidget;
    const QStringList files = ProjectStore::projectFiles();
    for (const QString& path : files) {
        ArchiveDocument candidate;
        QString error;
        QString title = QFileInfo(path).dir().dirName();
        if (ProjectStore::loadJsonFile(path, candidate, &error)) title = candidate.storyTitle().isEmpty() ? candidate.title() : candidate.storyTitle();
        auto* item = new QListWidgetItem(title);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
        list->addItem(item);
    }
    layout->addWidget(list, 1);
    auto* buttons = new QHBoxLayout;
    auto* open = makeButton(tr("Abrir"));
    auto* remove = makeButton(tr("Eliminar de la biblioteca"));
    auto* cancel = makeButton(tr("Cerrar"));
    buttons->addWidget(open);
    buttons->addWidget(remove);
    buttons->addStretch();
    buttons->addWidget(cancel);
    layout->addLayout(buttons);
    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(open, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(list, &QListWidget::itemDoubleClicked, &dialog, [&dialog](QListWidgetItem*) { dialog.accept(); });
    connect(remove, &QPushButton::clicked, &dialog, [this, list]() {
        auto* item = list->currentItem();
        if (!item) return;
        const QString path = item->data(Qt::UserRole).toString();
        if (QMessageBox::question(this, tr("Eliminar proyecto"), tr("¿Eliminar definitivamente este proyecto de la biblioteca local?")) != QMessageBox::Yes) return;
        QString error;
        if (!ProjectStore::removeProject(path, &error)) {
            QMessageBox::critical(this, tr("No se pudo eliminar"), error);
            return;
        }
        delete list->takeItem(list->row(item));
    });
    if (dialog.exec() != QDialog::Accepted) return;
    auto* selected = list->currentItem();
    if (!selected) return;
    loadPath(selected->data(Qt::UserRole).toString());
}

void MainWindow::openProject() {
    if (!confirmDiscard()) return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Abrir proyecto"), QString(), tr("Proyectos Worldbuilder Writer (*.json *.wbw);;JSON (*.json);;Paquete WBW (*.wbw)"));
    if (!path.isEmpty()) loadPath(path);
}

void MainWindow::importWbw() {
    if (!confirmDiscard()) return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Importar proyecto .wbw"), QString(), tr("Paquete Worldbuilder Writer (*.wbw)"));
    if (!path.isEmpty()) loadPath(path);
}

bool MainWindow::loadPath(const QString& path) {
    ArchiveDocument candidate;
    QString error;
    if (QFileInfo(path).suffix().compare(QStringLiteral("wbw"), Qt::CaseInsensitive) == 0) {
        if (!WbwPackage::importPackage(path, candidate, &error)) {
            QMessageBox::critical(this, tr("No se pudo importar"), error);
            return false;
        }
        const QString directory = ProjectStore::createProjectDirectory();
        if (!ProjectStore::saveIntoProjectDirectory(directory, candidate, &error)) {
            QMessageBox::critical(this, tr("No se pudo crear la copia editable"), error);
            return false;
        }
    } else {
        if (!ProjectStore::loadJsonFile(path, candidate, &error)) {
            QMessageBox::critical(this, tr("No se pudo abrir"), error);
            return false;
        }
    }
    setDocument(std::move(candidate));
    return true;
}

bool MainWindow::saveProject(bool quiet) {
    QString error;
    if (document_.sourcePath().isEmpty()) {
        const QString directory = ProjectStore::createProjectDirectory();
        if (!ProjectStore::saveIntoProjectDirectory(directory, document_, &error)) {
            if (!quiet) QMessageBox::critical(this, tr("No se pudo guardar"), error);
            return false;
        }
    } else if (!ProjectStore::saveJsonFile(document_.sourcePath(), document_, &error)) {
        if (!quiet) QMessageBox::critical(this, tr("No se pudo guardar"), error);
        return false;
    }
    updateWindowTitle();
    return true;
}

bool MainWindow::saveProjectAs() {
    const QString path = QFileDialog::getSaveFileName(this, tr("Guardar proyecto JSON"), document_.storyTitle().isEmpty() ? QStringLiteral("project.json") : safeFileName(document_.storyTitle()) + QStringLiteral(".json"), tr("JSON (*.json)"));
    if (path.isEmpty()) return false;
    QString target = path;
    if (!target.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) target += QStringLiteral(".json");
    QString error;
    if (!ProjectStore::saveJsonFile(target, document_, &error)) {
        QMessageBox::critical(this, tr("No se pudo guardar"), error);
        return false;
    }
    updateWindowTitle();
    return true;
}

void MainWindow::exportWbw() {
    saveProject(true);
    const QString suggested = safeFileName(document_.storyTitle().isEmpty() ? document_.title() : document_.storyTitle()) + QStringLiteral(".wbw");
    QString path = QFileDialog::getSaveFileName(this, tr("Exportar proyecto .wbw"), suggested, tr("Proyecto Worldbuilder Writer (*.wbw)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(QStringLiteral(".wbw"), Qt::CaseInsensitive)) path += QStringLiteral(".wbw");
    QString error;
    if (!WbwPackage::exportPackage(path, document_, &error)) {
        QMessageBox::critical(this, tr("No se pudo exportar"), error);
        return;
    }
    statusBar()->showMessage(tr("Proyecto exportado: %1").arg(path), 6000);
}

void MainWindow::exportPdf() {
    saveProject(true);
    const QString suggested = safeFileName(document_.storyTitle().isEmpty() ? document_.title() : document_.storyTitle()) + QStringLiteral(".pdf");
    QString path = QFileDialog::getSaveFileName(this, tr("Exportar manuscrito PDF"), suggested, tr("PDF (*.pdf)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)) path += QStringLiteral(".pdf");

    const QJsonObject layout = effectiveLayout(document_);
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    QPageSize pageSize(QSizeF(layout.value(QStringLiteral("pageWidthMm")).toDouble(), layout.value(QStringLiteral("pageHeightMm")).toDouble()), QPageSize::Millimeter, QStringLiteral("Worldbuilder Writer"));
    QPageLayout pageLayout(pageSize, QPageLayout::Portrait,
        QMarginsF(layout.value(QStringLiteral("marginLeftMm")).toDouble(),
                  layout.value(QStringLiteral("marginTopMm")).toDouble(),
                  layout.value(QStringLiteral("marginRightMm")).toDouble(),
                  layout.value(QStringLiteral("marginBottomMm")).toDouble()),
        QPageLayout::Millimeter);
    printer.setPageLayout(pageLayout);

    const QString fontFamily = layout.value(QStringLiteral("fontFamily")).toString(QStringLiteral("Garamond"));
    const double fontSize = layout.value(QStringLiteral("fontSizePt")).toDouble(11.0);
    const double lineHeight = layout.value(QStringLiteral("lineHeight")).toDouble(1.35);
    const double indent = layout.value(QStringLiteral("paragraphIndentMm")).toDouble(5.0);
    const QString opening = layout.value(QStringLiteral("chapterOpening")).toString(QStringLiteral("Página nueva"));
    const QString separator = layout.value(QStringLiteral("sceneSeparator")).toString(QStringLiteral("⁂"));
    const QJsonObject profile = document_.object(QStringLiteral("profile"));

    QString html = QStringLiteral("<html><head><meta charset='utf-8'><style>"
        "body{font-family:'%1';font-size:%2pt;line-height:%3;}"
        "p{margin:0 0 0.45em 0;text-indent:%4mm;}"
        "h1{font-size:1.65em;text-align:center;margin:2.2em 0 2em 0;page-break-after:avoid;}"
        ".subtitle{text-align:center;font-size:1.05em;margin-bottom:3em;text-indent:0;}"
        ".chapter{margin-top:2em;}"
        ".newpage{page-break-before:always;}"
        ".separator{text-align:center;text-indent:0;margin:1.5em 0;}"
        ".front-title{text-align:center;font-size:2.1em;font-weight:700;margin-top:7em;text-indent:0;}"
        ".front-author{text-align:center;margin-top:2em;text-indent:0;}"
        "</style></head><body>")
        .arg(fontFamily.toHtmlEscaped())
        .arg(fontSize, 0, 'f', 1)
        .arg(lineHeight, 0, 'f', 2)
        .arg(indent, 0, 'f', 1);

    const QString storyTitle = document_.storyTitle().isEmpty() ? document_.title() : document_.storyTitle();
    html += QStringLiteral("<p class='front-title'>%1</p>").arg(storyTitle.toHtmlEscaped());
    const QString subtitle = profile.value(QStringLiteral("subtitle")).toString();
    if (!subtitle.isEmpty()) html += QStringLiteral("<p class='subtitle'>%1</p>").arg(subtitle.toHtmlEscaped());
    const QString author = profile.value(QStringLiteral("author")).toString();
    if (!author.isEmpty()) html += QStringLiteral("<p class='front-author'>%1</p>").arg(author.toHtmlEscaped());

    const QJsonArray chapters = document_.array(QStringLiteral("writingChapters"));
    for (int c = 0; c < chapters.size(); ++c) {
        const QJsonObject chapter = chapters.at(c).toObject();
        const QString cssClass = (opening != QStringLiteral("Continuo") || c == 0) ? QStringLiteral("chapter newpage") : QStringLiteral("chapter");
        QString heading = chapter.value(QStringLiteral("label")).toString();
        const QString chapterTitle = chapter.value(QStringLiteral("title")).toString();
        if (!chapterTitle.isEmpty()) heading += heading.isEmpty() ? chapterTitle : QStringLiteral(" — ") + chapterTitle;
        html += QStringLiteral("<section class='%1'><h1>%2</h1>").arg(cssClass, heading.toHtmlEscaped());
        const QJsonArray scenes = chapter.value(QStringLiteral("scenes")).toArray();
        for (int s = 0; s < scenes.size(); ++s) {
            if (s > 0 && !separator.isEmpty()) html += QStringLiteral("<p class='separator'>%1</p>").arg(separator.toHtmlEscaped());
            html += sceneBodyHtml(scenes.at(s).toObject().value(QStringLiteral("content")).toString());
        }
        html += QStringLiteral("</section>");
    }
    html += QStringLiteral("</body></html>");

    QTextDocument output;
    QFont font(fontFamily);
    font.setPointSizeF(fontSize);
    output.setDefaultFont(font);
    output.setHtml(html);
    output.setDocumentMargin(0.0);
    output.print(&printer);
    statusBar()->showMessage(tr("PDF exportado: %1").arg(path), 6000);
}

void MainWindow::createBackup() {
    saveProject(true);
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
    const QString previous = settings.value(QStringLiteral("backupDirectory"), QDir::homePath()).toString();
    const QString directory = QFileDialog::getExistingDirectory(this, tr("Carpeta de copias de seguridad"), previous);
    if (directory.isEmpty()) return;
    settings.setValue(QStringLiteral("backupDirectory"), directory);
    const QString base = safeFileName(document_.storyTitle().isEmpty() ? document_.title() : document_.storyTitle());
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString path = QDir(directory).filePath(QStringLiteral("%1-%2.wbw").arg(base, stamp));
    QString error;
    if (!WbwPackage::exportPackage(path, document_, &error)) {
        QMessageBox::critical(this, tr("No se pudo crear la copia"), error);
        return;
    }
    statusBar()->showMessage(tr("Copia de seguridad creada: %1").arg(path), 7000);
}

bool MainWindow::confirmDiscard() {
    if (!document_.isDirty()) return true;
    const QMessageBox::StandardButton answer = QMessageBox::warning(this, tr("Cambios sin guardar"), tr("Hay cambios sin guardar. ¿Quieres guardarlos antes de continuar?"), QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Cancel) return false;
    if (answer == QMessageBox::Save) return saveProject();
    return true;
}

void MainWindow::setDocument(ArchiveDocument document) {
    document_ = std::move(document);
    planningPage_->setDocument(&document_);
    writingPage_->setDocument(&document_);
    worldPage_->setDocument(&document_);
    reviewPage_->setDocument(&document_);
    updateWindowTitle();
    applyTheme();
}

void MainWindow::refreshPages() {
    planningPage_->refresh();
    writingPage_->refresh();
    worldPage_->refresh();
    reviewPage_->refresh();
    updateWindowTitle();
}

void MainWindow::onDocumentChanged() {
    updateWindowTitle();
    applyTheme();
}

void MainWindow::updateWindowTitle() {
    const QString title = document_.storyTitle().isEmpty() ? document_.title() : document_.storyTitle();
    setWindowTitle(QStringLiteral("%1%2 — Worldbuilder Writer").arg(document_.isDirty() ? QStringLiteral("* ") : QString(), title.isEmpty() ? tr("Sin título") : title));
    projectTitle_->setText(title.isEmpty() ? tr("Sin título") : title);
    if (document_.isDirty()) saveState_->setText(tr("Cambios pendientes · guardado automático activo"));
    else saveState_->setText(document_.sourcePath().isEmpty() ? tr("Sin ubicación local") : tr("Guardado"));
}

void MainWindow::applyTheme() {
    const QString theme = document_.object(QStringLiteral("profile")).value(QStringLiteral("theme")).toString(QStringLiteral("grim"));
    QString background = QStringLiteral("#1b1c18");
    QString panel = QStringLiteral("#22231e");
    QString elevated = QStringLiteral("#292a24");
    QString text = QStringLiteral("#eee9dd");
    QString muted = QStringLiteral("#a4a197");
    QString accent = QStringLiteral("#b6985c");
    QString border = QStringLiteral("#3b3c34");
    if (theme == QStringLiteral("chronicle")) {
        background = QStringLiteral("#242019"); panel = QStringLiteral("#2d281f"); elevated = QStringLiteral("#373027"); text = QStringLiteral("#f0e2c6"); muted = QStringLiteral("#b5a58c"); accent = QStringLiteral("#c99858"); border = QStringLiteral("#514536");
    } else if (theme == QStringLiteral("desk")) {
        background = QStringLiteral("#1b2021"); panel = QStringLiteral("#222829"); elevated = QStringLiteral("#293132"); text = QStringLiteral("#e4e9e8"); muted = QStringLiteral("#9aa7a5"); accent = QStringLiteral("#77a5a0"); border = QStringLiteral("#3c4848");
    } else if (theme == QStringLiteral("classic")) {
        background = QStringLiteral("#252321"); panel = QStringLiteral("#302d29"); elevated = QStringLiteral("#393530"); text = QStringLiteral("#f2ede5"); muted = QStringLiteral("#aca39a"); accent = QStringLiteral("#bd8f67"); border = QStringLiteral("#514b44");
    } else if (theme == QStringLiteral("kawaii")) {
        background = QStringLiteral("#f2f0eb"); panel = QStringLiteral("#e8e5de"); elevated = QStringLiteral("#ffffff"); text = QStringLiteral("#292825"); muted = QStringLiteral("#6e6a63"); accent = QStringLiteral("#8a6e55"); border = QStringLiteral("#cbc5ba");
    }
    setStyleSheet(QStringLiteral(
        "QMainWindow,QDialog{background:%1;color:%4;}"
        "QWidget{color:%4;font-family:'Segoe UI';font-size:10pt;}"
        "#sidebar{background:%2;border-right:1px solid %7;}"
        "#appName{font-size:15pt;font-weight:800;letter-spacing:2px;color:%6;}"
        "#projectTitle{font-size:11pt;font-weight:700;}"
        "#saveState{font-size:8.5pt;color:%5;}"
        "QListWidget,QTreeWidget,QTextEdit,QLineEdit,QComboBox,QSpinBox,QDoubleSpinBox,QGraphicsView{background:%3;border:1px solid %7;border-radius:4px;padding:4px;}"
        "QListWidget::item,QTreeWidget::item{padding:7px;border-radius:3px;}"
        "QListWidget::item:selected,QTreeWidget::item:selected{background:%6;color:%1;}"
        "QPushButton,QToolButton{background:%3;border:1px solid %7;border-radius:4px;padding:7px 10px;}"
        "QPushButton:hover,QToolButton:hover{border-color:%6;}"
        "QToolButton:checked{background:%6;color:%1;}"
        "QTabWidget::pane{border:0;}"
        "QTabBar::tab{background:%2;color:%5;padding:9px 14px;border-bottom:2px solid transparent;}"
        "QTabBar::tab:selected{color:%4;border-bottom-color:%6;}"
        "QMenuBar,QMenu{background:%2;color:%4;}"
        "QMenu::item:selected{background:%6;color:%1;}"
        "QScrollBar:vertical{background:%2;width:10px;}QScrollBar::handle:vertical{background:%7;min-height:24px;border-radius:4px;}"
    ).arg(background, panel, elevated, text, muted, accent, border));
}

void MainWindow::handleReference(const QString& kind, const QString& id) {
    if (kind == tr("Personaje") || kind == QStringLiteral("character")) {
        navigation_->setCurrentRow(0);
        return;
    }
    navigation_->setCurrentRow(2);
    worldPage_->openReference(kind, id);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (confirmDiscard()) event->accept();
    else event->ignore();
}

} // namespace wbw
