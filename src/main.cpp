#include "ui/MainWindow.h"
#include "ui/SettingsWorkspace.h"
#include "ui/ThemeManager.h"
#include "ui/WritingShortcutBridge.h"

#include <QApplication>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
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
        setFixedHeight(42);
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(14, 0, 0, 0);
        layout->setSpacing(9);

        auto* mark = new QLabel(QStringLiteral("W"), this);
        mark->setObjectName(QStringLiteral("titleBarMark"));
        mark->setAlignment(Qt::AlignCenter);
        mark->setFixedSize(26, 26);
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
#appTitleBar{border-bottom:1px solid palette(mid);}
#titleBarMark{border-radius:6px;font-family:'Georgia';font-size:9.5pt;font-weight:700;}
#titleBarBrand{font-family:'Georgia';font-size:10pt;font-weight:600;}
#titleBarDivider{font-size:9pt;}
#titleBarWorkspace{font-size:8pt;}
#windowMinimize,#windowMaximize,#windowClose{background:transparent;border:0;border-radius:0;padding:0;font-size:10pt;}
#windowMinimize:hover,#windowMaximize:hover{background:palette(alternate-base);}
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
        button->setFixedSize(42, 42);
        button->setFocusPolicy(Qt::NoFocus);
        return button;
    }
};

void applyProductStructure(wbw::MainWindow& window) {
    if (auto* writing = window.findChild<QWidget*>(QStringLiteral("writingPage"))) {
        if (auto* hero = writing->findChild<QWidget*>(QStringLiteral("writingHero"))) hero->hide();
    }
    if (auto* top = window.findChild<QWidget*>(QStringLiteral("topShell"))) top->hide();
    if (auto* identity = window.findChild<QWidget*>(QStringLiteral("appIdentity"))) identity->hide();
    if (auto* rail = window.findChild<QWidget*>(QStringLiteral("sideRail"))) rail->setFixedWidth(90);
    if (auto* save = window.findChild<QPushButton*>(QStringLiteral("primarySave"))) save->hide();
    if (auto* focus = window.findChild<QPushButton*>(QStringLiteral("secondaryAction"))) focus->hide();
    if (auto* settings = window.findChild<wbw::SettingsWorkspace*>()) settings->finalizeProductControls();
}

void ensureFirstRunTheme() {
    QSettings settings(QStringLiteral("WorldbuilderWriter"), QStringLiteral("WorldbuilderWriter"));
    if (!settings.contains(QStringLiteral("ui/theme"))) {
        settings.setValue(QStringLiteral("ui/theme"), QStringLiteral("light"));
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
