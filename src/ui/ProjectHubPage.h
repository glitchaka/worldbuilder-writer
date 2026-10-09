#pragma once

#include <QWidget>

class QGridLayout;
class QHideEvent;
class QLabel;
class QShowEvent;
class QScrollArea;
class QVBoxLayout;

namespace wbw {

class ProjectHubPage final : public QWidget {
    Q_OBJECT
public:
    explicit ProjectHubPage(QWidget* parent = nullptr);

    void refresh();

signals:
    void newProjectRequested();
    void importProjectRequested();
    void settingsRequested();
    void openProjectRequested(const QString& path);
    void projectDeleted();

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void rebuildCards();

    QWidget* cardsHost_ = nullptr;
    QGridLayout* cards_ = nullptr;
    QLabel* emptyState_ = nullptr;
};

} // namespace wbw
