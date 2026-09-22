#pragma once

#include "VideoDecoder.h"

#include <QImage>
#include <QMainWindow>
#include <QTimer>

#include <deque>

class QLabel;
class QPushButton;
class QSlider;
class QComboBox;

class QDragEnterEvent;
class QDropEvent;
class QKeyEvent;
class QResizeEvent;

class MainWindow : public QMainWindow {
public:
    MainWindow();

protected:
    void dragEnterEvent(
        QDragEnterEvent* event
    ) override;

    void dropEvent(
        QDropEvent* event
    ) override;

    void keyPressEvent(
        QKeyEvent* event
    ) override;

    void resizeEvent(
        QResizeEvent* event
    ) override;

private:
    struct CachedFrame {
        QImage image;
        double timestamp = 0.0;
        qint64 frameNumber = 0;
    };

    static constexpr int MaxCachedFrames = 240;
    static constexpr int TimelineResolution = 10000;

    void chooseFile();
    void openFile(const QString& path);

    void nextFrame();
    void previousFrame();

    void togglePlayback();
    void stopPlayback();

    void seekTo(double seconds);
    void seekRelative(double seconds);

    void displayCachedFrame();
    void cacheDecodedFrame(
        QImage image,
        double timestamp
    );

    void renderCurrentFrame();

    void updatePlaybackTimer();
    void updateInfo();

    QString formatTime(double seconds) const;

    VideoDecoder decoder_;

    QLabel* videoLabel_;
    QLabel* infoLabel_;
    QLabel* timeLabel_;

    QPushButton* openButton_;
    QPushButton* previousButton_;
    QPushButton* playButton_;
    QPushButton* nextButton_;

    QSlider* timeline_;
    QComboBox* speedBox_;

    QTimer playbackTimer_;

    std::deque<CachedFrame> frameCache_;

    int cacheIndex_ = -1;

    QImage currentFrame_;

    qint64 currentFrameNumber_ = 0;

    double currentTimestamp_ = 0.0;
    double playbackSpeed_ = 1.0;

    bool playing_ = false;
    bool timelineDragging_ = false;
};