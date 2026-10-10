#include "ui/WorldPage.h"
#include "ui/PilinReyEditor.h"

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
    auto* index = findChild<QWidget*>(QStringLiteral("worldIndexPanel"));
    if (!editor || !index) return;
    auto* editorLayout = qobject_cast<QVBoxLayout*>(editor->layout());
    if (!editorLayout) return;

    const auto labels = editor->findChildren<QLabel*>();
    QList<QWidget*> fieldBlocks;
    for (QLabel* label : labels) {
        if (!label || label->objectName() != QStringLiteral("fieldTitle")) continue;
        QWidget* block = label->parentWidget();
        if (!block || block == editor || fieldBlocks.contains(block)) continue;
        fieldBlocks.append(block);
    }

    for (QLabel* label : editor->findChildren<QLabel*>()) {
        if (label->objectName() == QStringLiteral("panelTitle")) label->hide();
    }

    auto* editorialSplit = new QSplitter(Qt::Horizontal, editor);
    editorialSplit->setObjectName(QStringLiteral("atlasEditorialSplit"));
    editorialSplit->setChildrenCollapsible(false);
    editorialSplit->setHandleWidth(6);

    auto* centerScroll = new QScrollArea(editorialSplit);
    centerScroll->setObjectName(QStringLiteral("atlasCenterScroll"));
    centerScroll->setWidgetResizable(true);
    centerScroll->setFrameShape(QFrame::NoFrame);
    auto* center = new QWidget(centerScroll);
    center->setObjectName(QStringLiteral("atlasCenterEditorial"));
    auto* centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(18, 18, 18, 28);
    centerLayout->setSpacing(12);

    auto* right = new QFrame(editorialSplit);
    right->setObjectName(QStringLiteral("atlasContextRail"));
    right->setMinimumWidth(250);
    right->setMaximumWidth(330);
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(14, 16, 14, 18);
    rightLayout->setSpacing(10);

    auto addSectionLabel = [](QVBoxLayout* layout, const QString& text, const QString& objectName) {
        auto* label = new QLabel(text);
        label->setObjectName(objectName);
        layout->addWidget(label);
    };

    addSectionLabel(centerLayout, tr("FICHA EDITORIAL"), QStringLiteral("atlasEditorialKicker"));
    auto* centerTitle = new QLabel(tr("Entrada de atlas"));
    centerTitle->setObjectName(QStringLiteral("atlasEditorialTitle"));
    centerLayout->addWidget(centerTitle);

    addSectionLabel(rightLayout, tr("CONTEXTO"), QStringLiteral("atlasContextKicker"));

    auto* identity = new QFrame(center);
    identity->setObjectName(QStringLiteral("atlasEditorialCard"));
    auto* identityGrid = new QGridLayout(identity);
    identityGrid->setContentsMargins(14, 14, 14, 14);
    identityGrid->setHorizontalSpacing(10);
    identityGrid->setVerticalSpacing(10);

    auto* body = new QWidget(center);
    body->setObjectName(QStringLiteral("atlasEditorialBody"));
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(12);

    int identityItem = 0;
    for (QWidget* block : fieldBlocks) {
        QLabel* titleLabel = block->findChild<QLabel*>(QString(), Qt::FindDirectChildrenOnly);
        QString title;
        const auto childLabels = block->findChildren<QLabel*>();
        for (QLabel* candidate : childLabels) {
            if (candidate->objectName() == QStringLiteral("fieldTitle")) { title = candidate->text(); break; }
        }

        const bool isIdentity = title == tr("Tipo") || title == tr("Nombre") || title == tr("Alias") || title == tr("Resumen");
        const bool isContext = title == tr("Relaciones") || title == tr("Conflictos") || title == tr("Etiquetas") || title == tr("Adjuntos") || title == tr("Notas");

        block->setObjectName(QStringLiteral("atlasFieldCard"));
        if (isIdentity) {
            block->setParent(identity);
            const bool wide = !block->findChildren<QTextEdit*>().isEmpty();
            if (wide) {
                identityGrid->addWidget(block, 1, 0, 1, 3);
            } else {
                identityGrid->addWidget(block, 0, identityItem % 3);
                ++identityItem;
            }
        } else if (isContext) {
            block->setParent(right);
            rightLayout->addWidget(block);
        } else {
            block->setParent(body);
            bodyLayout->addWidget(block);
        }
    }

    centerLayout->addWidget(identity);
    centerLayout->addWidget(body);
    centerLayout->addStretch(1);
    rightLayout->addStretch(1);
    centerScroll->setWidget(center);

    editorialSplit->addWidget(centerScroll);
    editorialSplit->addWidget(right);
    editorialSplit->setStretchFactor(0, 1);
    editorialSplit->setStretchFactor(1, 0);
    editorialSplit->setSizes({900, 290});

    editorLayout->addWidget(editorialSplit, 1);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);

    index->setMinimumWidth(230);
    index->setMaximumWidth(310);

    atlasChromeApplied_ = true;
    setStyleSheet(styleSheet() + QStringLiteral(
        "#worldPage,#atlasTab{background:#0f1115;color:#e8ebef;}"
        "#worldTabs::pane{border:0;background:#0f1115;}"
        "#worldTabs QTabBar::tab{background:transparent;color:#8f97a3;border:0;padding:9px 13px;}"
        "#worldTabs QTabBar::tab:selected{color:#f0f2f5;border-bottom:2px solid #c59a5d;}"
        "#worldIndexPanel{background:#14171c;border:0;border-right:1px solid #2a2f36;}"
        "#worldIndexPanel QListWidget{background:transparent;color:#d9dde3;border:0;outline:0;}"
        "#worldIndexPanel QListWidget::item{padding:8px 9px;border-radius:6px;}"
        "#worldIndexPanel QListWidget::item:selected{background:#252b33;color:#ffffff;}"
        "#worldEditorCard,#atlasCenterScroll,#atlasCenterEditorial,#atlasEditorialBody{background:#0f1115;border:0;}"
        "#atlasContextRail{background:#14171c;border:0;border-left:1px solid #2a2f36;}"
        "#atlasEditorialKicker,#atlasContextKicker{color:#c59a5d;font-size:8pt;font-weight:700;letter-spacing:1px;}"
        "#atlasEditorialTitle{color:#f2f3f5;font-family:'Georgia';font-size:22pt;padding-bottom:2px;}"
        "#atlasEditorialCard,#atlasFieldCard{background:#171a20;border:1px solid #2a2f36;border-radius:10px;}"
        "#atlasFieldCard{padding:11px;}"
        "#atlasFieldCard #fieldTitle{color:#9ba3ae;font-size:8pt;font-weight:700;letter-spacing:.7px;}"
        "#atlasFieldCard QLineEdit,#atlasFieldCard QTextEdit,#atlasFieldCard QComboBox{background:#111419;color:#e7eaee;border:1px solid #30353d;border-radius:7px;padding:7px;}"
        "#atlasFieldCard QTextEdit{min-height:90px;}"
        "#atlasEditorialSplit::handle{background:#20242a;}"
        "#pageKicker,#pageTitle,#pageDescription{color:#8f97a3;}"
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

    auto* canvasHost = mapCanvas_->findChild<QWidget*>(QStringLiteral("pilinCanvasHost"));
    QWidget* overlayParent = canvasHost ? canvasHost : static_cast<QWidget*>(mapCanvas_);
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