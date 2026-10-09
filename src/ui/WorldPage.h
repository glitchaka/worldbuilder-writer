#pragma once

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QShowEvent>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

class QLineEdit;
class QListWidget;
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
        if (mapGeneratorOrganized_ || !mapStyle_ || !mapSeed_ || !mapContinents_ || !mapIslands_ || !mapRoughness_) return;

        auto* generator = findChild<QFrame*>(QStringLiteral("generatorBar"));
        auto* row = generator ? qobject_cast<QHBoxLayout*>(generator->layout()) : nullptr;
        if (!generator || !row) return;
        mapGeneratorOrganized_ = true;

        while (QLayoutItem* item = row->takeAt(0)) {
            if (QWidget* widget = item->widget()) {
                const bool keep = widget == mapStyle_ || widget == mapSeed_ || widget == mapContinents_ || widget == mapIslands_ || widget == mapRoughness_;
                if (!keep) widget->deleteLater();
            }
            delete item;
        }

        generator->setObjectName(QStringLiteral("generatorBar"));
        row->setContentsMargins(12, 8, 12, 8);
        row->setSpacing(10);

        auto* heading = new QWidget(generator);
        heading->setObjectName(QStringLiteral("mapGeneratorHeading"));
        auto* headingLayout = new QVBoxLayout(heading);
        headingLayout->setContentsMargins(0, 0, 8, 0);
        headingLayout->setSpacing(0);
        auto* title = new QLabel(tr("Generador"), heading);
        title->setObjectName(QStringLiteral("mapGeneratorTitle"));
        auto* subtitle = new QLabel(tr("AUXILIAR"), heading);
        subtitle->setObjectName(QStringLiteral("mapGeneratorSubtitle"));
        headingLayout->addWidget(title);
        headingLayout->addWidget(subtitle);
        row->addWidget(heading);

        auto addField = [generator, row](const QString& labelText, QWidget* field, int width) {
            auto* box = new QWidget(generator);
            box->setObjectName(QStringLiteral("mapGeneratorField"));
            auto* layout = new QVBoxLayout(box);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(2);
            auto* label = new QLabel(labelText, box);
            label->setObjectName(QStringLiteral("mapGeneratorLabel"));
            field->setObjectName(QStringLiteral("mapGeneratorValue"));
            field->setFixedWidth(width);
            field->setFixedHeight(28);
            layout->addWidget(label);
            layout->addWidget(field);
            row->addWidget(box);
        };

        mapSeed_->setButtonSymbols(QAbstractSpinBox::NoButtons);
        mapContinents_->setButtonSymbols(QAbstractSpinBox::NoButtons);
        mapIslands_->setButtonSymbols(QAbstractSpinBox::NoButtons);
        mapRoughness_->setButtonSymbols(QAbstractSpinBox::NoButtons);
        mapSeed_->setAlignment(Qt::AlignCenter);
        mapContinents_->setAlignment(Qt::AlignCenter);
        mapIslands_->setAlignment(Qt::AlignCenter);
        mapRoughness_->setAlignment(Qt::AlignCenter);

        addField(tr("Estilo"), mapStyle_, 118);
        addField(tr("Semilla"), mapSeed_, 96);
        addField(tr("Continentes"), mapContinents_, 76);
        addField(tr("Islas"), mapIslands_, 64);
        addField(tr("Rugosidad"), mapRoughness_, 76);
        row->addStretch(1);

        setStyleSheet(styleSheet() + QStringLiteral(
            "#generatorBar{background:#f7f9fc;border:1px solid #d8dee7;border-radius:3px;}"
            "#mapGeneratorTitle{font-weight:600;color:#344054;}"
            "#mapGeneratorSubtitle,#mapGeneratorLabel{font-size:7.5pt;font-weight:700;color:#7a8697;letter-spacing:.45px;}"
            "#mapGeneratorValue{background:#ffffff;border:1px solid #cfd6df;border-radius:2px;padding:3px 7px;}"
            "#mapGeneratorValue:focus{border-color:#6ea3e2;}"
            "#mapGeneratorValue::up-button,#mapGeneratorValue::down-button{width:0;height:0;border:0;}"
            "#mapGeneratorValue::drop-down{width:0;border:0;}"
            "#mapGeneratorValue::down-arrow{image:none;width:0;height:0;}"
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
    bool mapGeneratorOrganized_ = false;
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
