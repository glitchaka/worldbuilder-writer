#include "ui/PilinReyEditor.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>

#include <algorithm>
#include <cmath>

namespace wbw {

void PilinReyEditor::nudgeSelection(double dx, double dy) {
    if (selectedObjectIds_.isEmpty() || (dx == 0.0 && dy == 0.0)) return;
    mutateDocument([&](QJsonObject& pilin) {
        QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();
        for (int li = 0; li < layers.size(); ++li) {
            QJsonObject layer = layers.at(li).toObject();
            if (layer.value(QStringLiteral("locked")).toBool(false)) continue;
            QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
            bool changed = false;
            for (int oi = 0; oi < objects.size(); ++oi) {
                QJsonObject object = objects.at(oi).toObject();
                if (!selectedObjectIds_.contains(object.value(QStringLiteral("id")).toString())) continue;
                if (object.contains(QStringLiteral("x"))) {
                    object.insert(QStringLiteral("x"), object.value(QStringLiteral("x")).toDouble() + dx);
                    object.insert(QStringLiteral("y"), object.value(QStringLiteral("y")).toDouble() + dy);
                    changed = true;
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
                    changed = true;
                }
                if (changed) objects.replace(oi, object);
            }
            if (changed) {
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

} // namespace wbw
