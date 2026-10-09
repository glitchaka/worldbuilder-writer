#include "ui/WorldPage.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QShowEvent>
#include <QTabWidget>
#include <QVBoxLayout>

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
    if (mapGeneratorPopup_) mapGeneratorPopup_->hide();
    if (mapDescription_) mapDescription_->hide();
    if (mapStyle_) mapStyle_->hide();
    if (mapSeed_) mapSeed_->hide();
    if (mapContinents_) mapContinents_->hide();
    if (mapIslands_) mapIslands_->hide();
    if (mapRoughness_) mapRoughness_->hide();

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
    mapPicker_->setMinimumWidth(180);
    mapPicker_->setMaximumWidth(280);
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

    titleRow->insertWidget(0, mapPicker_);
    titleRow->insertWidget(1, addMapButton);
    titleRow->insertWidget(2, removeMapButton);
    if (mapName_) {
        mapName_->setPlaceholderText(tr("Nombre del mapa"));
        mapName_->setMaximumWidth(360);
    }

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
        "#mapChromeButton{background:transparent;border:0;padding:2px;font-size:12pt;}"
        "#mapChromeButton:hover{background:rgba(100,120,145,0.12);}"
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

} // namespace wbw
