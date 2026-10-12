#include "ui/MainWindow.h"
#include "ui/AtlasWorkspaceController.h"
#include "ui/CartographicCanvasController.h"
#include "ui/MapMockupController.h"
#include "ui/MapSelectionController.h"
#include "ui/MapWorkflowController.h"
#include "ui/TerrainModelSyncController.h"
#include "ui/PlanningMockupController.h"
#include "ui/ReviewMockupController.h"
#include "ui/SettingsWorkspace.h"
#include "ui/ThemeManager.h"
#include "ui/WritingMockupController.h"
#include "ui/WritingShortcutBridge.h"
#include "ui/WritingPerformanceController.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

namespace {

class AppTitleBar final : public QWidget {
public:
    explicit AppTitleBar(QWidget* parent = nullptr) : QWidget(parent) {
        setObjectName(QStringLiteral("appTitleBar"));
        setFixedHeight(44);
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(14, 0, 0, 0);
        layout->setSpacing(10);

        auto* mark = new QLabel(QStringLiteral("WW"), this);
        mark->setObjectName(QStringLiteral("titleBarMark"));
        mark->setAlignment(Qt::AlignCenter);
        mark->setFixedSize(28, 28);
        auto* brand = new QLabel(QStringLiteral("Worldbuilder Writer"), this);
        brand->setObjectName(QStringLiteral("titleBarBrand"));
        layout->addWidget(mark);
        layout->addWidget(brand);
        auto* divider = new QLabel(QStringLiteral("›"), this);
        divider->setObjectName(QStringLiteral("titleBarDivider"));
        layout->addWidget(divider);
        auto* workspace = new QLabel(QObject::tr("Espacio de trabajo"), this);
        workspace->setObjectName(QStringLiteral("titleBarWorkspace"));
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
#appTitleBar{background:#0a0f15;border-bottom:1px solid #202934;}
#titleBarMark{background:#142235;color:#9fd1ff;border:1px solid #315a7e;border-radius:6px;font-family:'Georgia';font-size:8pt;font-weight:700;}
#titleBarBrand{color:#eee9e1;font-family:'Georgia';font-size:10.5pt;font-weight:600;}
#titleBarDivider{color:#55616d;font-size:12pt;}
#titleBarWorkspace{color:#7f8b97;font-size:8.5pt;}
#windowMinimize,#windowMaximize,#windowClose{background:transparent;color:#8d99a5;border:0;border-radius:0;font-size:10pt;}
#windowMinimize:hover,#windowMaximize:hover{background:#17202a;color:#f0f4f7;}
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

void applyProductUi(wbw::MainWindow& window) {
    wbw::installMapMockupController(&window);
    wbw::installMapSelectionController(&window);
    wbw::installMapWorkflowController(&window);
    wbw::installTerrainModelSyncController(&window);
    wbw::installCartographicCanvasController(&window);
    wbw::installWritingMockupController(&window);
    wbw::installWritingPerformanceController(&window);
    wbw::installWritingShortcutBridge(&window);
    wbw::installPlanningMockupController(&window);
    wbw::installAtlasWorkspaceController(&window);
    wbw::installReviewMockupController(&window);
    wbw::ThemeManager::applySaved();
}

void settleUi() {
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 40);
}

int runRcSmoke(wbw::MainWindow& window, const QString& outputDirectory) {
    QDir dir(QDir::cleanPath(outputDirectory.isEmpty() ? QStringLiteral("rc-qa") : outputDirectory));
    if (!dir.exists() && !QDir().mkpath(dir.absolutePath())) return 90;
    QFile progress(dir.absoluteFilePath(QStringLiteral("rc-progress.txt")));
    if (progress.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream p(&progress); p << "START\n"; p.flush();
    }
    auto mark = [&progress](const QString& line) { if (!progress.isOpen()) return; QTextStream p(&progress); p << line << '\n'; p.flush(); };
    QStringList failures;
    auto require = [&failures](bool condition, const QString& name) { if (!condition) failures.append(name); };
    auto* nav = window.findChild<QListWidget*>(QStringLiteral("sideNavigation"));
    auto* pages = window.findChild<QStackedWidget*>(QStringLiteral("pageStack"));
    require(nav != nullptr, QStringLiteral("sideNavigation"));
    require(pages != nullptr, QStringLiteral("pageStack"));
    require(window.windowFlags().testFlag(Qt::FramelessWindowHint), QStringLiteral("framelessWindow"));
    QMenuBar* nativeMenu = window.findChild<QMenuBar*>(QString(), Qt::FindDirectChildrenOnly);
    require(!nativeMenu || !nativeMenu->isVisible(), QStringLiteral("nativeMenuHidden"));
    const QStringList expectedNavigation{QObject::tr("Biblioteca"), QObject::tr("Escritura"), QObject::tr("Tablero de escenas"), QObject::tr("Atlas / documentación"), QObject::tr("Mapas"), QObject::tr("Revisión"), QObject::tr("Configuración / salida")};
    require(nav && nav->count() == static_cast<int>(expectedNavigation.size()), QStringLiteral("sevenProductModules"));
    if (nav) for (int i = 0; i < qMin(nav->count(), static_cast<int>(expectedNavigation.size())); ++i) require(nav->item(i)->text() == expectedNavigation.at(i), QStringLiteral("navigation[%1]").arg(i));
    const QStringList slugs{QStringLiteral("library"), QStringLiteral("writing"), QStringLiteral("scenes"), QStringLiteral("atlas"), QStringLiteral("maps"), QStringLiteral("review"), QStringLiteral("settings")};
    if (nav) {
        for (int row = 0; row < qMin(nav->count(), static_cast<int>(slugs.size())); ++row) {
            const QString slug = slugs.at(row); mark(QStringLiteral("ROUTE %1 BEGIN").arg(slug)); nav->setCurrentRow(row); settleUi(); window.repaint(); settleUi(); mark(QStringLiteral("ROUTE %1 SETTLED").arg(slug));
            switch (row) {
                case 0: require(window.findChild<QWidget*>(QStringLiteral("projectHubPage")) != nullptr, QStringLiteral("libraryPage")); require(window.findChild<QWidget*>(QStringLiteral("hubCardsHost")) != nullptr, QStringLiteral("libraryCards")); break;
                case 1: require(window.findChild<QWidget*>(QStringLiteral("sceneEditor")) != nullptr, QStringLiteral("manuscriptEditor")); require(window.findChild<QWidget*>(QStringLiteral("indexPanel")) != nullptr, QStringLiteral("manuscriptIndex")); require(window.findChild<QShortcut*>(QStringLiteral("wbwFocusShortcut")) != nullptr, QStringLiteral("focusShortcut")); require(window.findChild<QShortcut*>(QStringLiteral("wbwProofShortcut")) != nullptr, QStringLiteral("proofShortcut")); break;
                case 2: require(window.findChild<QWidget*>(QStringLiteral("sceneBoardCanvas")) != nullptr, QStringLiteral("sceneBoard")); break;
                case 3: require(window.findChild<QWidget*>(QStringLiteral("worldIndexPanel")) != nullptr, QStringLiteral("atlasIndex")); require(window.findChild<QWidget*>(QStringLiteral("atlasContextRail")) != nullptr, QStringLiteral("atlasContextRail")); require(window.findChild<QWidget*>(QStringLiteral("planningContextRail")) != nullptr, QStringLiteral("planningContextRail")); break;
                case 4: require(window.findChild<QWidget*>(QStringLiteral("pilinReyEditor")) != nullptr, QStringLiteral("mapEditor")); require(window.findChild<QWidget*>(QStringLiteral("pilinToolPalette")) != nullptr, QStringLiteral("mapToolPalette")); require(window.findChild<QWidget*>(QStringLiteral("pilinTopCommands")) != nullptr, QStringLiteral("mapCommands")); require(window.findChild<QWidget*>(QStringLiteral("mapWorkflowStrip")) != nullptr, QStringLiteral("mapWorkflowStrip")); require(window.findChild<QWidget*>(QStringLiteral("cartographicCanvasV3")) != nullptr, QStringLiteral("terrainAwareCartographicCanvas")); require(window.findChild<QWidget*>(QStringLiteral("mapGenerateWorld")) != nullptr, QStringLiteral("proceduralWorldGenerator")); break;
                case 5: require(window.findChild<QWidget*>(QStringLiteral("reviewAnalysisTab")) != nullptr, QStringLiteral("reviewAnalysis")); require(window.findChild<QWidget*>(QStringLiteral("reviewDashboard")) != nullptr, QStringLiteral("reviewDashboard")); require(window.findChild<QWidget*>(QStringLiteral("reviewSectionBar")) != nullptr, QStringLiteral("reviewSectionBar")); break;
                case 6: require(window.findChild<wbw::SettingsWorkspace*>() != nullptr, QStringLiteral("settingsWorkspace")); require(window.findChild<QListWidget*>(QStringLiteral("cloudBackups")) != nullptr, QStringLiteral("cloudSettings")); break;
            }
            const QPixmap shot = window.grab();
            require(!shot.isNull(), QStringLiteral("screenshot-%1").arg(slug));
            require(!shot.isNull() && shot.save(dir.absoluteFilePath(QStringLiteral("%1.png").arg(slug)), "PNG"), QStringLiteral("screenshot-write-%1").arg(slug));
            mark(QStringLiteral("ROUTE %1 CAPTURED").arg(slug));
        }
    }
    QFile report(dir.absoluteFilePath(QStringLiteral("rc-smoke.txt")));
    if (!report.open(QIODevice::WriteOnly | QIODevice::Text)) return 93;
    QTextStream out(&report); out << (failures.isEmpty() ? QStringLiteral("PASS\n") : QStringLiteral("FAIL\n")); for (const QString& failure : failures) out << failure << '\n'; report.close(); mark(QStringLiteral("DONE")); progress.close();
    return failures.isEmpty() ? 0 : 2;
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    const QStringList args = QApplication::arguments();
    const bool rcSmoke = args.contains(QStringLiteral("--rc-smoke"));
    if (rcSmoke) app.setQuitOnLastWindowClosed(false);
    QCoreApplication::setApplicationName(QStringLiteral("Worldbuilder Writer"));
    QCoreApplication::setOrganizationName(QStringLiteral("Worldbuilder Writer"));
    app.setStyle(QStringLiteral("Fusion"));
    wbw::ThemeManager::applySaved();

    wbw::MainWindow window;
    window.setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    window.setMenuBar(nullptr);
    if (auto* root = qobject_cast<QVBoxLayout*>(window.centralWidget()->layout())) root->insertWidget(0, new AppTitleBar(window.centralWidget()));

    applyProductUi(window);
    window.resize(1600, 1000);
    window.show();

    if (rcSmoke) {
        QString qaDir = QStringLiteral("rc-qa");
        for (const QString& arg : args) if (arg.startsWith(QStringLiteral("--qa-dir="))) qaDir = arg.mid(QStringLiteral("--qa-dir=").size());
        QTimer::singleShot(900, &app, [&app, &window, qaDir]() { app.exit(runRcSmoke(window, qaDir)); });
    }
    return app.exec();
}
