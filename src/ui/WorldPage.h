#pragma once

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

class QLineEdit;
class QTabWidget;
class QTextEdit;

namespace wbw {

class ArchiveDocument;
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
        if (mapsChromeApplied_ || !mapList_ || !mapCanvas_) return;

        auto* mapsTab = findChild<QWidget*>(QStringLiteral("mapsTab"));
        auto* center = findChild<QWidget*>(QStringLiteral("mapEditorCard"));
        auto* left = findChild<QWidget*>(QStringLiteral("mapIndexPanel"));
        auto* right = findChild<QWidget*>(QStringLiteral("legacyMarkers"));
        auto* generator = findChild<QFrame*>(QStringLiteral("generatorBar"));
        if (!mapsTab || !center) return;

        mapsChromeApplied_ = true;

        if (left) left->hide();
        if (right) right->hide();
        if (generator) generator->hide();
        if (mapDescription_) mapDescription_->hide();

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
        mapPicker_->setMaximumWidth(260);
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

        titleRow->insertWidget(0, mapPicker_);
        titleRow->insertWidget(1, addMapButton);
        titleRow->insertWidget(2, removeMapButton);
        if (mapName_) {
            mapName_->setPlaceholderText(tr("Nombre del mapa"));
            mapName_->setMaximumWidth(420);
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
        connect(addMapButton, &QPushButton::clicked, this, &WorldPage::addMap);
        connect(removeMapButton, &QPushButton::clicked, this, &WorldPage::removeMap);
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
        ));
    }

private:
    friend class PilinReyEditor;

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
