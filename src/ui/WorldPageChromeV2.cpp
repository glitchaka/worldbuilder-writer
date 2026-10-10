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
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

namespace wbw {
namespace {

QString blockTitle(QWidget* block) {
    if (!block) return {};
    for (QLabel* label : block->findChildren<QLabel*>())
        if (label->objectName() == QStringLiteral("fieldTitle")) return label->text().trimmed();
    return {};
}

void makeEditorialBlock(QWidget* block, const QString& role) {
    if (!block) return;
    block->setObjectName(role);
    if (auto* layout = qobject_cast<QVBoxLayout*>(block->layout())) {
        layout->setContentsMargins(12, 10, 12, 12);
        layout->setSpacing(6);
    }
}

} // namespace

void WorldPage::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    applyAtlasChrome();
    applyMapsChrome();
}

void WorldPage::applyAtlasChrome() {
    if (atlasChromeApplied_) return;
    auto* editor = findChild<QWidget*>(QStringLiteral("worldEditorCard"));
    auto* index = findChild<QWidget*>(QStringLiteral("worldIndexPanel"));
    auto* atlasTab = findChild<QWidget*>(QStringLiteral("atlasTab"));
    if (!editor || !index || !atlasTab) return;
    auto* editorLayout = qobject_cast<QVBoxLayout*>(editor->layout());
    auto* atlasLayout = qobject_cast<QVBoxLayout*>(atlasTab->layout());
    if (!editorLayout || !atlasLayout) return;

    QList<QWidget*> fieldBlocks;
    for (QLabel* label : editor->findChildren<QLabel*>()) {
        if (!label || label->objectName() != QStringLiteral("fieldTitle")) continue;
        QWidget* block = label->parentWidget();
        if (!block || block == editor || fieldBlocks.contains(block)) continue;
        fieldBlocks.append(block);
    }
    for (QLabel* label : editor->findChildren<QLabel*>())
        if (label->objectName() == QStringLiteral("panelTitle")) label->hide();

    // The module title already exists in the application shell. Remove the old page hero so
    // the atlas starts directly with useful information instead of duplicated chrome.
    for (QLabel* label : atlasTab->findChildren<QLabel*>()) {
        const QString name = label->objectName();
        if (name == QStringLiteral("pageKicker") || name == QStringLiteral("pageTitle") || name == QStringLiteral("pageDescription")) label->hide();
    }
    atlasLayout->setContentsMargins(0, 0, 0, 0);
    atlasLayout->setSpacing(0);

    auto* editorialSplit = new QSplitter(Qt::Horizontal, editor);
    editorialSplit->setObjectName(QStringLiteral("atlasEditorialSplit"));
    editorialSplit->setChildrenCollapsible(true);
    editorialSplit->setHandleWidth(1);

    auto* centerScroll = new QScrollArea(editorialSplit);
    centerScroll->setObjectName(QStringLiteral("atlasCenterScroll"));
    centerScroll->setWidgetResizable(true);
    centerScroll->setFrameShape(QFrame::NoFrame);
    centerScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* center = new QWidget(centerScroll);
    center->setObjectName(QStringLiteral("atlasCenterEditorial"));
    auto* centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(24, 22, 24, 34);
    centerLayout->setSpacing(14);

    auto* titleRow = new QHBoxLayout;
    auto* titleColumn = new QVBoxLayout;
    titleColumn->setSpacing(3);
    auto* kicker = new QLabel(tr("FICHA DE MUNDO"), center);
    kicker->setObjectName(QStringLiteral("atlasEditorialKicker"));
    auto* centerTitle = new QLabel(tr("Entrada de atlas"), center);
    centerTitle->setObjectName(QStringLiteral("atlasEditorialTitle"));
    auto* centerSubtitle = new QLabel(tr("Información útil, relaciones y material de referencia en una sola superficie editorial."), center);
    centerSubtitle->setObjectName(QStringLiteral("atlasEditorialSubtitle"));
    centerSubtitle->setWordWrap(true);
    titleColumn->addWidget(kicker);
    titleColumn->addWidget(centerTitle);
    titleColumn->addWidget(centerSubtitle);
    titleRow->addLayout(titleColumn, 1);
    centerLayout->addLayout(titleRow);

    if (worldName_) {
        connect(worldName_, &QLineEdit::textChanged, this, [centerTitle](const QString& value) {
            centerTitle->setText(value.trimmed().isEmpty() ? QObject::tr("Entrada de atlas") : value.trimmed());
        });
        if (!worldName_->text().trimmed().isEmpty()) centerTitle->setText(worldName_->text().trimmed());
    }

    auto* identity = new QFrame(center);
    identity->setObjectName(QStringLiteral("atlasIdentityStrip"));
    auto* identityGrid = new QGridLayout(identity);
    identityGrid->setContentsMargins(12, 10, 12, 10);
    identityGrid->setHorizontalSpacing(10);
    identityGrid->setVerticalSpacing(8);

    auto* summaryHost = new QFrame(center);
    summaryHost->setObjectName(QStringLiteral("atlasSummaryCard"));
    auto* summaryLayout = new QVBoxLayout(summaryHost);
    summaryLayout->setContentsMargins(14, 12, 14, 14);
    summaryLayout->setSpacing(6);
    auto* summaryHeading = new QLabel(tr("RESUMEN"), summaryHost);
    summaryHeading->setObjectName(QStringLiteral("atlasSectionTitle"));
    summaryLayout->addWidget(summaryHeading);

    auto* contentGridHost = new QWidget(center);
    contentGridHost->setObjectName(QStringLiteral("atlasContentGrid"));
    auto* contentGrid = new QGridLayout(contentGridHost);
    contentGrid->setContentsMargins(0, 0, 0, 0);
    contentGrid->setHorizontalSpacing(12);
    contentGrid->setVerticalSpacing(12);
    contentGrid->setColumnStretch(0, 1);
    contentGrid->setColumnStretch(1, 1);

    auto* rightScroll = new QScrollArea(editorialSplit);
    rightScroll->setObjectName(QStringLiteral("atlasContextScroll"));
    rightScroll->setWidgetResizable(true);
    rightScroll->setFrameShape(QFrame::NoFrame);
    rightScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rightScroll->setMinimumWidth(265);
    rightScroll->setMaximumWidth(350);
    auto* right = new QFrame(rightScroll);
    right->setObjectName(QStringLiteral("atlasContextRail"));
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(14, 18, 14, 24);
    rightLayout->setSpacing(10);
    auto* contextKicker = new QLabel(tr("CONTEXTO"), right);
    contextKicker->setObjectName(QStringLiteral("atlasContextKicker"));
    auto* contextCopy = new QLabel(tr("Relaciones, conflictos, etiquetas, adjuntos y notas rápidas."), right);
    contextCopy->setObjectName(QStringLiteral("atlasContextCopy"));
    contextCopy->setWordWrap(true);
    rightLayout->addWidget(contextKicker);
    rightLayout->addWidget(contextCopy);

    int identityColumn = 0;
    int contentIndex = 0;
    for (QWidget* block : fieldBlocks) {
        const QString title = blockTitle(block);
        if (title == tr("Tipo") || title == tr("Nombre") || title == tr("Alias")) {
            makeEditorialBlock(block, QStringLiteral("atlasIdentityField"));
            block->setParent(identity);
            identityGrid->addWidget(block, 0, identityColumn++);
            continue;
        }
        if (title == tr("Resumen")) {
            makeEditorialBlock(block, QStringLiteral("atlasSummaryField"));
            for (QLabel* label : block->findChildren<QLabel*>()) if (label->objectName() == QStringLiteral("fieldTitle")) label->hide();
            block->setParent(summaryHost);
            summaryLayout->addWidget(block);
            continue;
        }
        const bool context = title == tr("Relaciones") || title == tr("Conflictos") || title == tr("Etiquetas") || title == tr("Adjuntos") || title == tr("Notas");
        if (context) {
            makeEditorialBlock(block, QStringLiteral("atlasContextCard"));
            block->setParent(right);
            rightLayout->addWidget(block);
            continue;
        }
        makeEditorialBlock(block, QStringLiteral("atlasEditorialSection"));
        block->setParent(contentGridHost);
        contentGrid->addWidget(block, contentIndex / 2, contentIndex % 2);
        ++contentIndex;
    }

    centerLayout->addWidget(identity);
    centerLayout->addWidget(summaryHost);
    auto* contentHeading = new QLabel(tr("MUNDO Y CONTEXTO"), center);
    contentHeading->setObjectName(QStringLiteral("atlasSectionTitle"));
    centerLayout->addWidget(contentHeading);
    centerLayout->addWidget(contentGridHost);
    centerLayout->addStretch(1);
    rightLayout->addStretch(1);

    centerScroll->setWidget(center);
    rightScroll->setWidget(right);
    editorialSplit->addWidget(centerScroll);
    editorialSplit->addWidget(rightScroll);
    editorialSplit->setStretchFactor(0, 1);
    editorialSplit->setStretchFactor(1, 0);
    editorialSplit->setSizes({980, 310});

    editorLayout->addWidget(editorialSplit, 1);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);
    index->setMinimumWidth(230);
    index->setMaximumWidth(300);

    atlasChromeApplied_ = true;
    setStyleSheet(styleSheet() + QStringLiteral(R"QSS(
#worldPage,#atlasTab{background:#0d1014;color:#e8ebef;}
#worldTabs::pane{border:0;background:#0d1014;}
#worldTabs QTabBar::tab{background:transparent;color:#7f8996;border:0;padding:9px 13px;}
#worldTabs QTabBar::tab:selected{color:#f0f2f5;border-bottom:2px solid #c59a5d;}
#worldIndexPanel{background:#11161c;border:0;border-right:1px solid #252d37;}
#worldIndexPanel QListWidget{background:transparent;color:#d8dde5;border:0;outline:0;padding:6px;}
#worldIndexPanel QListWidget::item{padding:9px 8px;border-radius:6px;margin:1px 0;}
#worldIndexPanel QListWidget::item:hover{background:#171e27;}
#worldIndexPanel QListWidget::item:selected{background:#232c37;color:#ffffff;}
#worldEditorCard,#atlasCenterScroll,#atlasCenterEditorial,#atlasContentGrid{background:#0d1014;border:0;}
#atlasContextScroll,#atlasContextRail{background:#11161c;border:0;}
#atlasContextRail{border-left:1px solid #252d37;}
#atlasEditorialKicker,#atlasContextKicker,#atlasSectionTitle{color:#c79b60;font-size:8pt;font-weight:700;letter-spacing:1px;}
#atlasEditorialTitle{color:#f2f3f5;font-family:'Georgia';font-size:24pt;font-weight:500;}
#atlasEditorialSubtitle,#atlasContextCopy{color:#7d8998;font-family:'Georgia';font-size:9pt;}
#atlasIdentityStrip,#atlasSummaryCard,#atlasEditorialSection,#atlasContextCard{background:#12171d;border:1px solid #28313b;border-radius:9px;}
#atlasIdentityField{background:transparent;border:0;}
#atlasSummaryField{background:transparent;border:0;padding:0;}
#atlasIdentityField #fieldTitle,#atlasEditorialSection #fieldTitle,#atlasContextCard #fieldTitle{color:#8e99a7;font-size:8pt;font-weight:700;letter-spacing:.65px;}
#atlasIdentityField QLineEdit,#atlasIdentityField QComboBox,#atlasSummaryField QTextEdit,#atlasEditorialSection QTextEdit,#atlasEditorialSection QLineEdit,#atlasEditorialSection QComboBox,#atlasContextCard QTextEdit,#atlasContextCard QLineEdit,#atlasContextCard QListWidget{background:#0d1116;color:#e2e6eb;border:0;border-radius:6px;padding:7px;selection-background-color:#5a4936;}
#atlasSummaryField QTextEdit{font-family:'Georgia';font-size:11pt;min-height:105px;}
#atlasEditorialSection QTextEdit{min-height:96px;}
#atlasContextCard QTextEdit{min-height:80px;}
#atlasEditorialSplit::handle{background:#252d37;}
#atlasCenterScroll QScrollBar:vertical,#atlasContextScroll QScrollBar:vertical{background:transparent;width:8px;}
#atlasCenterScroll QScrollBar::handle:vertical,#atlasContextScroll QScrollBar::handle:vertical{background:#343d48;border-radius:4px;min-height:30px;}
#pageKicker,#pageTitle,#pageDescription{background:transparent;color:transparent;max-height:0px;}
)QSS"));
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
        for (int i = 0; i < titleRow->count(); ++i)
            if (QWidget* widget = titleRow->itemAt(i)->widget()) widget->hide();
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
        "#mapWorkspaceBar{background:#171b21;border:1px solid #303843;border-radius:8px;}"
        "#mapWorkspaceKicker{color:#c59a5d;font-size:7pt;font-weight:700;letter-spacing:1px;padding:0 3px;}"
        "#mapPicker{color:#e1e6ec;border:0;background:transparent;font-weight:600;padding:4px 6px;}"
        "#mapWorkspaceName{background:#101419;color:#e1e6ec;border:1px solid #303843;border-radius:6px;min-height:20px;padding:4px 7px;}"
        "#mapChromeButton{color:#b9c2ce;background:transparent;border:0;padding:2px;font-size:12pt;}"
        "#mapChromeButton:hover{background:#252d37;}"
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
