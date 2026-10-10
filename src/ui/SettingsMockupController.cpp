#include "ui/SettingsMockupController.h"

#include "ui/SettingsWorkspace.h"

#include <QEvent>
#include <QListWidget>
#include <QMainWindow>
#include <QPointer>
#include <QTimer>

namespace wbw {
namespace {

class SettingsWorkspaceFilter final : public QObject {
public:
    explicit SettingsWorkspaceFilter(QMainWindow* window) : QObject(window), window_(window) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == window_ && (event->type() == QEvent::Show || event->type() == QEvent::Resize))
            QTimer::singleShot(0, this, [this]() { apply(); });
        return QObject::eventFilter(watched, event);
    }

    void apply() {
        if (!window_) return;
        auto* page = window_->findChild<SettingsWorkspace*>();
        if (!page) return;

        if (auto* nav = page->findChild<QListWidget*>(QStringLiteral("settingsWorkspaceNavigation"))) {
            nav->setMinimumWidth(window_->width() < 1180 ? 180 : 205);
            nav->setMaximumWidth(window_->width() < 1180 ? 190 : 220);
        }

        page->setStyleSheet(QStringLiteral(R"QSS(
#settingsWorkspace{background:#0e1116;color:#d9dee7;}
#settingsKicker{color:#c59a5d;font-size:8pt;font-weight:700;letter-spacing:1px;}
#settingsWorkspaceTitle{color:#f0f2f5;font-family:'Georgia';font-size:25pt;font-weight:600;}
#settingsWorkspaceCopy{color:#818c99;font-size:9.5pt;}
#settingsWorkspaceNavigation{background:#11161d;border:1px solid #29323d;border-radius:10px;padding:7px;color:#9ba5b2;outline:0;}
#settingsWorkspaceNavigation::item{padding:9px 10px;border-radius:7px;margin:1px 0;}
#settingsWorkspaceNavigation::item:hover{background:#18202a;color:#eef1f4;}
#settingsWorkspaceNavigation::item:selected{background:#222c38;color:#ffffff;border-left:2px solid #c59a5d;}
#settingsWorkspacePages,#settingsWorkspacePage{background:#0e1116;}
#settingsPageTitle{color:#f0f2f5;font-family:'Georgia';font-size:19pt;font-weight:600;}
#settingsPageCopy{color:#818c99;}
#settingsCard{background:#131920;border:1px solid #2a333f;border-radius:10px;}
#settingsCardTitle{color:#eef1f4;font-size:11pt;font-weight:700;}
#settingsCardCopy,#settingsOutputText,#cloudStatus{color:#8792a0;}
#settingsSecondary{background:#171e27;color:#cfd6df;border:1px solid #303a46;border-radius:7px;padding:7px 11px;}
#settingsSecondary:hover{background:#202a35;color:#ffffff;}
#settingsPrimary{background:#c59a5d;color:#111315;border:0;border-radius:7px;padding:7px 11px;font-weight:700;}
#settingsPrimary:hover{background:#d3a86c;}
#settingsWorkspace QLineEdit,#settingsWorkspace QComboBox,#settingsWorkspace QSpinBox,#settingsWorkspace QKeySequenceEdit{background:#10151b;color:#dce1e7;border:1px solid #2d3743;border-radius:7px;padding:8px 9px;}
#settingsWorkspace QLineEdit:focus,#settingsWorkspace QComboBox:focus,#settingsWorkspace QSpinBox:focus,#settingsWorkspace QKeySequenceEdit:focus{border-color:#8b6843;}
#settingsWorkspace QCheckBox{color:#c5ccd5;spacing:8px;}
#cloudBackups{background:#10151b;color:#dce1e7;border:1px solid #2d3743;border-radius:8px;outline:0;}
#cloudBackups::item{padding:8px;border-bottom:1px solid #242d37;}
#cloudBackups::item:hover{background:#18202a;}
#cloudBackups::item:selected{background:#222c38;color:#ffffff;}
#settingsWorkspace QScrollBar:vertical{background:#0e1116;width:9px;}
#settingsWorkspace QScrollBar::handle:vertical{background:#343c47;border-radius:4px;min-height:34px;}
)QSS"));
    }

private:
    QPointer<QMainWindow> window_;
};

} // namespace

void installSettingsMockupController(QMainWindow* window) {
    if (!window || window->property("wbwSettingsMockupController").toBool()) return;
    window->setProperty("wbwSettingsMockupController", true);
    auto* filter = new SettingsWorkspaceFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
