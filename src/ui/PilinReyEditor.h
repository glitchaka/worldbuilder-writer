#pragma once

#include "ui/MapExportDialog.h"
#include "ui/MapExporter.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
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
class QLabel;
class QListWidget;
class QSlider;
class QToolButton;

namespace wbw {

class PilinReyViewport;

class PilinReyEditor final : public QWidget {
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
    // Compatibility signals kept temporarily while legacy map records are migrated.
    void markerMoved(const QString& id, double x, double y);
    void addMarkerRequested(double x, double y);
    void markerActivated(const QString& id);
    void mapEdited(const QJsonObject& map);

protected:
    void showEvent(QShowEvent* event) override {
        QWidget::showEvent(event);
        if (exportButton_) return;

        exportButton_ = new QPushButton(tr("Exportar…"), this);
        if (auto* root = qobject_cast<QVBoxLayout*>(layout())) {
            if (root->count() > 1) {
                if (auto* toolbar = qobject_cast<QHBoxLayout*>(root->itemAt(1)->layout())) {
                    toolbar->addWidget(exportButton_);
                }
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
