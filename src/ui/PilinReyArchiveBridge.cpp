#include "ui/PilinReyEditor.h"

#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QToolButton>

namespace wbw {

void PilinReyEditor::setArchiveDocument(ArchiveDocument* document) {
    archiveDocument_ = document;
    if (!linkAtlasButton_) return;

    QObject::disconnect(linkAtlasButton_, &QToolButton::clicked, this, &PilinReyEditor::linkSelectedToAtlas);
    QObject::disconnect(linkAtlasButton_, &QToolButton::clicked, this, &PilinReyEditor::linkSelectedToArchiveAtlas);
    QObject::connect(linkAtlasButton_, &QToolButton::clicked, this, &PilinReyEditor::linkSelectedToArchiveAtlas, Qt::UniqueConnection);
}

void PilinReyEditor::linkSelectedToArchiveAtlas() {
    if (!archiveDocument_) {
        linkSelectedToAtlas();
        return;
    }

    const QJsonObject object = selectedObject();
    if (object.value(QStringLiteral("type")).toString() != QStringLiteral("settlement") || selectedObjectIds_.size() != 1) return;

    const QJsonArray atlasEntries = archiveDocument_->array(QStringLiteral("world"));
    if (atlasEntries.isEmpty()) {
        QMessageBox::information(this, tr("Atlas"), tr("Crea primero una entrada en el Atlas para poder vincular este asentamiento."));
        return;
    }

    QStringList names;
    QStringList ids;
    for (const QJsonValue& value : atlasEntries) {
        const QJsonObject entry = value.toObject();
        const QString id = entry.value(QStringLiteral("id")).toString();
        if (id.isEmpty()) continue;
        names.append(entry.value(QStringLiteral("name")).toString(tr("Entrada sin nombre")));
        ids.append(id);
    }
    if (ids.isEmpty()) return;

    bool ok = false;
    const QString chosen = QInputDialog::getItem(this, tr("Enlazar con Atlas"), tr("Entrada:"), names, 0, false, &ok);
    if (!ok) return;
    const int selectedIndex = names.indexOf(chosen);
    if (selectedIndex < 0 || selectedIndex >= ids.size()) return;

    const QString objectId = selectedObjectIds_.first();
    const QString atlasId = ids.at(selectedIndex);
    mutateDocument([&](QJsonObject& pilin) {
        QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();
        for (int li = 0; li < layers.size(); ++li) {
            QJsonObject layer = layers.at(li).toObject();
            QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
            for (int oi = 0; oi < objects.size(); ++oi) {
                QJsonObject current = objects.at(oi).toObject();
                if (current.value(QStringLiteral("id")).toString() != objectId) continue;
                current.insert(QStringLiteral("atlasId"), atlasId);
                current.insert(QStringLiteral("atlasLabel"), names.at(selectedIndex));
                objects.replace(oi, current);
                layer.insert(QStringLiteral("objects"), objects);
                layers.replace(li, layer);
                pilin.insert(QStringLiteral("layers"), layers);
                return;
            }
        }
    });
}

} // namespace wbw
