#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

class QScrollBar;
class QWidget;


/**
 * @brief Browser like auto scroll: while the middle mouse button is held, the page runs in the
 *        direction of the cursor offset from the press point, the further the faster
 */
class MiddleButtonAutoScroll : public QObject
{
public:
    /**
     * @param _viewport the widget that receives mouse events
     * @param _scrollBar the scroll bar to drive
     */
    MiddleButtonAutoScroll(QWidget* _viewport, QScrollBar* _scrollBar, QObject* _parent);
    ~MiddleButtonAutoScroll() override;

protected:
    bool eventFilter(QObject* _watched, QEvent* _event) override;

private:
    void start(qreal _y);
    void stop();
    void scroll();

    QWidget* m_viewport = nullptr;
    QScrollBar* m_scrollBar = nullptr;
    QTimer m_timer;
    QElapsedTimer m_elapsed;
    bool m_isActive = false;
    qreal m_originY = 0;
    qreal m_currentY = 0;
    qreal m_pendingPixels = 0;
};
