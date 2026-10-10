#include "ui/MapMockupController.h"

#include <QCheckBox>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPointer>
#include <QSettings>
#include <QTimer>
#include <QToolButton>
#include <QWidget>

namespace wbw {
namespace {

QString toolCaption(const QString& tip) {
    if (tip.startsWith(QObject::tr("Seleccionar"))) return QObject::tr("↖  Selección");
    if (tip.startsWith(QObject::tr("Mover lienzo"))) return QObject::tr("✥  Mover mapa");
    if (tip.startsWith(QObject::tr("Pintar tierra"))) return QObject::tr("▰  Tierra y costa");
    if (tip.startsWith(QObject::tr("Recortar tierra"))) return QObject::tr("◌  Mar / borrar");
    if (tip.startsWith(QObject::tr("Río"))) return QObject::tr("∿  Ríos");
    if (tip.startsWith(QObject::tr("Camino"))) return QObject::tr("━  Caminos");
    if (tip.startsWith(QObject::tr("Frontera"))) return QObject::tr("┄  Fronteras");
    if (tip.startsWith(QObject::tr("Región"))) return QObject::tr("◇  Regiones");
    if (tip.startsWith(QObject::tr("Pincel de bosque"))) return QObject::tr("♣  Bosques");
    if (tip.startsWith(QObject::tr("Pincel de cordillera"))) return QObject::tr("△  Montañas");
    if (tip.startsWith(QObject::tr("Asentamiento"))) return QObject::tr("●  Asentamientos");
    if (tip.startsWith(QObject::tr("Asset"))) return QObject::tr("✦  Assets y sellos");
    if (tip.startsWith(QObject::tr("Etiqueta"))) return QObject::tr("T  Etiquetas");
    if (tip.startsWith(QObject::tr("Medir"))) return QObject::tr("↔  Medir");
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

void styleRail(QWidget* rail) {
    if (!rail) return;
    rail->setMinimumWidth(196);
    rail->setMaximumWidth(196);
    if (auto* grid = qobject_cast<QGridLayout*>(rail->layout())) {
        grid->setContentsMargins(10, 48, 10, 10);
        grid->setHorizontalSpacing(0);
        grid->setVerticalSpacing(2);
    }
    for (QToolButton* button : rail->findChildren<QToolButton*>(QStringLiteral("pilinMapTool"))) {
        const QString caption = toolCaption(button->toolTip());
        if (!caption.isEmpty()) button->setText(caption);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setMinimumSize(176, 31);
        button->setMaximumSize(176, 31);
    }
}

void styleCommands(QWidget* commands) {
    if (!commands) return;
    for (QToolButton* button : commands->findChildren<QToolButton*>(QStringLiteral("pilinCommand"))) {
        const QString tip = button->toolTip();
        if (tip == QObject::tr("Capas") || tip == QObject::tr("Assets") || tip == QObject::tr("Plantilla") || tip == QObject::tr("Apariencia") || tip == QObject::tr("Encajar") || tip == QObject::tr("Exportar"))
            button->setText(tip);
        else if (tip == QObject::tr("Deshacer")) button->setText(QStringLiteral("↶"));
        else if (tip == QObject::tr("Rehacer")) button->setText(QStringLiteral("↷"));
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setMinimumHeight(30);
    }
}

class MapWorkspaceFilter final : public QObject {
public:
    explicit MapWorkspaceFilter(QMainWindow* window) : QObject(window), window_(window) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        Q_UNUSED(watched);
        switch (event->type()) {
            case QEvent::Show:
            case QEvent::Resize:
            case QEvent::LayoutRequest:
            case QEvent::ParentChange:
                scheduleApply();
                break;
            default:
                break;
        }
        return QObject::eventFilter(watched, event);
    }

    void apply() {
        pending_ = false;
        if (applying_ || !window_) return;
        applying_ = true;

        QWidget* editor = window_->findChild<QWidget*>(QStringLiteral("pilinReyEditor"));
        QWidget* canvas = editor ? editor->findChild<QWidget*>(QStringLiteral("pilinCanvasHost")) : nullptr;
        QWidget* rail = editor ? editor->findChild<QWidget*>(QStringLiteral("pilinToolPalette")) : nullptr;
        QWidget* commands = editor ? editor->findChild<QWidget*>(QStringLiteral("pilinTopCommands")) : nullptr;
        if (!editor || !canvas || canvas->width() < 200) {
            applying_ = false;
            return;
        }

        if (!editor->property("wbwMapWorkspaceObserved").toBool()) {
            editor->setProperty("wbwMapWorkspaceObserved", true);
            editor->installEventFilter(this);
            canvas->installEventFilter(this);
        }

        styleRail(rail);
        styleCommands(commands);

        QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
        for (QCheckBox* check : editor->findChildren<QCheckBox*>()) {
            if (check->text() == QObject::tr("Ajustar") && !check->property("wbwPersist").toBool()) {
                check->setChecked(settings.value(QStringLiteral("map/defaultSnap"), true).toBool());
                check->setProperty("wbwPersist", true);
                QObject::connect(check, &QCheckBox::toggled, editor, [](bool value) {
                    QSettings s(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
                    s.setValue(QStringLiteral("map/defaultSnap"), value);
                });
            }
            if (check->text() == QObject::tr("Cuadrícula") && !check->property("wbwPersist").toBool()) {
                check->setChecked(settings.value(QStringLiteral("map/defaultGrid"), false).toBool());
                check->setProperty("wbwPersist", true);
                QObject::connect(check, &QCheckBox::toggled, editor, [](bool value) {
                    QSettings s(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
                    s.setValue(QStringLiteral("map/defaultGrid"), value);
                });
            }
        }

        editor->setStyleSheet(QStringLiteral(R"QSS(
#pilinReyEditor{background:#090b0d;color:#ddd8cf;}
#pilinCanvasHost{background:#090b0d;}
#pilinToolPalette,#pilinTopCommands,#pilinPopover,#pilinSelectionPopover,#pilinToolOptions{background:rgba(17,18,18,242);border:1px solid #37352f;border-radius:10px;}
#mapPaletteEyebrow{color:#b98b55;font-size:7pt;font-weight:700;letter-spacing:1.2px;}
#mapPaletteTitle{color:#f1ede4;font-family:'Georgia';font-size:12pt;font-weight:600;}
#pilinMapTool,#pilinCommand{background:transparent;color:#c9c4ba;border:0;border-radius:7px;text-align:left;padding:0 9px;}
#pilinCommand{text-align:center;padding:0 8px;}
#pilinMapTool:hover,#pilinCommand:hover{background:#26241f;color:#fff9ee;}
#pilinMapTool:checked{background:#3a2f23;color:#edc68d;font-weight:700;}
#pilinPopoverTitle{color:#f1ede4;font-family:'Georgia';font-size:12pt;font-weight:600;}
#pilinTinyLabel,#pilinToolOptionsLabel{color:#8d8b84;font-size:8pt;}
#pilinPopover QListWidget{background:#0d0f10;color:#d9d5cd;border:1px solid #33332f;border-radius:7px;outline:0;}
#pilinPopover QListWidget::item{padding:8px;border-radius:5px;}
#pilinPopover QListWidget::item:selected{background:#332a20;color:#edc68d;}
#pilinPopover QPushButton{background:#191b1b;color:#d9d5cd;border:1px solid #373832;border-radius:7px;padding:7px 9px;}
#pilinPopover QPushButton:hover{background:#262620;color:#fff8eb;}
QSlider::groove:horizontal{height:4px;background:#34342f;border-radius:2px;}
QSlider::handle:horizontal{width:12px;margin:-4px 0;background:#ba8b55;border-radius:6px;}
QCheckBox{color:#c5c0b7;spacing:7px;}
)QSS"));

        const int margin = 14;
        if (QWidget* identity = canvas->findChild<QWidget*>(QStringLiteral("mapCanvasIdentity"))) identity->hide();

        if (rail) {
            rail->adjustSize();
            rail->resize(196, qMin(rail->sizeHint().height(), canvas->height() - 28));
            rail->move(margin, qMax(margin, (canvas->height() - rail->height()) / 2));
            rail->show();
            rail->raise();
        }

        QWidget* mapBar = canvas->findChild<QWidget*>(QStringLiteral("mapWorkspaceBar"));
        if (mapBar) {
            mapBar->adjustSize();
            mapBar->move(224, 14);
            mapBar->show();
            mapBar->raise();
        }

        if (commands) {
            commands->adjustSize();
            const int preferred = mapBar ? mapBar->geometry().right() + 10 : 224;
            const int rightLimit = canvas->width() - margin;
            const int x = qMin(qMax(preferred, 224), qMax(224, rightLimit - commands->width()));
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

        const bool persistentRight = canvas->width() >= 1080;
        int rightY = 66;
        auto placeRight = [&](QFrame* frame, bool visible) {
            if (!frame) return;
            frame->setMinimumWidth(278);
            frame->setMaximumWidth(300);
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
        placeRight(assets, persistentRight && rightY < canvas->height() - 180);
        for (QFrame* frame : contextual) if (frame->isVisible()) placeRight(frame, true);

        if (QWidget* selection = editor->findChild<QWidget*>(QStringLiteral("pilinSelectionPopover")); selection && selection->isVisible()) {
            selection->adjustSize();
            selection->move(canvas->width() - selection->width() - margin, qMax(68, canvas->height() - selection->height() - 20));
            selection->raise();
        }
        if (QWidget* options = editor->findChild<QWidget*>(QStringLiteral("pilinToolOptions")); options && options->isVisible()) {
            options->adjustSize();
            options->move(224, qMax(72, (canvas->height() - options->height()) / 2));
            options->raise();
        }

        applying_ = false;
    }

private:
    void scheduleApply() {
        if (pending_) return;
        pending_ = true;
        QTimer::singleShot(0, this, [this]() { apply(); });
    }

    QPointer<QMainWindow> window_;
    bool pending_ = false;
    bool applying_ = false;
};

} // namespace

void installMapMockupController(QMainWindow* window) {
    if (!window || window->property("wbwMapMockupControllerV4").toBool()) return;
    window->setProperty("wbwMapMockupControllerV4", true);
    auto* filter = new MapWorkspaceFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
