#include "ui/PilinReyEditor.h"

#include <QCheckBox>
#include <QSettings>

namespace wbw {

void PilinReyEditor::applyPersistentCanvasPreferences() {
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
    const bool snap = settings.value(QStringLiteral("map/defaultSnap"), true).toBool();
    const bool grid = settings.value(QStringLiteral("map/defaultGrid"), false).toBool();

    if (snapCheck_ && snapCheck_->isChecked() != snap) snapCheck_->setChecked(snap);
    if (gridCheck_ && gridCheck_->isChecked() != grid) gridCheck_->setChecked(grid);
}

} // namespace wbw
