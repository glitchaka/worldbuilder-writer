#include "ui/PilinReyEditor.h"

#include <QComboBox>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace wbw {

void PilinReyEditor::installAzgaarStudioControls() {
    if (property("wbwAzgaarStudioControls").toBool()) return;

    auto* left = findChild<QFrame*>(QStringLiteral("pilinStudioLeft"));
    auto* leftLayout = left ? qobject_cast<QVBoxLayout*>(left->layout()) : nullptr;
    if (!left || !leftLayout) return;

    setProperty("wbwAzgaarStudioControls", true);

    auto* card = new QFrame(left);
    card->setObjectName(QStringLiteral("pilinAzgaarStudioCard"));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 9, 10, 10);
    layout->setSpacing(7);

    auto* kicker = new QLabel(tr("GENERACIÓN AZGAAR"), card);
    kicker->setObjectName(QStringLiteral("pilinStudioSection"));
    auto* copy = new QLabel(tr("Crea una geografía base y sigue modelándola con los mismos pinceles del editor."), card);
    copy->setObjectName(QStringLiteral("pilinAzgaarStudioCopy"));
    copy->setWordWrap(true);
    layout->addWidget(kicker);
    layout->addWidget(copy);

    auto* shape = new QComboBox(card);
    shape->setObjectName(QStringLiteral("pilinAzgaarStudioTemplate"));
    shape->addItem(tr("Continentes"), QStringLiteral("continents"));
    shape->addItem(tr("Archipiélago"), QStringLiteral("archipelago"));
    shape->addItem(tr("Isla alta"), QStringLiteral("highisland"));
    shape->addItem(tr("Isla baja"), QStringLiteral("lowisland"));
    shape->addItem(tr("Volcán"), QStringLiteral("volcano"));
    shape->addItem(tr("Atolón"), QStringLiteral("atoll"));
    shape->addItem(tr("Mediterráneo"), QStringLiteral("mediterranean"));
    shape->addItem(tr("Península"), QStringLiteral("peninsula"));
    shape->addItem(tr("Pangea"), QStringLiteral("pangea"));
    shape->addItem(tr("Istmo"), QStringLiteral("isthmus"));
    shape->addItem(tr("Fragmentado"), QStringLiteral("shattered"));
    shape->addItem(tr("Cuenca cerrada"), QStringLiteral("taklamakan"));
    shape->addItem(tr("Viejo Mundo"), QStringLiteral("oldworld"));
    shape->addItem(tr("Fracturado"), QStringLiteral("fractious"));
    const QString savedTemplate = property("wbwGeneratorTemplate").toString();
    const int savedIndex = shape->findData(savedTemplate);
    shape->setCurrentIndex(savedIndex >= 0 ? savedIndex : 0);
    layout->addWidget(shape);

    auto* actions = new QHBoxLayout;
    actions->setContentsMargins(0, 0, 0, 0);
    actions->setSpacing(6);
    auto* generate = new QPushButton(tr("Generar base"), card);
    generate->setObjectName(QStringLiteral("pilinAzgaarStudioGenerate"));
    generate->setCursor(Qt::PointingHandCursor);
    generate->setToolTip(tr("Genera un heightmap editable y deriva clima, hidrología, biomas, regiones, asentamientos y rutas."));
    auto* advanced = new QPushButton(tr("Ajustes…"), card);
    advanced->setObjectName(QStringLiteral("pilinAzgaarStudioAdvanced"));
    advanced->setCursor(Qt::PointingHandCursor);
    actions->addWidget(generate, 1);
    actions->addWidget(advanced);
    layout->addLayout(actions);

    auto* pipeline = new QLabel(tr("heightmap → clima → agua → biomas → regiones"), card);
    pipeline->setObjectName(QStringLiteral("pilinAzgaarStudioPipeline"));
    pipeline->setWordWrap(true);
    layout->addWidget(pipeline);

    leftLayout->insertWidget(2, card);

    auto* popup = findChild<QFrame*>(QStringLiteral("pilinGeneratorPopover"));
    auto* popupTemplate = popup ? popup->findChild<QComboBox*>(QStringLiteral("pilinAzgaarTemplate")) : nullptr;

    connect(shape, &QComboBox::currentIndexChanged, this, [this, shape, popupTemplate](int) {
        const QString id = shape->currentData().toString();
        setProperty("wbwGeneratorTemplate", id);
        if (popupTemplate) {
            const int index = popupTemplate->findData(id);
            if (index >= 0 && popupTemplate->currentIndex() != index) {
                const QSignalBlocker blocker(popupTemplate);
                popupTemplate->setCurrentIndex(index);
            }
        }
    });

    if (popupTemplate) {
        connect(popupTemplate, &QComboBox::currentIndexChanged, this, [this, shape, popupTemplate](int) {
            const QString id = popupTemplate->currentData().toString();
            setProperty("wbwGeneratorTemplate", id);
            const int index = shape->findData(id);
            if (index >= 0 && shape->currentIndex() != index) {
                const QSignalBlocker blocker(shape);
                shape->setCurrentIndex(index);
            }
        });
    }

    connect(generate, &QPushButton::clicked, this, [this, shape]() {
        if (!property("wbwGeneratorSeed").isValid() || property("wbwGeneratorSeed").toInt() <= 0) {
            const int mapSeed = map_.value(QStringLiteral("seed")).toInt();
            setProperty("wbwGeneratorSeed", mapSeed > 0
                ? mapSeed
                : static_cast<int>(QDateTime::currentSecsSinceEpoch() & 0x7fffffff));
        }
        const QString id = shape->currentData().toString();
        setProperty("wbwGeneratorTemplate", id);
        generateAzgaarTemplateWorld(id);
    });

    connect(advanced, &QPushButton::clicked, this, [this, popup]() {
        if (!popup) return;
        popup->show();
        popup->raise();
        layoutFloatingPanels();
    });

    setStyleSheet(styleSheet() + QStringLiteral(R"QSS(
#pilinAzgaarStudioCard{background:#101820;border:1px solid #2b3945;border-radius:9px;}
#pilinAzgaarStudioCopy,#pilinAzgaarStudioPipeline{color:#7e8b96;font-size:7.8pt;}
#pilinAzgaarStudioTemplate{background:#0c131a;color:#dbe3e9;border:1px solid #2c3945;border-radius:7px;padding:6px 8px;}
#pilinAzgaarStudioGenerate{background:#315f7e;color:#fff;border:1px solid #477c9e;border-radius:7px;padding:7px 8px;font-weight:700;}
#pilinAzgaarStudioAdvanced{background:#121b23;color:#aeb9c3;border:1px solid #2e3a46;border-radius:7px;padding:7px 8px;}
#pilinAzgaarStudioGenerate:hover{background:#3b7093;}
#pilinAzgaarStudioAdvanced:hover{background:#1a2731;color:#fff;}
)QSS"));
}

} // namespace wbw
