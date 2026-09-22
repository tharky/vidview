#include "LoopSlider.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyleOptionSlider>

LoopSlider::LoopSlider(
    Qt::Orientation orientation,
    QWidget* parent
)
    : QSlider(
        orientation,
        parent
      ) {
}

void LoopSlider::setLoopMarkers(
    int startValue,
    int endValue
) {
    loopStartValue_ =
        startValue;

    loopEndValue_ =
        endValue;

    update();
}

int LoopSlider::markerX(
    int value
) const {
    QStyleOptionSlider option;

    initStyleOption(
        &option
    );

    QRect groove =
        style()->subControlRect(
            QStyle::CC_Slider,
            &option,
            QStyle::SC_SliderGroove,
            this
        );

    QRect handle =
        style()->subControlRect(
            QStyle::CC_Slider,
            &option,
            QStyle::SC_SliderHandle,
            this
        );

    int sliderMin =
        groove.x();

    int sliderMax =
        groove.right() -
        handle.width() +
        1;

    int position =
        QStyle::sliderPositionFromValue(
            minimum(),
            maximum(),
            value,
            sliderMax - sliderMin,
            option.upsideDown
        );

    return
        sliderMin +
        position +
        handle.width() / 2;
}

void LoopSlider::paintEvent(
    QPaintEvent* event
) {
    QSlider::paintEvent(
        event
    );

    QPainter painter(
        this
    );

    painter.setRenderHint(
        QPainter::Antialiasing
    );

    QStyleOptionSlider option;

    initStyleOption(
        &option
    );

    QRect groove =
        style()->subControlRect(
            QStyle::CC_Slider,
            &option,
            QStyle::SC_SliderGroove,
            this
        );

    auto drawMarker =
        [&](
            int value,
            const QColor& color
        ) {
            if (
                value < minimum() ||
                value > maximum()
            ) {
                return;
            }

            int x =
                markerX(
                    value
                );

            /*
                Vertical marker through the
                timeline groove.
            */
            QPen pen(
                color,
                2.0
            );

            painter.setPen(
                pen
            );

            painter.drawLine(
                x,
                groove.top() - 4,
                x,
                groove.bottom() + 4
            );

            /*
                Small downward triangle above
                the marker.
            */
            painter.setPen(
                Qt::NoPen
            );

            painter.setBrush(
                color
            );

            QPainterPath triangle;

            triangle.moveTo(
                x,
                groove.top() - 5
            );

            triangle.lineTo(
                x - 5,
                groove.top() - 12
            );

            triangle.lineTo(
                x + 5,
                groove.top() - 12
            );

            triangle.closeSubpath();

            painter.drawPath(
                triangle
            );
        };

    /*
        Start = teal
        End   = amber
    */
    if (
        loopStartValue_ >= 0
    ) {
        drawMarker(
            loopStartValue_,
            QColor("#2dd4bf")
        );
    }

    if (
        loopEndValue_ >= 0
    ) {
        drawMarker(
            loopEndValue_,
            QColor("#f59e0b")
        );
    }
}