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
        layout->addWidget(mark); layout->addWidget(brand); layout->addWidget(divider); layout->addWidget(workspace); layout->addStretch(1);
        auto* minimize = windowButton(QStringLiteral("—"), QStringLiteral("windowMinimize"));
        auto* maximize = windowButton(QStringLiteral("□"), QStringLiteral("windowMaximize"));
        auto* close = windowButton(QStringLiteral("×"), QStringLiteral("windowClose"));
        layout->addWidget(minimize); layout->addWidget(maximize); layout->addWidget(close);
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
        if (event->button() == Qt::LeftButton && window() && window()->windowHandle()) { window()->windowHandle()->startSystemMove(); event->accept(); return; }
        QWidget::mousePressEvent(event);
    }
    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && window()) { window()->isMaximized() ? window()->showNormal() : window()->showMaximized(); event->accept(); return; }
        QWidget::mouseDoubleClickEvent(event);
    }
private:
    QPushButton* windowButton(const QString& text, const QString& name) {
        auto* button = new QPushButton(text, this); button->setObjectName(name); button->setFixedSize(44, 46); button->setFocusPolicy(Qt::NoFocus); return button;
    }
};

void applyNativeVisualSystem(wbw::MainWindow& window) {
    const QString common = QStringLiteral(R"QSS(
QWidget{color:#d8e0e8;font-family:'Segoe UI';}
QLineEdit,QTextEdit,QPlainTextEdit,QComboBox,QSpinBox,QDoubleSpinBox,QListWidget,QTreeWidget{
 background:#0d131a;color:#dce4ec;border:1px solid #26313d;border-radius:7px;selection-background-color:#245d8f;selection-color:#fff;}
QLineEdit,QComboBox,QSpinBox,QDoubleSpinBox{min-height:24px;padding:5px 8px;}
QTextEdit,QPlainTextEdit{padding:8px;}
QListWidget::item,QTreeWidget::item{padding:6px 7px;border-radius:5px;}
QListWidget::item:selected,QTreeWidget::item:selected{background:#16283b;color:#dcecff;}
QPushButton,QToolButton{background:#111820;color:#c8d2dc;border:1px solid #2d3945;border-radius:7px;padding:7px 10px;}
QPushButton:hover,QToolButton:hover{background:#18232e;border-color:#3e5870;color:#fff;}
QPushButton:checked,QToolButton:checked{background:#17304a;border-color:#3d78ad;color:#9ed0ff;}
QTabWidget::pane{border:0;background:#0b0f14;}
QTabBar::tab{background:transparent;color:#82909d;border:0;border-bottom:2px solid transparent;padding:9px 13px;}
QTabBar::tab:selected{color:#e6edf3;border-bottom-color:#3c9ef3;}
QSplitter::handle{background:#111820;}
QScrollArea{border:0;background:#0b0f14;}
)QSS");

    if (auto* writing = window.findChild<QWidget*>(QStringLiteral("writingPage"))) {
        writing->setStyleSheet(common + QStringLiteral(R"QSS(
#writingPage,#writingTabs,#writingEditorTab,#sceneBoardTab{background:#0b0f14;}
#writingHero,#indexPanel,#metadataPanel,#sceneBoardToolbar,#sceneBoardOutlinePanel{background:#0f151c;border:1px solid #26313d;border-radius:9px;}
#editorPanel{background:#0b0f14;border:0;}
#writingTitle,#sceneBoardHeading{color:#f1ede6;font-family:'Georgia';font-size:20pt;font-weight:600;}
#writingDescription,#sceneBoardSubheading,#sceneBoardSelection,#proofState{color:#7f8b97;}
#writingKicker,#sectionLabel,#indexTitle,#editorPanelTitle{color:#7eaee0;font-size:8pt;font-weight:700;letter-spacing:1px;}
#sceneEditor{background:#0b0f14;color:#e7e2d9;border:0;padding:30px 42px;font-family:'Georgia';font-size:13pt;selection-background-color:#294e72;}
#writingCommandBar{background:#0d131a;border:1px solid #202b36;border-radius:8px;}
#focusBar{background:#090d12;border-bottom:1px solid #202832;}
#focusSceneName{color:#8995a0;}
#sceneBoardCanvas{background:#0b1016;border:1px solid #26313d;border-radius:9px;}
)QSS"));
    }

    if (auto* world = window.findChild<QWidget*>(QStringLiteral("worldPage"))) {
        world->setStyleSheet(common + QStringLiteral(R"QSS(
#worldPage,#worldTabs,#atlasTab,#magicTab,#mapsTab,#textsTab{background:#0b0f14;}
#worldIndexPanel,#worldEditorCard,#magicIndexPanel,#magicEditorCard,#mapIndexPanel,#mapEditorCard,#legacyMarkers{background:#0f151c;border:1px solid #26313d;border-radius:9px;}
#pageKicker,#fieldTitle,#panelTitle{color:#7eaee0;font-size:8pt;font-weight:700;letter-spacing:.7px;}
#pageTitle{font-family:'Georgia';font-size:25pt;color:#f1ede6;}
#pageDescription{font-family:'Georgia';color:#8995a0;}
#generatorBar{background:#0d131a;border:1px solid #26313d;border-radius:8px;}
)QSS"));
    }

    if (auto* review = window.findChild<QWidget*>(QStringLiteral("reviewPage"))) {
        review->setStyleSheet(common + QStringLiteral(R"QSS(
#reviewPage,#reviewTabs,#reviewProjectTab,#reviewAnalysisTab,#reviewLayoutTab,#reviewMediaTab{background:#0b0f14;}
#reviewHero,#reviewCard{background:#0f151c;border:1px solid #26313d;border-radius:9px;}
#reviewKicker,#reviewFieldTitle{color:#7eaee0;font-size:8pt;font-weight:700;letter-spacing:.7px;}
#reviewTitle{font-family:'Georgia';font-size:25pt;color:#f1ede6;}
#reviewDescription{font-family:'Georgia';color:#8995a0;}
#reviewCardTitle{font-family:'Georgia';font-size:14pt;color:#eee9e1;}
#reviewPrimary{background:#2e7fc7;color:#fff;border:0;font-weight:700;}
#reviewPrimary:hover{background:#378ed7;}
)QSS"));
    }

    if (auto* settings = window.findChild<QWidget*>(QStringLiteral("settingsWorkspace"))) {
        settings->setStyleSheet(common + QStringLiteral(R"QSS(
#settingsWorkspace,#settingsContent,#settingsWorkspacePages{background:#0b0f14;}
#settingsSide{background:#090d12;border-right:1px solid #202832;}
#settingsWorkspaceNavigation{background:transparent;border:0;outline:0;}
#settingsWorkspaceNavigation::item{padding:9px;border-radius:8px;color:#93a0ad;}
#settingsWorkspaceNavigation::item:selected{background:#16283b;color:#dbeeff;border-left:2px solid #43a4f5;}
#settingsNavKicker{color:#768594;font-size:8pt;font-weight:700;letter-spacing:1px;}
#settingsWorkspaceTitle{color:#f1ede6;font-family:'Georgia';font-size:25pt;font-weight:600;}
#settingsWorkspaceCopy{color:#8995a0;font-family:'Georgia';}
#settingsCard{background:#0f151c;border:1px solid #26313d;border-radius:9px;}
#settingsCardTitle{color:#eee9e1;font-family:'Georgia';font-size:13pt;font-weight:600;}
#settingsCardCopy,#settingsOutputText{color:#85919d;}
#settingsPrimary{background:#2e7fc7;color:#fff;border:0;font-weight:700;}
)QSS"));
    }
}

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
    QCoreApplication::setApplicationVersion(QStringLiteral("0.5.0-rc1"));
    QCoreApplication::setOrganizationName(QStringLiteral("Worldbuilder Writer"));
    app.setStyle(QStringLiteral("Fusion"));
    wbw::ThemeManager::applySaved();

    wbw::MainWindow window;
    window.setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    window.setMenuBar(nullptr);
    if (auto* root = qobject_cast<QVBoxLayout*>(window.centralWidget()->layout())) root->insertWidget(0, new AppTitleBar(window.centralWidget()));

    installNativeProductServices(window);
    applyNativeVisualSystem(window);
    window.resize(1600, 1000);
    window.show();
    return app.exec();
}
