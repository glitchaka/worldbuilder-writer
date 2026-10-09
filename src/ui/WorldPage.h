#pragma once

#include "core/ArchiveDocument.h"

#include <QComboBox>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRandomGenerator>
#include <QShowEvent>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>

namespace wbw {

class PilinReyEditor;

class WorldPage final : public QWidget {
    Q_OBJECT
public:
    explicit WorldPage(QWidget* parent = nullptr);

    void setDocument(ArchiveDocument* document);
    void refresh();
    bool openReference(const QString& kind, const QString& id);

signals:
    void changed();

protected:
    void showEvent(QShowEvent* event) override {
        QWidget::showEvent(event);

        if (!atlasChromeApplied_) {
            if (auto* editor = findChild<QWidget*>(QStringLiteral("worldEditorCard"))) {
                if (auto* editorLayout = qobject_cast<QVBoxLayout*>(editor->layout())) {
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
                        if (!target || !target->layout()) continue;
                        block->setParent(target);
                        auto* layout = qobject_cast<QVBoxLayout*>(target->layout());
                        if (!layout) continue;
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
            }
        }

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

        auto* addMapButton = new QPushButton(QStringLiteral("+"), center);
        addMapButton->setObjectName(QStringLiteral("mapChromeButton"));
        addMapButton->setToolTip(tr("Nuevo mapa"));
        addMapButton->setFixedWidth(30);
        auto* removeMapButton = new QPushButton(QStringLiteral("−"), center);
        removeMapButton->setObjectName(QStringLiteral("mapChromeButton"));
        removeMapButton->setToolTip(tr("Eliminar mapa"));
        removeMapButton->setFixedWidth(30);
        auto* generateButton = new QPushButton(tr("Generar terreno"), center);
        generateButton->setObjectName(QStringLiteral("mapGenerateButton"));

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

        connect(generateButton, &QPushButton::clicked, this, [this, center]() {
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
        connect(addMapButton, &QPushButton::clicked, this, [this]() {
            addMap();
            syncMapPicker();
        });
        connect(removeMapButton, &QPushButton::clicked, this, [this]() {
            removeMap();
            syncMapPicker();
        });
        connect(mapName_, &QLineEdit::editingFinished, this, [this]() {
            if (!mapPicker_ || !mapList_) return;
            const int row = mapList_->currentRow();
            if (row >= 0 && row < mapPicker_->count()) mapPicker_->setItemText(row, mapName_->text().trimmed().isEmpty() ? tr("Mapa sin nombre") : mapName_->text().trimmed());
        });

        setStyleSheet(styleSheet() + QStringLiteral(
            "#mapEditorCard{border:0;background:transparent;}"
            "#mapPicker{border:0;background:transparent;font-weight:600;padding-left:4px;}"
            "#mapChromeButton{background:transparent;border:0;font-size:13pt;padding:2px;}"
            "#mapChromeButton:hover{background:rgba(100,120,145,0.12);}"
            "#mapGenerateButton{background:transparent;border:0;padding:5px 8px;font-weight:600;}"
            "#mapGenerateButton:hover{background:rgba(100,120,145,0.12);}"
            "#mapGeneratorPopup{background:#ffffff;border:1px solid #cfd6df;border-radius:6px;}"
        ));
    }

private:
    friend class PilinReyEditor;

    void syncMapPicker() {
        if (!mapPicker_ || !mapList_) return;
        mapPicker_->blockSignals(true);
        mapPicker_->clear();
        for (int i = 0; i < mapList_->count(); ++i) mapPicker_->addItem(mapList_->item(i)->text());
        mapPicker_->setCurrentIndex(mapList_->currentRow());
        mapPicker_->blockSignals(false);
    }

    void generateTerrain() {
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
            if (layers.at(i).toObject().value(QStringLiteral("generatedTerrain")).toBool(false)) {
                generatedLayer = i;
                break;
            }
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
            double phase = rng.generateDouble() * pi * 2.0;
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

    QWidget* buildAtlasTab();
    QWidget* buildMagicTab();
    QWidget* buildMapsTab();
    QWidget* buildTextsTab();

    void refreshAtlas();
    void selectWorld(int row);
    void applyWorld();
    void addWorld();
    void removeWorld();
    void addWorldAttachment();
    void removeWorldAttachment();
    void exportWorldAttachment();

    void refreshMagic();
    void selectMagic(int row);
    void applyMagic();
    void addMagic();
    void removeMagic();
    void addMagicAttachment();
    void removeMagicAttachment();
    void exportMagicAttachment();

    void refreshMaps();
    void selectMap(int row);
    void applyMap();
    void addMap();
    void removeMap();
    void chooseMapBackground();
    void clearMapBackground();
    void addMapMarker(double x, double y);
    void removeMapMarker();
    void moveMapMarker(const QString& id, double x, double y);

    void refreshTexts();
    void selectText(bool magic, int row);
    void applyText(bool magic);
    void importText(bool magic);
    void addText(bool magic);
    void removeText(bool magic);

    ArchiveDocument* document_ = nullptr;
    bool refreshing_ = false;
    bool mapsChromeApplied_ = false;
    bool atlasChromeApplied_ = false;
    QTabWidget* tabs_ = nullptr;

    QListWidget* worldList_ = nullptr;
    QComboBox* worldKind_ = nullptr;
    QLineEdit* worldName_ = nullptr;
    QLineEdit* worldAliases_ = nullptr;
    QTextEdit* worldSummary_ = nullptr;
    QTextEdit* worldGeography_ = nullptr;
    QTextEdit* worldGovernment_ = nullptr;
    QTextEdit* worldPeoples_ = nullptr;
    QTextEdit* worldCulture_ = nullptr;
    QTextEdit* worldEconomy_ = nullptr;
    QTextEdit* worldCurrency_ = nullptr;
    QTextEdit* worldLanguages_ = nullptr;
    QTextEdit* worldReligions_ = nullptr;
    QTextEdit* worldMilitary_ = nullptr;
    QTextEdit* worldHistory_ = nullptr;
    QTextEdit* worldRelations_ = nullptr;
    QTextEdit* worldLocations_ = nullptr;
    QTextEdit* worldConflicts_ = nullptr;
    QTextEdit* worldNotes_ = nullptr;
    QLineEdit* worldTags_ = nullptr;
    QListWidget* worldAttachments_ = nullptr;

    QListWidget* magicList_ = nullptr;
    QLineEdit* magicName_ = nullptr;
    QLineEdit* magicCategory_ = nullptr;
    QComboBox* magicStatus_ = nullptr;
    QTextEdit* magicSource_ = nullptr;
    QTextEdit* magicPrinciple_ = nullptr;
    QTextEdit* magicAccess_ = nullptr;
    QTextEdit* magicCost_ = nullptr;
    QTextEdit* magicLimits_ = nullptr;
    QTextEdit* magicManifestations_ = nullptr;
    QTextEdit* magicMaterials_ = nullptr;
    QTextEdit* magicInstitutions_ = nullptr;
    QTextEdit* magicUsers_ = nullptr;
    QTextEdit* magicRisks_ = nullptr;
    QTextEdit* magicHistory_ = nullptr;
    QTextEdit* magicNotes_ = nullptr;
    QTextEdit* magicEvidence_ = nullptr;
    QLineEdit* magicTags_ = nullptr;
    QListWidget* magicAttachments_ = nullptr;

    QListWidget* mapList_ = nullptr;
    QComboBox* mapPicker_ = nullptr;
    QLineEdit* mapName_ = nullptr;
    QTextEdit* mapDescription_ = nullptr;
    QSpinBox* mapSeed_ = nullptr;
    QComboBox* mapStyle_ = nullptr;
    QSpinBox* mapContinents_ = nullptr;
    QSpinBox* mapIslands_ = nullptr;
    QSpinBox* mapRoughness_ = nullptr;
    QListWidget* mapMarkers_ = nullptr;
    PilinReyEditor* mapCanvas_ = nullptr;
    QFrame* mapGeneratorPopup_ = nullptr;

    QListWidget* worldTextList_ = nullptr;
    QLineEdit* worldTextTitle_ = nullptr;
    QLineEdit* worldTextSource_ = nullptr;
    QTextEdit* worldTextContent_ = nullptr;
    QListWidget* magicTextList_ = nullptr;
    QLineEdit* magicTextTitle_ = nullptr;
    QLineEdit* magicTextSource_ = nullptr;
    QTextEdit* magicTextContent_ = nullptr;
};

} // namespace wbw
