#include "ui/PilinReyEditor.h"

#include <QFrame>
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
    QSpinBox* continents = nullptr;
    QSpinBox* islands = nullptr;
    for (QSpinBox* spin : popover->findChildren<QSpinBox*>()) {
        if (spin->maximum() > 1000000) seed = spin;
        else if (spin->minimum() == 1 && spin->maximum() <= 10) continents = spin;
        else if (spin->minimum() == 0 && spin->maximum() >= 20) islands = spin;
    }

    QSlider* roughness = nullptr;
    QSlider* seaLevel = nullptr;
    for (QSlider* slider : popover->findChildren<QSlider*>()) {
        if (slider->minimum() >= 25 && slider->maximum() >= 75) roughness = slider;
        if (slider->minimum() >= 20 && slider->maximum() <= 65) seaLevel = slider;
    }

    QObject::disconnect(buildButton, nullptr, this, nullptr);
    connect(buildButton, &QPushButton::clicked, this,
        [this, popover, seed, continents, islands, roughness, seaLevel]() {
            if (seed) setProperty("wbwGeneratorSeed", seed->value());
            if (continents) setProperty("wbwGeneratorContinents", continents->value());
            if (islands) setProperty("wbwGeneratorIslands", islands->value());
            if (roughness) setProperty("wbwGeneratorRoughness", roughness->value());
            if (seaLevel) setProperty("wbwGeneratorSeaLevel", seaLevel->value());
            generateProceduralWorld();
            popover->hide();
        });

    if (auto* layout = qobject_cast<QVBoxLayout*>(popover->layout())) {
        auto* recalc = new QPushButton(tr("Recalcular geografía desde el terreno"), popover);
        recalc->setObjectName(QStringLiteral("pilinRecalculateGeography"));
        recalc->setToolTip(tr("Conserva el heightmap que editaste y vuelve a derivar biomas e hidrología."));
        layout->addWidget(recalc);
        connect(recalc, &QPushButton::clicked, this, [this, popover]() {
            recalculateDerivedGeography();
            popover->hide();
        });
    }

    setProperty("wbwAzgaarGeneratorConnected", true);
}

} // namespace wbw
