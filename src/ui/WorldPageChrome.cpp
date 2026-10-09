#include "ui/WorldPage.h"

#include "core/ArchiveDocument.h"
#include "ui/PilinReyEditor.h"

#include <QComboBox>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRandomGenerator>
#include <QShowEvent>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

#include <cmath>

namespace wbw {

void WorldPage::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    applyAtlasChrome();
    applyMapsChrome();
}

void WorldPage::applyAtlasChrome() {
    if (atlasChromeApplied_) return;
    auto* editor = findChild<QWidget*>(QStringLiteral("worldEditorCard"));
    if (!editor) return;
    auto* editorLayout = qobject_cast<QVBoxLayout*>(editor->layout());
    if (!editorLayout) return;

    auto* sections = new QTabWidget(editor);
    sections->setObjectName(QStringLiteral("atlasSections"));

    auto makePage = [sections](const QString& name) {
        auto* page = new QWidget(sections);
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(10, 12, 10, 12);
        layout->setSpacing(10);
        layout->addStretch(1);
        sections->addTab(page, name);
        return page;
    };

    QWidget* overview = makePage(tr("Esencia"));
    QWidget* territory = makePage(tr("Territorio"));
    QWidget* society = makePage(tr("Sociedad"));
    QWidget* history = makePage(tr("Historia"));
    QWidget* notes = makePage(tr("Notas"));

    auto targetFor = [&](const QString& title) -> QWidget* {
        if (title == tr("Tipo") || title == tr("Nombre") || title == tr("Alias") || title == tr("Resumen")) return overview;
        if (title == tr("Geografía") || title == tr("Localizaciones")) return territory;
        if (title == tr("Gobierno") || title == tr("Pueblos") || title == tr("Cultura") || title == tr("Economía") || title == tr("Moneda") || title == tr("Idiomas") || title == tr("Religiones") || title == tr("Fuerza militar")) return society;
        if (title == tr("Historia") || title == tr("Relaciones") || title == tr("Conflictos")) return history;
        return notes;
    };

    const auto labels = editor->findChildren<QLabel*>();
    for (QLabel* label : labels) {
        if (!label || label->objectName() != QStringLiteral("fieldTitle")) continue;
        QWidget* block = label->parentWidget();
        if (!block || block == editor) continue;
        QWidget* target = targetFor(label->text());
        auto* layout = target ? qobject_cast<QVBoxLayout*>(target->layout()) : nullptr;
        if (!layout) continue;
        block->setParent(target);
        layout->insertWidget(qMax(0, layout->count() - 1), block);
    }

    editorLayout->insertWidget(1, sections, 1);
    editorLayout->setSpacing(8);
    atlasChromeApplied_ = true;
    setStyleSheet(styleSheet() + QStringLiteral(
        "#atlasSections::pane{border:0;background:transparent;}"
        "#atlasSections QTabBar::tab{padding:7px 10px;}"
    ));
}

void WorldPage::applyMapsChrome() {
    if (mapsChromeApplied_ || !mapList_ || !mapCanvas_) return;

    auto* mapsTab = findChild<QWidget*>(QStringLiteral("mapsTab"));
    auto* center = findChild<QWidget*>(QStringLiteral("mapEditorCard"));
    auto* left = findChild<QWidget*>(QStringLiteral("mapIndexPanel"));
    auto* right = findChild<QWidget*>(QStringLiteral("legacyMarkers"));
    auto* oldGenerator = findChild<QFrame*>(QStringLiteral("generatorBar"));
    if (!mapsTab || !center) return;

    mapsChromeApplied_ = true;
    if (left) left->hide();
    if (right) right->hide();
    if (oldGenerator) oldGenerator->hide();
    if (mapDescription_) mapDescription_->hide();
    if (mapStyle_) mapStyle_->hide();

    if (auto* tabLayout = qobject_cast<QVBoxLayout*>(mapsTab->layout())) {
        tabLayout->setContentsMargins(8, 8, 8, 8);
        tabLayout->setSpacing(6);
    }

    auto* centerLayout = qobject_cast<QVBoxLayout*>(center->layout());
    if (!centerLayout || centerLayout->count() < 1) return;
    auto* titleRow = qobject_cast<QHBoxLayout*>(centerLayout->itemAt(0)->layout());
    if (!titleRow) return;

    for (int i = 0; i < titleRow->count(); ++i) {
        QWidget* widget = titleRow->itemAt(i)->widget();
        if (widget && widget != mapName_) widget->hide();
    }

    mapPicker_ = new QComboBox(center);
    mapPicker_->setObjectName(QStringLiteral("mapPicker"));
    mapPicker_->setMinimumWidth(170);
    mapPicker_->setMaximumWidth(250);
    for (int i = 0; i < mapList_->count(); ++i) mapPicker_->addItem(mapList_->item(i)->text());
    mapPicker_->setCurrentIndex(mapList_->currentRow());

    auto* addMapButton = new QToolButton(center);
    addMapButton->setText(QStringLiteral("+"));
    addMapButton->setObjectName(QStringLiteral("mapChromeButton"));
    addMapButton->setToolTip(tr("Nuevo mapa"));
    addMapButton->setFixedSize(30, 30);
    auto* removeMapButton = new QToolButton(center);
    removeMapButton->setText(QStringLiteral("−"));
    removeMapButton->setObjectName(QStringLiteral("mapChromeButton"));
    removeMapButton->setToolTip(tr("Eliminar mapa"));
    removeMapButton->setFixedSize(30, 30);
    auto* generateButton = new QToolButton(center);
    generateButton->setText(QStringLiteral("⌁"));
    generateButton->setToolTip(tr("Generar terreno"));
    generateButton->setObjectName(QStringLiteral("mapGenerateButton"));
    generateButton->setFixedSize(32, 30);

    titleRow->insertWidget(0, mapPicker_);
    titleRow->insertWidget(1, addMapButton);
    titleRow->insertWidget(2, removeMapButton);
    titleRow->addWidget(generateButton);
    if (mapName_) {
        mapName_->setPlaceholderText(tr("Nombre del mapa"));
        mapName_->setMaximumWidth(420);
    }

    mapGeneratorPopup_ = new QFrame(center);
    mapGeneratorPopup_->setObjectName(QStringLiteral("mapGeneratorPopup"));
    mapGeneratorPopup_->setFixedWidth(390);
    auto* popupLayout = new QVBoxLayout(mapGeneratorPopup_);
    popupLayout->setContentsMargins(12, 10, 12, 10);
    popupLayout->setSpacing(8);
    auto* popupTitle = new QLabel(tr("Generador de terreno"), mapGeneratorPopup_);
    popupTitle->setObjectName(QStringLiteral("panelTitle"));
    popupLayout->addWidget(popupTitle);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(6);
    auto addField = [&](int row, int col, const QString& text, QWidget* field) {
        auto* label = new QLabel(text, mapGeneratorPopup_);
        label->setObjectName(QStringLiteral("fieldTitle"));
        grid->addWidget(label, row * 2, col);
        field->setParent(mapGeneratorPopup_);
        field->setMinimumWidth(110);
        grid->addWidget(field, row * 2 + 1, col);
    };
    addField(0, 0, tr("Semilla"), mapSeed_);
    addField(0, 1, tr("Continentes"), mapContinents_);
    addField(1, 0, tr("Islas"), mapIslands_);
    addField(1, 1, tr("Rugosidad"), mapRoughness_);
    popupLayout->addLayout(grid);

    auto* generatorActions = new QHBoxLayout;
    auto* randomSeed = new QPushButton(tr("Nueva semilla"), mapGeneratorPopup_);
    auto* runGenerator = new QPushButton(tr("Generar"), mapGeneratorPopup_);
    runGenerator->setObjectName(QStringLiteral("primaryAction"));
    generatorActions->addWidget(randomSeed);
    generatorActions->addStretch();
    generatorActions->addWidget(runGenerator);
    popupLayout->addLayout(generatorActions);
    mapGeneratorPopup_->hide();

    connect(generateButton, &QToolButton::clicked, this, [this, center]() {
        if (!mapGeneratorPopup_) return;
        mapGeneratorPopup_->adjustSize();
        const int x = qMax(8, center->width() - mapGeneratorPopup_->width() - 12);
        mapGeneratorPopup_->move(x, 44);
        mapGeneratorPopup_->setVisible(!mapGeneratorPopup_->isVisible());
        mapGeneratorPopup_->raise();
    });
    connect(randomSeed, &QPushButton::clicked, this, [this]() {
        mapSeed_->setValue(static_cast<int>(QRandomGenerator::global()->generate() & 0x7fffffff));
    });
    connect(runGenerator, &QPushButton::clicked, this, [this]() {
        generateTerrain();
        if (mapGeneratorPopup_) mapGeneratorPopup_->hide();
    });

    connect(mapPicker_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index >= 0 && mapList_ && mapList_->currentRow() != index) mapList_->setCurrentRow(index);
    });
    connect(mapList_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (!mapPicker_) return;
        mapPicker_->blockSignals(true);
        mapPicker_->setCurrentIndex(row);
        mapPicker_->blockSignals(false);
    });
    connect(addMapButton, &QToolButton::clicked, this, [this]() { addMap(); syncMapPicker(); });
    connect(removeMapButton, &QToolButton::clicked, this, [this]() { removeMap(); syncMapPicker(); });
    connect(mapName_, &QLineEdit::editingFinished, this, [this]() {
        if (!mapPicker_ || !mapList_) return;
        const int row = mapList_->currentRow();
        if (row >= 0 && row < mapPicker_->count()) mapPicker_->setItemText(row, mapName_->text().trimmed().isEmpty() ? tr("Mapa sin nombre") : mapName_->text().trimmed());
    });

    setStyleSheet(styleSheet() + QStringLiteral(
        "#mapEditorCard{border:0;background:transparent;}"
        "#mapPicker{border:0;background:transparent;font-weight:600;padding-left:4px;}"
        "#mapChromeButton,#mapGenerateButton{background:transparent;border:0;padding:2px;font-size:12pt;}"
        "#mapChromeButton:hover,#mapGenerateButton:hover{background:rgba(100,120,145,0.12);}"
        "#mapGeneratorPopup{background:palette(base);color:palette(text);border:1px solid palette(mid);border-radius:6px;}"
    ));
}

void WorldPage::syncMapPicker() {
    if (!mapPicker_ || !mapList_) return;
    mapPicker_->blockSignals(true);
    mapPicker_->clear();
    for (int i = 0; i < mapList_->count(); ++i) mapPicker_->addItem(mapList_->item(i)->text());
    mapPicker_->setCurrentIndex(mapList_->currentRow());
    mapPicker_->blockSignals(false);
}

void WorldPage::generateTerrain() {
    if (!document_ || !mapCanvas_ || !mapList_) return;
    const int row = mapList_->currentRow();
    QJsonArray maps = document_->array(QStringLiteral("maps"));
    if (row < 0 || row >= maps.size()) return;

    QJsonObject map = maps.at(row).toObject();
    QJsonObject pilin = map.value(QStringLiteral("pilinRey")).toObject();
    const double width = pilin.value(QStringLiteral("width")).toDouble(4096.0);
    const double height = pilin.value(QStringLiteral("height")).toDouble(2304.0);
    QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();

    int generatedLayer = -1;
    for (int i = 0; i < layers.size(); ++i) {
        if (layers.at(i).toObject().value(QStringLiteral("generatedTerrain")).toBool(false)) { generatedLayer = i; break; }
    }
    if (generatedLayer < 0) {
        QJsonObject layer{
            {QStringLiteral("id"), QStringLiteral("layer-generated-terrain")},
            {QStringLiteral("name"), tr("Terreno generado")},
            {QStringLiteral("visible"), true},
            {QStringLiteral("locked"), false},
            {QStringLiteral("opacity"), 1.0},
            {QStringLiteral("generatedTerrain"), true},
            {QStringLiteral("objects"), QJsonArray()}
        };
        layers.insert(0, layer);
        generatedLayer = 0;
    }

    QRandomGenerator rng(static_cast<quint32>(mapSeed_->value()));
    QJsonArray objects;
    const int continents = qBound(1, mapContinents_->value(), 12);
    const int islands = qBound(0, mapIslands_->value(), 80);
    const double roughness = qBound(1, mapRoughness_->value(), 10) / 10.0;
    const int columns = qMax(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(continents)))));
    const int rows = qMax(1, static_cast<int>(std::ceil(continents / static_cast<double>(columns))));
    constexpr double pi = 3.14159265358979323846;

    auto addLand = [&](double cx, double cy, double rx, double ry, int samples, int index) {
        QJsonArray points;
        const double phase = rng.generateDouble() * pi * 2.0;
        for (int p = 0; p < samples; ++p) {
            const double angle = (pi * 2.0 * p) / samples;
            const double harmonic = std::sin(angle * 3.0 + phase) * 0.12 + std::sin(angle * 7.0 - phase * 0.7) * 0.06;
            const double noise = (rng.generateDouble() - 0.5) * (0.16 + roughness * 0.42);
            const double radial = qBound(0.55, 1.0 + harmonic * roughness + noise, 1.38);
            points.append(QJsonObject{{QStringLiteral("x"), cx + std::cos(angle) * rx * radial}, {QStringLiteral("y"), cy + std::sin(angle) * ry * radial}});
        }
        if (!points.isEmpty()) points.append(points.first());
        objects.append(QJsonObject{
            {QStringLiteral("id"), QStringLiteral("generated-land-%1-%2").arg(index).arg(mapSeed_->value())},
            {QStringLiteral("type"), QStringLiteral("coast")},
            {QStringLiteral("closed"), true},
            {QStringLiteral("generated"), true},
            {QStringLiteral("points"), points}
        });
    };

    for (int i = 0; i < continents; ++i) {
        const int col = i % columns;
        const int r = i / columns;
        const double cellW = width / columns;
        const double cellH = height / rows;
        const double cx = cellW * (col + 0.5) + (rng.generateDouble() - 0.5) * cellW * 0.30;
        const double cy = cellH * (r + 0.5) + (rng.generateDouble() - 0.5) * cellH * 0.30;
        const double rx = cellW * (0.24 + rng.generateDouble() * 0.12);
        const double ry = cellH * (0.24 + rng.generateDouble() * 0.12);
        addLand(cx, cy, rx, ry, 72, i);
    }

    for (int i = 0; i < islands; ++i) {
        const double marginX = width * 0.07;
        const double marginY = height * 0.07;
        const double cx = marginX + rng.generateDouble() * (width - marginX * 2.0);
        const double cy = marginY + rng.generateDouble() * (height - marginY * 2.0);
        const double radius = qMin(width, height) * (0.012 + rng.generateDouble() * 0.028);
        addLand(cx, cy, radius * (0.8 + rng.generateDouble() * 0.5), radius * (0.7 + rng.generateDouble() * 0.6), 34, continents + i);
    }

    QJsonObject generated = layers.at(generatedLayer).toObject();
    generated.insert(QStringLiteral("objects"), objects);
    generated.insert(QStringLiteral("visible"), true);
    generated.insert(QStringLiteral("generatedTerrain"), true);
    layers.replace(generatedLayer, generated);
    pilin.insert(QStringLiteral("layers"), layers);
    pilin.insert(QStringLiteral("activeLayerId"), generated.value(QStringLiteral("id")).toString());
    map.insert(QStringLiteral("pilinRey"), pilin);
    map.insert(QStringLiteral("seed"), mapSeed_->value());
    map.insert(QStringLiteral("continents"), continents);
    map.insert(QStringLiteral("islands"), islands);
    map.insert(QStringLiteral("roughness"), mapRoughness_->value());
    map.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    maps.replace(row, map);
    document_->setArray(QStringLiteral("maps"), maps);
    mapCanvas_->setMap(map);
    emit changed();
}

} // namespace wbw
