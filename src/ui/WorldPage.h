#pragma once

#include <QWidget>

class QComboBox;
class QFrame;
class QLineEdit;
class QListWidget;
class QShowEvent;
class QSpinBox;
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
    void showEvent(QShowEvent* event) override;

private:
    friend class PilinReyEditor;

    void applyAtlasChrome();
    void applyMapsChrome();
    void syncMapPicker();
    void generateTerrain();

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
