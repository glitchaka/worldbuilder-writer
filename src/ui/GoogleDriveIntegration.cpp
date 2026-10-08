#include "ui/GoogleDriveIntegration.h"

#include "storage/ProjectStore.h"
#include "storage/WbwPackage.h"
#include "ui/GoogleDriveDialog.h"
#include "ui/MainWindow.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QRegularExpression>
#include <QStatusBar>
#include <QTemporaryFile>
#include <QUuid>

namespace wbw {
namespace {

QString safeProjectId(QString value) {
    value = value.trimmed();
    if (!QRegularExpression(QStringLiteral("^[A-Za-z0-9._-]{1,120}$")).match(value).hasMatch()) {
        value = QStringLiteral("project-") + QUuid::createUuid().toString(QUuid::Id128);
    }
    return value;
}

} // namespace

void installGoogleDriveIntegration(MainWindow* window) {
    if (!window) return;
    QMenu* fileMenu = nullptr;
    for (QAction* action : window->menuBar()->actions()) {
        if (action->menu() && action->text().contains(QObject::tr("Archivo"), Qt::CaseInsensitive)) {
            fileMenu = action->menu();
            break;
        }
    }
    if (!fileMenu) fileMenu = window->menuBar()->addMenu(QObject::tr("Archivo"));

    auto* driveAction = new QAction(QObject::tr("Google Drive: respaldo y restauración…"), window);
    const QList<QAction*> actions = fileMenu->actions();
    QAction* before = actions.isEmpty() ? nullptr : actions.last();
    if (before) fileMenu->insertAction(before, driveAction);
    else fileMenu->addAction(driveAction);

    QObject::connect(driveAction, &QAction::triggered, window, [window]() {
        GoogleDriveDialog dialog(window);
        QObject::connect(&dialog, &GoogleDriveDialog::restorePackageReady, window,
            [window](const QByteArray& bytes, const QString& requestedProjectId) {
                QTemporaryFile temporary(QDir::tempPath() + QStringLiteral("/WorldbuilderWriterRestore-XXXXXX.wbw"));
                temporary.setAutoRemove(true);
                if (!temporary.open() || temporary.write(bytes) != bytes.size()) {
                    QMessageBox::critical(window, QObject::tr("No se pudo restaurar"), QObject::tr("No se pudo crear el paquete temporal de restauración."));
                    return;
                }
                temporary.flush();
                const QString tempPath = temporary.fileName();
                temporary.close();

                ArchiveDocument restored;
                QString error;
                if (!WbwPackage::importPackage(tempPath, restored, &error)) {
                    QMessageBox::critical(window, QObject::tr("No se pudo restaurar"), error);
                    return;
                }

                const QString projectId = safeProjectId(requestedProjectId);
                const QString directory = QDir(ProjectStore::defaultRoot()).filePath(projectId);
                QDir().mkpath(directory);
                if (!ProjectStore::saveIntoProjectDirectory(directory, restored, &error)) {
                    QMessageBox::critical(window, QObject::tr("No se pudo restaurar"), error);
                    return;
                }
                window->replaceDocument(std::move(restored));
                window->statusBar()->showMessage(QObject::tr("Proyecto restaurado desde Google Drive."), 7000);
            });
        dialog.exec();
    });
}

} // namespace wbw
