#pragma once

#include <QImage>
#include <QJsonObject>
#include <QPointF>
#include <QPointer>
#include <QWidget>

namespace wbw {

class PilinReyEditor;

class CartographicCanvasV3 final : public QWidget {
    Q_OBJECT
public:
    explicit CartographicCanvasV3(PilinReyEditor* editor, QWidget* inputViewport, QWidget* parent = nullptr);

    void setMap(const QJsonObject& map);
    void fitDocument();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void scheduleRebuild();
    void rebuildCache();
    void drawTerrainModel(QPainter& painter, const QJsonObject& pilin, int width, int height);
    void drawObject(QPainter& painter, const QJsonObject& object, const QJsonObject& pilin, double sx, double sy);
    QPointF screenToWorld(const QPointF& p) const;
    QPointF worldToScreen(const QPointF& p) const;
    QPointF worldToInputScreen(const QPointF& p) const;
    void forwardMouse(QMouseEvent* event);
    bool panMode() const;

    QPointer<PilinReyEditor> editor_;
    QPointer<QWidget> inputViewport_;
    QJsonObject map_;
    QImage cache_;
    bool rebuildPending_ = false;
    bool panning_ = false;
    QPointF lastMouse_;
    double zoom_ = 1.0;
    QPointF pan_;
};

} // namespace wbw
