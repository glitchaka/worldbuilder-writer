#include "ui/MapSelectionController.h"

#include "ui/PilinReyEditor.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QPointer>
#include <QShortcut>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace wbw {
namespace {

QToolButton* command(QWidget* parent, const QString& text, const QString& tip) {
    auto* button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("pilinCommand"));
    button->setText(text);
    button->setToolTip(tip);
    button->setMinimumSize(32, 30);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

QString objectCaption(const QJsonObject& object) {
    const QString type = object.value(QStringLiteral("type")).toString();
    QString kind;
    if (type == QStringLiteral("land")) kind = QObject::tr("Terreno");
    else if (type == QStringLiteral("sea")) kind = QObject::tr("Mar");
    else if (type == QStringLiteral("river")) kind = QObject::tr("Río");
    else if (type == QStringLiteral("road")) kind = QObject::tr("Camino");
    else if (type == QStringLiteral("border")) kind = QObject::tr("Frontera");
    else if (type == QStringLiteral("region")) kind = QObject::tr("Región");
    else if (type == QStringLiteral("forestArea")) kind = QObject::tr("Bosque");
    else if (type == QStringLiteral("mountainArea")) kind = QObject::tr("Montañas");
    else if (type == QStringLiteral("settlement")) kind = QObject::tr("Asentamiento");
    else if (type == QStringLiteral("stamp")) kind = QObject::tr("Asset");
    else if (type == QStringLiteral("label")) kind = QObject::tr("Etiqueta");
    else kind = type.isEmpty() ? QObject::tr("Objeto") : type;

    QString name = object.value(QStringLiteral("label")).toString().trimmed();
    if (name.isEmpty()) name = object.value(QStringLiteral("text")).toString().trimmed();
    if (name.isEmpty()) name = object.value(QStringLiteral("assetKind")).toString().trimmed();
    return name.isEmpty() ? kind : QStringLiteral("%1 · %2").arg(kind, name);
}

QFrame* layersPanel(PilinReyEditor* editor) {
    if (!editor) return nullptr;
    for (QFrame* frame : editor->findChildren<QFrame*>(QStringLiteral("pilinPopover"))) {
        for (QLabel* label : frame->findChildren<QLabel*>())
            if (label->text().trimmed() == QObject::tr("Capas")) return frame;
    }
    return nullptr;
}

void refreshObjectList(PilinReyEditor* editor, QListWidget* list) {
    if (!editor || !list) return;
    const QJsonObject pilin = editor->map().value(QStringLiteral("pilinRey")).toObject();
    const QString active = pilin.value(QStringLiteral("activeLayerId")).toString();
    const QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();
    QJsonArray objects;
    for (const QJsonValue& value : layers) {
        const QJsonObject layer = value.toObject();
        if (layer.value(QStringLiteral("id")).toString() == active) {
            objects = layer.value(QStringLiteral("objects")).toArray();
            break;
        }
    }

    list->blockSignals(true);
    list->clear();
    for (const QJsonValue& value : objects) {
        const QJsonObject object = value.toObject();
        auto* item = new QListWidgetItem(objectCaption(object), list);
        item->setData(Qt::UserRole, object.value(QStringLiteral("id")).toString());
        item->setToolTip(QObject::tr("Seleccionar y transformar este objeto"));
    }
    if (objects.isEmpty()) {
        auto* empty = new QListWidgetItem(QObject::tr("Esta capa aún no tiene objetos"), list);
        empty->setFlags(Qt::NoItemFlags);
    }
    list->blockSignals(false);
}

class MapSelectionFilter final : public QObject {
public:
    explicit MapSelectionFilter(QMainWindow* window) : QObject(window), window_(window) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        Q_UNUSED(watched);
        if (event->type() == QEvent::Show || event->type() == QEvent::LayoutRequest || event->type() == QEvent::ParentChange)
            QTimer::singleShot(0, this, [this]() { apply(); });
        return QObject::eventFilter(watched, event);
    }

    void apply() {
        if (!window_) return;
        auto* editor = window_->findChild<PilinReyEditor*>(QStringLiteral("pilinReyEditor"));
        auto* panel = window_->findChild<QFrame*>(QStringLiteral("pilinSelectionPopover"));
        if (!editor || !panel) return;

        if (!editor->property("wbwSelectionGeometryShortcuts").toBool()) {
            editor->setProperty("wbwSelectionGeometryShortcuts", true);
            auto makeShortcut = [editor](const QKeySequence& key, double dx, double dy) {
                auto* shortcut = new QShortcut(key, editor);
                shortcut->setContext(Qt::WidgetWithChildrenShortcut);
                QObject::connect(shortcut, &QShortcut::activated, editor, [editor, dx, dy]() { editor->nudgeSelection(dx, dy); });
            };
            makeShortcut(QKeySequence(QStringLiteral("Alt+Left")), -25.0, 0.0);
            makeShortcut(QKeySequence(QStringLiteral("Alt+Right")), 25.0, 0.0);
            makeShortcut(QKeySequence(QStringLiteral("Alt+Up")), 0.0, -25.0);
            makeShortcut(QKeySequence(QStringLiteral("Alt+Down")), 0.0, 25.0);
        }

        if (!panel->property("wbwSelectionGeometryControls").toBool()) {
            auto* layout = qobject_cast<QVBoxLayout*>(panel->layout());
            if (layout) {
                auto* label = new QLabel(QObject::tr("MOVER / GEOMETRÍA"), panel);
                label->setObjectName(QStringLiteral("pilinTinyLabel"));
                auto* movement = new QHBoxLayout;
                movement->setSpacing(4);
                auto* left = command(panel, QStringLiteral("←"), QObject::tr("Mover selección a la izquierda"));
                auto* up = command(panel, QStringLiteral("↑"), QObject::tr("Mover selección arriba"));
                auto* down = command(panel, QStringLiteral("↓"), QObject::tr("Mover selección abajo"));
                auto* right = command(panel, QStringLiteral("→"), QObject::tr("Mover selección a la derecha"));
                auto* rotateLeft = command(panel, QStringLiteral("↺"), QObject::tr("Rotar trazado −15°"));
                auto* rotateRight = command(panel, QStringLiteral("↻"), QObject::tr("Rotar trazado +15°"));
                auto* shrink = command(panel, QStringLiteral("↙"), QObject::tr("Reducir geometría de trazados"));
                auto* grow = command(panel, QStringLiteral("↗"), QObject::tr("Aumentar geometría de trazados"));
                auto* labelStyle = command(panel, QStringLiteral("Aa"), QObject::tr("Tamaño y peso de la etiqueta"));
                movement->addWidget(left); movement->addWidget(up); movement->addWidget(down); movement->addWidget(right);
                movement->addSpacing(5); movement->addWidget(rotateLeft); movement->addWidget(rotateRight); movement->addWidget(shrink); movement->addWidget(grow); movement->addWidget(labelStyle);
                layout->addWidget(label);
                layout->addLayout(movement);
                QObject::connect(left, &QToolButton::clicked, editor, [editor]() { editor->nudgeSelection(-25.0, 0.0); });
                QObject::connect(right, &QToolButton::clicked, editor, [editor]() { editor->nudgeSelection(25.0, 0.0); });
                QObject::connect(up, &QToolButton::clicked, editor, [editor]() { editor->nudgeSelection(0.0, -25.0); });
                QObject::connect(down, &QToolButton::clicked, editor, [editor]() { editor->nudgeSelection(0.0, 25.0); });
                QObject::connect(rotateLeft, &QToolButton::clicked, editor, [editor]() { editor->transformSelectedPathGeometry(1.0, -15.0); });
                QObject::connect(rotateRight, &QToolButton::clicked, editor, [editor]() { editor->transformSelectedPathGeometry(1.0, 15.0); });
                QObject::connect(shrink, &QToolButton::clicked, editor, [editor]() { editor->transformSelectedPathGeometry(0.9, 0.0); });
                QObject::connect(grow, &QToolButton::clicked, editor, [editor]() { editor->transformSelectedPathGeometry(1.1, 0.0); });
                QObject::connect(labelStyle, &QToolButton::clicked, editor, &PilinReyEditor::editSelectedLabelStyle);
                panel->setProperty("wbwSelectionGeometryControls", true);
                panel->adjustSize();
            }
        }

        QFrame* layers = layersPanel(editor);
        if (!layers) return;
        auto* objectList = layers->findChild<QListWidget*>(QStringLiteral("pilinLayerObjects"));
        if (!objectList) {
            auto* layout = qobject_cast<QVBoxLayout*>(layers->layout());
            if (!layout) return;
            auto* heading = new QLabel(QObject::tr("OBJETOS DE LA CAPA"), layers);
            heading->setObjectName(QStringLiteral("pilinTinyLabel"));
            objectList = new QListWidget(layers);
            objectList->setObjectName(QStringLiteral("pilinLayerObjects"));
            objectList->setMinimumHeight(96);
            objectList->setMaximumHeight(150);
            layout->insertWidget(2, heading);
            layout->insertWidget(3, objectList);
            QObject::connect(objectList, &QListWidget::itemClicked, editor, [editor](QListWidgetItem* item) {
                if (!item || !(item->flags() & Qt::ItemIsEnabled)) return;
                const QString id = item->data(Qt::UserRole).toString();
                if (!id.isEmpty()) editor->selectObjectIds(QStringList{id});
            });
            QObject::connect(editor, &PilinReyEditor::mapEdited, objectList, [editor, objectList](const QJsonObject&) {
                refreshObjectList(editor, objectList);
            });
            for (QListWidget* candidate : layers->findChildren<QListWidget*>()) {
                if (candidate == objectList) continue;
                QObject::connect(candidate, &QListWidget::currentRowChanged, objectList, [editor, objectList](int) {
                    QTimer::singleShot(0, objectList, [editor, objectList]() { refreshObjectList(editor, objectList); });
                });
                break;
            }
        }
        refreshObjectList(editor, objectList);
    }

private:
    QPointer<QMainWindow> window_;
};

} // namespace

void installMapSelectionController(QMainWindow* window) {
    if (!window || window->property("wbwMapSelectionControllerV3").toBool()) return;
    window->setProperty("wbwMapSelectionControllerV3", true);
    auto* filter = new MapSelectionFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
