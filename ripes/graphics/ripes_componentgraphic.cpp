#include "ripes_componentgraphic.h"

#include "ripes_graphics_defines.h"
#include "ripes_graphics_util.h"

#include <qmath.h>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

namespace ripes {

ComponentGraphic::ComponentGraphic(Component* c) : m_component(c) {}

void ComponentGraphic::initialize() {
    Q_ASSERT(scene() != nullptr);

    setFlags(ItemIsSelectable | ItemIsMovable | ItemSendsScenePositionChanges);
    setAcceptHoverEvents(true);

    m_displayText = QString::fromStdString(m_component->getDisplayName());
    m_font = QFont("Times", 10);

    // Get IO of Component
    for (const auto& c : m_component->getInputs()) {
        m_inputPositionMap[c] = QPointF();
    }
    for (const auto& c : m_component->getOutputs()) {
        m_outputPositionMap[c] = QPointF();
    }

    m_hasSubcomponents = m_component->getSubComponents().size() > 0;
    if (m_hasSubcomponents) {
        // Setup expand button
        m_expandButton = new QToolButton();
        m_expandButton->setCheckable(true);
        QObject::connect(m_expandButton, &QToolButton::toggled, [=](bool state) { setExpandState(state); });
        m_expandButtonProxy = scene()->addWidget(m_expandButton);
        m_expandButtonProxy->setParentItem(this);

        setExpandState(false);
    }

    createSubcomponents();

    calculateGeometry();
}

void ComponentGraphic::createSubcomponents() {
    for (auto& c : m_component->getSubComponents()) {
        auto nc = new ComponentGraphic(c);
        scene()->addItem(nc);
        nc->initialize();
        nc->setParentItem(this);
        m_subcomponents.push_back(nc);
        if (!m_isExpanded) {
            nc->hide();
        }
    }
}

void ComponentGraphic::setExpandState(bool expanded) {
    m_isExpanded = expanded;

    if (!m_isExpanded) {
        m_expandButton->setIcon(QIcon(":/icons/plus.svg"));
        for (const auto& c : m_subcomponents) {
            c->hide();
        }
    } else {
        m_expandButton->setIcon(QIcon(":/icons/minus.svg"));
        for (const auto& c : m_subcomponents) {
            c->show();
        }
    }

    // Recalculate geometry based on now showing child components
    calculateGeometry();

    update();
}

void ComponentGraphic::calculateGeometry() {
    // Order matters!
    calculateSubcomponentRect();
    calculateBaseRect();
    calculateTextPosition();
    calculateIOPositions();
}

void ComponentGraphic::calculateSubcomponentRect() {
    if (m_isExpanded) {
        QRectF subBoundingRect(pos(), QSize(0, 0));
        for (const auto& c : m_subcomponents) {
            // c->setPosition(subBoundingRect.topRight());
            subBoundingRect = boundingRectOfRects(subBoundingRect, c->boundingRect());
        }
        m_subcomponentRect = subBoundingRect;
    } else {
        m_subcomponentRect = QRectF();
    }
}

QRectF ComponentGraphic::sceneBaseRect() const {
    return baseRect().translated(scenePos());
}

QVariant ComponentGraphic::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionChange && scene() && parentItem()) {
        // Restrict position changes to inside parent item
        const QRectF parentRect = static_cast<ComponentGraphic*>(parentItem())->baseRect();
        const QRectF thisRect = boundingRect();
        const QPointF offset = thisRect.topLeft();
        QPointF newPos = value.toPointF();
        if (!parentRect.contains(thisRect.translated(newPos))) {
            // Keep the item inside the scene rect.
            newPos.setX(qMin(parentRect.right() - thisRect.width(), qMax(newPos.x(), parentRect.left())));
            newPos.setY(qMin(parentRect.bottom() - thisRect.height(), qMax(newPos.y(), parentRect.top())));
            return newPos - offset;
        }
    }
    return QGraphicsItem::itemChange(change, value);
}

void ComponentGraphic::calculateTextPosition() {
    QPointF basePos(m_baseRect.width() / 2 - m_textRect.width() / 2, 0);
    if (m_isExpanded) {
        // Move text to top of component to make space for subcomponents
        basePos.setY(BUTTON_INDENT + m_textRect.height());
    } else {
        basePos.setY(m_baseRect.height() / 2);
    }
    m_textPos = basePos;
}

void ComponentGraphic::calculateIOPositions() {
    if (m_isExpanded) {
        // Some fancy logic for positioning IO positions in the best way to facilitate nice signal lines between
        // components
    } else {
        // Component is unexpanded - IO should be positionen in even positions
        int i = 0;
        for (auto& c : m_inputPositionMap) {
            c = QPointF(m_baseRect.left(), (m_baseRect.height() / (m_inputPositionMap.size() + 1)) * (1 + i));
            i++;
        }
        i = 0;
        for (auto& c : m_outputPositionMap) {
            c = QPointF(m_baseRect.right(), (m_baseRect.height() / (m_outputPositionMap.size() + 1)) * (1 + i));
            i++;
        }
    }
}

void ComponentGraphic::calculateBaseRect() {
    // ------------------ Base rect ------------------------
    QRectF baseRect(0, 0, TOP_MARGIN + BOT_MARGIN, SIDE_MARGIN * 2);

    // Calculate text width
    QFontMetrics fm(m_font);
    m_textRect = fm.boundingRect(m_displayText);
    baseRect.adjust(0, 0, m_textRect.width(), m_textRect.height());

    // Include expand button in baserect sizing
    if (m_hasSubcomponents) {
        baseRect.adjust(0, 0, m_expandButtonProxy->boundingRect().width(),
                        m_expandButtonProxy->boundingRect().height());

        // Adjust for size of the subcomponent rectangle
        if (m_isExpanded) {
            baseRect.adjust(0, 0, m_subcomponentRect.width(), m_subcomponentRect.height());
        }
    }

    m_baseRect = baseRect;
    // ------------------ Post Base rect ------------------------
    m_expandButtonPos = QPointF(BUTTON_INDENT, BUTTON_INDENT);

    // ------------------ Bounding rect ------------------------
    m_boundingRect = baseRect;
    // Adjust for a potential shadow
    m_boundingRect.adjust(0, 0, SHADOW_OFFSET + SHADOW_WIDTH, SHADOW_OFFSET + SHADOW_WIDTH);

    // Adjust for IO pins
    m_boundingRect.adjust(-IO_PIN_LEN, 0, IO_PIN_LEN, 0);
}

void ComponentGraphic::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*) {
    QColor color(Qt::white);
    QColor fillColor = (option->state & QStyle::State_Selected) ? color.dark(150) : color;
    if (option->state & QStyle::State_MouseOver)
        fillColor = fillColor.light(125);

    const qreal lod = option->levelOfDetailFromTransform(painter->worldTransform());
    if (lod < 0.2) {
        if (lod < 0.125) {
            painter->fillRect(m_baseRect, fillColor);
            return;
        }

        QBrush b = painter->brush();
        painter->setBrush(fillColor);
        painter->drawRect(m_baseRect);
        painter->setBrush(b);
        return;
    }

    QPen oldPen = painter->pen();
    QPen pen = oldPen;
    int width = 0;
    if (option->state & QStyle::State_Selected)
        width += 2;

    pen.setWidth(width);
    QBrush b = painter->brush();
    painter->setBrush(QBrush(fillColor.dark(option->state & QStyle::State_Sunken ? 120 : 100)));

    painter->drawRect(m_baseRect);
    painter->setBrush(b);

    // Draw shadow
    if (lod >= 0.5) {
        painter->setPen(QPen(Qt::gray, SHADOW_WIDTH));
        painter->drawLine(m_baseRect.topRight() + QPointF(SHADOW_OFFSET, 0),
                          m_baseRect.bottomRight() + QPointF(SHADOW_OFFSET, SHADOW_OFFSET));
        painter->drawLine(m_baseRect.bottomLeft() + QPointF(0, SHADOW_OFFSET),
                          m_baseRect.bottomRight() + QPointF(SHADOW_OFFSET, SHADOW_OFFSET));
        painter->setPen(QPen(Qt::black, 1));
    }

    // Draw text
    if (lod >= 0.35) {
        painter->setFont(m_font);
        painter->save();
        // painter->scale(0.1, 0.1);
        painter->drawText(m_textPos, m_displayText);
        painter->restore();
    }

    // Draw IO markers
    if (lod >= 0.5) {
        painter->setPen(QPen(Qt::black, 1));
        for (const auto& p : m_inputPositionMap) {
            painter->drawLine(p, p - QPointF(IO_PIN_LEN, 0));
        }
        for (const auto& p : m_outputPositionMap) {
            painter->drawLine(p, p + QPointF(IO_PIN_LEN, 0));
        }
    }

    // DEBUG: draw bounding rect and base rect
    painter->setPen(QPen(Qt::red, 1));
    painter->drawRect(boundingRect());
    painter->setPen(QPen(Qt::blue, 1));
    painter->drawRect(baseRect());
    painter->setPen(oldPen);
    painter->setPen(QPen(Qt::red, 5));
    painter->drawPoint(mapFromScene(pos()));
    painter->setPen(oldPen);

    if (m_hasSubcomponents) {
        // Determine whether expand button should be shown
        if (lod >= 0.35) {
            m_expandButtonProxy->show();
        } else {
            m_expandButtonProxy->hide();
        }
    }
}  // namespace ripes

QRectF ComponentGraphic::boundingRect() const {
    return m_boundingRect;
}

void ComponentGraphic::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    QGraphicsItem::mousePressEvent(event);
    update();
}

void ComponentGraphic::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    QGraphicsItem::mouseMoveEvent(event);
}

void ComponentGraphic::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    QGraphicsItem::mouseReleaseEvent(event);
    update();
}
}  // namespace ripes
