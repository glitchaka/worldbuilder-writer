#pragma once

#include <QWidget>

class QComboBox;
class QLineEdit;
class QListWidget;
class QSpinBox;
class QStackedWidget;

namespace wbw {

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
    void chooseBackupDirectory();

    QListWidget* navigation_ = nullptr;
    QStackedWidget* pages_ = nullptr;
    QComboBox* theme_ = nullptr;
    QComboBox* editorFont_ = nullptr;
    QSpinBox* editorFontSize_ = nullptr;
    QSpinBox* autosaveSeconds_ = nullptr;
    QComboBox* proofLanguage_ = nullptr;
    QLineEdit* backupDirectory_ = nullptr;
};

} // namespace wbw
