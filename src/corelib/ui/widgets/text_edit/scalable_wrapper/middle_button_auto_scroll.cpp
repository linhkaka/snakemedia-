#include "middle_button_auto_scroll.h"

#include <QApplication>
#include <QCursor>
#include <QEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QWidget>

namespace {

const int kTickIntervalMs = 16;

/**
 * @brief Cursor offset from the press point that does not scroll yet, so a shaky hand does not
 *        move the page
 */
const qreal kDeadZone = 8.0;

/**
 * @brief Scrolling speed in pixels per second, slow near the press point and fast far from it
 */
qreal speedForDistance(qreal _distance)
{
    return 4.0 * _distance + _distance * _distance / 50.0;
}

} // namespace


MiddleButtonAutoScroll::MiddleButtonAutoScroll(QWidget* _viewport, QScrollBar* _scrollBar,
                                               QObject* _parent)
    : QObject(_parent)
    , m_viewport(_viewport)
    , m_scrollBar(_scrollBar)
{
    m_timer.setInterval(kTickIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, [this] { scroll(); });
    m_viewport->installEventFilter(this);
}

MiddleButtonAutoScroll::~MiddleButtonAutoScroll()
{
    stop();
}

bool MiddleButtonAutoScroll::eventFilter(QObject* _watched, QEvent* _event)
{
    switch (_event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick: {
        const auto mouseEvent = static_cast<QMouseEvent*>(_event);
        if (mouseEvent->button() == Qt::MiddleButton) {
            start(mouseEvent->globalPosition().y());
            return true;
        }
        break;
    }

    case QEvent::MouseMove: {
        if (m_isActive) {
            m_currentY = static_cast<QMouseEvent*>(_event)->globalPosition().y();
            return true;
        }
        break;
    }

    case QEvent::MouseButtonRelease: {
        if (m_isActive && static_cast<QMouseEvent*>(_event)->button() == Qt::MiddleButton) {
            stop();
            return true;
        }
        break;
    }

    default: {
        break;
    }
    }

    return QObject::eventFilter(_watched, _event);
}

void MiddleButtonAutoScroll::start(qreal _y)
{
    if (m_isActive) {
        return;
    }

    m_isActive = true;
    m_originY = _y;
    m_currentY = _y;
    m_pendingPixels = 0;
    QApplication::setOverrideCursor(Qt::SizeVerCursor);
    m_elapsed.start();
    m_timer.start();
}

void MiddleButtonAutoScroll::stop()
{
    if (!m_isActive) {
        return;
    }

    m_isActive = false;
    m_timer.stop();
    QApplication::restoreOverrideCursor();
}

void MiddleButtonAutoScroll::scroll()
{
    //
    // The release event may never come here, for example when the window loses focus
    //
    if (!QApplication::mouseButtons().testFlag(Qt::MiddleButton) || !m_viewport->isVisible()) {
        stop();
        return;
    }

    const qreal elapsedSeconds = m_elapsed.restart() / 1000.0;
    const qreal offset = m_currentY - m_originY;
    const qreal distance = qAbs(offset) - kDeadZone;
    if (distance <= 0) {
        m_pendingPixels = 0;
        return;
    }

    //
    // Keep the fraction of a pixel for the next tick, otherwise slow scrolling never starts
    //
    const qreal direction = offset > 0 ? 1.0 : -1.0;
    m_pendingPixels += direction * speedForDistance(distance) * elapsedSeconds;
    const int wholePixels = static_cast<int>(m_pendingPixels);
    if (wholePixels != 0) {
        m_pendingPixels -= wholePixels;
        m_scrollBar->setValue(m_scrollBar->value() + wholePixels);
    }
}
