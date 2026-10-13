#include "ui/MainWindow.h"
#include "ui/SettingsWorkspace.h"
#include "ui/ThemeManager.h"
#include "ui/WritingShortcutBridge.h"

#include <QApplication>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QListWidget>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>
#include <QWindow>

namespace {

class AppTitleBar final : public QWidget {
public:
    explicit AppTitleBar(QWidget* parent = nullptr) : QWidget(parent) {
        setObjectName(QStringLiteral("appTitleBar"));
        setFixedHeight(44);
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(16, 0, 0, 0);
        layout->setSpacing(10);

        auto* mark = new QLabel(QStringLiteral("W"), this);
        mark->setObjectName(QStringLiteral("titleBarMark"));
        mark->setAlignment(Qt::AlignCenter);
        mark->setFixedSize(28, 28);
        auto* brand = new QLabel(QStringLiteral("Worldbuilder Writer"), this);
        brand->setObjectName(QStringLiteral("titleBarBrand"));
        auto* divider = new QLabel(QStringLiteral("/"), this);
        divider->setObjectName(QStringLiteral("titleBarDivider"));
        auto* workspace = new QLabel(QObject::tr("Proyecto"), this);
        workspace->setObjectName(QStringLiteral("titleBarWorkspace"));

        layout->addWidget(mark);
        layout->addWidget(brand);
        layout->addWidget(divider);
        layout->addWidget(workspace);
        layout->addStretch(1);

        auto* minimize = windowButton(QStringLiteral("—"), QStringLiteral("windowMinimize"));
        auto* maximize = windowButton(QStringLiteral("□"), QStringLiteral("windowMaximize"));
        auto* close = windowButton(QStringLiteral("×"), QStringLiteral("windowClose"));
        layout->addWidget(minimize);
        layout->addWidget(maximize);
        layout->addWidget(close);

        connect(minimize, &QPushButton::clicked, this, [this]() {
            if (window()) window()->showMinimized();
        });
        connect(maximize, &QPushButton::clicked, this, [this]() {
            if (!window()) return;
            window()->isMaximized() ? window()->showNormal() : window()->showMaximized();
        });
        connect(close, &QPushButton::clicked, this, [this]() {
            if (window()) window()->close();
        });

        setStyleSheet(QStringLiteral(R"QSS(
#appTitleBar{background:#0d1218;color:#dbe3ea;border-bottom:1px solid #252f39;}
#titleBarMark{background:#111b27;color:#a9d3ff;border:1px solid #315a7e;border-radius:7px;font-family:'Georgia';font-size:10pt;font-weight:700;}
#titleBarBrand{color:#f0ece4;font-family:'Georgia';font-size:10.5pt;font-weight:600;}
#titleBarDivider{color:#45515d;font-size:9pt;}
#titleBarWorkspace{color:#7f8b96;font-size:8pt;}
#windowMinimize,#windowMaximize,#windowClose{background:transparent;color:#8f99a3;border:0;border-radius:0;padding:0;font-size:10pt;}
#windowMinimize:hover,#windowMaximize:hover{background:#18212a;color:#ffffff;}
#windowClose:hover{background:#a83d45;color:#ffffff;}
)QSS"));
    }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && window() && window()->windowHandle()) {
            window()->windowHandle()->startSystemMove();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && window()) {
            window()->isMaximized() ? window()->showNormal() : window()->showMaximized();
            event->accept();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }

private:
    QPushButton* windowButton(const QString& text, const QString& name) {
        auto* button = new QPushButton(text, this);
        button->setObjectName(name);
        button->setFixedSize(44, 44);
        button->setFocusPolicy(Qt::NoFocus);
        return button;
    }
};

void applyProductStructure(wbw::MainWindow& window) {
    if (auto* writing = window.findChild<QWidget*>(QStringLiteral("writingPage"))) {
        if (auto* hero = writing->findChild<QWidget*>(QStringLiteral("writingHero"))) hero->hide();
    }

    auto* rail = window.findChild<QWidget*>(QStringLiteral("sideRail"));
    auto* top = window.findChild<QWidget*>(QStringLiteral("topShell"));
    auto* nav = window.findChild<QListWidget*>(QStringLiteral("sideNavigation"));

    if (rail) rail->hide();
    if (auto* identity = window.findChild<QWidget*>(QStringLiteral("appIdentity"))) identity->hide();

    if (top) {
        top->show();
        top->setFixedHeight(54);
        if (auto* layout = qobject_cast<QHBoxLayout*>(top->layout())) {
            layout->setContentsMargins(16, 0, 16, 0);
            layout->setSpacing(10);

            if (nav && nav->parentWidget() != top) {
                nav->setParent(top);
                nav->setObjectName(QStringLiteral("productNavigation"));
                nav->setFlow(QListView::LeftToRight);
                nav->setWrapping(false);
                nav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                nav->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                nav->setSelectionMode(QAbstractItemView::SingleSelection);
                nav->setFixedHeight(50);
                nav->setMinimumWidth(710);
                nav->setMaximumWidth(860);

                for (int i = 0; i < nav->count(); ++i) {
                    QListWidgetItem* item = nav->item(i);
                    nav->removeItemWidget(item);
                    QString label = item->text();
                    if (label == QObject::tr("Tablero de escenas")) label = QObject::tr("Planificación");
                    else if (label == QObject::tr("Atlas / documentación")) label = QObject::tr("Mundo");
                    else if (label == QObject::tr("Configuración / salida")) label = QObject::tr("Configuración");
                    item->setText(label);
                    item->setTextAlignment(Qt::AlignCenter);
                    item->setSizeHint(QSize(label == QObject::tr("Planificación") ? 126 : 108, 46));
                }
                layout->insertWidget(0, nav, 0, Qt::AlignVCenter);
            }
        }
    }

    if (auto* title = window.findChild<QLabel*>(QStringLiteral("projectTitle"))) title->hide();
    if (auto* state = window.findChild<QLabel*>(QStringLiteral("saveState"))) state->hide();
    if (auto* save = window.findChild<QPushButton*>(QStringLiteral("primarySave"))) save->hide();
    if (auto* focus = window.findChild<QPushButton*>(QStringLiteral("secondaryAction"))) focus->hide();
    if (auto* settings = window.findChild<wbw::SettingsWorkspace*>()) settings->finalizeProductControls();

    window.setStyleSheet(window.styleSheet() + QStringLiteral(R"QSS(
#topShell{background:#0d1218;border-bottom:1px solid #252f39;}
#productNavigation{background:transparent;border:0;outline:0;color:#8f9aa5;font-size:9pt;}
#productNavigation::item{background:transparent;border:0;border-bottom:2px solid transparent;border-radius:0;padding:0 12px;margin:0 1px;}
#productNavigation::item:hover{background:#121a22;color:#d8e0e8;}
#productNavigation::item:selected{background:transparent;color:#59d5d7;border-bottom:2px solid #32c7c9;}
#globalSearch{background:#0a1016;color:#cbd4dc;border:1px solid #2a3540;border-radius:8px;padding:7px 11px;}
#globalSearch:focus{border-color:#3c718f;background:#0d151d;}
)QSS"));
}

void ensureFirstRunTheme() {
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
    if (!settings.contains(QStringLiteral("ui/theme"))) {
        settings.setValue(QStringLiteral("ui/theme"), QStringLiteral("dark"));
        settings.sync();
    }
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Worldbuilder Writer"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.5.0-dev"));
    QCoreApplication::setOrganizationName(QStringLiteral("Worldbuilder Writer"));
    app.setStyle(QStringLiteral("Fusion"));

    ensureFirstRunTheme();
    wbw::ThemeManager::applySaved();

    wbw::MainWindow window;
    window.setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    window.setMenuBar(nullptr);
    if (auto* root = qobject_cast<QVBoxLayout*>(window.centralWidget()->layout())) {
        root->insertWidget(0, new AppTitleBar(window.centralWidget()));
    }

    applyProductStructure(window);
    wbw::installWritingShortcutBridge(&window);
    wbw::ThemeManager::applySaved();

    window.resize(1600, 1000);
    window.show();
    return app.exec();
}
