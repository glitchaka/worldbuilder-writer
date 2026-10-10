#pragma once

#include <QDialog>
#include <QJsonArray>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QStackedWidget;

namespace wbw {

class GoogleDriveService;

class SettingsDialog final : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private:
    void refreshState();
    void populateBackups(const QJsonArray& backups);
    void persistEditorPreferences();

    GoogleDriveService* drive_ = nullptr;
    QListWidget* sectionList_ = nullptr;
    QStackedWidget* sectionStack_ = nullptr;
    QComboBox* theme_ = nullptr;
    QComboBox* editorFont_ = nullptr;
    QSpinBox* editorFontSize_ = nullptr;
    QSpinBox* autosaveSeconds_ = nullptr;
    QComboBox* proofLanguage_ = nullptr;
    QLineEdit* backupDirectory_ = nullptr;
    QLineEdit* clientId_ = nullptr;
    QLineEdit* clientSecret_ = nullptr;
    QLabel* status_ = nullptr;
    QListWidget* backups_ = nullptr;
    QPushButton* connectButton_ = nullptr;
    QPushButton* disconnectButton_ = nullptr;
    QPushButton* refreshButton_ = nullptr;
};

} // namespace wbw
