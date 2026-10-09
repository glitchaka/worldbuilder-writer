#include "ui/MainWindow.h"

#include "storage/ProjectStore.h"
#include "storage/WbwPackage.h"
#include "ui/PlanningPage.h"
#include "ui/ProjectHubPage.h"
#include "ui/ReviewPage.h"
#include "ui/WorldPage.h"
#include "ui/WritingPage.h"

#include <QAction>
#include <QCloseEvent>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QListView>
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
    central->setObjectName(QStringLiteral("appRoot"));
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* top = new QWidget;
    top->setObjectName(QStringLiteral("topShell"));
    top->setFixedHeight(64);
    auto* topLayout = new QHBoxLayout(top);
    topLayout->setContentsMargins(18, 0, 18, 0);
    topLayout->setSpacing(14);

    auto* brand = new QWidget;
    brand->setObjectName(QStringLiteral("appIdentity"));
    auto* brandLayout = new QHBoxLayout(brand);
    brandLayout->setContentsMargins(0, 0, 0, 0);
    brandLayout->setSpacing(10);
    auto* mark = new QLabel(QStringLiteral("WW"));
    mark->setObjectName(QStringLiteral("appMark"));
    mark->setAlignment(Qt::AlignCenter);
    mark->setFixedSize(38, 38);
    auto* brandText = new QVBoxLayout;
    brandText->setSpacing(0);
    auto* name = new QLabel(QStringLiteral("Worldbuilder Writer"));
    name->setObjectName(QStringLiteral("appName"));
    auto* mode = new QLabel(tr("ARCHIVO DE PROYECTO"));
    mode->setObjectName(QStringLiteral("appMode"));
    brandText->addWidget(name);
    brandText->addWidget(mode);
    brandLayout->addWidget(mark);
    brandLayout->addLayout(brandText);
    topLayout->addWidget(brand);

    auto* libraryButton = makeButton(tr("Biblioteca"));
    libraryButton->setObjectName(QStringLiteral("libraryAction"));
    topLayout->addWidget(libraryButton);

    navigation_ = new QListWidget;
    navigation_->setObjectName(QStringLiteral("topNavigation"));
    navigation_->setFlow(QListView::LeftToRight);
    navigation_->setWrapping(false);
    navigation_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navigation_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navigation_->setFixedHeight(63);
    navigation_->setMinimumWidth(470);
    navigation_->addItems({tr("Planificación"), tr("Escritura"), tr("Mundo"), tr("Revisión")});
    navigation_->setCurrentRow(-1);
    for (int i = 0; i < navigation_->count(); ++i) {
        navigation_->item(i)->setTextAlignment(Qt::AlignCenter);
        navigation_->item(i)->setSizeHint(QSize(i == 0 ? 118 : 100, 62));
    }
    topLayout->addWidget(navigation_);
    topLayout->addStretch(1);

    auto* projectInfo = new QVBoxLayout;
    projectInfo->setSpacing(0);
    projectTitle_ = new QLabel(tr("Biblioteca local"));
    projectTitle_->setObjectName(QStringLiteral("projectTitle"));
    projectTitle_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    saveState_ = new QLabel;
    saveState_->setObjectName(QStringLiteral("saveState"));
    saveState_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    projectInfo->addWidget(projectTitle_);
    projectInfo->addWidget(saveState_);
    topLayout->addLayout(projectInfo);

    auto* quickSave = makeButton(tr("Guardar"));
    quickSave->setObjectName(QStringLiteral("primarySave"));
    auto* focus = makeButton(tr("Sin distracciones"));
    focus->setObjectName(QStringLiteral("secondaryAction"));
    topLayout->addWidget(quickSave);
    topLayout->addWidget(focus);
    root->addWidget(top);

    pages_ = new QStackedWidget;
    pages_->setObjectName(QStringLiteral("pageStack"));
    hubPage_ = new ProjectHubPage;
    planningPage_ = new PlanningPage;
    writingPage_ = new WritingPage;
    worldPage_ = new WorldPage;
    reviewPage_ = new ReviewPage;
    pages_->addWidget(hubPage_);
    pages_->addWidget(planningPage_);
    pages_->addWidget(writingPage_);
    pages_->addWidget(worldPage_);
    pages_->addWidget(reviewPage_);
    pages_->setCurrentIndex(0);
    root->addWidget(pages_, 1);
    setCentralWidget(central);

    connect(libraryButton, &QPushButton::clicked, this, &MainWindow::openLibrary);
    connect(navigation_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0) pages_->setCurrentIndex(row + 1);
    });
    connect(quickSave, &QPushButton::clicked, this, [this]() { saveProject(); });
    connect(focus, &QPushButton::clicked, writingPage_, &WritingPage::openFocusMode);
    connect(hubPage_, &ProjectHubPage::newProjectRequested, this, &MainWindow::newProject);
    connect(hubPage_, &ProjectHubPage::importProjectRequested, this, &MainWindow::importWbw);
    connect(hubPage_, &ProjectHubPage::openProjectRequested, this, [this](const QString& path) { loadPath(path); });
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
    QAction* library = fileMenu->addAction(tr("Biblioteca local"));
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
    QAction* review = viewMenu->addAction(tr("Revisión"));
    viewMenu->addSeparator();
    QAction* focus = viewMenu->addAction(tr("Modo enfoque"));
    connect(planning, &QAction::triggered, this, [this]() { navigation_->setCurrentRow(0); });
    connect(writing, &QAction::triggered, this, [this]() { navigation_->setCurrentRow(1); });
    connect(world, &QAction::triggered, this, [this]() { navigation_->setCurrentRow(2); });
    connect(review, &QAction::triggered, this, [this]() { navigation_->setCurrentRow(3); });
    connect(focus, &QAction::triggered, writingPage_, &WritingPage::openFocusMode);
}

void MainWindow::loadStartupProject() {
    showLibrary();
}

void MainWindow::showLibrary() {
    hubPage_->refresh();
    pages_->setCurrentIndex(0);
    navigation_->setCurrentRow(-1);
    projectTitle_->setText(tr("Biblioteca local"));
    saveState_->setText(tr("Archivo de proyectos"));
    setWindowTitle(tr("Worldbuilder Writer — Biblioteca local"));
}

void MainWindow::newProject() {
    if (!confirmDiscard()) return;

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Nueva obra"));
    dialog.setModal(true);
    dialog.setMinimumWidth(480);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(22, 20, 22, 20);
    layout->setSpacing(12);
    auto* kicker = new QLabel(tr("NUEVA OBRA"));
    kicker->setObjectName(QStringLiteral("dialogKicker"));
    auto* title = new QLabel(tr("Crear una obra desde cero"));
    title->setObjectName(QStringLiteral("dialogTitle"));
    auto* explainer = new QLabel(tr("El archivo comenzará vacío y usará la interfaz nativa de Worldbuilder Writer."));
    explainer->setWordWrap(true);
    explainer->setObjectName(QStringLiteral("dialogDescription"));
    auto* archiveLabel = new QLabel(tr("Nombre del archivo"));
    archiveLabel->setObjectName(QStringLiteral("fieldLabel"));
    auto* archiveTitle = new QLineEdit;
    archiveTitle->setPlaceholderText(tr("Ej. Archivo de la Torre Hundida"));
    auto* storyLabel = new QLabel(tr("Título de la historia (opcional)"));
    storyLabel->setObjectName(QStringLiteral("fieldLabel"));
    auto* storyTitle = new QLineEdit;
    storyTitle->setPlaceholderText(tr("Puede cambiarse después"));
    auto* actions = new QHBoxLayout;
    auto* cancel = makeButton(tr("Cancelar"));
    auto* create = makeButton(tr("Crear obra vacía"));
    create->setObjectName(QStringLiteral("primaryAction"));
    actions->addStretch();
    actions->addWidget(cancel);
    actions->addWidget(create);
    layout->addWidget(kicker);
    layout->addWidget(title);
    layout->addWidget(explainer);
    layout->addSpacing(4);
    layout->addWidget(archiveLabel);
    layout->addWidget(archiveTitle);
    layout->addWidget(storyLabel);
    layout->addWidget(storyTitle);
    layout->addLayout(actions);
    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(create, &QPushButton::clicked, &dialog, [&dialog, archiveTitle]() {
        if (!archiveTitle->text().trimmed().isEmpty()) dialog.accept();
    });
    archiveTitle->setFocus();
    if (dialog.exec() != QDialog::Accepted) return;

    const QString archive = archiveTitle->text().trimmed();
    const QString story = storyTitle->text().trimmed();
    ArchiveDocument document = ArchiveDocument::empty(archive, story.isEmpty() ? archive : story);
    const QString directory = ProjectStore::createProjectDirectory();
    QString error;
    if (!ProjectStore::saveIntoProjectDirectory(directory, document, &error)) {
        QMessageBox::critical(this, tr("No se pudo crear el proyecto"), error);
        return;
    }
    setDocument(std::move(document));
    hubPage_->refresh();
    navigation_->setCurrentRow(1);
}

void MainWindow::openLibrary() {
    if (!confirmDiscard()) return;
    showLibrary();
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
    hubPage_->refresh();
    navigation_->setCurrentRow(1);
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
    if (navigation_->currentRow() < 0) navigation_->setCurrentRow(1);
    pages_->setCurrentIndex(navigation_->currentRow() + 1);
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
    if (document_.isDirty()) saveState_->setText(tr("Cambios pendientes"));
    else saveState_->setText(document_.sourcePath().isEmpty() ? tr("Sin ubicación local") : tr("Guardado local"));
}

void MainWindow::applyTheme() {
    setStyleSheet(QStringLiteral(
        "QMainWindow,QDialog{background:#edf1f5;color:#1f2937;}"
        "QWidget{color:#1f2937;font-family:'Segoe UI';font-size:9.5pt;}"
        "#appRoot,#pageStack,#projectHubPage,#hubCardsHost{background:#edf1f5;}"
        "#topShell{background:#ffffff;border-bottom:1px solid #d7dde5;}"
        "#appIdentity{background:transparent;}"
        "#appMark{background:#17233a;color:#ffffff;border-radius:3px;font-family:'Georgia';font-size:11pt;font-weight:700;}"
        "#appName{color:#243247;font-weight:700;font-size:10pt;}"
        "#appMode{color:#98a2b3;font-size:7pt;font-weight:700;letter-spacing:1px;}"
        "#projectTitle{font-size:9pt;font-weight:600;color:#344054;padding:0 4px;}"
        "#saveState{font-size:8pt;color:#2f855a;padding:0 4px;}"
        "#topNavigation{background:#ffffff;border:0;padding:0;margin:0;}"
        "#topNavigation::item{border:0;border-bottom:3px solid transparent;padding:0 14px;color:#475467;background:#ffffff;}"
        "#topNavigation::item:hover{background:#f7f9fc;color:#175cd3;}"
        "#topNavigation::item:selected{background:#ffffff;color:#175cd3;border-bottom:3px solid #175cd3;font-weight:600;}"
        "#libraryAction{background:#ffffff;border:1px solid #d4dbe5;color:#344054;padding:7px 10px;}"
        "QListWidget,QTreeWidget,QTextEdit,QLineEdit,QComboBox,QSpinBox,QDoubleSpinBox,QGraphicsView{background:#ffffff;border:1px solid #cfd6df;border-radius:3px;padding:5px;selection-background-color:#dceafe;selection-color:#1f2937;}"
        "QListWidget::item,QTreeWidget::item{padding:6px;border:0;}"
        "QListWidget::item:hover,QTreeWidget::item:hover{background:#f3f6fa;}"
        "QListWidget::item:selected,QTreeWidget::item:selected{background:#e7f0ff;color:#175cd3;border-left:2px solid #175cd3;}"
        "QPushButton,QToolButton{background:#ffffff;color:#344054;border:1px solid #cbd3dd;border-radius:3px;padding:7px 11px;}"
        "QPushButton:hover,QToolButton:hover{background:#f6f8fb;border-color:#98a2b3;}"
        "QPushButton:pressed,QToolButton:pressed{background:#edf2f7;}"
        "QPushButton#primarySave,QPushButton#primaryAction,QPushButton#hubPrimary{background:#1668d4;color:#ffffff;border-color:#1668d4;font-weight:600;}"
        "QPushButton#primarySave:hover,QPushButton#primaryAction:hover,QPushButton#hubPrimary:hover{background:#0f5fc8;border-color:#0f5fc8;}"
        "QPushButton#secondaryAction{background:#ffffff;color:#344054;}"
        "QToolButton:checked{background:#20262e;color:#ffffff;border-color:#20262e;}"
        "QTabWidget::pane{border:0;background:transparent;}"
        "QTabBar::tab{background:transparent;color:#475467;padding:10px 14px;border:0;border-bottom:2px solid transparent;}"
        "QTabBar::tab:hover{color:#175cd3;background:#f4f7fb;}"
        "QTabBar::tab:selected{color:#175cd3;background:transparent;border-bottom:2px solid #175cd3;}"
        "QMenuBar{background:#ffffff;color:#344054;border-bottom:1px solid #e2e7ed;}"
        "QMenuBar::item{background:transparent;padding:5px 9px;}"
        "QMenuBar::item:selected{background:#f2f5f9;color:#175cd3;}"
        "QMenu{background:#ffffff;color:#1f2937;border:1px solid #cfd6df;}"
        "QMenu::item{padding:7px 22px;}"
        "QMenu::item:selected{background:#e7f0ff;color:#175cd3;}"
        "QScrollArea{background:transparent;border:0;}"
        "QScrollBar:vertical{background:#edf1f5;width:10px;margin:0;}"
        "QScrollBar::handle:vertical{background:#b7c0cb;min-height:28px;border-radius:4px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
        "QStatusBar{background:#ffffff;color:#667085;border-top:1px solid #d7dde5;}"
        "#dialogKicker,#fieldLabel,#hubKicker,#projectCardGenre{color:#667085;font-size:8pt;font-weight:700;letter-spacing:.7px;}"
        "#dialogTitle,#hubTitle{font-family:'Georgia';font-size:25pt;color:#344054;}"
        "#dialogDescription,#hubDescription{color:#667085;font-family:'Georgia';}"
        "#hubStorage{background:#f7faf8;border:1px solid #d8e8dc;}"
        "#hubStorageDot{color:#2f855a;}"
        "#hubStorageTitle{font-weight:700;color:#344054;}"
        "#hubStorageDetail{color:#667085;font-size:8.5pt;}"
        "#projectCard{background:#ffffff;border:1px solid #d7dee8;border-radius:4px;}"
        "#projectCardMark{background:#17233a;color:#ffffff;border-radius:3px;font-family:'Georgia';font-weight:700;}"
        "#projectCardState{background:#f2f4f7;color:#475467;border:1px solid #e1e5ea;border-radius:3px;padding:3px 7px;font-size:8pt;}"
        "#projectCardTitle{font-family:'Georgia';font-size:16pt;font-weight:700;color:#344054;}"
        "#projectCardArchive{color:#667085;}"
        "#projectStatValue{font-weight:700;color:#344054;}"
        "#projectStatLabel{color:#98a2b3;font-size:8pt;}"
        "#projectDelete{color:#b42318;}"
        "#hubEmpty{background:#ffffff;border:1px dashed #cbd3dd;color:#667085;padding:40px;}"
    ));
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
