#include "ui/PilinReyEditor.h"

#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>

#include <algorithm>
#include <cmath>

namespace wbw {

void PilinReyEditor::selectObjectIds(const QStringList& ids) {
    selectObjects(ids);
}

void PilinReyEditor::nudgeSelection(double dx, double dy) {
    if (selectedObjectIds_.isEmpty() || (dx == 0.0 && dy == 0.0)) return;
    mutateDocument([&](QJsonObject& pilin) {
        QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();
        for (int li = 0; li < layers.size(); ++li) {
            QJsonObject layer = layers.at(li).toObject();
            if (layer.value(QStringLiteral("locked")).toBool(false)) continue;
            QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
            bool layerChanged = false;
            for (int oi = 0; oi < objects.size(); ++oi) {
                QJsonObject object = objects.at(oi).toObject();
                if (!selectedObjectIds_.contains(object.value(QStringLiteral("id")).toString())) continue;
                bool objectChanged = false;
                if (object.contains(QStringLiteral("x"))) {
                    object.insert(QStringLiteral("x"), object.value(QStringLiteral("x")).toDouble() + dx);
                    object.insert(QStringLiteral("y"), object.value(QStringLiteral("y")).toDouble() + dy);
                    objectChanged = true;
                }
                QJsonArray points = object.value(QStringLiteral("points")).toArray();
                if (!points.isEmpty()) {
                    for (int pi = 0; pi < points.size(); ++pi) {
                        QJsonObject point = points.at(pi).toObject();
                        point.insert(QStringLiteral("x"), point.value(QStringLiteral("x")).toDouble() + dx);
                        point.insert(QStringLiteral("y"), point.value(QStringLiteral("y")).toDouble() + dy);
                        points.replace(pi, point);
                    }
                    object.insert(QStringLiteral("points"), points);
                    objectChanged = true;
                }
                if (objectChanged) {
                    objects.replace(oi, object);
                    layerChanged = true;
                }
            }
            if (layerChanged) {
                layer.insert(QStringLiteral("objects"), objects);
                layers.replace(li, layer);
            }
        }
        pilin.insert(QStringLiteral("layers"), layers);
    });
}

void PilinReyEditor::transformSelectedPathGeometry(double scaleFactor, double rotationDegrees) {
    if (selectedObjectIds_.isEmpty()) return;
    const double radians = rotationDegrees * 3.14159265358979323846 / 180.0;
    const double cs = std::cos(radians);
    const double sn = std::sin(radians);
    const double safeScale = std::clamp(scaleFactor, 0.25, 4.0);

    mutateDocument([&](QJsonObject& pilin) {
        QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();
        for (int li = 0; li < layers.size(); ++li) {
            QJsonObject layer = layers.at(li).toObject();
            if (layer.value(QStringLiteral("locked")).toBool(false)) continue;
            QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
            bool layerChanged = false;
            for (int oi = 0; oi < objects.size(); ++oi) {
                QJsonObject object = objects.at(oi).toObject();
                if (!selectedObjectIds_.contains(object.value(QStringLiteral("id")).toString())) continue;
                QJsonArray points = object.value(QStringLiteral("points")).toArray();
                if (points.size() < 2) continue;

                int logicalCount = points.size();
                const bool closed = object.value(QStringLiteral("closed")).toBool(false) && points.size() > 2;
                if (closed) logicalCount -= 1;
                if (logicalCount <= 0) continue;

                QPointF center;
                for (int pi = 0; pi < logicalCount; ++pi) {
                    const QJsonObject point = points.at(pi).toObject();
                    center += QPointF(point.value(QStringLiteral("x")).toDouble(), point.value(QStringLiteral("y")).toDouble());
                }
                center /= static_cast<qreal>(logicalCount);

                for (int pi = 0; pi < logicalCount; ++pi) {
                    QJsonObject point = points.at(pi).toObject();
                    const QPointF source(point.value(QStringLiteral("x")).toDouble(), point.value(QStringLiteral("y")).toDouble());
                    const QPointF d = (source - center) * safeScale;
                    const QPointF transformed(center.x() + d.x() * cs - d.y() * sn,
                                              center.y() + d.x() * sn + d.y() * cs);
                    point.insert(QStringLiteral("x"), transformed.x());
                    point.insert(QStringLiteral("y"), transformed.y());
                    points.replace(pi, point);
                }
                if (closed && !points.isEmpty()) points.replace(points.size() - 1, points.first());
                object.insert(QStringLiteral("points"), points);
                objects.replace(oi, object);
                layerChanged = true;
            }
            if (layerChanged) {
                layer.insert(QStringLiteral("objects"), objects);
                layers.replace(li, layer);
            }
        }
        pilin.insert(QStringLiteral("layers"), layers);
    });
}

void PilinReyEditor::editSelectedLabelStyle() {
    if (selectedObjectIds_.size() != 1) return;
    const QString id = selectedObjectIds_.first();
    QJsonObject current;
    for (const QJsonValue& layerValue : map_.value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray()) {
        for (const QJsonValue& objectValue : layerValue.toObject().value(QStringLiteral("objects")).toArray()) {
            const QJsonObject object = objectValue.toObject();
            if (object.value(QStringLiteral("id")).toString() == id) {
                current = object;
                break;
            }
        }
        if (!current.isEmpty()) break;
    }
    if (current.value(QStringLiteral("type")).toString() != QStringLiteral("label")) return;

    bool ok = false;
    const int fontSize = QInputDialog::getInt(this, tr("Estilo de etiqueta"), tr("Tamaño:"),
                                               current.value(QStringLiteral("fontSize")).toInt(54), 18, 220, 2, &ok);
    if (!ok) return;
    const QStringList weights{tr("Normal"), tr("Negrita")};
    const QString weight = QInputDialog::getItem(this, tr("Estilo de etiqueta"), tr("Peso:"), weights,
                                                  current.value(QStringLiteral("bold")).toBool(false) ? 1 : 0,
                                                  false, &ok);
    if (!ok) return;

    mutateDocument([&](QJsonObject& pilin) {
        QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();
        for (int li = 0; li < layers.size(); ++li) {
            QJsonObject layer = layers.at(li).toObject();
            QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
            for (int oi = 0; oi < objects.size(); ++oi) {
                QJsonObject object = objects.at(oi).toObject();
                if (object.value(QStringLiteral("id")).toString() != id) continue;
                object.insert(QStringLiteral("fontSize"), fontSize);
                object.insert(QStringLiteral("bold"), weight == tr("Negrita"));
                objects.replace(oi, object);
                layer.insert(QStringLiteral("objects"), objects);
                layers.replace(li, layer);
                pilin.insert(QStringLiteral("layers"), layers);
                return;
            }
        }
    });
}

} // namespace wbw
