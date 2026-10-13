#include "ui/PilinReyEditor.h"

#include <QApplication>
#include <QEvent>
#include <QFont>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QToolButton>
#include <QVector>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <cmath>

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
        QTimer::singleShot(0, this, [this]() { fitDocument(); });
    }

    void setMap(const QJsonObject& map) {
        map_ = map;
        const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        docW_ = std::max(1.0, pilin.value(QStringLiteral("width")).toDouble(4096.0));
        docH_ = std::max(1.0, pilin.value(QStringLiteral("height")).toDouble(2304.0));
        rebuildLinework();
        if (autoFit_) fitDocument();
        update();
    }

    void fitDocument() {
        if (!viewport_ || viewport_->width() < 100 || viewport_->height() < 100) return;
        zoom_ = std::clamp(std::min((viewport_->width() - 104.0) / docW_, (viewport_->height() - 92.0) / docH_), .025, 10.0);
        pan_ = QPointF((viewport_->width() - docW_ * zoom_) * .5, (viewport_->height() - docH_ * zoom_) * .5);
        autoFit_ = true;
        update();
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched != viewport_) return QWidget::eventFilter(watched, event);

        if (event->type() == QEvent::Resize) {
            setGeometry(viewport_->rect());
            if (autoFit_) fitDocument();
            else update();
        } else if (event->type() == QEvent::Wheel) {
            auto* wheel = static_cast<QWheelEvent*>(event);
            autoFit_ = false;
            const QPointF cursor = wheel->position();
            const QPointF before = screenToWorld(cursor);
            zoom_ = std::clamp(zoom_ * (wheel->angleDelta().y() > 0 ? 1.12 : 1.0 / 1.12), .02, 14.0);
            pan_ = cursor - QPointF(before.x() * zoom_, before.y() * zoom_);
            update();
        } else if (event->type() == QEvent::MouseButtonPress) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            const Qt::CursorShape cursorShape = viewport_->cursor().shape();
            if (mouse->button() == Qt::MiddleButton || cursorShape == Qt::OpenHandCursor || cursorShape == Qt::ClosedHandCursor) {
                panning_ = true;
                autoFit_ = false;
                lastMouse_ = mouse->position();
            }
        } else if (event->type() == QEvent::MouseMove && panning_) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            pan_ += mouse->position() - lastMouse_;
            lastMouse_ = mouse->position();
            update();
        } else if (event->type() == QEvent::MouseButtonRelease) {
            panning_ = false;
        }
        return QWidget::eventFilter(watched, event);
    }

    void paintEvent(QPaintEvent*) override {
        if (width() < 420 || height() < 280) return;
        const bool dark = qApp && qApp->property("wbwDarkMode").toBool();
        const QColor furnitureStrong = dark ? QColor(229, 221, 203, 165) : QColor(73, 62, 49, 158);
        const QColor furnitureWeak = dark ? QColor(229, 221, 203, 76) : QColor(73, 62, 49, 70);
        const QColor plate = dark ? QColor(9, 13, 18, 145) : QColor(246, 240, 228, 180);
        const QColor coast = dark ? QColor(228, 216, 193, 126) : QColor(63, 55, 44, 150);
        const QColor contour = dark ? QColor(220, 207, 182, 36) : QColor(92, 75, 57, 52);
        const QColor bathy = dark ? QColor(169, 204, 220, 34) : QColor(69, 110, 128, 46);
        const QColor hachure = dark ? QColor(225, 216, 197, 56) : QColor(74, 61, 47, 70);
        const QColor vegetation = dark ? QColor(167, 193, 159, 42) : QColor(48, 79, 48, 52);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF page(worldToScreen(QPointF(0, 0)), worldToScreen(QPointF(docW_, docH_)));
        painter.save();
        painter.setClipRect(page);

        if (zoom_ > .06) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(bathy, 0.85, Qt::DashLine));
            drawSegments(painter, bathymetric_);
            painter.setPen(QPen(contour, 0.8));
            drawSegments(painter, contours_);
        }

        painter.setPen(QPen(coast, 1.15));
        drawSegments(painter, coastline_);

        if (zoom_ > .095) {
            painter.setPen(QPen(hachure, 0.75, Qt::SolidLine, Qt::RoundCap));
            drawSegments(painter, hachures_);
        }
        if (zoom_ > .12) {
            painter.setPen(QPen(vegetation, 0.7, Qt::SolidLine, Qt::RoundCap));
            drawSegments(painter, forestMarks_);
        }
        painter.restore();

        drawFrameAndCompass(painter, furnitureStrong, furnitureWeak, plate, dark);
    }

private:
    QPointF worldToScreen(const QPointF& point) const {
        return QPointF(point.x() * zoom_ + pan_.x(), point.y() * zoom_ + pan_.y());
    }

    QPointF screenToWorld(const QPointF& point) const {
        return QPointF((point.x() - pan_.x()) / zoom_, (point.y() - pan_.y()) / zoom_);
    }

    void drawSegments(QPainter& painter, const QVector<QLineF>& segments) const {
        for (const QLineF& worldLine : segments) {
            painter.drawLine(QLineF(worldToScreen(worldLine.p1()), worldToScreen(worldLine.p2())));
        }
    }

    void rebuildLinework() {
        coastline_.clear();
        contours_.clear();
        bathymetric_.clear();
        hachures_.clear();
        forestMarks_.clear();

        const QJsonObject pilin = map_.value(QStringLiteral("pilinRey")).toObject();
        const QJsonObject terrain = pilin.value(QStringLiteral("terrainModel")).toObject();
        const int tw = terrain.value(QStringLiteral("width")).toInt();
        const int th = terrain.value(QStringLiteral("height")).toInt();
        const QJsonArray values = terrain.value(QStringLiteral("values")).toArray();
        const QJsonArray biomes = terrain.value(QStringLiteral("biomes")).toArray();
        if (tw < 3 || th < 3 || values.size() < tw * th) return;

        const double sea = std::clamp(terrain.value(QStringLiteral("seaLevel")).toDouble(.42), .05, .85);
        auto heightAt = [&](int x, int y) {
            x = std::clamp(x, 0, tw - 1);
            y = std::clamp(y, 0, th - 1);
            return values.at(y * tw + x).toInt() / 255.0;
        };
        auto biomeAt = [&](int x, int y) {
            if (biomes.size() < tw * th) return 0;
            x = std::clamp(x, 0, tw - 1);
            y = std::clamp(y, 0, th - 1);
            return biomes.at(y * tw + x).toInt();
        };
        auto toWorld = [&](double gx, double gy) {
            return QPointF(gx / std::max(1, tw - 1) * docW_, gy / std::max(1, th - 1) * docH_);
        };

        auto appendContour = [&](double level, QVector<QLineF>& target) {
            for (int y = 0; y < th - 1; ++y) {
                for (int x = 0; x < tw - 1; ++x) {
                    const double v0 = heightAt(x, y);
                    const double v1 = heightAt(x + 1, y);
                    const double v2 = heightAt(x + 1, y + 1);
                    const double v3 = heightAt(x, y + 1);
                    QVector<QPointF> crosses;
                    crosses.reserve(4);

                    auto edge = [&](double ax, double ay, double av, double bx, double by, double bv) {
                        const double da = av - level;
                        const double db = bv - level;
                        if ((da < 0.0 && db < 0.0) || (da > 0.0 && db > 0.0) || std::abs(av - bv) < 1e-9) return;
                        const double t = std::clamp((level - av) / (bv - av), 0.0, 1.0);
                        crosses.append(toWorld(ax + (bx - ax) * t, ay + (by - ay) * t));
                    };

                    edge(x, y, v0, x + 1, y, v1);
                    edge(x + 1, y, v1, x + 1, y + 1, v2);
                    edge(x + 1, y + 1, v2, x, y + 1, v3);
                    edge(x, y + 1, v3, x, y, v0);

                    if (crosses.size() == 2) target.append(QLineF(crosses.at(0), crosses.at(1)));
                    else if (crosses.size() == 4) {
                        const double center = (v0 + v1 + v2 + v3) * .25;
                        if (center >= level) {
                            target.append(QLineF(crosses.at(0), crosses.at(3)));
                            target.append(QLineF(crosses.at(1), crosses.at(2)));
                        } else {
                            target.append(QLineF(crosses.at(0), crosses.at(1)));
                            target.append(QLineF(crosses.at(2), crosses.at(3)));
                        }
                    }
                }
            }
        };

        appendContour(sea, coastline_);
        for (double level : {sea + .08, sea + .16, sea + .25, sea + .34, sea + .44}) {
            if (level < .97) appendContour(level, contours_);
        }
        for (double level : {sea - .08, sea - .17}) {
            if (level > .03) appendContour(level, bathymetric_);
        }

        const double cellW = docW_ / std::max(1, tw - 1);
        const double cellH = docH_ / std::max(1, th - 1);
        for (int y = 3; y < th - 3; y += 4) {
            for (int x = 3; x < tw - 3; x += 4) {
                const double h = heightAt(x, y);
                if (h <= sea + .11) continue;
                const double dx = heightAt(x + 1, y) - heightAt(x - 1, y);
                const double dy = heightAt(x, y + 1) - heightAt(x, y - 1);
                const double slope = std::hypot(dx, dy);
                if (slope > .027) {
                    const double inv = 1.0 / std::max(slope, 1e-6);
                    const double length = std::clamp((slope - .02) * 80.0, .45, 1.45);
                    const QPointF center = toWorld(x, y);
                    const QPointF delta(dx * inv * cellW * length, dy * inv * cellH * length);
                    hachures_.append(QLineF(center - delta * .42, center + delta * .58));
                }

                if (biomeAt(x, y) == 2 && ((x * 73856093u) ^ (y * 19349663u)) % 3u == 0u) {
                    const QPointF base = toWorld(x, y);
                    const double trunk = cellH * .72;
                    const double crown = cellW * .58;
                    forestMarks_.append(QLineF(base + QPointF(0, trunk * .22), base - QPointF(0, trunk * .36)));
                    forestMarks_.append(QLineF(base - QPointF(0, trunk * .28), base + QPointF(-crown, trunk * .04)));
                    forestMarks_.append(QLineF(base - QPointF(0, trunk * .28), base + QPointF(crown, trunk * .04)));
                }
            }
        }
    }

    void drawFrameAndCompass(QPainter& painter, const QColor& strong, const QColor& weak,
                             const QColor& plate, bool dark) const {
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

        drawCompass(painter, QPointF(outer.right() - 54, outer.top() + 58), strong, weak, plate, dark);
    }

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
    QJsonObject map_;
    QVector<QLineF> coastline_;
    QVector<QLineF> contours_;
    QVector<QLineF> bathymetric_;
    QVector<QLineF> hachures_;
    QVector<QLineF> forestMarks_;
    double docW_ = 4096.0;
    double docH_ = 2304.0;
    double zoom_ = .2;
    QPointF pan_{40, 40};
    QPointF lastMouse_;
    bool autoFit_ = true;
    bool panning_ = false;
};

} // namespace

void PilinReyEditor::installCartographicFurniture() {
    if (!canvasHost_) return;
    QWidget* viewportWidget = canvasHost_->findChild<QWidget*>(QStringLiteral("pilinQtViewport"));
    if (!viewportWidget) return;

    auto* overlay = viewportWidget->findChild<CartographicFurniture*>(QStringLiteral("pilinCartographicFurniture"), Qt::FindDirectChildrenOnly);
    if (!overlay) {
        overlay = new CartographicFurniture(viewportWidget);
        connect(this, &PilinReyEditor::mapEdited, overlay, [overlay](const QJsonObject& map) { overlay->setMap(map); });
        for (QToolButton* button : findChildren<QToolButton*>()) {
            if (button->toolTip().contains(tr("Encajar mapa"), Qt::CaseInsensitive)) {
                connect(button, &QToolButton::clicked, overlay, [overlay]() { overlay->fitDocument(); }, Qt::UniqueConnection);
            }
        }
    }
    overlay->setMap(map_);
    overlay->raise();
}

} // namespace wbw
