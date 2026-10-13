#include "ui/MainWindow.h"
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
#titleBarMark{background:#132131;color:#9ed0ff;border:1px solid #315a7e;border-radius:7px;font-family:'Georgia';font-size:10pt;font-weight:700;}
#titleBarBrand{color:#e7edf2;font-family:'Georgia';font-size:10.5pt;font-weight:600;}
#titleBarDivider{color:#3f4b57;font-size:10pt;}
#titleBarWorkspace{color:#75818d;font-size:8.5pt;}
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
        auto* button = new QPushButton(text, this);
        button->setObjectName(name);
        button->setFixedSize(44, 44);
        button->setFocusPolicy(Qt::NoFocus);
        return button;
    }
};

QString commonControls() {
    return QStringLiteral(R"QSS(
QWidget{color:#d8e0e8;font-family:'Segoe UI';}
QLineEdit,QTextEdit,QPlainTextEdit,QComboBox,QSpinBox,QDoubleSpinBox,QListWidget,QTreeWidget{
 background:#0d131a;color:#dce4ec;border:1px solid #26313d;border-radius:7px;selection-background-color:#245d8f;selection-color:#fff;}
QLineEdit,QComboBox,QSpinBox,QDoubleSpinBox{min-height:24px;padding:5px 8px;}
QListWidget::item,QTreeWidget::item{padding:6px 7px;border-radius:5px;}
QListWidget::item:selected,QTreeWidget::item:selected{background:#16283b;color:#dcecff;}
QPushButton,QToolButton{background:#111820;color:#c8d2dc;border:1px solid #2d3945;border-radius:7px;padding:7px 10px;}
QPushButton:hover,QToolButton:hover{background:#18232e;border-color:#3e5870;color:#fff;}
QPushButton:checked,QToolButton:checked{background:#17304a;border-color:#3d78ad;color:#9ed0ff;}
QTabWidget::pane{border:0;background:#0b0f14;}
QTabBar::tab{background:transparent;color:#82909d;border:0;border-bottom:2px solid transparent;padding:9px 13px;}
QTabBar::tab:selected{color:#e6edf3;border-bottom-color:#3c9ef3;}
QSplitter::handle{background:#1d2731;}
QScrollArea{border:0;background:#0b0f14;}
)QSS");
}

void applyNativeVisualSystem(wbw::MainWindow& window) {
    const QString common = commonControls();

    if (auto* writing = window.findChild<QWidget*>(QStringLiteral("writingPage"))) {
        if (auto* hero = writing->findChild<QWidget*>(QStringLiteral("writingHero"))) hero->hide();
        writing->setStyleSheet(common + QStringLiteral(R"QSS(
#writingPage,#writingTabs,#writingEditorTab,#sceneBoardTab{background:#0b1016;color:#d8dee7;}
#writingTabs::pane{border:0;background:#0b1016;}
#writingCommandBar{background:#0d141c;border:0;border-bottom:1px solid #28313c;padding:4px 18px;}
#writingCommandBar QToolButton,#writingCommandBar QPushButton{background:transparent;color:#a7b0ba;border:0;border-radius:5px;padding:5px 9px;}
#writingCommandBar QToolButton:hover,#writingCommandBar QPushButton:hover{background:#18222d;color:#f1f5f8;}
#writingCommandBar QToolButton:checked{background:#17364b;color:#79cfff;}
#indexPanel{background:#0c1219;border:0;border-right:1px solid #25303b;}
#indexTitle{color:#748596;font-size:7.8pt;font-weight:700;letter-spacing:1px;}
#manuscriptTree{background:transparent;color:#c7d0d8;border:0;outline:0;padding:4px 0;}
#manuscriptTree::item{padding:8px 7px;border-radius:5px;margin:1px 0;}
#manuscriptTree::item:hover{background:#131d27;color:#f0f5f8;}
#manuscriptTree::item:selected{background:#12333f;color:#78d4dc;border-left:2px solid #2dcad0;}
#editorPanel{background:#0b1016;border:0;}
#sceneEditor{background:#0b1016;color:#e6dfd5;border:0;padding:38px 64px;selection-background-color:#315b72;selection-color:#ffffff;font-family:'Georgia';font-size:13pt;}
#metadataPanel{background:#0c1219;border:0;border-left:1px solid #25303b;}
#metadataPanel QLineEdit,#metadataPanel QComboBox{background:#101821;color:#dce3e9;border:1px solid #2b3641;border-radius:5px;padding:7px 9px;}
#proofState,#wordCount{color:#75818c;font-size:8pt;}
#focusBar{background:#090d12;border-bottom:1px solid #202a34;}
#focusSceneName{color:#8995a0;font-family:'Georgia';}
#focusExit{background:transparent;border:0;color:#8a96a2;}
#sceneBoardToolbar{background:#0d141c;border:0;border-bottom:1px solid #28313c;}
#sceneBoardOutlinePanel{background:#0c1219;border:0;border-right:1px solid #25303b;}
#sceneBoardHeading{color:#eef2f4;font-family:'Georgia';font-size:18pt;font-weight:600;}
#sceneBoardSubheading,#sceneBoardSelection{color:#75818c;}
#sceneBoardCanvas{background:#0a0f15;border:0;}
#sceneBoardOutline{background:transparent;color:#bfc9d2;border:0;outline:0;}
#sceneBoardOutline::item{padding:7px;border-radius:5px;}
#sceneBoardOutline::item:selected{background:#172b3d;color:#90cfff;}
)QSS"));
    }

    if (auto* world = window.findChild<QWidget*>(QStringLiteral("worldPage"))) {
        world->setStyleSheet(common + QStringLiteral(R"QSS(
#worldPage,#worldTabs,#atlasTab,#magicTab,#mapsTab,#textsTab{background:#0b0f14;}
#worldIndexPanel{background:#0c1219;border:0;border-right:1px solid #25303b;border-radius:0;}
#worldEditorCard,#magicEditorCard,#mapEditorCard{background:#0b1016;border:0;}
#magicIndexPanel,#mapIndexPanel,#legacyMarkers{background:#0c1219;border:1px solid #26313d;border-radius:9px;}
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
    wbw::installWritingShortcutBridge(&window);
    wbw::ThemeManager::applySaved();
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Worldbuilder Writer"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.5.0-dev"));
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
