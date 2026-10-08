#include "ui/RelationshipBoard.h"

#include <QFont>
#include <QFrame>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QHash>
#include <QJsonObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QSet>
#include <QWheelEvent>

namespace wbw {
namespace {

constexpr qreal CardWidth = 180.0;
constexpr qreal CardHeight = 78.0;

QColor relationshipColor(const QString& type) {
    const QString value = type.toLower();
    if (value.contains(QStringLiteral("enem")) || value.contains(QStringLiteral("conflict"))) return QColor(QStringLiteral("#b7655f"));
    if (value.contains(QStringLiteral("famil"))) return QColor(QStringLiteral("#8d78b5"));
    if (value.contains(QStringLiteral("amor")) || value.contains(QStringLiteral("rom"))) return QColor(QStringLiteral("#b76f87"));
    if (value.contains(QStringLiteral("ali")) || value.contains(QStringLiteral("amist"))) return QColor(QStringLiteral("#6f9b79"));
    return QColor(QStringLiteral("#a88a56"));
}

QPointF boardPosition(const QJsonObject& character, int index) {
    const QJsonObject board = character.value(QStringLiteral("board")).toObject();
    if (board.contains(QStringLiteral("x")) && board.contains(QStringLiteral("y"))) {
        return QPointF(board.value(QStringLiteral("x")).toDouble(), board.value(QStringLiteral("y")).toDouble());
    }
    const int column = index % 4;
    const int row = index / 4;
    return QPointF(60.0 + column * 240.0, 60.0 + row * 140.0);
}

bool visibleOnBoard(const QJsonObject& character) {
    const QJsonObject board = character.value(QStringLiteral("board")).toObject();
    return !board.contains(QStringLiteral("visible")) || board.value(QStringLiteral("visible")).toBool(true);
}

} // namespace

RelationshipBoard::RelationshipBoard(QWidget* parent) : QGraphicsView(parent) {
    setScene(new QGraphicsScene(this));
    setRenderHint(QPainter::Antialiasing, true);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setBackgroundBrush(QColor(QStringLiteral("#181914")));
    setFrameShape(QFrame::NoFrame);
}

void RelationshipBoard::setData(const QJsonArray& characters, const QJsonArray& relationships) {
    characters_ = characters;
    relationships_ = relationships;
    rebuild();
}

void RelationshipBoard::focusCharacter(const QString& characterId) {
    focusId_ = characterId;
    rebuild();
}

void RelationshipBoard::showAllCharacters() {
    focusId_.clear();
    rebuild();
}

void RelationshipBoard::rebuild() {
    scene()->clear();
    QHash<QString, QPointF> positions;
    QHash<QString, QJsonObject> byId;
    QSet<QString> allowed;

    if (!focusId_.isEmpty()) {
        allowed.insert(focusId_);
        for (const QJsonValue value : relationships_) {
            const QJsonObject relationship = value.toObject();
            const QString source = relationship.value(QStringLiteral("source")).toString();
            const QString target = relationship.value(QStringLiteral("target")).toString();
            if (source == focusId_) allowed.insert(target);
            if (target == focusId_) allowed.insert(source);
        }
    }

    for (int i = 0; i < characters_.size(); ++i) {
        const QJsonObject character = characters_.at(i).toObject();
        const QString id = character.value(QStringLiteral("id")).toString();
        if (id.isEmpty() || !visibleOnBoard(character)) continue;
        if (!focusId_.isEmpty() && !allowed.contains(id)) continue;
        positions.insert(id, boardPosition(character, i));
        byId.insert(id, character);
    }

    for (const QJsonValue value : relationships_) {
        const QJsonObject relationship = value.toObject();
        const QString source = relationship.value(QStringLiteral("source")).toString();
        const QString target = relationship.value(QStringLiteral("target")).toString();
        if (!positions.contains(source) || !positions.contains(target)) continue;
        const QPointF a = positions.value(source) + QPointF(CardWidth / 2.0, CardHeight / 2.0);
        const QPointF b = positions.value(target) + QPointF(CardWidth / 2.0, CardHeight / 2.0);
        const double strength = qBound(1.0, relationship.value(QStringLiteral("strength")).toDouble(2.0), 5.0);
        QPen pen(relationshipColor(relationship.value(QStringLiteral("type")).toString()), 1.0 + strength * 0.55);
        pen.setCosmetic(true);
        scene()->addLine(QLineF(a, b), pen);
        const QString label = relationship.value(QStringLiteral("label")).toString();
        if (!label.isEmpty()) {
            auto* textItem = scene()->addSimpleText(label);
            textItem->setBrush(QColor(QStringLiteral("#bcb7ab")));
            textItem->setPos((a + b) / 2.0 + QPointF(6.0, -18.0));
            textItem->setZValue(1.0);
        }
    }

    for (auto it = positions.cbegin(); it != positions.cend(); ++it) {
        const QString id = it.key();
        const QJsonObject character = byId.value(id);
        auto* card = scene()->addRect(QRectF(0.0, 0.0, CardWidth, CardHeight), QPen(QColor(QStringLiteral("#4b493f"))), QBrush(QColor(QStringLiteral("#262720"))));
        card->setPos(it.value());
        card->setFlag(QGraphicsItem::ItemIsMovable, true);
        card->setFlag(QGraphicsItem::ItemIsSelectable, true);
        card->setData(0, id);
        card->setZValue(2.0);

        auto* name = new QGraphicsSimpleTextItem(character.value(QStringLiteral("name")).toString(QStringLiteral("Personaje")), card);
        name->setBrush(QColor(QStringLiteral("#f0eadf")));
        QFont nameFont = name->font();
        nameFont.setBold(true);
        nameFont.setPointSizeF(11.0);
        name->setFont(nameFont);
        name->setPos(12.0, 11.0);

        QString detail = character.value(QStringLiteral("role")).toString();
        const QString affiliation = character.value(QStringLiteral("affiliation")).toString();
        if (!affiliation.isEmpty()) detail += detail.isEmpty() ? affiliation : QStringLiteral(" · ") + affiliation;
        auto* sub = new QGraphicsSimpleTextItem(detail, card);
        sub->setBrush(QColor(QStringLiteral("#aaa69b")));
        sub->setPos(12.0, 39.0);
    }

    const QRectF bounds = scene()->itemsBoundingRect().adjusted(-120.0, -120.0, 120.0, 120.0);
    scene()->setSceneRect(bounds.isValid() ? bounds : QRectF(-500.0, -350.0, 1000.0, 700.0));
}

void RelationshipBoard::wheelEvent(QWheelEvent* event) {
    const qreal factor = event->angleDelta().y() > 0 ? 1.12 : 1.0 / 1.12;
    const qreal current = transform().m11();
    if ((factor > 1.0 && current < 3.5) || (factor < 1.0 && current > 0.25)) scale(factor, factor);
    event->accept();
}

void RelationshipBoard::mouseDoubleClickEvent(QMouseEvent* event) {
    if (auto* item = itemAt(event->pos())) {
        QGraphicsItem* card = item;
        while (card && card->data(0).toString().isEmpty()) card = card->parentItem();
        if (card && !card->data(0).toString().isEmpty()) emit characterActivated(card->data(0).toString());
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void RelationshipBoard::mouseReleaseEvent(QMouseEvent* event) {
    QGraphicsView::mouseReleaseEvent(event);
    for (QGraphicsItem* item : scene()->items()) {
        const QString id = item->data(0).toString();
        if (id.isEmpty()) continue;
        emit characterMoved(id, item->pos().x(), item->pos().y());
    }
}

} // namespace wbw
