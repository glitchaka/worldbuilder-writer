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
            nav->setMinimumWidth(window_->width() < 1180 ? 184 : 208);
            nav->setMaximumWidth(window_->width() < 1180 ? 192 : 222);
        }

        page->setStyleSheet(QStringLiteral(R"QSS(
#settingsWorkspace{background:#0a0c0f;color:#d9d4ca;}
#settingsKicker{color:#b88a52;font-size:7.5pt;font-weight:700;letter-spacing:1.3px;}
#settingsWorkspaceTitle{color:#f2eee5;font-family:'Georgia';font-size:25pt;font-weight:600;}
#settingsWorkspaceCopy{color:#7c7e78;font-size:9.5pt;}
#settingsWorkspaceNavigation{background:#0d1013;border:1px solid #2b2b27;border-radius:10px;padding:7px;color:#94958f;outline:0;}
#settingsWorkspaceNavigation::item{padding:9px 10px;border-radius:7px;margin:1px 0;}
#settingsWorkspaceNavigation::item:hover{background:#171a1d;color:#f1ede4;}
#settingsWorkspaceNavigation::item:selected{background:#28251f;color:#e7c28a;border-left:2px solid #b88a52;}
#settingsWorkspacePages,#settingsWorkspacePage{background:#0a0c0f;}
#settingsPageTitle{color:#f1ede4;font-family:'Georgia';font-size:19pt;font-weight:600;}
#settingsPageCopy{color:#7b7d77;}
#settingsCard{background:#111416;border:1px solid #2b2b27;border-radius:10px;}
#settingsCardTitle{color:#f0ece3;font-size:11pt;font-weight:700;}
#settingsCardCopy,#settingsOutputText,#cloudStatus{color:#7f817c;}
#settingsSecondary{background:#151819;color:#cac6be;border:1px solid #34352f;border-radius:7px;padding:7px 11px;}
#settingsSecondary:hover{background:#20211f;color:#fff9ee;}
#settingsPrimary{background:#b88a52;color:#11120f;border:0;border-radius:7px;padding:7px 11px;font-weight:700;}
#settingsPrimary:hover{background:#c79a61;}
#settingsWorkspace QLineEdit,#settingsWorkspace QComboBox,#settingsWorkspace QSpinBox,#settingsWorkspace QKeySequenceEdit{background:#0d1013;color:#ddd8cf;border:1px solid #33342f;border-radius:7px;padding:8px 9px;}
#settingsWorkspace QLineEdit:focus,#settingsWorkspace QComboBox:focus,#settingsWorkspace QSpinBox:focus,#settingsWorkspace QKeySequenceEdit:focus{border-color:#8d6841;}
#settingsWorkspace QCheckBox{color:#c5c0b7;spacing:8px;}
#cloudBackups{background:#0d1013;color:#dcd7ce;border:1px solid #30312c;border-radius:8px;outline:0;}
#cloudBackups::item{padding:8px;border-bottom:1px solid #23241f;}
#cloudBackups::item:hover{background:#171a1d;}
#cloudBackups::item:selected{background:#28251f;color:#e7c28a;}
)QSS"));
    }

private:
    QPointer<QMainWindow> window_;
};

} // namespace

void installSettingsMockupController(QMainWindow* window) {
    if (!window || window->property("wbwSettingsMockupControllerV3").toBool()) return;
    window->setProperty("wbwSettingsMockupControllerV3", true);
    auto* filter = new SettingsWorkspaceFilter(window);
    window->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter]() { filter->apply(); });
}

} // namespace wbw
