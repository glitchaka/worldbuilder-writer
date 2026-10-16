#include "ui/SettingsWorkspace.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QSpinBox>
#include <QStringList>
#include <QVBoxLayout>

namespace wbw {
namespace {

QFrame* cardForTitle(QWidget* root, const QString& title) {
    if (!root) return nullptr;
    for (QLabel* label : root->findChildren<QLabel*>(QStringLiteral("settingsCardTitle"))) {
        if (label && label->text() == title) return qobject_cast<QFrame*>(label->parentWidget());
    }
    return nullptr;
}

void replaceCopy(QWidget* root, const QString& from, const QString& to) {
    for (QLabel* label : root->findChildren<QLabel*>()) {
        if (label && label->text() == from) label->setText(to);
    }
}

} // namespace

void SettingsWorkspace::finalizeProductControls() {
    if (property("wbwProductSettingsFinalized").toBool()) return;
    setProperty("wbwProductSettingsFinalized", true);

    // Never expose preferences that no production subsystem consumes. Hiding a
    // control is preferable to offering a switch that has no effect.
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

    replaceCopy(this,
                tr("Ortografía, gramática y estilo sin reescritura automática."),
                tr("Corrector ortográfico integrado y no destructivo. Cada sustitución requiere una acción explícita."));
    replaceCopy(this,
                tr("Idioma y nivel de asistencia del corrector integrado."),
                tr("Idioma utilizado por el corrector ortográfico integrado."));
    replaceCopy(this,
                tr("Las correcciones permanecen no destructivas hasta que confirmas cada cambio."),
                tr("Las observaciones se muestran en contexto y no modifican el manuscrito por sí solas."));
    replaceCopy(this,
                tr("Control del autoguardado y destino de versiones locales."),
                tr("Estado del autoguardado y destino real de las copias WBW locales."));
    replaceCopy(this,
                tr("Preferencias de salida del manuscrito y de los mapas."),
                tr("Formatos de salida implementados para manuscrito, proyecto y cartografía."));

    if (auto* output = findChild<QLabel*>(QStringLiteral("settingsOutputText"))) {
        output->setText(tr("Manuscrito: PDF · DOCX · ODT · EPUB · TXT · WBW\nMapas: PNG · SVG"));
        output->setWordWrap(true);
    }

    // The dedicated backup page used to become an empty card after removing
    // non-functional retention options. Replace them with live information from
    // the controls that really drive autosave and local backups.
    if (QFrame* backupCard = cardForTitle(this, tr("Versionado"))) {
        for (QLabel* title : backupCard->findChildren<QLabel*>(QStringLiteral("settingsCardTitle")))
            title->setText(tr("Respaldo local"));
        replaceCopy(backupCard,
                    tr("Los respaldos contienen el proyecto completo, no solo el manuscrito."),
                    tr("Cada copia contiene el proyecto completo en WBW: manuscrito, Atlas, personajes, notas, adjuntos y mapas."));
        if (auto* box = qobject_cast<QVBoxLayout*>(backupCard->layout())) {
            auto* autosave = new QLabel(backupCard);
            autosave->setObjectName(QStringLiteral("settingsOutputText"));
            auto* destination = new QLabel(backupCard);
            destination->setObjectName(QStringLiteral("settingsOutputText"));
            destination->setWordWrap(true);
            const auto refreshAutosave = [this, autosave]() {
                autosave->setText(tr("Autoguardado: cada %1 segundos").arg(autosaveSeconds_ ? autosaveSeconds_->value() : 15));
            };
            const auto refreshDestination = [this, destination]() {
                const QString path = backupDirectory_ ? backupDirectory_->text().trimmed() : QString();
                destination->setText(path.isEmpty()
                    ? tr("Destino de copias: se solicitará al crear la primera copia.")
                    : tr("Destino de copias: %1").arg(path));
            };
            refreshAutosave();
            refreshDestination();
            box->addWidget(autosave);
            box->addWidget(destination);
            if (autosaveSeconds_) connect(autosaveSeconds_, &QSpinBox::valueChanged, this, [refreshAutosave](int) { refreshAutosave(); });
            if (backupDirectory_) connect(backupDirectory_, &QLineEdit::textChanged, this, [refreshDestination](const QString&) { refreshDestination(); });
        }
    }

    // The export page now states only outputs backed by working exporters.
    if (QFrame* exportCard = cardForTitle(this, tr("Manuscrito"))) {
        replaceCopy(exportCard,
                    tr("Configuración por defecto al exportar desde Revisión."),
                    tr("Los formatos se eligen en Revisión al momento de exportar; no existe un selector global ficticio."));
        if (auto* box = qobject_cast<QVBoxLayout*>(exportCard->layout())) {
            auto* formats = new QLabel(tr("PDF · DOCX · ODT · EPUB · TXT · WBW"), exportCard);
            formats->setObjectName(QStringLiteral("settingsOutputText"));
            formats->setWordWrap(true);
            box->addWidget(formats);
        }
    }

    if (navigation_ && navigation_->count() >= 7) {
        navigation_->item(2)->setText(tr("Corrector\nOrtografía e idioma"));
        navigation_->item(3)->setText(tr("Copias de seguridad\nAutoguardado y destino local"));
        navigation_->item(5)->setText(tr("Exportación\nFormatos disponibles"));
        navigation_->item(6)->setText(tr("Mapas\nEditor cartográfico"));
    }
}

} // namespace wbw
