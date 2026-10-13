#include "ui/MainWindow.h"

#include "storage/ProjectStore.h"
#include "storage/WbwPackage.h"
#include "ui/PlanningPage.h"
#include "ui/ProjectHubPage.h"
#include "ui/ReviewPage.h"
#include "ui/SettingsWorkspace.h"
#include "ui/WorldPage.h"
#include "ui/WritingPage.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMarginsF>
#include <QMessageBox>
#include <QPageLayout>
#include <QPageSize>
#include <QPrinter>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QShortcut>
#include <QSizeF>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFormat>
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

QString scenePlainText(const QJsonObject& scene) {
    if (scene.contains(QStringLiteral("text"))) return scene.value(QStringLiteral("text")).toString();
    QTextDocument legacy;
    legacy.setHtml(scene.value(QStringLiteral("content")).toString());
    return legacy.toPlainText();
}

void applyFormattingRuns(QTextDocument& document, int base, int textLength, const QJsonArray& formatting) {
    for (const QJsonValue value : formatting) {
        const QJsonObject run = value.toObject();
        const int start = run.value(QStringLiteral("start")).toInt();
        const int length = run.value(QStringLiteral("length")).toInt();
        if (start < 0 || length <= 0 || start >= textLength) continue;
        const int end = qMin(textLength, start + length);
        QTextCursor cursor(&document);
        cursor.setPosition(base + start);
        cursor.setPosition(base + end, QTextCursor::KeepAnchor);
        QTextCharFormat format;
        if (run.value(QStringLiteral("bold")).toBool()) format.setFontWeight(QFont::Bold);
        if (run.value(QStringLiteral("italic")).toBool()) format.setFontItalic(true);
        if (run.value(QStringLiteral("underline")).toBool()) format.setFontUnderline(true);
        cursor.mergeCharFormat(format);
    }
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
    return layout;
}

int savedAutosaveIntervalMs() {
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
    return qBound(5, settings.value(QStringLiteral("editor/autosaveSeconds"), 15).toInt(), 300) * 1000;
}

QWidget* navigationCell(const QString& glyph, const QString& text, QWidget* parent) {
    auto* cell = new QWidget(parent);
    cell->setObjectName(QStringLiteral("navigationCell"));
    cell->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    auto* box = new QVBoxLayout(cell);
    box->setContentsMargins(2, 7, 2, 6);
    box->setSpacing(3);
    auto* icon = new QLabel(glyph, cell);
    icon->setObjectName(QStringLiteral("navigationGlyph"));
    icon->setAlignment(Qt::AlignCenter);
    auto* label = new QLabel(text, cell);
    label->setObjectName(QStringLiteral("navigationLabel"));
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    box->addWidget(icon);
    box->addWidget(label);
    return cell;
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    resize(1480, 900);
    setMinimumSize(1040, 680);
    createShell();

    autosaveTimer_ = new QTimer(this);
    autosaveTimer_->setInterval(savedAutosaveIntervalMs());
    connect(autosaveTimer_, &QTimer::timeout, this, [this]() {
        commitPendingEdits();
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

    auto* workspace = new QWidget;
    workspace->setObjectName(QStringLiteral("workspaceShell"));
    auto* workspaceLayout = new QHBoxLayout(workspace);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(0);

    auto* rail = new QWidget;
    rail->setObjectName(QStringLiteral("sideRail"));
    rail->setFixedWidth(104);
    auto* railLayout = new QVBoxLayout(rail);
    railLayout->setContentsMargins(8, 10, 8, 10);
    railLayout->setSpacing(4);

    auto* brand = new QWidget;
    brand->setObjectName(QStringLiteral("appIdentity"));
    brand->setFixedHeight(58);
    auto* brandLayout = new QVBoxLayout(brand);
    brandLayout->setContentsMargins(0, 2, 0, 8);
    brandLayout->setSpacing(0);
    auto* mark = new QLabel(QStringLiteral("W"));
    mark->setObjectName(QStringLiteral("appMark"));
    mark->setAlignment(Qt::AlignCenter);
    mark->setFixedSize(42, 42);
    brandLayout->addWidget(mark, 0, Qt::AlignHCenter);
    railLayout->addWidget(brand);

    navigation_ = new QListWidget;
    navigation_->setObjectName(QStringLiteral("sideNavigation"));
    navigation_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navigation_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navigation_->setSpacing(2);
    const QStringList labels{
        tr("Biblioteca"), tr("Escritura"), tr("Tablero de escenas"), tr("Atlas / documentación"),
        tr("Mapas"), tr("Revisión"), tr("Configuración / salida")
    };
    const QStringList glyphs{QStringLiteral("▥"), QStringLiteral("▤"), QStringLiteral("▦"), QStringLiteral("◎"), QStringLiteral("⌘"), QStringLiteral("▥"), QStringLiteral("⚙")};
    for (int i = 0; i < labels.size(); ++i) {
        auto* item = new QListWidgetItem(labels.at(i), navigation_);
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        item->setSizeHint(QSize(88, i == 6 ? 72 : 66));
        item->setToolTip(labels.at(i));
        navigation_->setItemWidget(item, navigationCell(glyphs.at(i), labels.at(i), navigation_));
    }
    navigation_->setCurrentRow(0);
    railLayout->addWidget(navigation_, 1);
    workspaceLayout->addWidget(rail);

    auto* contentShell = new QWidget;
    contentShell->setObjectName(QStringLiteral("contentShell"));
    auto* contentLayout = new QVBoxLayout(contentShell);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    auto* top = new QWidget;
    top->setObjectName(QStringLiteral("topShell"));
    top->setFixedHeight(62);
    auto* topLayout = new QHBoxLayout(top);
    topLayout->setContentsMargins(22, 0, 20, 0);
    topLayout->setSpacing(14);

    auto* projectInfo = new QVBoxLayout;
    projectInfo->setContentsMargins(0, 0, 0, 0);
    projectInfo->setSpacing(1);
    projectTitle_ = new QLabel(tr("Biblioteca"));
    projectTitle_->setObjectName(QStringLiteral("projectTitle"));
    saveState_ = new QLabel(tr("Tus historias"));
    saveState_->setObjectName(QStringLiteral("saveState"));
    projectInfo->addWidget(projectTitle_);
    projectInfo->addWidget(saveState_);
    topLayout->addLayout(projectInfo);
    topLayout->addStretch(1);

    auto* globalSearch = new QLineEdit(top);
    globalSearch->setObjectName(QStringLiteral("globalSearch"));
    globalSearch->setPlaceholderText(tr("Buscar en el proyecto…   Ctrl K"));
    globalSearch->setClearButtonEnabled(true);
    globalSearch->setMaximumWidth(440);
    globalSearch->setMinimumWidth(320);
    topLayout->addWidget(globalSearch);
    topLayout->addStretch(1);

    auto* focus = makeButton(tr("Enfoque"));
    focus->setObjectName(QStringLiteral("secondaryAction"));
    auto* quickSave = makeButton(tr("Guardar"));
    quickSave->setObjectName(QStringLiteral("primarySave"));
    focus->hide();
    quickSave->hide();
    topLayout->addWidget(focus);
    topLayout->addWidget(quickSave);
    contentLayout->addWidget(top);

    pages_ = new QStackedWidget;
    pages_->setObjectName(QStringLiteral("pageStack"));
    hubPage_ = new ProjectHubPage;
    planningPage_ = new PlanningPage;
    writingPage_ = new WritingPage;
    worldPage_ = new WorldPage;
    reviewPage_ = new ReviewPage;
    settingsPage_ = new SettingsWorkspace;
    settingsPage_->setObjectName(QStringLiteral("settingsWorkspace"));

    if (auto* worldTabs = worldPage_->findChild<QTabWidget*>(QStringLiteral("worldTabs"))) {
        worldTabs->insertTab(0, planningPage_, tr("Personajes y tramas"));
        if (worldTabs->count() >= 5) {
            worldTabs->setTabText(0, tr("Personajes y tramas"));
            worldTabs->setTabText(1, tr("Atlas"));
            worldTabs->setTabText(2, tr("Magia"));
            worldTabs->setTabText(3, tr("Mapas"));
            worldTabs->setTabText(4, tr("Textos de referencia"));
        }
    }

    pages_->addWidget(hubPage_);
    pages_->addWidget(writingPage_);
    pages_->addWidget(worldPage_);
    pages_->addWidget(reviewPage_);
    pages_->addWidget(settingsPage_);
    pages_->setCurrentWidget(hubPage_);
    contentLayout->addWidget(pages_, 1);
    workspaceLayout->addWidget(contentShell, 1);
    root->addWidget(workspace, 1);
    setCentralWidget(central);

    setStyleSheet(QStringLiteral(R"QSS(
#appRoot,#workspaceShell,#contentShell{background:#0b0f14;}
#sideRail{background:#090d12;border-right:1px solid #202934;}
#appIdentity{background:transparent;border:0;}
#appMark{background:#111b27;color:#a9d3ff;border:1px solid #315a7e;border-radius:9px;font-family:'Georgia';font-size:14pt;font-weight:700;}
#sideNavigation{background:transparent;border:0;outline:0;color:#8d99a6;}
#sideNavigation::item{border:0;border-left:2px solid transparent;border-radius:8px;padding:0;margin:1px 0;}
#sideNavigation::item:hover{background:#121a23;}
#sideNavigation::item:selected{background:#16283b;border-left:2px solid #3f9ef6;}
#navigationCell{background:transparent;border:0;}
#navigationGlyph{color:#9aa8b7;font-size:16pt;font-weight:500;}
#navigationLabel{color:#8d99a6;font-size:7.5pt;}
#sideNavigation::item:selected #navigationGlyph{color:#79bfff;}
#sideNavigation::item:selected #navigationLabel{color:#d9ebfb;}
#topShell{background:#0c1117;border-bottom:1px solid #202a34;}
#projectTitle{color:#f0ece4;font-family:'Georgia';font-size:12pt;font-weight:600;}
#saveState{color:#78838e;font-size:8pt;}
#globalSearch{background:#0a1016;color:#cbd4dc;border:1px solid #273440;border-radius:8px;padding:8px 11px;}
#globalSearch:focus{border-color:#3c6f9a;background:#0d151d;}
#primarySave{background:#2d7fc8;color:#f8fbff;border:0;border-radius:7px;padding:8px 14px;font-weight:700;}
#primarySave:hover{background:#388fd9;}
#secondaryAction{background:#111820;color:#c8d0d8;border:1px solid #34414d;border-radius:7px;padding:7px 12px;}
#secondaryAction:hover{background:#18232e;color:#fff;}
)QSS"));

    connect(navigation_, &QListWidget::currentRowChanged, this, &MainWindow::routeProductModule);
    connect(quickSave, &QPushButton::clicked, this, [this]() { saveProject(); });

    auto* saveShortcut = new QShortcut(QKeySequence::Save, this);
    saveShortcut->setObjectName(QStringLiteral("wbwSaveShortcut"));
    saveShortcut->setContext(Qt::WindowShortcut);
    saveShortcut->setAutoRepeat(false);
    connect(saveShortcut, &QShortcut::activated, this, [this]() {
        if (!document_.sourcePath().isEmpty()) saveProject();
    });

    auto* searchShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+K")), this);
    searchShortcut->setObjectName(QStringLiteral("wbwGlobalSearchShortcut"));
    searchShortcut->setContext(Qt::WindowShortcut);
    connect(searchShortcut, &QShortcut::activated, globalSearch, [globalSearch]() {
        globalSearch->setFocus(Qt::ShortcutFocusReason);
        globalSearch->selectAll();
    });
    connect(globalSearch, &QLineEdit::returnPressed, this, [this, globalSearch]() {
        const QString query = globalSearch->text().trimmed();
        if (query.isEmpty()) return;
        if (navigation_->currentRow() == 1 || navigation_->currentRow() == 2) {
            if (auto* sceneSearch = writingPage_->findChild<QLineEdit*>(QStringLiteral("sceneBoardSearch"))) {
                sceneSearch->setText(query);
                sceneSearch->setFocus();
            } else if (auto* editor = writingPage_->findChild<QTextEdit*>(QStringLiteral("sceneEditor"))) {
                editor->find(query);
                editor->setFocus();
            }
        } else if (navigation_->currentRow() == 3) {
            if (auto* atlasSearch = worldPage_->findChild<QLineEdit*>(QStringLiteral("atlasSearch"))) {
                atlasSearch->setText(query);
                atlasSearch->setFocus();
            }
        }
    });

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
    connect(settingsPage_, &SettingsWorkspace::preferencesChanged, this, [this]() {
        if (autosaveTimer_) autosaveTimer_->setInterval(savedAutosaveIntervalMs());
        for (QWidget* target : {static_cast<QWidget*>(writingPage_), static_cast<QWidget*>(worldPage_), static_cast<QWidget*>(reviewPage_)})
            if (target) QCoreApplication::postEvent(target, new QEvent(QEvent::LayoutRequest));
    });

    routeProductModule(0);
}

void MainWindow::loadStartupProject() {
    showLibrary();
}

void MainWindow::showLibrary() {
    hubPage_->refresh();
    navigation_->setCurrentRow(0);
    routeProductModule(0);
    setWindowTitle(tr("Worldbuilder Writer — Biblioteca"));
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
}

void MainWindow::openLibrary() { showLibrary(); }

void MainWindow::openProject() {
    const QString path = QFileDialog::getOpenFileName(this, tr("Abrir proyecto"), QString(), tr("Proyectos Worldbuilder Writer (*.json *.wbw);;JSON (*.json);;Paquete WBW (*.wbw)"));
    if (!path.isEmpty()) loadPath(path);
}

void MainWindow::importWbw() {
    const QString path = QFileDialog::getOpenFileName(this, tr("Importar proyecto .wbw"), QString(), tr("Paquete Worldbuilder Writer (*.wbw)"));
    if (!path.isEmpty()) loadPath(path);
}

bool MainWindow::loadPath(const QString& path) {
    if (!confirmDiscard()) return false;
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
    } else if (!ProjectStore::loadJsonFile(path, candidate, &error)) {
        QMessageBox::critical(this, tr("No se pudo abrir"), error);
        return false;
    }

    setDocument(std::move(candidate));
    hubPage_->refresh();
    return true;
}

bool MainWindow::saveProject(bool quiet) {
    commitPendingEdits();
    QString error;
    if (document_.sourcePath().isEmpty()) {
        const QString directory = ProjectStore::createProjectDirectory();
        if (!ProjectStore::saveIntoProjectDirectory(directory, document_, &error)) {
            reportSaveFailure(error, quiet);
            return false;
        }
    } else if (!ProjectStore::saveJsonFile(document_.sourcePath(), document_, &error)) {
        reportSaveFailure(error, quiet);
        return false;
    }
    saveError_.clear();
    updateWindowTitle();
    if (!quiet) statusBar()->showMessage(tr("Proyecto guardado."), 5000);
    return true;
}

bool MainWindow::saveProjectAs() {
    commitPendingEdits();
    const QString path = QFileDialog::getSaveFileName(this, tr("Guardar proyecto JSON"), document_.storyTitle().isEmpty() ? QStringLiteral("project.json") : safeFileName(document_.storyTitle()) + QStringLiteral(".json"), tr("JSON (*.json)"));
    if (path.isEmpty()) return false;
    QString target = path;
    if (!target.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) target += QStringLiteral(".json");
    QString error;
    if (!ProjectStore::saveJsonFile(target, document_, &error)) {
        reportSaveFailure(error, false);
        return false;
    }
    saveError_.clear();
    updateWindowTitle();
    return true;
}

void MainWindow::commitPendingEdits() {
    const QList<QWidget*> dataPages{planningPage_, writingPage_, worldPage_, reviewPage_};
    for (QWidget* page : dataPages) {
        if (!page) continue;
        for (QLineEdit* field : page->findChildren<QLineEdit*>()) {
            if (!field->isModified() || !field->hasAcceptableInput()) continue;
            QMetaObject::invokeMethod(field, "editingFinished", Qt::DirectConnection);
            field->setModified(false);
        }
    }
}

void MainWindow::reportSaveFailure(const QString& error, bool quiet) {
    saveError_ = error;
    updateWindowTitle();
    statusBar()->showMessage(tr("No se pudo guardar el proyecto: %1").arg(error), 10000);
    if (!quiet) QMessageBox::critical(this, tr("No se pudo guardar"), error);
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
    const QPageSize pageSize(QSizeF(layout.value(QStringLiteral("pageWidthMm")).toDouble(), layout.value(QStringLiteral("pageHeightMm")).toDouble()), QPageSize::Millimeter, QStringLiteral("Worldbuilder Writer"));
    printer.setPageLayout(QPageLayout(pageSize, QPageLayout::Portrait,
        QMarginsF(layout.value(QStringLiteral("marginLeftMm")).toDouble(),
                  layout.value(QStringLiteral("marginTopMm")).toDouble(),
                  layout.value(QStringLiteral("marginRightMm")).toDouble(),
                  layout.value(QStringLiteral("marginBottomMm")).toDouble()),
        QPageLayout::Millimeter));

    const QString fontFamily = layout.value(QStringLiteral("fontFamily")).toString(QStringLiteral("Garamond"));
    const double fontSize = layout.value(QStringLiteral("fontSizePt")).toDouble(11.0);
    const double lineHeight = layout.value(QStringLiteral("lineHeight")).toDouble(1.35);
    const double indentMm = layout.value(QStringLiteral("paragraphIndentMm")).toDouble(5.0);
    const QString opening = layout.value(QStringLiteral("chapterOpening")).toString(QStringLiteral("Página nueva"));
    const QString separator = layout.value(QStringLiteral("sceneSeparator")).toString(QStringLiteral("⁂"));
    const QJsonObject profile = document_.object(QStringLiteral("profile"));

    QTextDocument output;
    QFont baseFont(fontFamily);
    baseFont.setPointSizeF(fontSize);
    output.setDefaultFont(baseFont);
    output.setDocumentMargin(0.0);
    QTextCursor cursor(&output);

    QTextCharFormat titleFormat;
    titleFormat.setFontFamily(fontFamily);
    titleFormat.setFontPointSize(fontSize * 2.1);
    titleFormat.setFontWeight(QFont::Bold);
    QTextBlockFormat centered;
    centered.setAlignment(Qt::AlignCenter);
    cursor.setBlockFormat(centered);
    cursor.insertText(document_.storyTitle().isEmpty() ? document_.title() : document_.storyTitle(), titleFormat);

    const QString subtitle = profile.value(QStringLiteral("subtitle")).toString();
    if (!subtitle.isEmpty()) {
        cursor.insertBlock(centered);
        QTextCharFormat subtitleFormat;
        subtitleFormat.setFontFamily(fontFamily);
        subtitleFormat.setFontPointSize(fontSize * 1.05);
        cursor.insertText(subtitle, subtitleFormat);
    }
    const QString author = profile.value(QStringLiteral("author")).toString();
    if (!author.isEmpty()) {
        cursor.insertBlock(centered);
        cursor.insertText(author, QTextCharFormat());
    }

    QTextCharFormat bodyFormat;
    bodyFormat.setFontFamily(fontFamily);
    bodyFormat.setFontPointSize(fontSize);
    QTextBlockFormat bodyBlock;
    bodyBlock.setTextIndent(indentMm * 72.0 / 25.4);
    bodyBlock.setLineHeight(lineHeight * 100.0, QTextBlockFormat::ProportionalHeight);

    const QJsonArray chapters = document_.array(QStringLiteral("writingChapters"));
    for (int c = 0; c < chapters.size(); ++c) {
        const QJsonObject chapter = chapters.at(c).toObject();
        QTextBlockFormat chapterBlock = centered;
        if (opening != QStringLiteral("Continuo") || c == 0) chapterBlock.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
        cursor.insertBlock(chapterBlock);
        QTextCharFormat chapterFormat;
        chapterFormat.setFontFamily(fontFamily);
        chapterFormat.setFontPointSize(fontSize * 1.65);
        chapterFormat.setFontWeight(QFont::Bold);
        QString heading = chapter.value(QStringLiteral("label")).toString();
        const QString chapterTitle = chapter.value(QStringLiteral("title")).toString();
        if (!chapterTitle.isEmpty()) heading += heading.isEmpty() ? chapterTitle : QStringLiteral(" — ") + chapterTitle;
        cursor.insertText(heading, chapterFormat);
        cursor.insertBlock(bodyBlock, bodyFormat);

        const QJsonArray scenes = chapter.value(QStringLiteral("scenes")).toArray();
        for (int s = 0; s < scenes.size(); ++s) {
            if (s > 0 && !separator.isEmpty()) {
                QTextBlockFormat separatorBlock;
                separatorBlock.setAlignment(Qt::AlignCenter);
                cursor.insertBlock(separatorBlock, bodyFormat);
                cursor.insertText(separator, bodyFormat);
                cursor.insertBlock(bodyBlock, bodyFormat);
            }

            const QJsonObject scene = scenes.at(s).toObject();
            const QString text = scenePlainText(scene);
            const int base = cursor.position();
            cursor.setBlockFormat(bodyBlock);
            cursor.setCharFormat(bodyFormat);
            cursor.insertText(text, bodyFormat);
            applyFormattingRuns(output, base, static_cast<int>(text.size()), scene.value(QStringLiteral("formatting")).toArray());
            cursor.movePosition(QTextCursor::End);
            cursor.insertBlock(bodyBlock, bodyFormat);
        }
    }

    output.print(&printer);
    statusBar()->showMessage(tr("PDF exportado: %1").arg(path), 6000);
}

void MainWindow::createBackup() {
    saveProject(true);
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
    const QString previous = settings.value(QStringLiteral("backupDirectory"), QString()).toString();
    QString directory = previous;
    if (directory.isEmpty() || !QDir(directory).exists()) {
        directory = QFileDialog::getExistingDirectory(this, tr("Carpeta de copias de seguridad"), QDir::homePath());
        if (directory.isEmpty()) return;
        settings.setValue(QStringLiteral("backupDirectory"), directory);
    }
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
    commitPendingEdits();
    if (!document_.isDirty()) return true;
    const QMessageBox::StandardButton answer = QMessageBox::warning(this, tr("Cambios sin guardar"), tr("Hay cambios sin guardar. ¿Quieres guardarlos antes de continuar?"), QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Cancel) return false;
    if (answer == QMessageBox::Save) return saveProject();
    return true;
}

void MainWindow::setDocument(ArchiveDocument document) {
    document_ = std::move(document);
    saveError_.clear();
    saveState_->setProperty("projectLoaded", true);
    planningPage_->setDocument(&document_);
    writingPage_->setDocument(&document_);
    worldPage_->setDocument(&document_);
    reviewPage_->setDocument(&document_);
    navigation_->setCurrentRow(1);
    routeProductModule(1);
    updateWindowTitle();
}

void MainWindow::refreshPages() {
    planningPage_->refresh();
    writingPage_->refresh();
    worldPage_->refresh();
    reviewPage_->refresh();
    updateWindowTitle();
}

void MainWindow::onDocumentChanged() { updateWindowTitle(); }

void MainWindow::updateWindowTitle() {
    const QString title = document_.storyTitle().isEmpty() ? document_.title() : document_.storyTitle();
    setWindowTitle(QStringLiteral("%1%2 — Worldbuilder Writer").arg(document_.isDirty() ? QStringLiteral("* ") : QString(), title.isEmpty() ? tr("Sin título") : title));
    if (navigation_ && navigation_->currentRow() == 0) return;
    projectTitle_->setText(title.isEmpty() ? tr("Sin título") : title);
    saveState_->setToolTip(saveError_);
    if (!saveError_.isEmpty()) saveState_->setText(tr("No se pudo guardar"));
    else if (document_.isDirty()) saveState_->setText(tr("Cambios pendientes"));
    else saveState_->setText(document_.sourcePath().isEmpty() ? tr("Sin ubicación local") : tr("Guardado local"));
}

void MainWindow::handleReference(const QString& kind, const QString& id) {
    navigation_->setCurrentRow(3);
    routeProductModule(3);
    if (kind == QStringLiteral("character")) {
        if (auto* tabs = worldPage_->findChild<QTabWidget*>(QStringLiteral("worldTabs"))) {
            if (tabs->count() > 0) tabs->setCurrentIndex(0);
        }
        return;
    }
    worldPage_->openReference(kind, id);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (confirmDiscard()) event->accept();
    else event->ignore();
}

} // namespace wbw

