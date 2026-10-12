#include "ui/MainWindow.h"
#include "ui/CartographicCanvasController.h"
#include "ui/MapSelectionController.h"
#include "ui/TerrainModelSyncController.h"
#include "ui/ThemeManager.h"
#include "ui/WritingShortcutBridge.h"

#include <QApplication>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWindow>

namespace {

class AppTitleBar final : public QWidget {
public:
    explicit AppTitleBar(QWidget* parent = nullptr) : QWidget(parent) {
        setObjectName(QStringLiteral("appTitleBar"));
        setFixedHeight(46);
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(18, 0, 0, 0);
        layout->setSpacing(11);

        auto* mark = new QLabel(QStringLiteral("WW"), this);
        mark->setObjectName(QStringLiteral("titleBarMark"));
        mark->setAlignment(Qt::AlignCenter);
        mark->setFixedSize(30, 30);
        auto* brand = new QLabel(QStringLiteral("Worldbuilder Writer"), this);
        brand->setObjectName(QStringLiteral("titleBarBrand"));
        auto* divider = new QLabel(QStringLiteral("›"), this);
        divider->setObjectName(QStringLiteral("titleBarDivider"));
        auto* workspace = new QLabel(QObject::tr("Espacio de trabajo"), this);
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

        connect(minimize, &QPushButton::clicked, this, [this]() { if (window()) window()->showMinimized(); });
        connect(maximize, &QPushButton::clicked, this, [this]() { if (window()) window()->isMaximized() ? window()->showNormal() : window()->showMaximized(); });
        connect(close, &QPushButton::clicked, this, [this]() { if (window()) window()->close(); });

        setStyleSheet(QStringLiteral(R"QSS(
#appTitleBar{background:#090d12;border-bottom:1px solid #202832;}
#titleBarMark{background:#111b28;color:#a8d5ff;border:1px solid #315a7e;border-radius:7px;font-family:'Georgia';font-size:8.5pt;font-weight:700;}
#titleBarBrand{color:#f0ece5;font-family:'Georgia';font-size:11pt;font-weight:600;}
#titleBarDivider{color:#4f5a66;font-size:12pt;}
#titleBarWorkspace{color:#798591;font-size:8.5pt;}
#windowMinimize,#windowMaximize,#windowClose{background:transparent;color:#8e99a4;border:0;border-radius:0;font-size:10pt;}
#windowMinimize:hover,#windowMaximize:hover{background:#151d26;color:#f0f4f7;}
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
        button->setFixedSize(44, 46);
        button->setFocusPolicy(Qt::NoFocus);
        return button;
    }
};

void installNativeProductServices(wbw::MainWindow& window) {
    wbw::installMapSelectionController(&window);
    wbw::installTerrainModelSyncController(&window);
    wbw::installCartographicCanvasController(&window);
    wbw::installWritingShortcutBridge(&window);
    wbw::ThemeManager::applySaved();
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Worldbuilder Writer"));
    QCoreApplication::setOrganizationName(QStringLiteral("Worldbuilder Writer"));
    app.setStyle(QStringLiteral("Fusion"));
    wbw::ThemeManager::applySaved();

    wbw::MainWindow window;
    window.setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    window.setMenuBar(nullptr);
    if (auto* root = qobject_cast<QVBoxLayout*>(window.centralWidget()->layout()))
        root->insertWidget(0, new AppTitleBar(window.centralWidget()));

    installNativeProductServices(window);
    window.resize(1600, 1000);
    window.show();
    return app.exec();
}
