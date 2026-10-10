#include "ui/WorldPage.h"

#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QScrollArea>
#include <QShowEvent>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolButton>
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

    struct AtlasPage {
        QWidget* page = nullptr;
        QWidget* content = nullptr;
        QGridLayout* grid = nullptr;
        int item = 0;
    };

    auto makePage = [sections](const QString& name) {
        AtlasPage result;
        result.page = new QWidget(sections);
        result.page->setObjectName(QStringLiteral("atlasSectionPage"));
        auto* outer = new QVBoxLayout(result.page);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(0);
        auto* scroll = new QScrollArea(result.page);
        scroll->setObjectName(QStringLiteral("atlasSectionScroll"));
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        result.content = new QWidget(scroll);
        result.content->setObjectName(QStringLiteral("atlasSectionContent"));
        result.grid = new QGridLayout(result.content);
        result.grid->setContentsMargins(12, 12, 12, 16);
        result.grid->setHorizontalSpacing(10);
        result.grid->setVerticalSpacing(10);
        result.grid->setColumnStretch(0, 1);
        result.grid->setColumnStretch(1, 1);
        scroll->setWidget(result.content);
        outer->addWidget(scroll);
        sections->addTab(result.page, name);
        return result;
    };

    AtlasPage overview = makePage(tr("Esencia"));
    AtlasPage territory = makePage(tr("Territorio"));
    AtlasPage society = makePage(tr("Sociedad"));
    AtlasPage history = makePage(tr("Historia"));
    AtlasPage notes = makePage(tr("Notas"));

    auto targetFor = [&](const QString& title) -> AtlasPage* {
        if (title == tr("Tipo") || title == tr("Nombre") || title == tr("Alias") || title == tr("Resumen")) return &overview;
        if (title == tr("Geografía") || title == tr("Localizaciones")) return &territory;
        if (title == tr("Gobierno") || title == tr("Pueblos") || title == tr("Cultura") || title == tr("Economía") || title == tr("Moneda") || title == tr("Idiomas") || title == tr("Religiones") || title == tr("Fuerza militar")) return &society;
        if (title == tr("Historia") || title == tr("Relaciones") || title == tr("Conflictos")) return &history;
        return &notes;
    };

    const auto labels = editor->findChildren<QLabel*>();
    for (QLabel* label : labels) {
        if (!label || label->objectName() != QStringLiteral("fieldTitle")) continue;
        QWidget* block = label->parentWidget();
        if (!block || block == editor) continue;
        AtlasPage* target = targetFor(label->text());
        if (!target || !target->grid) continue;

        block->setParent(target->content);
        block->setObjectName(QStringLiteral("atlasFieldCard"));
        const bool editorial = !block->findChildren<QTextEdit*>().isEmpty();
        if (editorial) {
            const int row = (target->item + 1) / 2;
            target->grid->addWidget(block, row, 0, 1, 2);
            target->item = (row + 1) * 2;
        } else {
            const int row = target->item / 2;
            const int column = target->item % 2;
            target->grid->addWidget(block, row, column);
            ++target->item;
        }
    }

    for (AtlasPage* page : {&overview, &territory, &society, &history, &notes}) {
        if (!page->grid) continue;
        const int lastRow = qMax(1, (page->item + 1) / 2);
        page->grid->setRowStretch(lastRow + 1, 1);
    }

    editorLayout->insertWidget(1, sections, 1);
    editorLayout->setContentsMargins(10, 10, 10, 10);
    editorLayout->setSpacing(7);
    atlasChromeApplied_ = true;
    setStyleSheet(styleSheet() + QStringLiteral(
        "#atlasSections::pane{border:0;background:transparent;}"
        "#atlasSections QTabBar::tab{padding:8px 11px;}"
        "#atlasSectionPage,#atlasSectionContent,#atlasSectionScroll{background:transparent;border:0;}"
        "#atlasFieldCard{border-radius:6px;padding:10px;}"
        "#atlasFieldCard QTextEdit{min-height:90px;}"
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

    // The map workspace is canvas-first. The legacy administrative columns stay alive
    // only as backing widgets for the persisted data model, never as visible UI.
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
        tabLayout->setContentsMargins(0, 0, 0, 0);
        tabLayout->setSpacing(0);
    }

    center->setContentsMargins(0, 0, 0, 0);
    auto* centerLayout = qobject_cast<QVBoxLayout*>(center->layout());
    if (!centerLayout || centerLayout->count() < 1) return;
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);

    auto* titleRow = qobject_cast<QHBoxLayout*>(centerLayout->itemAt(0)->layout());
    if (titleRow) {
        for (int i = 0; i < titleRow->count(); ++i) {
            if (QWidget* widget = titleRow->itemAt(i)->widget()) widget->hide();
        }
    }

    // Compact map switcher floats over the canvas instead of consuming a permanent row.
    auto* canvasHost = mapCanvas_->findChild<QWidget*>(QStringLiteral("pilinCanvasHost"));
    QWidget* overlayParent = canvasHost ? canvasHost : mapCanvas_;
    auto* workspaceBar = new QFrame(overlayParent);
    workspaceBar->setObjectName(QStringLiteral("mapWorkspaceBar"));
    auto* bar = new QHBoxLayout(workspaceBar);
    bar->setContentsMargins(8, 5, 8, 5);
    bar->setSpacing(5);

    auto* context = new QLabel(tr("MAPA"), workspaceBar);
    context->setObjectName(QStringLiteral("mapWorkspaceKicker"));
    bar->addWidget(context);

    mapPicker_ = new QComboBox(workspaceBar);
    mapPicker_->setObjectName(QStringLiteral("mapPicker"));
    mapPicker_->setMinimumWidth(150);
    mapPicker_->setMaximumWidth(230);
    for (int i = 0; i < mapList_->count(); ++i) mapPicker_->addItem(mapList_->item(i)->text());
    mapPicker_->setCurrentIndex(mapList_->currentRow());
    bar->addWidget(mapPicker_);

    auto* addMapButton = new QToolButton(workspaceBar);
    addMapButton->setText(QStringLiteral("+"));
    addMapButton->setObjectName(QStringLiteral("mapChromeButton"));
    addMapButton->setToolTip(tr("Nuevo mapa"));
    addMapButton->setFixedSize(28, 28);
    bar->addWidget(addMapButton);

    auto* removeMapButton = new QToolButton(workspaceBar);
    removeMapButton->setText(QStringLiteral("−"));
    removeMapButton->setObjectName(QStringLiteral("mapChromeButton"));
    removeMapButton->setToolTip(tr("Eliminar mapa"));
    removeMapButton->setFixedSize(28, 28);
    bar->addWidget(removeMapButton);

    if (mapName_) {
        mapName_->setParent(workspaceBar);
        mapName_->setObjectName(QStringLiteral("mapWorkspaceName"));
        mapName_->setPlaceholderText(tr("Nombre del mapa"));
        mapName_->setMinimumWidth(140);
        mapName_->setMaximumWidth(240);
        mapName_->show();
        bar->addWidget(mapName_);
    }

    workspaceBar->adjustSize();
    workspaceBar->move(62, 12);
    workspaceBar->raise();
    workspaceBar->show();

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
    if (mapName_) {
        connect(mapName_, &QLineEdit::editingFinished, this, [this]() {
            if (!mapPicker_ || !mapList_) return;
            const int row = mapList_->currentRow();
            if (row >= 0 && row < mapPicker_->count())
                mapPicker_->setItemText(row, mapName_->text().trimmed().isEmpty() ? tr("Mapa sin nombre") : mapName_->text().trimmed());
        });
    }

    setStyleSheet(styleSheet() + QStringLiteral(
        "#mapEditorCard{border:0;background:transparent;}"
        "#mapWorkspaceBar{border:1px solid rgba(125,125,125,0.24);border-radius:7px;}"
        "#mapWorkspaceKicker{font-size:7pt;font-weight:700;letter-spacing:1px;padding:0 3px;}"
        "#mapPicker{border:0;background:transparent;font-weight:600;padding:4px 6px;}"
        "#mapWorkspaceName{min-height:20px;padding:4px 7px;}"
        "#mapChromeButton{background:transparent;border:0;padding:2px;font-size:12pt;}"
        "#mapChromeButton:hover{background:rgba(130,130,130,0.15);}"
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
