#pragma once

#include "VideoDecoder.h"

#include <QImage>
#include <QMainWindow>
#include <QTimer>

class QLabel;
class QPushButton;
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
    void chooseFile();
    void openFile(const QString& path);

    void nextFrame();
    void togglePlayback();
    void stopPlayback();

    void renderCurrentFrame();

    VideoDecoder decoder_;

    QLabel* videoLabel_;
    QLabel* infoLabel_;

    QPushButton* openButton_;
    QPushButton* playButton_;
    QPushButton* nextButton_;

    QTimer playbackTimer_;

    QImage currentFrame_;

    QString currentPath_;

    qint64 frameNumber_ = 0;
    double currentTimestamp_ = 0.0;

    bool playing_ = false;
};