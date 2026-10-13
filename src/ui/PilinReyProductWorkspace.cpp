#include "ui/PilinReyEditor.h"

#include <QFrame>
#include <QListWidget>
#include <QToolButton>
#include <QTimer>

namespace wbw {

void PilinReyEditor::configureProductWorkspace() {
    if (property("wbwProductWorkspaceV1").toBool()) {
        layoutFloatingPanels();
        return;
    }
    setProperty("wbwProductWorkspaceV1", true);

    if (toolRail_) {
        toolRail_->setMinimumWidth(54);
        toolRail_->setMaximumWidth(54);
    }
    if (topCommands_) {
        topCommands_->setMinimumHeight(46);
        topCommands_->setMaximumHeight(52);
    }

    for (QFrame* panel : {layersPopover_, assetsPopover_, templatePopover_, appearancePopover_}) {
        if (!panel) continue;
        panel->setMinimumWidth(300);
        panel->setMaximumWidth(340);
    }

    if (layers_) {
        layers_->setMinimumHeight(230);
        layers_->setAlternatingRowColors(false);
    }
    if (assets_) assets_->setMinimumHeight(230);

    // Layers are useful continuously while authoring a map, so they open with
    // the map. The existing toolbar can hide/swap them for Assets, Reference or
    // Style at any time; no permanent document-width sidebar is introduced.
    QTimer::singleShot(0, this, [this]() {
        if (layersPopover_) layersPopover_->show();
        layoutFloatingPanels();
        if (layersPopover_) layersPopover_->raise();
        if (toolRail_) toolRail_->raise();
        if (topCommands_) topCommands_->raise();
    });
}

} // namespace wbw
