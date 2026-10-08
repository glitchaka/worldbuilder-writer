#pragma once

#include <QGraphicsView>
#include <QJsonObject>
#include <QPixmap>
#include <QtMath>

namespace wbw {

class MapCanvas final : public QGraphicsView {
    Q_OBJECT
public:
    explicit MapCanvas(QWidget* parent = nullptr);

    void setMap(const QJsonObject& map);

signals:
    void markerMoved(const QString& id, double x, double y);
    void addMarkerRequested(double x, double y);
    void markerActivated(const QString& id);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void rebuild();
    QPixmap generatedBackground() const;

    QJsonObject map_;
};

} // namespace wbw
