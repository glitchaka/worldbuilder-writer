#include "ui/PilinReyEditor.h"

#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>

namespace wbw {
namespace {

class AzgaarGeneratorBridge final : public QObject {
public:
    explicit AzgaarGeneratorBridge(QObject* parent = nullptr) : QObject(parent) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() != QEvent::Polish && event->type() != QEvent::Show) return QObject::eventFilter(watched, event);
        auto* editor = qobject_cast<PilinReyEditor*>(watched);
        if (!editor || editor->property("wbwAzgaarGeneratorConnected").toBool()) return QObject::eventFilter(watched, event);

        QTimer::singleShot(0, editor, [editor]() {
            if (!editor || editor->property("wbwAzgaarGeneratorConnected").toBool()) return;
            auto* popover = editor->findChild<QFrame*>(QStringLiteral("pilinGeneratorPopover"));
            if (!popover) return;

            QPushButton* buildButton = nullptr;
            for (QPushButton* button : popover->findChildren<QPushButton*>()) {
                if (button->text().contains(QObject::tr("Generar mundo"), Qt::CaseInsensitive)) {
                    buildButton = button;
                    break;
                }
            }
            if (!buildButton) return;

            QList<QSpinBox*> spins = popover->findChildren<QSpinBox*>();
            QSpinBox* seed = nullptr;
            QSpinBox* continents = nullptr;
            QSpinBox* islands = nullptr;
            for (QSpinBox* spin : spins) {
                if (spin->maximum() > 1000000) seed = spin;
                else if (spin->minimum() == 1 && spin->maximum() <= 10) continents = spin;
                else if (spin->minimum() == 0 && spin->maximum() >= 20) islands = spin;
            }

            QList<QSlider*> sliders = popover->findChildren<QSlider*>();
            QSlider* roughness = nullptr;
            QSlider* seaLevel = nullptr;
            for (QSlider* slider : sliders) {
                if (slider->minimum() >= 25 && slider->maximum() >= 75) roughness = slider;
                if (slider->minimum() >= 20 && slider->maximum() <= 65) seaLevel = slider;
            }

            QObject::disconnect(buildButton, nullptr, editor, nullptr);
            QObject::connect(buildButton, &QPushButton::clicked, editor,
                [editor, popover, seed, continents, islands, roughness, seaLevel]() {
                    if (seed) editor->setProperty("wbwGeneratorSeed", seed->value());
                    if (continents) editor->setProperty("wbwGeneratorContinents", continents->value());
                    if (islands) editor->setProperty("wbwGeneratorIslands", islands->value());
                    if (roughness) editor->setProperty("wbwGeneratorRoughness", roughness->value());
                    if (seaLevel) editor->setProperty("wbwGeneratorSeaLevel", seaLevel->value());
                    editor->generateProceduralWorld();
                    popover->hide();
                });

            editor->setProperty("wbwAzgaarGeneratorConnected", true);
        });
        return QObject::eventFilter(watched, event);
    }
};

void installAzgaarGeneratorBridge() {
    if (!qApp) return;
    qApp->installEventFilter(new AzgaarGeneratorBridge(qApp));
}

} // namespace
} // namespace wbw

Q_COREAPP_STARTUP_FUNCTION(wbw::installAzgaarGeneratorBridge)
