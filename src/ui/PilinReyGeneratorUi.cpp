#include "ui/PilinReyEditor.h"

#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace wbw {

void PilinReyEditor::activateAzgaarGenerationPipeline() {
    if (property("wbwAzgaarGeneratorConnected").toBool()) return;

    auto* popover = findChild<QFrame*>(QStringLiteral("pilinGeneratorPopover"));
    if (!popover) return;

    QPushButton* buildButton = nullptr;
    for (QPushButton* button : popover->findChildren<QPushButton*>()) {
        if (button->text().contains(tr("Generar mundo"), Qt::CaseInsensitive)) {
            buildButton = button;
            break;
        }
    }
    if (!buildButton) return;

    QSpinBox* seed = nullptr;
    QSpinBox* politicalCenters = nullptr;
    QSpinBox* settlementDensity = nullptr;
    for (QSpinBox* spin : popover->findChildren<QSpinBox*>()) {
        if (spin->maximum() > 1000000) seed = spin;
        else if (spin->minimum() == 1 && spin->maximum() <= 10) politicalCenters = spin;
        else if (spin->minimum() == 0 && spin->maximum() >= 20) settlementDensity = spin;
    }

    QSlider* roughness = nullptr;
    QSlider* seaLevel = nullptr;
    for (QSlider* slider : popover->findChildren<QSlider*>()) {
        if (slider->minimum() >= 25 && slider->maximum() >= 75) roughness = slider;
        if (slider->minimum() >= 20 && slider->maximum() <= 65) seaLevel = slider;
    }

    // The old controls were named for the superseded custom generator. In the
    // Azgaar pipeline the physical landmass shape comes from the selected
    // heightmap template; these two values drive political and settlement
    // generation. Rename the labels instead of pretending they still control
    // continent/island geometry.
    for (QFormLayout* form : popover->findChildren<QFormLayout*>()) {
        if (politicalCenters) {
            if (auto* label = qobject_cast<QLabel*>(form->labelForField(politicalCenters)))
                label->setText(tr("Centros políticos"));
        }
        if (settlementDensity) {
            if (auto* label = qobject_cast<QLabel*>(form->labelForField(settlementDensity)))
                label->setText(tr("Densidad urbana"));
        }
    }

    auto* templateChoice = new QComboBox(popover);
    templateChoice->setObjectName(QStringLiteral("pilinAzgaarTemplate"));
    templateChoice->addItem(tr("Continentes"), QStringLiteral("continents"));
    templateChoice->addItem(tr("Archipiélago"), QStringLiteral("archipelago"));
    templateChoice->addItem(tr("Isla alta"), QStringLiteral("highisland"));
    templateChoice->addItem(tr("Isla baja"), QStringLiteral("lowisland"));
    templateChoice->addItem(tr("Volcán"), QStringLiteral("volcano"));
    templateChoice->addItem(tr("Atolón"), QStringLiteral("atoll"));
    templateChoice->addItem(tr("Mediterráneo"), QStringLiteral("mediterranean"));
    templateChoice->addItem(tr("Península"), QStringLiteral("peninsula"));
    templateChoice->addItem(tr("Pangea"), QStringLiteral("pangea"));
    templateChoice->addItem(tr("Istmo"), QStringLiteral("isthmus"));
    templateChoice->addItem(tr("Fragmentado"), QStringLiteral("shattered"));
    templateChoice->addItem(tr("Cuenca cerrada"), QStringLiteral("taklamakan"));
    templateChoice->addItem(tr("Viejo Mundo"), QStringLiteral("oldworld"));
    templateChoice->addItem(tr("Fracturado"), QStringLiteral("fractious"));

    if (auto* layout = qobject_cast<QVBoxLayout*>(popover->layout())) {
        auto* source = new QLabel(
            tr("Motor nativo adaptado de Azgaar/Fantasy-Map-Generator (MIT). La forma base crea el heightmap; relieve, nivel del mar, centros políticos y densidad urbana modifican después el mundo editable."),
            popover);
        source->setObjectName(QStringLiteral("pilinAzgaarSource"));
        source->setWordWrap(true);
        layout->insertWidget(1, source);

        auto* templateRow = new QWidget(popover);
        templateRow->setObjectName(QStringLiteral("pilinAzgaarTemplateRow"));
        auto* row = new QHBoxLayout(templateRow);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(9);
        auto* label = new QLabel(tr("Forma base"), templateRow);
        label->setObjectName(QStringLiteral("pilinAzgaarTemplateLabel"));
        row->addWidget(label);
        row->addWidget(templateChoice, 1);
        layout->insertWidget(2, templateRow);
    }

    buildButton->setText(tr("Generar con Azgaar"));
    buildButton->setToolTip(tr("Ejecuta el pipeline nativo: heightmap → relieve → clima → hidrología → biomas → regiones → asentamientos → rutas."));

    QObject::disconnect(buildButton, nullptr, this, nullptr);
    connect(buildButton, &QPushButton::clicked, this,
        [this, popover, seed, politicalCenters, settlementDensity, roughness, seaLevel, templateChoice]() {
            if (seed) setProperty("wbwGeneratorSeed", seed->value());
            if (politicalCenters) setProperty("wbwGeneratorContinents", politicalCenters->value());
            if (settlementDensity) setProperty("wbwGeneratorIslands", settlementDensity->value());
            if (roughness) setProperty("wbwGeneratorRoughness", roughness->value());
            if (seaLevel) setProperty("wbwGeneratorSeaLevel", seaLevel->value());
            const QString templateId = templateChoice->currentData().toString();
            setProperty("wbwGeneratorTemplate", templateId);
            generateAzgaarTemplateWorld(templateId);
            popover->hide();
        });

    if (auto* layout = qobject_cast<QVBoxLayout*>(popover->layout())) {
        auto* recalc = new QPushButton(tr("Recalcular geografía desde el terreno"), popover);
        recalc->setObjectName(QStringLiteral("pilinRecalculateGeography"));
        recalc->setToolTip(tr("Conserva el heightmap que editaste y vuelve a derivar clima, biomas e hidrología."));
        layout->addWidget(recalc);
        connect(recalc, &QPushButton::clicked, this, [this, popover]() {
            recalculateDerivedGeography();
            popover->hide();
        });
    }

    setProperty("wbwAzgaarGeneratorConnected", true);
}

} // namespace wbw
