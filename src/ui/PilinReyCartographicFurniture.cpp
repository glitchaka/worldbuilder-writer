#include "ui/PilinReyEditor.h"

#include <QApplication>
#include <QEvent>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QWidget>

namespace wbw {
namespace {

class CartographicFurniture final : public QWidget {
public:
    explicit CartographicFurniture(QWidget* viewport) : QWidget(viewport), viewport_(viewport) {
        setObjectName(QStringLiteral("pilinCartographicFurniture"));
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setFocusPolicy(Qt::NoFocus);
        viewport_->installEventFilter(this);
        setGeometry(viewport_->rect());
        show();
        raise();
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == viewport_ && event->type() == QEvent::Resize) {
            setGeometry(viewport_->rect());
            update();
        }
        return QWidget::eventFilter(watched, event);
    }

    void paintEvent(QPaintEvent*) override {
        if (width() < 540 || height() < 340) return;
        const bool dark = qApp && qApp->property("wbwDarkMode").toBool();
        const QColor strong = dark ? QColor(229, 221, 203, 165) : QColor(73, 62, 49, 158);
        const QColor weak = dark ? QColor(229, 221, 203, 76) : QColor(73, 62, 49, 70);
        const QColor plate = dark ? QColor(9, 13, 18, 145) : QColor(246, 240, 228, 180);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF outer = rect().adjusted(24, 24, -24, -24);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(weak, 1.0));
        painter.drawRect(outer);
        painter.setPen(QPen(QColor(weak.red(), weak.green(), weak.blue(), weak.alpha() / 2), 1.0));
        painter.drawRect(outer.adjusted(4, 4, -4, -4));

        const qreal corner = 24.0;
        painter.setPen(QPen(strong, 1.35));
        auto cornerMark = [&](const QPointF& p, qreal sx, qreal sy) {
            painter.drawLine(p, p + QPointF(corner * sx, 0));
            painter.drawLine(p, p + QPointF(0, corner * sy));
            painter.drawLine(p + QPointF(6 * sx, 6 * sy), p + QPointF(15 * sx, 6 * sy));
            painter.drawLine(p + QPointF(6 * sx, 6 * sy), p + QPointF(6 * sx, 15 * sy));
        };
        cornerMark(outer.topLeft(), 1, 1);
        cornerMark(outer.topRight(), -1, 1);
        cornerMark(outer.bottomLeft(), 1, -1);
        cornerMark(outer.bottomRight(), -1, -1);

        // Only draw information that is semantically valid without assuming a projection
        // or world-unit scale. Distance remains the job of the real Measure tool.
        drawCompass(painter, QPointF(outer.right() - 54, outer.top() + 58), strong, weak, plate, dark);
    }

private:
    static void drawCompass(QPainter& painter, const QPointF& center, const QColor& strong,
                            const QColor& weak, const QColor& plate, bool dark) {
        painter.save();
        painter.setPen(QPen(weak, 1.0));
        painter.setBrush(plate);
        painter.drawEllipse(center, 35, 35);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(center, 27, 27);

        QPainterPath north;
        north.moveTo(center + QPointF(0, -28));
        north.lineTo(center + QPointF(-6, 4));
        north.lineTo(center);
        north.lineTo(center + QPointF(6, 4));
        north.closeSubpath();
        painter.setPen(QPen(strong, 1.1));
        painter.setBrush(strong);
        painter.drawPath(north);

        QPainterPath south;
        south.moveTo(center + QPointF(0, 28));
        south.lineTo(center + QPointF(-5, -3));
        south.lineTo(center);
        south.lineTo(center + QPointF(5, -3));
        south.closeSubpath();
        painter.setBrush(dark ? QColor(185, 177, 161, 65) : QColor(73, 62, 49, 48));
        painter.drawPath(south);

        painter.setPen(QPen(strong, 1.0));
        painter.drawLine(center + QPointF(-25, 0), center + QPointF(25, 0));
        painter.drawLine(center + QPointF(0, -25), center + QPointF(0, 25));

        QFont font(QStringLiteral("Georgia"));
        font.setBold(true);
        font.setPointSizeF(8.5);
        painter.setFont(font);
        painter.setPen(strong);
        painter.drawText(QRectF(center.x() - 10, center.y() - 50, 20, 16), Qt::AlignCenter, QStringLiteral("N"));
        painter.restore();
    }

    QWidget* viewport_ = nullptr;
};

} // namespace

void PilinReyEditor::installCartographicFurniture() {
    if (!canvasHost_) return;
    QWidget* viewportWidget = canvasHost_->findChild<QWidget*>(QStringLiteral("pilinQtViewport"));
    if (!viewportWidget) return;
    if (viewportWidget->findChild<QWidget*>(QStringLiteral("pilinCartographicFurniture"), Qt::FindDirectChildrenOnly)) return;
    new CartographicFurniture(viewportWidget);
}

} // namespace wbw
