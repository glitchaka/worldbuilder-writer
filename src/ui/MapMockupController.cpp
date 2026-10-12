#include "ui/MapMockupController.h"

#include <QCheckBox>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPointer>
#include <QSettings>
#include <QSlider>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

namespace wbw {
namespace {

QString toolCaption(const QString& tip) {
    if (tip.startsWith(QObject::tr("Seleccionar"))) return QObject::tr("↖\nSelección");
    if (tip.startsWith(QObject::tr("Mover lienzo"))) return QObject::tr("✥\nMover");
    if (tip.startsWith(QObject::tr("Pintar tierra"))) return QObject::tr("▰\nTerreno");
    if (tip.startsWith(QObject::tr("Recortar tierra"))) return QObject::tr("◌\nAgua");
    if (tip.startsWith(QObject::tr("Río"))) return QObject::tr("∿\nRíos");
    if (tip.startsWith(QObject::tr("Camino"))) return QObject::tr("━\nCaminos");
    if (tip.startsWith(QObject::tr("Frontera"))) return QObject::tr("┄\nFronteras");
    if (tip.startsWith(QObject::tr("Región"))) return QObject::tr("◇\nRegiones");
    if (tip.startsWith(QObject::tr("Pincel de bosque"))) return QObject::tr("♣\nBosques");
    if (tip.startsWith(QObject::tr("Pincel de cordillera"))) return QObject::tr("△\nRelieve");
    if (tip.startsWith(QObject::tr("Asentamiento"))) return QObject::tr("●\nCiudades");
    if (tip.startsWith(QObject::tr("Asset"))) return QObject::tr("✦\nAssets");
    if (tip.startsWith(QObject::tr("Etiqueta"))) return QObject::tr("T\nTexto");
    if (tip.startsWith(QObject::tr("Medir"))) return QObject::tr("↔\nMedir");
    return {};
}

QString frameTitle(QFrame* frame) {
    if (!frame) return {};
    for (QLabel* label : frame->findChildren<QLabel*>()) {
        const QString text = label->text().trimmed();
        if (text == QObject::tr("Capas") || text == QObject::tr("Assets") || text == QObject::tr("Assets cartográficos") ||
            text == QObject::tr("Assets y sellos") || text == QObject::tr("Plantilla") || text == QObject::tr("Plantilla de referencia") ||
            text == QObject::tr("Apariencia")) return text;
    }
    return {};
}

void rebuildRail(QWidget* rail, QWidget* editor) {
    if (!rail || !editor || rail->property("wbwMapPaletteStableV1").toBool()) return;
    rail->setProperty("wbwMapPaletteStableV1", true);
    rail->setMinimumWidth(286);
    rail->setMaximumWidth(286);

    QList<QToolButton*> buttons = rail->findChildren<QToolButton*>(QStringLiteral("pilinMapTool"), Qt::FindDirectChildrenOnly);
    for (QToolButton* button : buttons) button->setParent(nullptr);
    if (QLayout* old = rail->layout()) {
        while (QLayoutItem* item = old->takeAt(0)) delete item;
        delete old;
    }

    auto* root = new QVBoxLayout(rail);
    root->setContentsMargins(14, 12, 14, 14);
    root->setSpacing(10);

    auto* heading = new QHBoxLayout;
    auto* copy = new QVBoxLayout;
    copy->setSpacing(1);
    auto* kicker = new QLabel(QObject::tr("PILÍN REY"), rail);
    kicker->setObjectName(QStringLiteral("mapPaletteEyebrow"));
    auto* title = new QLabel(QObject::tr("Herramientas del mapa"), rail);
    title->setObjectName(QStringLiteral("mapPaletteTitle"));
    copy->addWidget(kicker);
    copy->addWidget(title);
    heading->addLayout(copy);
    heading->addStretch();
    root->addLayout(heading);

    auto* tabs = new QHBoxLayout;
    tabs->setSpacing(4);
    auto* terrainTab = new QToolButton(rail);
    terrainTab->setObjectName(QStringLiteral("mapPaletteTab"));
    terrainTab->setText(QObject::tr("Terreno"));
    terrainTab->setCheckable(true);
    terrainTab->setChecked(true);
    auto* objectsTab = new QToolButton(rail);
    objectsTab->setObjectName(QStringLiteral("mapPaletteTab"));
    objectsTab->setText(QObject::tr("Objetos"));
    objectsTab->setEnabled(false);
    tabs->addWidget(terrainTab);
    tabs->addWidget(objectsTab);
    tabs->addStretch();
    root->addLayout(tabs);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);
    for (int i = 0; i < buttons.size(); ++i) {
        QToolButton* button = buttons.at(i);
        button->setParent(rail);
        const QString caption = toolCaption(button->toolTip());
        if (!caption.isEmpty()) button->setText(caption);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setMinimumSize(56, 56);
        button->setMaximumSize(QWIDGETSIZE_MAX, 56);
        grid->addWidget(button, i / 4, i % 4);
    }
    for (int c = 0; c < 4; ++c) grid->setColumnStretch(c, 1);
    root->addLayout(grid);

    auto* divider = new QFrame(rail);
    divider->setObjectName(QStringLiteral("mapPaletteDivider"));
    divider->setFrameShape(QFrame::HLine);
    root->addWidget(divider);

    QWidget* sourceOptions = editor->findChild<QWidget*>(QStringLiteral("pilinToolOptions"));
    const QList<QSlider*> sliders = sourceOptions ? sourceOptions->findChildren<QSlider*>() : QList<QSlider*>();
    const QStringList names{QObject::tr("Tamaño"), QObject::tr("Densidad"), QObject::tr("Trazo"), QObject::tr("Símbolo")};
    for (int i = 0; i < qMin(sliders.size(), names.size()); ++i) {
        auto* line = new QWidget(rail);
        line->setObjectName(QStringLiteral("mapPaletteControl"));
        auto* box = new QVBoxLayout(line);
        box->setContentsMargins(0, 0, 0, 0);
        box->setSpacing(3);
        auto* labelRow = new QHBoxLayout;
        auto* label = new QLabel(names.at(i), line);
        label->setObjectName(QStringLiteral("mapPaletteControlLabel"));
        auto* value = new QLabel(QString::number(sliders.at(i)->value()), line);
        value->setObjectName(QStringLiteral("mapPaletteControlValue"));
        labelRow->addWidget(label);
        labelRow->addStretch();
        labelRow->addWidget(value);
        box->addLayout(labelRow);
        auto* mirror = new QSlider(Qt::Horizontal, line);
        mirror->setRange(sliders.at(i)->minimum(), sliders.at(i)->maximum());
        mirror->setValue(sliders.at(i)->value());
        box->addWidget(mirror);
        QObject::connect(mirror, &QSlider::valueChanged, sliders.at(i), &QSlider::setValue);
        QObject::connect(sliders.at(i), &QSlider::valueChanged, mirror, &QSlider::setValue);
        QObject::connect(mirror, &QSlider::valueChanged, value, [value](int v) { value->setText(QString::number(v)); });
        root->addWidget(line);
    }
    root->addStretch(1);
    if (sourceOptions) sourceOptions->hide();
}

void styleCommands(QWidget* commands) {
    if (!commands || commands->property("wbwMapCommandsStableV1").toBool()) return;
    commands->setProperty("wbwMapCommandsStableV1", true);
    for (QToolButton* button : commands->findChildren<QToolButton*>(QStringLiteral("pilinCommand"))) {
        const QString tip = button->toolTip();
        if (tip == QObject::tr("Capas") || tip == QObject::tr("Assets") || tip == QObject::tr("Plantilla") ||
            tip == QObject::tr("Apariencia") || tip == QObject::tr("Encajar") || tip == QObject::tr("Exportar"))
            button->setText(tip);
        else if (tip == QObject::tr("Deshacer")) button->setText(QStringLiteral("↶"));
        else if (tip == QObject::tr("Rehacer")) button->setText(QStringLiteral("↷"));
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setMinimumHeight(34);
    }
}

void styleEditor(QWidget* editor) {
    if (!editor || editor->property("wbwMapStyledStableV1").toBool()) return;
    editor->setProperty("wbwMapStyledStableV1", true);
    editor->setStyleSheet(QStringLiteral(R"QSS(
#pilinReyEditor{background:#090d12;color:#dde5ec;}
#pilinCanvasHost{background:#090d12;}
#pilinToolPalette,#pilinTopCommands,#pilinPopover,#pilinSelectionPopover,#pilinToolOptions{background:rgba(13,19,26,247);border:1px solid #2c3946;border-radius:10px;}
#mapPaletteEyebrow{color:#6e9fc8;font-size:7pt;font-weight:700;letter-spacing:1.2px;}
#mapPaletteTitle{color:#f1f4f7;font-family:'Georgia';font-size:12.5pt;font-weight:600;}
#mapPaletteTab{background:transparent;color:#8c98a4;border:0;border-bottom:2px solid transparent;padding:7px 12px;}
#mapPaletteTab:checked{color:#8dccff;border-bottom-color:#3698e6;}
#pilinMapTool{background:#111923;color:#aeb9c4;border:1px solid #273544;border-radius:7px;text-align:center;padding:4px;font-size:7.5pt;}
#pilinMapTool:hover{background:#172431;color:#eef6fc;border-color:#3a5a73;}
#pilinMapTool:checked{background:#17314a;color:#8fd0ff;border:1px solid #3d86bd;font-weight:700;}
#pilinCommand{background:transparent;color:#c5d0d9;border:0;border-radius:7px;padding:0 9px;}
#pilinCommand:hover{background:#172431;color:#fff;}
#pilinPopoverTitle{color:#f1f4f7;font-family:'Georgia';font-size:12pt;font-weight:600;}
#pilinTinyLabel,#pilinToolOptionsLabel,#mapPaletteControlLabel{color:#929da7;font-size:8pt;}
#mapPaletteControlValue{color:#cbd5de;font-size:8pt;}
#mapPaletteDivider{background:#27323d;max-height:1px;border:0;}
#pilinPopover QListWidget{background:#0b1117;color:#d9e1e8;border:1px solid #2c3946;border-radius:7px;outline:0;}
#pilinPopover QListWidget::item{padding:8px;border-radius:5px;}
#pilinPopover QListWidget::item:selected{background:#17314a;color:#8fd0ff;}
#pilinPopover QPushButton{background:#111923;color:#d2dbe3;border:1px solid #2e3c49;border-radius:7px;padding:7px 9px;}
#pilinPopover QPushButton:hover{background:#192735;color:#fff;}
QSlider::groove:horizontal{height:4px;background:#303b46;border-radius:2px;}
QSlider::handle:horizontal{width:12px;margin:-4px 0;background:#4b9de0;border-radius:6px;}
QCheckBox{color:#c5cfd8;spacing:7px;}
)QSS"));
}

class MapWorkspaceFilter final : public QObject {
public:
    explicit MapWorkspaceFilter(QMainWindow* window) : QObject(window), window_(window) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        if ((watched == window_ || watched == canvas_) && (event->type() == QEvent::Show || event->type() == QEvent::Resize))
            schedule();
        return false;
    }

    void apply() {
        pending_ = false;
        if (!window_) return;
        QWidget* editor = window_->findChild<QWidget*>(QStringLiteral("pilinReyEditor"));
        QWidget* canvas = editor ? editor->findChild<QWidget*>(QStringLiteral("pilinCanvasHost")) : nullptr;
        QWidget* rail = editor ? editor->findChild<QWidget*>(QStringLiteral("pilinToolPalette")) : nullptr;
        QWidget* commands = editor ? editor->findChild<QWidget*>(QStringLiteral("pilinTopCommands")) : nullptr;
        if (!editor || !canvas || canvas->width() < 200) return;
        canvas_ = canvas;

        if (!initialized_) {
            rebuildRail(rail, editor);
            styleCommands(commands);
            styleEditor(editor);
            if (QWidget* identity = canvas->findChild<QWidget*>(QStringLiteral("mapCanvasIdentity"))) identity->hide();

            QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
            for (QCheckBox* check : editor->findChildren<QCheckBox*>()) {
                if (check->text() == QObject::tr("Ajustar")) check->setChecked(settings.value(QStringLiteral("map/defaultSnap"), true).toBool());
                if (check->text() == QObject::tr("Cuadrícula")) check->setChecked(settings.value(QStringLiteral("map/defaultGrid"), false).toBool());
            }
            canvas->installEventFilter(this);
            initialized_ = true;
        }

        const int margin = 14;
        if (rail) {
            rail->adjustSize();
            rail->resize(286, qMin(rail->sizeHint().height(), canvas->height() - margin * 2));
            rail->move(margin, qMax(margin, (canvas->height() - rail->height()) / 2));
            rail->show();
            rail->raise();
        }

        QWidget* mapBar = canvas->findChild<QWidget*>(QStringLiteral("mapWorkspaceBar"));
        if (mapBar) {
            mapBar->adjustSize();
            mapBar->move(310, 14);
            mapBar->show();
            mapBar->raise();
        }
        if (commands) {
            commands->adjustSize();
            const int preferred = mapBar ? mapBar->geometry().right() + 10 : 310;
            const int x = qMin(qMax(preferred, 310), qMax(310, canvas->width() - commands->width() - margin));
            commands->move(x, 14);
            commands->show();
            commands->raise();
        }

        QFrame* layers = nullptr;
        QFrame* assets = nullptr;
        QList<QFrame*> contextual;
        for (QFrame* frame : editor->findChildren<QFrame*>(QStringLiteral("pilinPopover"))) {
            const QString title = frameTitle(frame);
            if (title == QObject::tr("Capas")) layers = frame;
            else if (title == QObject::tr("Assets") || title == QObject::tr("Assets cartográficos") || title == QObject::tr("Assets y sellos")) assets = frame;
            else contextual.append(frame);
        }

        const bool persistentRight = canvas->width() >= 1180;
        int rightY = 64;
        auto placeRight = [&](QFrame* frame, bool visible) {
            if (!frame) return;
            frame->setMinimumWidth(292);
            frame->setMaximumWidth(316);
            frame->setVisible(visible);
            if (!visible) return;
            frame->adjustSize();
            const int maxHeight = qMax(140, canvas->height() - rightY - margin);
            if (frame->height() > maxHeight) frame->resize(frame->width(), maxHeight);
            frame->move(canvas->width() - frame->width() - margin, rightY);
            frame->raise();
            rightY = frame->geometry().bottom() + 10;
        };
        placeRight(layers, persistentRight);
        placeRight(assets, persistentRight && rightY < canvas->height() - 190);
        for (QFrame* frame : contextual) if (frame->isVisible()) placeRight(frame, true);

        if (QWidget* selection = editor->findChild<QWidget*>(QStringLiteral("pilinSelectionPopover")); selection && selection->isVisible()) {
            selection->adjustSize();
            selection->move(canvas->width() - selection->width() - margin, qMax(68, canvas->height() - selection->height() - 20));
            selection->raise();
        }
    }

private:
    void schedule() {
        if (pending_) return;
        pending_ = true;
        QTimer::singleShot(45, this, [this]() { apply(); });
    }

    QPointer<QMainWindow> window_;
    QPointer<QWidget> canvas_;
    bool initialized_ = false;
    bool pending_ = false;
};

} // namespace

void installMapMockupController(QMainWindow* window) {
    if (!window || window->property("wbwMapMockupStableV1").toBool()) return;
    window->setProperty("wbwMapMockupStableV1", true);
    auto* filter = new MapWorkspaceFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
