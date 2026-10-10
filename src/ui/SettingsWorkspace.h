#pragma once

#include <QJsonArray>
#include <QWidget>

class QComboBox;
class QLabel;
class QKeySequenceEdit;
class QLayout;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QStackedWidget;

namespace wbw {

class GoogleDriveService;

class SettingsWorkspace final : public QWidget {
    Q_OBJECT
public:
    explicit SettingsWorkspace(QWidget* parent = nullptr);

signals:
    void preferencesChanged();

private:
    QWidget* makePage(const QString& title, const QString& description);
    QWidget* makeCard(const QString& title, const QString& description, QLayout* body);
    void persistEditor();
    void persistShortcuts();
    void chooseBackupDirectory();
    void refreshCloudState();
    void populateCloudBackups(const QJsonArray& files);

    QListWidget* navigation_ = nullptr;
    QStackedWidget* pages_ = nullptr;
    QComboBox* theme_ = nullptr;
    QComboBox* editorFont_ = nullptr;
    QSpinBox* editorFontSize_ = nullptr;
    QSpinBox* autosaveSeconds_ = nullptr;
    QComboBox* proofLanguage_ = nullptr;
    QKeySequenceEdit* focusShortcut_ = nullptr;
    QKeySequenceEdit* proofShortcut_ = nullptr;
    QLineEdit* backupDirectory_ = nullptr;

    GoogleDriveService* drive_ = nullptr;
    QLineEdit* clientId_ = nullptr;
    QLineEdit* clientSecret_ = nullptr;
    QLabel* cloudStatus_ = nullptr;
    QListWidget* cloudBackups_ = nullptr;
    QPushButton* connectDrive_ = nullptr;
    QPushButton* disconnectDrive_ = nullptr;
};

} // namespace wbw
