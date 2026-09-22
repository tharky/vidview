#include "LoopSlider.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyleOptionSlider>

LoopSlider::LoopSlider(Qt::Orientation orientation, QWidget* parent)
    : QSlider(orientation, parent) {}

void LoopSlider::setLoopMarkers(int startValue, int endValue) {
    loopStartValue_ = startValue;
    loopEndValue_ = endValue;
    update();
}

int LoopSlider::markerX(int value) const {
    QStyleOptionSlider option;
    initStyleOption(&option);

    const QRect groove = style()->subControlRect(
        QStyle::CC_Slider,
        &option,
        QStyle::SC_SliderGroove,
        this
    );

    const QRect handle = style()->subControlRect(
        QStyle::CC_Slider,
        &option,
        QStyle::SC_SliderHandle,
        this
    );

    const int sliderMin = groove.x();
    const int sliderMax = groove.right() - handle.width() + 1;
    const int position = QStyle::sliderPositionFromValue(
        minimum(),
        maximum(),
        value,
        sliderMax - sliderMin,
        option.upsideDown
    );

    return sliderMin + position + handle.width() / 2;
}

void LoopSlider::paintEvent(QPaintEvent* event) {
    QSlider::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QStyleOptionSlider option;
    initStyleOption(&option);

    const QRect groove = style()->subControlRect(
        QStyle::CC_Slider,
        &option,
        QStyle::SC_SliderGroove,
        this
    );

    const auto drawMarker = [&](int value, const QColor& color) {
        if (value < minimum() || value > maximum()) return;

        const int x = markerX(value);

        painter.setPen(QPen(color, 2.0));
        painter.drawLine(x, groove.top() - 4, x, groove.bottom() + 4);

        painter.setPen(Qt::NoPen);
        painter.setBrush(color);

        QPainterPath triangle;
        triangle.moveTo(x, groove.top() - 5);
        triangle.lineTo(x - 5, groove.top() - 12);
        triangle.lineTo(x + 5, groove.top() - 12);
        triangle.closeSubpath();

        painter.drawPath(triangle);
    };

    if (loopStartValue_ >= 0) {
        drawMarker(loopStartValue_, QColor("#2dd4bf"));
    }

    if (loopEndValue_ >= 0) {
        drawMarker(loopEndValue_, QColor("#f59e0b"));
    }
}
