#include "ui/PilinReyEditor.h"

#include <QJsonObject>

namespace wbw {
namespace {

QJsonObject paletteForStyle(const QString& requested) {
    const QString style = requested.trimmed().toLower();

    if (style.contains(QStringLiteral("pergam"))) {
        return QJsonObject{
            {QStringLiteral("preset"), QStringLiteral("pergamino")},
            {QStringLiteral("sea"), QStringLiteral("#9bb1aa")},
            {QStringLiteral("river"), QStringLiteral("#587a82")},
            {QStringLiteral("road"), QStringLiteral("#825f3e")},
            {QStringLiteral("border"), QStringLiteral("#8b4c46")},
            {QStringLiteral("region"), QStringLiteral("#9b7651")},
            {QStringLiteral("symbol"), QStringLiteral("#3c3025")},
            {QStringLiteral("text"), QStringLiteral("#33291f")}
        };
    }

    if (style.contains(QStringLiteral("noct"))) {
        return QJsonObject{
            {QStringLiteral("preset"), QStringLiteral("nocturno")},
            {QStringLiteral("sea"), QStringLiteral("#203d54")},
            {QStringLiteral("river"), QStringLiteral("#5d9fc2")},
            {QStringLiteral("road"), QStringLiteral("#b69568")},
            {QStringLiteral("border"), QStringLiteral("#b56c79")},
            {QStringLiteral("region"), QStringLiteral("#8d789f")},
            {QStringLiteral("symbol"), QStringLiteral("#e5d7ba")},
            {QStringLiteral("text"), QStringLiteral("#eee1c7")}
        };
    }

    return QJsonObject{
        {QStringLiteral("preset"), QStringLiteral("atlas")},
        {QStringLiteral("sea"), QStringLiteral("#789fad")},
        {QStringLiteral("river"), QStringLiteral("#2d6382")},
        {QStringLiteral("road"), QStringLiteral("#75512f")},
        {QStringLiteral("border"), QStringLiteral("#7c4650")},
        {QStringLiteral("region"), QStringLiteral("#8e6547")},
        {QStringLiteral("symbol"), QStringLiteral("#2c2924")},
        {QStringLiteral("text"), QStringLiteral("#29251f")}
    };
}

} // namespace

void PilinReyEditor::applyMapStylePreset(const QString& styleName) {
    if (map_.isEmpty()) return;

    const QJsonObject desired = paletteForStyle(styleName);
    const QString preset = desired.value(QStringLiteral("preset")).toString();
    const QJsonObject current = map_.value(QStringLiteral("pilinRey")).toObject()
                                    .value(QStringLiteral("theme")).toObject();

    // A preset is applied once when the map style changes. Subsequent manual
    // colour edits stay intact because they do not clear the preset marker.
    if (current.value(QStringLiteral("preset")).toString() == preset) return;

    mutateDocument([&](QJsonObject& pilin) {
        QJsonObject theme = pilin.value(QStringLiteral("theme")).toObject();
        for (auto it = desired.constBegin(); it != desired.constEnd(); ++it)
            theme.insert(it.key(), it.value());
        pilin.insert(QStringLiteral("theme"), theme);
    });
}

} // namespace wbw
