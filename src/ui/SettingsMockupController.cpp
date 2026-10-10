#include "ui/SettingsMockupController.h"

#include "ui/SettingsWorkspace.h"

#include <QEvent>
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
        page->setStyleSheet(QStringLiteral(R"QSS(
#settingsWorkspace{background:palette(window);color:palette(window-text);}
#settingsKicker{color:#b98a53;font-size:8pt;font-weight:700;letter-spacing:1px;}
#settingsWorkspaceTitle{color:palette(text);font-family:'Georgia';font-size:25pt;font-weight:600;}
#settingsWorkspaceCopy{color:palette(window-text);font-size:9.5pt;}
#settingsWorkspaceNavigation{background:palette(base);border:1px solid palette(mid);border-radius:10px;padding:7px;color:palette(window-text);outline:0;}
#settingsWorkspaceNavigation::item{padding:8px 10px;border-radius:7px;}
#settingsWorkspaceNavigation::item:selected{background:palette(highlight);color:palette(highlighted-text);border-left:2px solid #bd8e56;}
#settingsWorkspacePages,#settingsWorkspacePage{background:palette(window);}
#settingsPageTitle{color:palette(text);font-family:'Georgia';font-size:18pt;font-weight:600;}
#settingsPageCopy{color:palette(window-text);}
#settingsCard{background:palette(base);border:1px solid palette(mid);border-radius:10px;}
#settingsCardTitle{color:palette(text);font-size:11pt;font-weight:700;}
#settingsCardCopy,#settingsOutputText,#cloudStatus{color:palette(window-text);}
#settingsSecondary{background:palette(alternate-base);color:palette(button-text);border:1px solid palette(mid);border-radius:7px;padding:7px 11px;}
#settingsPrimary{background:#c59a5d;color:#111315;border:0;border-radius:7px;padding:7px 11px;font-weight:700;}
#settingsWorkspace QLineEdit,#settingsWorkspace QComboBox,#settingsWorkspace QSpinBox,#settingsWorkspace QKeySequenceEdit{background:palette(base);color:palette(text);border:1px solid palette(mid);border-radius:7px;padding:7px 9px;}
#settingsWorkspace QCheckBox{color:palette(window-text);spacing:8px;}
#cloudBackups{background:palette(base);color:palette(text);border:1px solid palette(mid);border-radius:8px;outline:0;}
#cloudBackups::item{padding:8px;border-bottom:1px solid palette(mid);}
#cloudBackups::item:selected{background:palette(highlight);color:palette(highlighted-text);}
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
