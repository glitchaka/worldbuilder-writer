#include "ui/MainWindow.h"
#include "ui/LibraryMockupController.h"
#include "ui/MapMockupController.h"
#include "ui/PlanningMockupController.h"
#include "ui/ProductReorganizer.h"
#include "ui/ReviewMockupController.h"
#include "ui/SettingsWorkspace.h"
#include "ui/ThemeManager.h"
#include "ui/WritingMockupController.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
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
        setFixedHeight(30);
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(0); layout->addStretch(1);
        auto* minimize = windowButton(QStringLiteral("—"), QStringLiteral("windowMinimize"));
        auto* maximize = windowButton(QStringLiteral("□"), QStringLiteral("windowMaximize"));
        auto* close = windowButton(QStringLiteral("×"), QStringLiteral("windowClose"));
        layout->addWidget(minimize); layout->addWidget(maximize); layout->addWidget(close);
        connect(minimize, &QPushButton::clicked, this, [this]() { if (window()) window()->showMinimized(); });
        connect(maximize, &QPushButton::clicked, this, [this]() { if (window()) window()->isMaximized() ? window()->showNormal() : window()->showMaximized(); });
        connect(close, &QPushButton::clicked, this, [this]() { if (window()) window()->close(); });
    }
protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && window() && window()->windowHandle()) { window()->windowHandle()->startSystemMove(); event->accept(); return; }
        QWidget::mousePressEvent(event);
    }
    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && window()) { window()->isMaximized() ? window()->showNormal() : window()->showMaximized(); event->accept(); return; }
        QWidget::mouseDoubleClickEvent(event);
    }
private:
    QPushButton* windowButton(const QString& text, const QString& name) {
        auto* button = new QPushButton(text, this); button->setObjectName(name); button->setFixedSize(44, 30); button->setFocusPolicy(Qt::NoFocus); return button;
    }
};

void installSettingsWorkspace(wbw::MainWindow& window) {
    auto* navigation = window.findChild<QListWidget*>(QStringLiteral("sideNavigation"));
    auto* pages = window.findChild<QStackedWidget*>(QStringLiteral("pageStack"));
    if (!navigation || !pages || window.findChild<wbw::SettingsWorkspace*>()) return;
    auto* settings = new wbw::SettingsWorkspace(pages);
    settings->setObjectName(QStringLiteral("settingsWorkspace"));
    pages->addWidget(settings);
    navigation->addItem(QObject::tr("Configuración / salida"));
    QListWidgetItem* item = navigation->item(navigation->count() - 1);
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter); item->setSizeHint(QSize(136, 38));
    if (QWidget* oldAction = window.findChild<QWidget*>(QStringLiteral("settingsAction"))) oldAction->hide();
    QObject::connect(navigation, &QListWidget::currentRowChanged, &window, [&window, pages, settings](int row) {
        if (row != 6) return;
        pages->setCurrentWidget(settings);
        if (auto* title = window.findChild<QLabel*>(QStringLiteral("projectTitle"))) title->setText(QObject::tr("Configuración / salida"));
        if (auto* state = window.findChild<QLabel*>(QStringLiteral("saveState"))) state->setText(QObject::tr("Preferencias, copias y exportación"));
        if (auto* save = window.findChild<QPushButton*>(QStringLiteral("primarySave"))) save->hide();
        if (auto* focus = window.findChild<QPushButton*>(QStringLiteral("secondaryAction"))) focus->hide();
    });
}

void applyProductUi(wbw::MainWindow& window) {
    wbw::applyProductReorganization(&window);
    installSettingsWorkspace(window);
    wbw::installLibraryMockupController(&window);
    wbw::installMapMockupController(&window);
    wbw::installWritingMockupController(&window);
    wbw::installPlanningMockupController(&window);
    wbw::installReviewMockupController(&window);
    wbw::ThemeManager::applySaved();
}

int runRcSmoke(wbw::MainWindow& window, const QString& outputDirectory) {
    QDir dir(QDir::cleanPath(outputDirectory.isEmpty() ? QStringLiteral("rc-qa") : outputDirectory));
    if (!dir.exists() && !QDir().mkpath(dir.absolutePath())) return 90;
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
            nav->setCurrentRow(row);
            QApplication::processEvents();
            applyProductUi(window);
            QApplication::processEvents();
            switch (row) {
                case 0:
                    require(window.findChild<QWidget*>(QStringLiteral("projectHubPage")) != nullptr, QStringLiteral("libraryPage"));
                    require(window.findChild<QWidget*>(QStringLiteral("hubCardsHost")) != nullptr, QStringLiteral("libraryCards"));
                    break;
                case 1:
                    require(window.findChild<QWidget*>(QStringLiteral("sceneEditor")) != nullptr, QStringLiteral("manuscriptEditor"));
                    require(window.findChild<QWidget*>(QStringLiteral("indexPanel")) != nullptr, QStringLiteral("manuscriptIndex"));
                    require(window.findChild<QShortcut*>(QStringLiteral("wbwFocusShortcut")) != nullptr, QStringLiteral("focusShortcut"));
                    require(window.findChild<QShortcut*>(QStringLiteral("wbwProofShortcut")) != nullptr, QStringLiteral("proofShortcut"));
                    break;
                case 2:
                    require(window.findChild<QWidget*>(QStringLiteral("sceneBoardCanvas")) != nullptr, QStringLiteral("sceneBoard"));
                    break;
                case 3:
                    require(window.findChild<QWidget*>(QStringLiteral("worldIndexPanel")) != nullptr, QStringLiteral("atlasIndex"));
                    require(window.findChild<QWidget*>(QStringLiteral("atlasContextRail")) != nullptr, QStringLiteral("atlasContextRail"));
                    require(window.findChild<QWidget*>(QStringLiteral("planningContextRail")) != nullptr, QStringLiteral("planningContextRail"));
                    break;
                case 4:
                    require(window.findChild<QWidget*>(QStringLiteral("pilinReyEditor")) != nullptr, QStringLiteral("mapEditor"));
                    require(window.findChild<QWidget*>(QStringLiteral("pilinToolPalette")) != nullptr, QStringLiteral("mapToolPalette"));
                    require(window.findChild<QWidget*>(QStringLiteral("pilinTopCommands")) != nullptr, QStringLiteral("mapCommands"));
                    break;
                case 5:
                    require(window.findChild<QWidget*>(QStringLiteral("reviewAnalysisTab")) != nullptr, QStringLiteral("reviewAnalysis"));
                    require(window.findChild<QWidget*>(QStringLiteral("reviewDashboard")) != nullptr, QStringLiteral("reviewDashboard"));
                    require(window.findChild<QWidget*>(QStringLiteral("reviewSectionBar")) != nullptr, QStringLiteral("reviewSectionBar"));
                    break;
                case 6:
                    require(window.findChild<wbw::SettingsWorkspace*>() != nullptr, QStringLiteral("settingsWorkspace"));
                    require(window.findChild<QListWidget*>(QStringLiteral("cloudBackups")) != nullptr, QStringLiteral("cloudSettings"));
                    break;
            }
            const QPixmap shot = window.grab();
            const QString screenshotPath = dir.absoluteFilePath(QStringLiteral("%1.png").arg(slugs.at(row)));
            require(!shot.isNull(), QStringLiteral("screenshot-%1").arg(slugs.at(row)));
            require(!shot.isNull() && shot.save(screenshotPath, "PNG"), QStringLiteral("screenshot-write-%1").arg(slugs.at(row)));
        }
    }

    QFile report(dir.absoluteFilePath(QStringLiteral("rc-smoke.txt")));
    if (!report.open(QIODevice::WriteOnly | QIODevice::Text)) return 93;
    QTextStream out(&report);
    out << (failures.isEmpty() ? QStringLiteral("PASS\n") : QStringLiteral("FAIL\n"));
    for (const QString& failure : failures) out << failure << '\n';
    report.close();
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
    QTimer::singleShot(0, [&window]() { applyProductUi(window); });
    if (rcSmoke) {
        QString qaDir = QStringLiteral("rc-qa");
        for (const QString& arg : args) if (arg.startsWith(QStringLiteral("--qa-dir="))) qaDir = arg.mid(QStringLiteral("--qa-dir=").size());
        QTimer::singleShot(900, &app, [&app, &window, qaDir]() { app.exit(runRcSmoke(window, qaDir)); });
    }
    return app.exec();
}
