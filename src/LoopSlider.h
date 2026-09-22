#pragma once

#include <QSlider>

class QPaintEvent;

class LoopSlider final : public QSlider {
public:
    explicit LoopSlider(Qt::Orientation orientation, QWidget* parent = nullptr);

    void setLoopMarkers(int startValue, int endValue);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int markerX(int value) const;

    int loopStartValue_ = -1;
    int loopEndValue_ = -1;
};
