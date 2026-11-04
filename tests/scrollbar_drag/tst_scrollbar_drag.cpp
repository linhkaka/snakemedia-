#include <ui/modules/text_scrollbar_manager/screenplay_text_scrollbar_manager.h>
#include <ui/widgets/scroll_bar/scroll_bar.h>
#include <ui/widgets/text_edit/page/page_text_edit.h>
#include <ui/widgets/text_edit/scalable_wrapper/scalable_wrapper.h>
#include <utils/helpers/ui_helper.h>

#include <QApplication>
#include <QProxyStyle>
#include <QScrollBar>
#include <QStyleFactory>
#include <QStyleOptionSlider>
#include <QVBoxLayout>
#include <QtTest>

/**
 * @brief Reproduces dragging the vertical scroll bar of the text editors.
 *
 * The editors are wrapped into ScalableWrapper and get an overlay ScrollBar from
 * UiHelper::setupScrolling, exactly like the text views of the plugins do.
 */
class ScrollBarDragTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void dragMovesEditor_data();
    void dragMovesEditor();
};

namespace {

/**
 * @brief Copy of the scroll bar related parts of ApplicationStyle (src/core/ui/application_style.cpp),
 *        which lives in the core library and can not be linked here
 */
class TestApplicationStyle : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;

    QRect subControlRect(ComplexControl _complexControl, const QStyleOptionComplex* _option,
                         SubControl _subControl, const QWidget* _widget) const override
    {
        if (_complexControl == CC_ScrollBar
            && (_subControl == SC_ScrollBarAddLine || _subControl == SC_ScrollBarSubLine)) {
            return {};
        }
        return QProxyStyle::subControlRect(_complexControl, _option, _subControl, _widget);
    }

    SubControl hitTestComplexControl(ComplexControl _control, const QStyleOptionComplex* _option,
                                     const QPoint& _pos, const QWidget* _widget) const override
    {
        const auto subControl = QProxyStyle::hitTestComplexControl(_control, _option, _pos, _widget);
        if (_control == CC_ScrollBar
            && (subControl == SC_ScrollBarAddLine || subControl == SC_ScrollBarSubLine)) {
            return SC_ScrollBarSlider;
        }
        return subControl;
    }

    int pixelMetric(PixelMetric _metric, const QStyleOption* _option,
                    const QWidget* _widget) const override
    {
        if (_metric == PM_ScrollBarExtent) {
            return 0;
        }
        return QProxyStyle::pixelMetric(_metric, _option, _widget);
    }
};

QString describe(const QWidget* _widget)
{
    if (_widget == nullptr) {
        return "null";
    }
    return QString("%1(%2) geometry=%3,%4 %5x%6 visible=%7")
        .arg(_widget->metaObject()->className(), _widget->objectName())
        .arg(_widget->x())
        .arg(_widget->y())
        .arg(_widget->width())
        .arg(_widget->height())
        .arg(_widget->isVisible());
}

ScrollBar* overlayVerticalScrollBar(ScalableWrapper* _wrapper)
{
    for (auto scrollBar : _wrapper->findChildren<ScrollBar*>(Qt::FindDirectChildrenOnly)) {
        if (scrollBar->orientation() == Qt::Vertical) {
            return scrollBar;
        }
    }
    return nullptr;
}

} // namespace

void ScrollBarDragTest::initTestCase()
{
    QApplication::setStyle(new TestApplicationStyle(QStyleFactory::create("Fusion")));
}

void ScrollBarDragTest::dragMovesEditor_data()
{
    QTest::addColumn<bool>("withScreenplayManager");

    QTest::newRow("text-editor") << false;
    QTest::newRow("screenplay-editor") << true;
}

void ScrollBarDragTest::dragMovesEditor()
{
    QFETCH(bool, withScreenplayManager);

    QWidget window;
    window.resize(900, 700);
    auto layout = new QVBoxLayout(&window);
    layout->setContentsMargins({});

    auto editor = new PageTextEdit;
    QStringList lines;
    for (int line = 0; line < 2000; ++line) {
        lines.append(QString("Line %1 of a long screenplay").arg(line));
    }
    editor->setPlainText(lines.join('\n'));

    auto wrapper = new ScalableWrapper(editor);
    layout->addWidget(wrapper);

    //
    // Same order as in the text views of the plugins
    //
    wrapper->initScrollBarsSyncing();
    if (withScreenplayManager) {
        auto manager = new ScreenplayTextScrollBarManager(wrapper);
        manager->initScrollBarsSyncing();
        UiHelper::setupScrolling(wrapper, true);
        //
        // The screenplay view switches the line type when it reads the settings
        //
        manager->setScrollBarType(ScrollBarType::Timeline);
    } else {
        UiHelper::setupScrolling(wrapper, true);
    }

    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTest::qWait(200);

    auto overlay = overlayVerticalScrollBar(wrapper);
    QVERIFY(overlay != nullptr);

    qInfo().noquote() << "wrapper children (bottom to top):";
    for (auto child : wrapper->children()) {
        if (auto widget = qobject_cast<QWidget*>(child)) {
            qInfo().noquote() << "  " << describe(widget);
        }
    }
    qInfo() << "editor vbar" << editor->verticalScrollBar()->minimum()
            << editor->verticalScrollBar()->maximum() << "visible"
            << editor->verticalScrollBar()->isVisible();
    qInfo() << "wrapper vbar" << wrapper->verticalScrollBar()->minimum()
            << wrapper->verticalScrollBar()->maximum() << "visible"
            << wrapper->verticalScrollBar()->isVisible();
    qInfo() << "overlay vbar" << overlay->minimum() << overlay->maximum() << "visible"
            << overlay->isVisible();

    QVERIFY(overlay->isVisible());
    QVERIFY(overlay->maximum() > overlay->minimum());

    //
    // Find the handle of the overlay scroll bar and check that nothing covers it
    //
    QStyleOptionSlider option;
    option.initFrom(overlay);
    option.subControls = QStyle::SC_All;
    option.orientation = overlay->orientation();
    option.minimum = overlay->minimum();
    option.maximum = overlay->maximum();
    option.sliderPosition = overlay->sliderPosition();
    option.sliderValue = overlay->value();
    option.singleStep = overlay->singleStep();
    option.pageStep = overlay->pageStep();
    option.upsideDown = overlay->invertedAppearance();
    if (overlay->orientation() == Qt::Horizontal) {
        option.state |= QStyle::State_Horizontal;
    }
    const auto handle = overlay->style()->subControlRect(QStyle::CC_ScrollBar, &option,
                                                         QStyle::SC_ScrollBarSlider, overlay);
    qInfo() << "overlay handle" << handle;
    QVERIFY(!handle.isEmpty());

    const QPoint pressPosition = overlay->mapTo(&window, handle.center());
    const auto widgetUnderMouse = window.childAt(pressPosition);
    qInfo().noquote() << "widget under the handle:" << describe(widgetUnderMouse);

    const int editorValueBefore = editor->verticalScrollBar()->value();
    auto windowHandle = window.windowHandle();
    QVERIFY(windowHandle != nullptr);
    QTest::mouseMove(windowHandle, pressPosition);
    QTest::qWait(100);
    QTest::mousePress(windowHandle, Qt::LeftButton, {}, pressPosition);
    qInfo() << "after press: sliderDown" << overlay->isSliderDown() << "mouse grabber"
            << (QWidget::mouseGrabber() ? QWidget::mouseGrabber()->metaObject()->className()
                                        : "none");
    for (int step = 1; step <= 10; ++step) {
        QTest::mouseMove(windowHandle, pressPosition + QPoint(0, step * 20));
        QTest::qWait(20);
        qInfo() << "move" << step << "overlay" << overlay->sliderPosition() << overlay->value()
                << "editor" << editor->verticalScrollBar()->value();
    }
    const QPoint releasePosition = pressPosition + QPoint(0, 200);
    QTest::mouseRelease(windowHandle, Qt::LeftButton, {}, releasePosition);
    QTest::qWait(100);

    qInfo() << "after drag: overlay" << overlay->value() << "wrapper"
            << wrapper->verticalScrollBar()->value() << "editor"
            << editor->verticalScrollBar()->value();

    QCOMPARE(widgetUnderMouse, static_cast<QWidget*>(overlay));
    QVERIFY(overlay->value() > 0);
    QVERIFY(editor->verticalScrollBar()->value() > editorValueBefore);
}

QTEST_MAIN(ScrollBarDragTest)

#include "tst_scrollbar_drag.moc"
