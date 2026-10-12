#include "ui/MapSelectionController.h"

#include "ui/PilinReyEditor.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QPainter>
#include <QPointer>
#include <QShortcut>
#include <QSpinBox>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>

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
        for (QLabel* label : frame->findChildren<QLabel*>()) {
            if (label->text().trimmed() == QObject::tr("Capas")) return frame;
        }
    }
    return nullptr;
}

QJsonObject objectById(PilinReyEditor* editor, const QString& id) {
    if (!editor || id.isEmpty()) return {};
    const QJsonArray layers = editor->map().value(QStringLiteral("pilinRey")).toObject().value(QStringLiteral("layers")).toArray();
    for (const QJsonValue& layerValue : layers) {
        for (const QJsonValue& objectValue : layerValue.toObject().value(QStringLiteral("objects")).toArray()) {
            const QJsonObject object = objectValue.toObject();
            if (object.value(QStringLiteral("id")).toString() == id) return object;
        }
    }
    return {};
}

void replaceObject(PilinReyEditor* editor, const QString& id, const QJsonObject& replacement) {
    if (!editor || id.isEmpty()) return;
    QJsonObject map = editor->map();
    QJsonObject pilin = map.value(QStringLiteral("pilinRey")).toObject();
    QJsonArray layers = pilin.value(QStringLiteral("layers")).toArray();
    bool found = false;

    for (int li = 0; li < layers.size() && !found; ++li) {
        QJsonObject layer = layers.at(li).toObject();
        QJsonArray objects = layer.value(QStringLiteral("objects")).toArray();
        for (int oi = 0; oi < objects.size(); ++oi) {
            if (objects.at(oi).toObject().value(QStringLiteral("id")).toString() != id) continue;
            objects.replace(oi, replacement);
            layer.insert(QStringLiteral("objects"), objects);
            layers.replace(li, layer);
            found = true;
            break;
        }
    }
    if (!found) return;

    pilin.insert(QStringLiteral("layers"), layers);
    map.insert(QStringLiteral("pilinRey"), pilin);
    editor->setMap(map);
    QMetaObject::invokeMethod(editor, "mapEdited", Qt::DirectConnection, Q_ARG(QJsonObject, map));
    editor->selectObjectIds(QStringList{id});
}

class CityPreview final : public QWidget {
public:
    explicit CityPreview(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(190);
        setObjectName(QStringLiteral("settlementPreview"));
    }
    void setSeed(QString seed) { seed_ = std::move(seed); update(); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), QColor(QStringLiteral("#c8c7ba")));
        quint32 hash = qHash(seed_);
        painter.setPen(QPen(QColor(103, 99, 88), 2));
        const QPointF center(width() * .5, height() * .52);
        for (int r = 0; r < 9; ++r) {
            hash = hash * 1664525u + 1013904223u;
            const double angle = (hash % 628) / 100.0;
            hash = hash * 1664525u + 1013904223u;
            const double len = 45 + (hash % 110);
            painter.drawLine(center, center + QPointF(std::cos(angle) * len, std::sin(angle) * len * .65));
        }
        painter.setPen(QPen(QColor(72, 72, 68), 1));
        painter.setBrush(QColor(119, 123, 122));
        for (int i = 0; i < 58; ++i) {
            hash = hash * 1103515245u + 12345u;
            const double angle = (hash % 628) / 100.0;
            hash = hash * 1103515245u + 12345u;
            const double radius = 22 + (hash % 135);
            const double w = 7 + (hash % 14), h = 5 + ((hash >> 8) % 11);
            const QPointF c = center + QPointF(std::cos(angle) * radius, std::sin(angle) * radius * .58);
            painter.save();
            painter.translate(c);
            painter.rotate(hash % 180);
            painter.drawRect(QRectF(-w * .5, -h * .5, w, h));
            painter.restore();
        }
        painter.setBrush(QColor(81, 76, 69));
        painter.drawRect(QRectF(center.x() - 16, center.y() - 13, 32, 26));
    }

private:
    QString seed_;
};

void editSettlement(PilinReyEditor* editor, const QString& id) {
    QJsonObject object = objectById(editor, id);
    if (object.value(QStringLiteral("type")).toString() != QStringLiteral("settlement")) return;

    QDialog dialog(editor);
    dialog.setWindowTitle(QObject::tr("Editar asentamiento"));
    dialog.resize(720, 560);
    auto* root = new QHBoxLayout(&dialog);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(16);

    auto* formHost = new QWidget(&dialog);
    auto* form = new QFormLayout(formHost);
    form->setContentsMargins(0, 0, 0, 0);
    form->setVerticalSpacing(9);

    auto* name = new QLineEdit(object.value(QStringLiteral("label")).toString(), formHost);
    auto* group = new QLineEdit(object.value(QStringLiteral("group")).toString(), formHost);
    auto* type = new QComboBox(formHost);
    type->addItems({QObject::tr("Aldea"), QObject::tr("Pueblo"), QObject::tr("Ciudad"), QObject::tr("Ciudad grande"), QObject::tr("Capital"), QObject::tr("Fortaleza"), QObject::tr("Puerto")});
    const int typeIndex = type->findText(object.value(QStringLiteral("settlementType")).toString());
    if (typeIndex >= 0) type->setCurrentIndex(typeIndex);
    auto* culture = new QLineEdit(object.value(QStringLiteral("culture")).toString(), formHost);
    auto* population = new QSpinBox(formHost);
    population->setRange(0, 50000000);
    population->setGroupSeparatorShown(true);
    population->setValue(object.value(QStringLiteral("population")).toInt());
    auto* elevation = new QSpinBox(formHost);
    elevation->setRange(-500, 9000);
    elevation->setSuffix(QObject::tr(" m"));
    elevation->setValue(object.value(QStringLiteral("elevation")).toInt());
    auto* atlas = new QLineEdit(object.value(QStringLiteral("atlasId")).toString(), formHost);
    atlas->setPlaceholderText(QObject::tr("ID de ficha del Atlas (opcional)"));
    auto* notes = new QTextEdit(object.value(QStringLiteral("notes")).toString(), formHost);
    notes->setMinimumHeight(100);

    form->addRow(QObject::tr("Nombre"), name);
    form->addRow(QObject::tr("Grupo / reino"), group);
    form->addRow(QObject::tr("Tipo"), type);
    form->addRow(QObject::tr("Cultura"), culture);
    form->addRow(QObject::tr("Población"), population);
    form->addRow(QObject::tr("Elevación"), elevation);
    form->addRow(QObject::tr("Ficha Atlas"), atlas);
    form->addRow(QObject::tr("Notas"), notes);
    root->addWidget(formHost, 1);

    auto* right = new QVBoxLayout;
    auto* title = new QLabel(QObject::tr("Vista urbana"), &dialog);
    title->setObjectName(QStringLiteral("settlementPreviewTitle"));
    auto* preview = new CityPreview(&dialog);
    preview->setSeed(id + name->text());
    auto* hint = new QLabel(QObject::tr("La vista urbana se genera de forma determinista y permanece vinculada con la entidad del mapa y su ficha del Atlas."), &dialog);
    hint->setWordWrap(true);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    right->addWidget(title);
    right->addWidget(preview, 1);
    right->addWidget(hint);
    right->addWidget(buttons);
    root->addLayout(right, 1);

    QObject::connect(name, &QLineEdit::textChanged, preview, [preview, id](const QString& text) { preview->setSeed(id + text); });
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    dialog.setStyleSheet(QStringLiteral(R"QSS(
QDialog{background:#0c1117;color:#dfe5eb;}
QLineEdit,QComboBox,QSpinBox,QTextEdit{background:#101820;color:#e4e9ee;border:1px solid #31404d;border-radius:7px;padding:7px;}
#settlementPreview{border:1px solid #39434a;border-radius:8px;}
#settlementPreviewTitle{font-family:'Georgia';font-size:15pt;font-weight:700;color:#f3efe8;}
QPushButton{background:#17212b;color:#dbe3ea;border:1px solid #394a59;border-radius:7px;padding:8px 12px;}
QPushButton:hover{background:#20303f;}
)QSS"));

    if (dialog.exec() != QDialog::Accepted) return;
    object.insert(QStringLiteral("label"), name->text().trimmed());
    object.insert(QStringLiteral("group"), group->text().trimmed());
    object.insert(QStringLiteral("settlementType"), type->currentText());
    object.insert(QStringLiteral("culture"), culture->text().trimmed());
    object.insert(QStringLiteral("population"), population->value());
    object.insert(QStringLiteral("elevation"), elevation->value());
    object.insert(QStringLiteral("atlasId"), atlas->text().trimmed());
    object.insert(QStringLiteral("notes"), notes->toPlainText());
    replaceObject(editor, id, object);
}

void refreshObjectList(PilinReyEditor* editor, QListWidget* list) {
    if (!editor || !list || !editor->isVisible()) return;
    const QJsonObject pilin = editor->map().value(QStringLiteral("pilinRey")).toObject();
    const QString active = pilin.value(QStringLiteral("activeLayerId")).toString();
    QJsonArray objects;
    for (const QJsonValue& value : pilin.value(QStringLiteral("layers")).toArray()) {
        const QJsonObject layer = value.toObject();
        if (layer.value(QStringLiteral("id")).toString() == active) {
            objects = layer.value(QStringLiteral("objects")).toArray();
            break;
        }
    }

    list->setUpdatesEnabled(false);
    list->blockSignals(true);
    list->clear();
    for (const QJsonValue& value : objects) {
        const QJsonObject object = value.toObject();
        auto* item = new QListWidgetItem(objectCaption(object), list);
        item->setData(Qt::UserRole, object.value(QStringLiteral("id")).toString());
        item->setData(Qt::UserRole + 1, object.value(QStringLiteral("type")).toString());
        item->setToolTip(object.value(QStringLiteral("type")).toString() == QStringLiteral("settlement")
            ? QObject::tr("Doble clic para editar la entidad urbana")
            : QObject::tr("Seleccionar y transformar este objeto"));
    }
    if (objects.isEmpty()) {
        auto* empty = new QListWidgetItem(QObject::tr("Esta capa aún no tiene objetos"), list);
        empty->setFlags(Qt::NoItemFlags);
    }
    list->blockSignals(false);
    list->setUpdatesEnabled(true);
}

class MapSelectionFilter final : public QObject {
public:
    explicit MapSelectionFilter(QMainWindow* window) : QObject(window), window_(window) {
        refreshTimer_.setSingleShot(true);
        refreshTimer_.setInterval(220);
        QObject::connect(&refreshTimer_, &QTimer::timeout, this, [this]() {
            if (editor_ && objectList_) refreshObjectList(editor_, objectList_);
        });
    }

    bool eventFilter(QObject* watched, QEvent* event) override {
        if ((watched == window_ || watched == editor_) && (event->type() == QEvent::Show || event->type() == QEvent::Resize))
            scheduleApply();
        return false;
    }

    void apply() {
        applyPending_ = false;
        if (!window_) return;
        auto* editor = window_->findChild<PilinReyEditor*>(QStringLiteral("pilinReyEditor"));
        auto* panel = window_->findChild<QFrame*>(QStringLiteral("pilinSelectionPopover"));
        if (!editor || !panel) return;
        editor_ = editor;

        if (!initialized_) {
            auto makeShortcut = [editor](const QKeySequence& key, double dx, double dy) {
                auto* shortcut = new QShortcut(key, editor);
                shortcut->setContext(Qt::WidgetWithChildrenShortcut);
                QObject::connect(shortcut, &QShortcut::activated, editor, [editor, dx, dy]() { editor->nudgeSelection(dx, dy); });
            };
            makeShortcut(QKeySequence(QStringLiteral("Alt+Left")), -25, 0);
            makeShortcut(QKeySequence(QStringLiteral("Alt+Right")), 25, 0);
            makeShortcut(QKeySequence(QStringLiteral("Alt+Up")), 0, -25);
            makeShortcut(QKeySequence(QStringLiteral("Alt+Down")), 0, 25);

            if (auto* layout = qobject_cast<QVBoxLayout*>(panel->layout())) {
                auto* title = new QLabel(QObject::tr("MOVER / GEOMETRÍA"), panel);
                title->setObjectName(QStringLiteral("pilinTinyLabel"));
                auto* row = new QHBoxLayout;
                row->setSpacing(4);
                auto* left = command(panel, QStringLiteral("←"), QObject::tr("Mover selección a la izquierda"));
                auto* up = command(panel, QStringLiteral("↑"), QObject::tr("Mover selección arriba"));
                auto* down = command(panel, QStringLiteral("↓"), QObject::tr("Mover selección abajo"));
                auto* right = command(panel, QStringLiteral("→"), QObject::tr("Mover selección a la derecha"));
                auto* rotateLeft = command(panel, QStringLiteral("↺"), QObject::tr("Rotar trazado −15°"));
                auto* rotateRight = command(panel, QStringLiteral("↻"), QObject::tr("Rotar trazado +15°"));
                auto* shrink = command(panel, QStringLiteral("↙"), QObject::tr("Reducir geometría"));
                auto* grow = command(panel, QStringLiteral("↗"), QObject::tr("Aumentar geometría"));
                auto* labelStyle = command(panel, QStringLiteral("Aa"), QObject::tr("Estilo de etiqueta"));
                for (QToolButton* button : {left, up, down, right, rotateLeft, rotateRight, shrink, grow, labelStyle}) row->addWidget(button);
                layout->addWidget(title);
                layout->addLayout(row);
                QObject::connect(left, &QToolButton::clicked, editor, [editor]() { editor->nudgeSelection(-25, 0); });
                QObject::connect(right, &QToolButton::clicked, editor, [editor]() { editor->nudgeSelection(25, 0); });
                QObject::connect(up, &QToolButton::clicked, editor, [editor]() { editor->nudgeSelection(0, -25); });
                QObject::connect(down, &QToolButton::clicked, editor, [editor]() { editor->nudgeSelection(0, 25); });
                QObject::connect(rotateLeft, &QToolButton::clicked, editor, [editor]() { editor->transformSelectedPathGeometry(1, -15); });
                QObject::connect(rotateRight, &QToolButton::clicked, editor, [editor]() { editor->transformSelectedPathGeometry(1, 15); });
                QObject::connect(shrink, &QToolButton::clicked, editor, [editor]() { editor->transformSelectedPathGeometry(.9, 0); });
                QObject::connect(grow, &QToolButton::clicked, editor, [editor]() { editor->transformSelectedPathGeometry(1.1, 0); });
                QObject::connect(labelStyle, &QToolButton::clicked, editor, &PilinReyEditor::editSelectedLabelStyle);
            }

            if (QFrame* layers = layersPanel(editor)) {
                auto* layout = qobject_cast<QVBoxLayout*>(layers->layout());
                if (layout) {
                    auto* heading = new QLabel(QObject::tr("OBJETOS DE LA CAPA"), layers);
                    heading->setObjectName(QStringLiteral("pilinTinyLabel"));
                    objectList_ = new QListWidget(layers);
                    objectList_->setObjectName(QStringLiteral("pilinLayerObjects"));
                    objectList_->setMinimumHeight(118);
                    objectList_->setMaximumHeight(210);
                    layout->addWidget(heading);
                    layout->addWidget(objectList_);
                    QObject::connect(objectList_, &QListWidget::itemSelectionChanged, editor, [this]() {
                        if (!objectList_ || !editor_) return;
                        QStringList ids;
                        for (QListWidgetItem* item : objectList_->selectedItems()) {
                            const QString id = item->data(Qt::UserRole).toString();
                            if (!id.isEmpty()) ids.append(id);
                        }
                        editor_->selectObjectIds(ids);
                    });
                    QObject::connect(objectList_, &QListWidget::itemDoubleClicked, editor, [editor](QListWidgetItem* item) {
                        if (!item) return;
                        if (item->data(Qt::UserRole + 1).toString() == QStringLiteral("settlement"))
                            editSettlement(editor, item->data(Qt::UserRole).toString());
                    });
                }
            }

            QObject::connect(editor, &PilinReyEditor::mapEdited, this, [this](const QJsonObject&) {
                if (editor_ && editor_->isVisible()) refreshTimer_.start();
            });
            editor->installEventFilter(this);
            initialized_ = true;
        }

        panel->adjustSize();
        refreshTimer_.start(0);
    }

private:
    void scheduleApply() {
        if (applyPending_) return;
        applyPending_ = true;
        QTimer::singleShot(45, this, [this]() { apply(); });
    }

    QPointer<QMainWindow> window_;
    QPointer<PilinReyEditor> editor_;
    QPointer<QListWidget> objectList_;
    QTimer refreshTimer_;
    bool initialized_ = false;
    bool applyPending_ = false;
};

} // namespace

void installMapSelectionController(QMainWindow* window) {
    if (!window || window->property("wbwMapSelectionControllerV4").toBool()) return;
    window->setProperty("wbwMapSelectionControllerV4", true);
    auto* filter = new MapSelectionFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
