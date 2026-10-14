#include "ui/PilinReyEditor.h"

#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace wbw {
namespace {

QLabel* sectionLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("pilinStudioSection"));
    return label;
}

QWidget* sliderRow(const QString& name, QSlider* slider, QWidget* parent) {
    auto* row = new QWidget(parent);
    row->setObjectName(QStringLiteral("pilinStudioSliderRow"));
    auto* layout = new QVBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);

    auto* header = new QHBoxLayout;
    auto* label = new QLabel(name, row);
    label->setObjectName(QStringLiteral("pilinStudioField"));
    auto* value = new QLabel(QString::number(slider ? slider->value() : 0), row);
    value->setObjectName(QStringLiteral("pilinStudioValue"));
    header->addWidget(label);
    header->addStretch(1);
    header->addWidget(value);
    layout->addLayout(header);

    if (slider) {
        slider->setParent(row);
        slider->setMinimumWidth(0);
        slider->setMaximumWidth(QWIDGETSIZE_MAX);
        layout->addWidget(slider);
        QObject::connect(slider, &QSlider::valueChanged, value, [value](int v) { value->setText(QString::number(v)); });
    }
    return row;
}

} // namespace

void PilinReyEditor::configureProductWorkspace() {
    if (property("wbwProductWorkspaceV3").toBool()) {
        if (auto* left = findChild<QWidget*>(QStringLiteral("pilinStudioLeft"))) left->show();
        if (auto* right = findChild<QWidget*>(QStringLiteral("pilinStudioRight"))) right->show();
        if (layersPopover_) layersPopover_->show();
        if (assetsPopover_) assetsPopover_->show();
        return;
    }
    setProperty("wbwProductWorkspaceV3", true);

    auto* root = qobject_cast<QVBoxLayout*>(layout());
    if (!root || !canvasHost_) return;

    // The old product patch floated every control over the canvas. Replace that
    // architecture with the actual map-studio layout: toolbar, tool dock,
    // document canvas, and persistent layers/assets dock.
    if (topCommands_) {
        QFrame* toolbar = topCommands_;
        toolbar->setParent(this);
        toolbar->setObjectName(QStringLiteral("pilinStudioToolbar"));
        toolbar->setMinimumHeight(50);
        toolbar->setMaximumHeight(54);
        root->insertWidget(0, toolbar);
        topCommands_ = nullptr; // prevents the legacy floating-position code from moving it again
    }

    root->removeWidget(canvasHost_);

    auto* body = new QFrame(this);
    body->setObjectName(QStringLiteral("pilinStudioBody"));
    auto* bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    auto* left = new QFrame(body);
    left->setObjectName(QStringLiteral("pilinStudioLeft"));
    left->setMinimumWidth(270);
    left->setMaximumWidth(300);
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(14, 14, 14, 14);
    leftLayout->setSpacing(9);

    auto* leftTitle = new QLabel(tr("Pinceles y terreno"), left);
    leftTitle->setObjectName(QStringLiteral("pilinStudioTitle"));
    auto* leftCopy = new QLabel(tr("Modela el mundo directamente. El relieve modifica el heightmap; no pinta una imagen por encima."), left);
    leftCopy->setObjectName(QStringLiteral("pilinStudioCopy"));
    leftCopy->setWordWrap(true);
    leftLayout->addWidget(leftTitle);
    leftLayout->addWidget(leftCopy);
    leftLayout->addSpacing(4);

    auto* toolGrid = new QGridLayout;
    toolGrid->setContentsMargins(0, 0, 0, 0);
    toolGrid->setHorizontalSpacing(7);
    toolGrid->setVerticalSpacing(7);
    const QStringList names{
        tr("Seleccionar"), tr("Mover"), tr("Elevar"), tr("Hundir"),
        tr("Cordillera"), tr("Bioma"), tr("Río"), tr("Lago"),
        tr("Ruta"), tr("Frontera"), tr("Región"), tr("Asentamiento"),
        tr("Etiqueta"), tr("Medir")
    };
    for (int i = 0; i < toolButtons_.size(); ++i) {
        QToolButton* button = toolButtons_.at(i);
        if (!button) continue;
        const QString glyph = button->text();
        button->setParent(left);
        button->setObjectName(QStringLiteral("pilinStudioTool"));
        button->setText(glyph + QLatin1Char('\n') + (i < names.size() ? names.at(i) : button->toolTip()));
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setMinimumSize(112, 52);
        button->setMaximumHeight(58);
        toolGrid->addWidget(button, i / 2, i % 2);
    }
    leftLayout->addLayout(toolGrid);

    if (toolRail_) {
        toolRail_->hide();
        toolRail_->deleteLater();
        toolRail_ = nullptr; // no more floating rail
    }

    if (QWidget* legacyOptions = canvasHost_->findChild<QWidget*>(QStringLiteral("pilinToolOptions"))) {
        legacyOptions->setObjectName(QStringLiteral("pilinToolOptionsLegacy"));
        legacyOptions->hide();
    }

    leftLayout->addSpacing(5);
    leftLayout->addWidget(sectionLabel(tr("PINCEL"), left));
    leftLayout->addWidget(sliderRow(tr("Tamaño"), brushRadiusSlider_, left));
    leftLayout->addWidget(sliderRow(tr("Intensidad"), densitySlider_, left));
    leftLayout->addWidget(sliderRow(tr("Trazo"), strokeWidthSlider_, left));
    leftLayout->addWidget(sliderRow(tr("Símbolo"), symbolSizeSlider_, left));

    auto* terrainActions = new QHBoxLayout;
    auto* smooth = new QPushButton(tr("Suavizar"), left);
    auto* erode = new QPushButton(tr("Erosión"), left);
    smooth->setObjectName(QStringLiteral("pilinStudioAction"));
    erode->setObjectName(QStringLiteral("pilinStudioAction"));
    terrainActions->addWidget(smooth);
    terrainActions->addWidget(erode);
    leftLayout->addLayout(terrainActions);
    connect(smooth, &QPushButton::clicked, this, &PilinReyEditor::smoothTerrain);
    connect(erode, &QPushButton::clicked, this, &PilinReyEditor::erodeTerrain);

    auto* normalize = new QPushButton(tr("Normalizar alturas"), left);
    normalize->setObjectName(QStringLiteral("pilinStudioAction"));
    connect(normalize, &QPushButton::clicked, this, &PilinReyEditor::normalizeTerrain);
    leftLayout->addWidget(normalize);
    leftLayout->addStretch(1);

    canvasHost_->setParent(body);
    canvasHost_->setMinimumWidth(520);

    auto* right = new QFrame(body);
    right->setObjectName(QStringLiteral("pilinStudioRight"));
    right->setMinimumWidth(300);
    right->setMaximumWidth(340);
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    auto* tabs = new QTabWidget(right);
    tabs->setObjectName(QStringLiteral("pilinStudioTabs"));
    tabs->setDocumentMode(true);
    tabs->setUsesScrollButtons(false);
    rightLayout->addWidget(tabs, 1);

    if (layersPopover_) {
        layersPopover_->setParent(tabs);
        layersPopover_->setMinimumWidth(0);
        layersPopover_->setMaximumWidth(QWIDGETSIZE_MAX);
        layersPopover_->setFrameShape(QFrame::NoFrame);
        layersPopover_->show();
        tabs->addTab(layersPopover_, tr("Capas"));
    }
    if (assetsPopover_) {
        assetsPopover_->setParent(tabs);
        assetsPopover_->setMinimumWidth(0);
        assetsPopover_->setMaximumWidth(QWIDGETSIZE_MAX);
        assetsPopover_->setFrameShape(QFrame::NoFrame);
        assetsPopover_->show();
        tabs->addTab(assetsPopover_, tr("Assets"));
    }

    if (layersButton_) layersButton_->hide();
    if (assetsButton_) assetsButton_->hide();

    if (layers_) {
        layers_->setMinimumHeight(260);
        layers_->setMaximumHeight(QWIDGETSIZE_MAX);
        layers_->setAlternatingRowColors(false);
    }
    if (assets_) {
        assets_->setMinimumHeight(230);
        assets_->setMaximumHeight(QWIDGETSIZE_MAX);
        assets_->setViewMode(QListView::IconMode);
        assets_->setResizeMode(QListView::Adjust);
        assets_->setMovement(QListView::Static);
        assets_->setGridSize(QSize(92, 78));
        assets_->setIconSize(QSize(54, 54));
    }

    if (assetsPopover_ && assets_ && !property("wbwAssetBrowserV3").toBool()) {
        setProperty("wbwAssetBrowserV3", true);
        if (auto* layout = qobject_cast<QVBoxLayout*>(assetsPopover_->layout())) {
            auto* search = new QLineEdit(assetsPopover_);
            search->setObjectName(QStringLiteral("pilinAssetSearch"));
            search->setPlaceholderText(tr("Buscar assets…"));
            search->setClearButtonEnabled(true);

            auto* categories = new QComboBox(assetsPopover_);
            categories->setObjectName(QStringLiteral("pilinAssetCategory"));

            auto* importFolder = new QPushButton(tr("Importar carpeta de assets…"), assetsPopover_);
            importFolder->setObjectName(QStringLiteral("pilinImportAssetFolder"));
            importFolder->setToolTip(tr("Importa PNG, JPG y WebP propios o con licencia compatible. No se generan recursos mediante IA."));

            layout->insertWidget(1, search);
            layout->insertWidget(2, categories);
            layout->addWidget(importFolder);

            const auto refreshCategories = [this, categories]() {
                const QSignalBlocker blocker(categories);
                const QString previous = categories->currentText();
                categories->clear();
                categories->addItem(tr("Todos"));
                QStringList values;
                for (const QJsonValue& value : map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("assets")).toArray()) {
                    const QString category = value.toObject().value(QStringLiteral("category")).toString().trimmed();
                    if (!category.isEmpty() && !values.contains(category, Qt::CaseInsensitive)) values.append(category);
                }
                values.sort(Qt::CaseInsensitive);
                categories->addItems(values);
                const int index = categories->findText(previous);
                categories->setCurrentIndex(index >= 0 ? index : 0);
            };

            const auto filterAssets = [this, search, categories]() {
                const QString query = search->text().trimmed();
                const QString category = categories->currentIndex() > 0 ? categories->currentText() : QString();
                const QJsonArray values = map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("assets")).toArray();
                for (int row = 0; row < assets_->count(); ++row) {
                    QListWidgetItem* item = assets_->item(row);
                    const QJsonObject asset = row < values.size() ? values.at(row).toObject() : QJsonObject();
                    const bool textMatches = query.isEmpty() || asset.value(QStringLiteral("name")).toString().contains(query, Qt::CaseInsensitive);
                    const bool categoryMatches = category.isEmpty() || asset.value(QStringLiteral("category")).toString().compare(category, Qt::CaseInsensitive) == 0;
                    item->setHidden(!(textMatches && categoryMatches));
                }
            };

            refreshCategories();
            connect(search, &QLineEdit::textChanged, this, [filterAssets](const QString&) { filterAssets(); });
            connect(categories, &QComboBox::currentTextChanged, this, [filterAssets](const QString&) { filterAssets(); });
            connect(importFolder, &QPushButton::clicked, this, [this, refreshCategories, filterAssets]() {
                importAssetDirectory();
                refreshCategories();
                filterAssets();
            });
            connect(assets_, &QListWidget::itemClicked, this, [this]() {
                QTimer::singleShot(0, this, [this]() {
                    if (assetsPopover_) assetsPopover_->show();
                });
            });
        }
    }

    bodyLayout->addWidget(left);
    bodyLayout->addWidget(canvasHost_, 1);
    bodyLayout->addWidget(right);
    root->addWidget(body, 1);

    setStyleSheet(styleSheet() + QStringLiteral(R"QSS(
#pilinStudioToolbar{background:#0c1117;border:0;border-bottom:1px solid #26313b;border-radius:0;}
#pilinStudioBody{background:#090d12;}
#pilinStudioLeft,#pilinStudioRight{background:#0d131a;border:0;}
#pilinStudioLeft{border-right:1px solid #26313b;}
#pilinStudioRight{border-left:1px solid #26313b;}
#pilinStudioTitle{color:#f0ebe3;font-family:'Georgia';font-size:14pt;font-weight:600;}
#pilinStudioCopy{color:#7e8b96;font-size:8.2pt;}
#pilinStudioSection{color:#7c8994;font-size:7.5pt;font-weight:700;letter-spacing:1px;margin-top:3px;}
#pilinStudioField{color:#aab5bf;font-size:8pt;}
#pilinStudioValue{color:#6f7d89;font-size:8pt;}
QToolButton#pilinStudioTool{background:#111922;color:#aab6c0;border:1px solid #293641;border-radius:8px;padding:6px;font-size:8.2pt;}
QToolButton#pilinStudioTool:hover{background:#182530;color:#ffffff;border-color:#385064;}
QToolButton#pilinStudioTool:checked{background:#17384a;color:#9bd9e7;border-color:#3f8199;}
#pilinStudioAction{background:#111922;color:#c7d0d8;border:1px solid #2d3944;border-radius:7px;padding:7px 8px;}
#pilinStudioAction:hover{background:#1a2731;color:#ffffff;}
#pilinStudioTabs::pane{border:0;background:#0d131a;}
#pilinStudioTabs QTabBar::tab{background:#0d131a;color:#7f8b96;border:0;border-bottom:2px solid transparent;padding:10px 18px;}
#pilinStudioTabs QTabBar::tab:selected{color:#dbe6ec;border-bottom-color:#3fc1c6;}
#pilinStudioRight #pilinPopover{background:#0d131a;border:0;border-radius:0;}
#pilinAssetSearch,#pilinAssetCategory{background:#101820;color:#dce4ea;border:1px solid #2b3742;border-radius:7px;padding:7px 9px;}
#pilinImportAssetFolder{background:#16232d;color:#cfd9e0;border:1px solid #334554;border-radius:7px;padding:8px 10px;text-align:left;}
#pilinImportAssetFolder:hover{background:#20313e;color:#ffffff;}
)QSS"));

    QTimer::singleShot(0, this, [this, tabs]() {
        if (layersPopover_) layersPopover_->show();
        if (assetsPopover_) assetsPopover_->show();
        if (tabs && tabs->count()) tabs->setCurrentIndex(0);
        if (viewport_) viewport_->fitDocument();
    });
}

} // namespace wbw
