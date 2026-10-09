#pragma once

#include "ui/MapExportDialog.h"
#include "ui/MapExporter.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QList>
#include <QMessageBox>
#include <QPushButton>
#include <QShowEvent>
#include <QSize>
#include <QVBoxLayout>
#include <QWidget>

#include <functional>

class QCheckBox;
class QComboBox;
class QListWidget;
class QSlider;
class QToolButton;

namespace wbw {

class PilinReyViewport;

class PilinReyEditor : public QWidget {
    Q_OBJECT
public:
    enum class Tool {
        Select,
        Pan,
        Coast,
        River,
        Road,
        Border,
        Forest,
        Mountain,
        Settlement,
        Label
    };

    explicit PilinReyEditor(QWidget* parent = nullptr);
    ~PilinReyEditor() override;

    void setMap(const QJsonObject& map);
    QJsonObject map() const { return map_; }

signals:
    void markerMoved(const QString& id, double x, double y);
    void addMarkerRequested(double x, double y);
    void markerActivated(const QString& id);
    void mapEdited(const QJsonObject& map);

protected:
    void showEvent(QShowEvent* event) override {
        QWidget::showEvent(event);

        auto* root = qobject_cast<QVBoxLayout*>(layout());
        if (!root) return;

        // Turn the original button row into a graphics-editor workspace the
        // first time the editor becomes visible. The tool rail follows the
        // Paint/Aseprite pattern: tools on the left, canvas in the middle,
        // properties/layers on the right, global commands above.
        if (!chromeApplied_ && root->count() >= 4) {
            chromeApplied_ = true;

            auto* commandBar = qobject_cast<QHBoxLayout*>(root->itemAt(1)->layout());
            auto* workspace = qobject_cast<QHBoxLayout*>(root->itemAt(2)->layout());
            if (commandBar && workspace && viewport_) {
                auto* inspector = workspace->count() > 1 ? workspace->itemAt(1)->widget() : nullptr;

                auto* toolRail = new QWidget(this);
                toolRail->setObjectName(QStringLiteral("pilinToolRail"));
                toolRail->setFixedWidth(48);
                auto* rail = new QVBoxLayout(toolRail);
                rail->setContentsMargins(5, 6, 5, 6);
                rail->setSpacing(5);

                const QStringList symbols{
                    QStringLiteral("↖"), QStringLiteral("✥"), QStringLiteral("≈"),
                    QStringLiteral("∿"), QStringLiteral("━"), QStringLiteral("┄"),
                    QStringLiteral("♣"), QStringLiteral("△"), QStringLiteral("●"),
                    QStringLiteral("T")
                };

                for (int i = 0; i < toolButtons_.size(); ++i) {
                    QToolButton* button = toolButtons_.at(i);
                    const QString label = button->text();
                    commandBar->removeWidget(button);
                    button->setToolTip(label);
                    button->setText(i < symbols.size() ? symbols.at(i) : label.left(1));
                    button->setObjectName(QStringLiteral("pilinMapTool"));
                    button->setFixedSize(36, 36);
                    button->setAutoRaise(false);
                    rail->addWidget(button, 0, Qt::AlignHCenter);
                }
                rail->addStretch(1);

                workspace->removeWidget(viewport_);
                if (inspector) workspace->removeWidget(inspector);
                workspace->insertWidget(0, toolRail, 0);
                workspace->insertWidget(1, viewport_, 1);
                if (inspector) {
                    inspector->setObjectName(QStringLiteral("pilinInspector"));
                    inspector->setMinimumWidth(250);
                    inspector->setMaximumWidth(290);
                    workspace->insertWidget(2, inspector, 0);

                    if (auto* inspectorLayout = qobject_cast<QVBoxLayout*>(inspector->layout())) {
                        commandBar->removeWidget(editObjectButton_);
                        commandBar->removeWidget(linkAtlasButton_);
                        commandBar->removeWidget(deleteObjectButton_);
                        inspectorLayout->addSpacing(8);
                        auto* selectionTitle = new QLabel(tr("Selección"));
                        selectionTitle->setObjectName(QStringLiteral("pilinInspectorTitle"));
                        inspectorLayout->addWidget(selectionTitle);
                        inspectorLayout->addWidget(editObjectButton_);
                        inspectorLayout->addWidget(linkAtlasButton_);
                        inspectorLayout->addWidget(deleteObjectButton_);
                    }
                }

                undoButton_->setText(tr("Deshacer"));
                redoButton_->setText(tr("Rehacer"));
                undoButton_->setObjectName(QStringLiteral("pilinCommand"));
                redoButton_->setObjectName(QStringLiteral("pilinCommand"));

                setStyleSheet(QStringLiteral(
                    "#pilinToolRail{background:#f5f7fa;border:1px solid #d7dee8;}"
                    "QToolButton#pilinMapTool{background:#ffffff;color:#344054;border:1px solid #d0d7e2;border-radius:3px;font-size:14pt;padding:0;}"
                    "QToolButton#pilinMapTool:hover{background:#eef4ff;border-color:#8bb5ef;}"
                    "QToolButton#pilinMapTool:checked{background:#1668d4;color:#ffffff;border-color:#1668d4;}"
                    "#pilinInspector{background:#ffffff;border-left:1px solid #d7dee8;}"
                    "#pilinInspectorTitle{color:#667085;font-size:8pt;font-weight:700;letter-spacing:.7px;padding-top:4px;}"
                    "QPushButton#pilinCommand{padding:5px 9px;}"
                    "QListWidget{background:#ffffff;border:1px solid #d7dee8;}"
                    "QSlider::groove:horizontal{height:4px;background:#d7dee8;}"
                    "QSlider::handle:horizontal{width:14px;margin:-5px 0;background:#1668d4;border-radius:7px;}"
                ));
            }
        }

        if (exportButton_) return;
        exportButton_ = new QPushButton(tr("Exportar…"), this);
        exportButton_->setObjectName(QStringLiteral("pilinCommand"));
        if (root->count() > 1) {
            if (auto* toolbar = qobject_cast<QHBoxLayout*>(root->itemAt(1)->layout())) {
                toolbar->addWidget(exportButton_);
            }
        }

        connect(exportButton_, &QPushButton::clicked, this, [this]() {
            const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
            const QSize logicalSize(
                qMax(1, pilin.value(QStringLiteral("width")).toInt(4096)),
                qMax(1, pilin.value(QStringLiteral("height")).toInt(2304)));

            MapExportDialog dialog(logicalSize, this);
            if (dialog.exec() != QDialog::Accepted) return;

            const QString format = dialog.format();
            QString filter;
            QString extension;
            if (format == QStringLiteral("svg")) {
                filter = tr("SVG (*.svg)");
                extension = QStringLiteral(".svg");
            } else if (format == QStringLiteral("pdf")) {
                filter = tr("PDF (*.pdf)");
                extension = QStringLiteral(".pdf");
            } else {
                filter = tr("PNG (*.png)");
                extension = QStringLiteral(".png");
            }

            QString base = map_.value(QStringLiteral("name")).toString().trimmed();
            if (base.isEmpty()) base = tr("mapa");
            QString path = QFileDialog::getSaveFileName(this, tr("Exportar mapa"), base + extension, filter);
            if (path.isEmpty()) return;
            if (!path.endsWith(extension, Qt::CaseInsensitive)) path += extension;

            QString error;
            bool ok = false;
            if (format == QStringLiteral("svg")) ok = MapExporter::exportSvg(map_, path, dialog.outputSize(), &error);
            else if (format == QStringLiteral("pdf")) ok = MapExporter::exportPdf(map_, path, dialog.outputSize(), &error);
            else ok = MapExporter::exportPng(map_, path, dialog.outputSize(), &error);

            if (!ok) {
                QMessageBox::critical(this, tr("No se pudo exportar"), error.isEmpty() ? tr("La exportación del mapa falló.") : error);
                return;
            }
            if (status_) status_->setText(tr("Mapa exportado: %1").arg(path));
        });
    }

private:
    void buildUi();
    void ensurePilinDocument();
    void refreshLayers();
    void refreshLayerControls();
    void refreshSelectionControls();
    void refreshViewport();
    void chooseTemplate();
    void clearTemplate();
    void addLayer();
    void duplicateLayer();
    void removeLayer();
    void moveLayer(int delta);
    void setLayerLocked(bool locked);
    void setLayerOpacity(int value);
    void setActiveTool(Tool tool);
    void addPathObject(const QString& type, const QJsonArray& points);
    void addSettlement(double x, double y);
    void addLabel(double x, double y);
    void selectObject(const QString& id);
    void movePointObject(const QString& id, double x, double y);
    void editSelectedObject();
    void deleteSelectedObject();
    void linkSelectedToAtlas();
    QJsonObject selectedObject() const;
    void mutateDocument(const std::function<void(QJsonObject&)>& mutation);
    void persistToArchive();
    void pushUndo();
    void undo();
    void redo();
    QString activeLayerId() const;

    QJsonObject map_;
    QList<QJsonObject> undoStack_;
    QList<QJsonObject> redoStack_;
    bool refreshing_ = false;
    bool chromeApplied_ = false;
    QString selectedObjectId_;

    PilinReyViewport* viewport_ = nullptr;
    QListWidget* layers_ = nullptr;
    QComboBox* style_ = nullptr;
    QSlider* templateOpacity_ = nullptr;
    QSlider* layerOpacity_ = nullptr;
    QCheckBox* layerLocked_ = nullptr;
    QLabel* status_ = nullptr;
    QPushButton* undoButton_ = nullptr;
    QPushButton* redoButton_ = nullptr;
    QPushButton* editObjectButton_ = nullptr;
    QPushButton* linkAtlasButton_ = nullptr;
    QPushButton* deleteObjectButton_ = nullptr;
    QPushButton* exportButton_ = nullptr;
    QList<QToolButton*> toolButtons_;
};

} // namespace wbw
