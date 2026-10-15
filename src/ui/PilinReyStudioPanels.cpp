#include "ui/PilinReyEditor.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QSettings>
#include <QToolButton>

namespace wbw {

void PilinReyEditor::installWorkspacePanelControls() {
    if (property("wbwStudioPanelControls").toBool()) return;

    auto* toolbar = findChild<QFrame*>(QStringLiteral("pilinStudioToolbar"));
    auto* toolsPanel = findChild<QWidget*>(QStringLiteral("pilinStudioLeft"));
    auto* libraryPanel = findChild<QWidget*>(QStringLiteral("pilinStudioRight"));
    auto* toolbarLayout = toolbar ? qobject_cast<QHBoxLayout*>(toolbar->layout()) : nullptr;
    if (!toolbar || !toolsPanel || !libraryPanel || !toolbarLayout) return;

    setProperty("wbwStudioPanelControls", true);
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
    const QString migratedKey = QStringLiteral("map/studioPanelDefaultsV4Applied");
    if (!settings.value(migratedKey, false).toBool()) {
        settings.setValue(QStringLiteral("map/toolsPanelVisible"), true);
        settings.setValue(QStringLiteral("map/libraryPanelVisible"), true);
        settings.setValue(migratedKey, true);
        settings.sync();
    }
    const bool toolsVisible = settings.value(QStringLiteral("map/toolsPanelVisible"), true).toBool();
    const bool libraryVisible = settings.value(QStringLiteral("map/libraryPanelVisible"), true).toBool();

    auto* toolsToggle = new QToolButton(toolbar);
    toolsToggle->setObjectName(QStringLiteral("pilinStudioPanelToggle"));
    toolsToggle->setText(tr("Herramientas"));
    toolsToggle->setToolTip(tr("Mostrar u ocultar pinceles y terreno"));
    toolsToggle->setCheckable(true);
    toolsToggle->setChecked(toolsVisible);
    toolsToggle->setCursor(Qt::PointingHandCursor);

    auto* libraryToggle = new QToolButton(toolbar);
    libraryToggle->setObjectName(QStringLiteral("pilinStudioPanelToggle"));
    libraryToggle->setText(tr("Capas y assets"));
    libraryToggle->setToolTip(tr("Mostrar u ocultar capas y biblioteca cartográfica"));
    libraryToggle->setCheckable(true);
    libraryToggle->setChecked(libraryVisible);
    libraryToggle->setCursor(Qt::PointingHandCursor);

    toolbarLayout->insertWidget(0, toolsToggle);
    toolbarLayout->insertWidget(1, libraryToggle);
    toolsPanel->setVisible(toolsVisible);
    libraryPanel->setVisible(libraryVisible);

    connect(toolsToggle, &QToolButton::toggled, this, [toolsPanel](bool visible) {
        toolsPanel->setVisible(visible);
        QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
        settings.setValue(QStringLiteral("map/toolsPanelVisible"), visible);
    });
    connect(libraryToggle, &QToolButton::toggled, this, [libraryPanel](bool visible) {
        libraryPanel->setVisible(visible);
        QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
        settings.setValue(QStringLiteral("map/libraryPanelVisible"), visible);
    });

    setStyleSheet(styleSheet() + QStringLiteral(R"QSS(
QToolButton#pilinStudioPanelToggle{background:transparent;color:#87949f;border:1px solid transparent;border-radius:7px;padding:6px 9px;}
QToolButton#pilinStudioPanelToggle:hover{background:#17212a;color:#eef2f4;}
QToolButton#pilinStudioPanelToggle:checked{background:#142833;color:#9bd9e7;border-color:#31576a;}
)QSS"));
}

} // namespace wbw
