#include "ui/ReviewPage.h"

#include "core/ArchiveDocument.h"
#include "export/ManuscriptExporter.h"

#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QStatusBar>
#include <QVBoxLayout>

namespace wbw {
namespace {

QString safeOutputName(QString value) {
    value = value.trimmed();
    value.replace(QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*]+")), QStringLiteral("-"));
    value.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    if (value.isEmpty()) value = QStringLiteral("worldbuilder-writer");
    return value.left(100);
}

QPushButton* exportFormatButton(const QString& text, QWidget* parent) {
    auto* button = new QPushButton(text, parent);
    button->setObjectName(QStringLiteral("reviewExportButton"));
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

} // namespace

void ReviewPage::installManuscriptExportActions() {
    applyApprovedReviewChrome();
    if (property("wbwNativeManuscriptExports").toBool()) return;

    auto* rail = findChild<QFrame*>(QStringLiteral("reviewExportRail"));
    auto* railLayout = rail ? qobject_cast<QVBoxLayout*>(rail->layout()) : nullptr;
    if (!rail || !railLayout) return;

    auto* formats = new QFrame(rail);
    formats->setObjectName(QStringLiteral("reviewExportFormats"));
    auto* grid = new QGridLayout(formats);
    grid->setContentsMargins(0, 3, 0, 3);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);

    auto* label = new QLabel(tr("FORMATOS EDITABLES"), formats);
    label->setObjectName(QStringLiteral("reviewExportFormatsLabel"));
    grid->addWidget(label, 0, 0, 1, 2);

    auto* txt = exportFormatButton(tr("TXT"), formats);
    auto* docx = exportFormatButton(tr("DOCX"), formats);
    auto* odt = exportFormatButton(tr("ODT"), formats);
    auto* epub = exportFormatButton(tr("EPUB"), formats);
    grid->addWidget(txt, 1, 0);
    grid->addWidget(docx, 1, 1);
    grid->addWidget(odt, 2, 0);
    grid->addWidget(epub, 2, 1);

    QPushButton* backupButton = nullptr;
    for (QPushButton* button : rail->findChildren<QPushButton*>(QString(), Qt::FindDirectChildrenOnly)) {
        if (button->text() == tr("Crear copia")) {
            backupButton = button;
            break;
        }
    }
    const int insertAt = backupButton ? railLayout->indexOf(backupButton) : qMax(0, railLayout->count() - 1);
    railLayout->insertWidget(insertAt, formats);

    const auto exportDocument = [this](ManuscriptExporter::Format format,
                                       const QString& extension,
                                       const QString& filter,
                                       const QString& labelText) {
        if (!document_) return;
        const QString base = safeOutputName(document_->storyTitle().isEmpty() ? document_->title() : document_->storyTitle());
        QString path = QFileDialog::getSaveFileName(this,
                                                     tr("Exportar manuscrito %1").arg(labelText),
                                                     base + extension,
                                                     filter);
        if (path.isEmpty()) return;
        if (!path.endsWith(extension, Qt::CaseInsensitive)) path += extension;

        QString error;
        if (!ManuscriptExporter::exportDocument(format, path, *document_, &error)) {
            QMessageBox::critical(this,
                                  tr("No se pudo exportar"),
                                  error.isEmpty() ? tr("No se pudo crear el archivo de manuscrito.") : error);
            return;
        }

        if (auto* mainWindow = qobject_cast<QMainWindow*>(window()))
            mainWindow->statusBar()->showMessage(tr("Manuscrito exportado: %1").arg(path), 6000);
    };

    connect(txt, &QPushButton::clicked, this, [exportDocument]() {
        exportDocument(ManuscriptExporter::Format::Txt, QStringLiteral(".txt"), QObject::tr("Texto (*.txt)"), QObject::tr("TXT"));
    });
    connect(docx, &QPushButton::clicked, this, [exportDocument]() {
        exportDocument(ManuscriptExporter::Format::Docx, QStringLiteral(".docx"), QObject::tr("Microsoft Word (*.docx)"), QObject::tr("DOCX"));
    });
    connect(odt, &QPushButton::clicked, this, [exportDocument]() {
        exportDocument(ManuscriptExporter::Format::Odt, QStringLiteral(".odt"), QObject::tr("OpenDocument Text (*.odt)"), QObject::tr("ODT"));
    });
    connect(epub, &QPushButton::clicked, this, [exportDocument]() {
        exportDocument(ManuscriptExporter::Format::Epub, QStringLiteral(".epub"), QObject::tr("EPUB (*.epub)"), QObject::tr("EPUB"));
    });

    setProperty("wbwNativeManuscriptExports", true);
    setStyleSheet(styleSheet() + QStringLiteral(R"QSS(
#reviewExportFormats{background:transparent;border:0;}
#reviewExportFormatsLabel{color:#74818c;font-size:7.5pt;font-weight:700;letter-spacing:.7px;padding-top:3px;}
#reviewExportFormats #reviewExportButton{padding:8px 7px;}
)QSS"));
}

} // namespace wbw
