#pragma once

#include <QJsonObject>
#include <QImage>
#include <QPointF>
#include <QPointer>
#include <QWidget>

namespace wbw {

class PilinReyEditor;

class CartographicCanvasV2 final : public QWidget {
public:
    explicit CartographicCanvasV2(PilinReyEditor* editor, QWidget* inputViewport, QWidget* parent = nullptr);

    void setMap(const QJsonObject& map);
    void fitDocument();
    QWidget* inputViewport() const { return inputViewport_; }

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
    void drawObject(QPainter& painter, const QJsonObject& object, const QJsonObject& pilin, double sx, double sy);
    void drawTree(QPainter& painter, const QPointF& p, double size, const QColor& ink);
    void drawMountain(QPainter& painter, const QPointF& p, double size, const QColor& ink);
    QPointF screenToWorld(const QPointF& p) const;
    QPointF worldToScreen(const QPointF& p) const;
    QPointF worldToInputScreen(const QPointF& world) const;
    bool panMode() const;
    void forwardMouse(QMouseEvent* event);

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
