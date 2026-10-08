#pragma once

#include <QGraphicsView>
#include <QJsonArray>

namespace wbw {

class RelationshipBoard final : public QGraphicsView {
    Q_OBJECT
public:
    explicit RelationshipBoard(QWidget* parent = nullptr);

    void setData(const QJsonArray& characters, const QJsonArray& relationships);
    void focusCharacter(const QString& characterId);
    void showAllCharacters();

signals:
    void characterMoved(const QString& id, double x, double y);
    void characterActivated(const QString& id);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void rebuild();

    QJsonArray characters_;
    QJsonArray relationships_;
    QString focusId_;
};

} // namespace wbw
