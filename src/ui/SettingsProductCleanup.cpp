#include "ui/SettingsWorkspace.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QStringList>

namespace wbw {

void SettingsWorkspace::finalizeProductControls() {
    if (property("wbwProductSettingsFinalized").toBool()) return;
    setProperty("wbwProductSettingsFinalized", true);

    // Do not expose controls that merely write a preference which no production subsystem
    // consumes yet. The settings page should only promise behaviour that really exists.
    const QStringList hiddenComboFirstItems{
        tr("Azul"),
        tr("Conservador"),
        tr("Durante 30 días"),
        tr("PDF formateado"),
        tr("Rápida")
    };
    for (QComboBox* combo : findChildren<QComboBox*>()) {
        if (!combo || combo->count() == 0) continue;
        if (hiddenComboFirstItems.contains(combo->itemText(0))) combo->hide();
    }

    const QStringList hiddenChecks{
        tr("Corrección gramatical"),
        tr("Sugerencias de estilo"),
        tr("Incluir notas y referencias"),
        tr("Incluir mundo y personajes"),
        tr("Incluir portada"),
        tr("Incluir índice")
    };
    for (QCheckBox* check : findChildren<QCheckBox*>()) {
        if (check && hiddenChecks.contains(check->text())) check->hide();
    }

    const QStringList hiddenLabels{
        tr("Color de acento"),
        tr("Nivel de sugerencias"),
        tr("Conservar versiones"),
        tr("Formato por defecto"),
        tr("Calidad de render")
    };
    for (QLabel* label : findChildren<QLabel*>()) {
        if (!label) continue;
        if (hiddenLabels.contains(label->text())) label->hide();
        if (label->text() == tr("Pilín Rey")) label->setText(tr("Mapas"));
    }

    if (auto* output = findChild<QLabel*>(QStringLiteral("settingsOutputText"))) {
        output->setText(tr("Manuscrito: PDF · DOCX · ODT · EPUB · TXT · WBW\nMapas: PNG · SVG"));
        output->setWordWrap(true);
    }

    if (navigation_ && navigation_->count() >= 7) {
        navigation_->item(6)->setText(tr("Mapas\nEditor cartográfico"));
    }
}

} // namespace wbw
