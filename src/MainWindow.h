#pragma once

#include "DecoderWorker.h"
#include "VideoWidget.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QImage>
#include <QMainWindow>
#include <QThread>
#include <QTimer>

#include <deque>

class QLabel;
class QPushButton;
class LoopSlider;
class QComboBox;
class QAudioSink;
class QIODevice;
class QAction;

class QDragEnterEvent;
class QDropEvent;
class QKeyEvent;

class MainWindow final
    : public QMainWindow {

public:
    MainWindow();
    ~MainWindow() override;

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

    void keyReleaseEvent(
        QKeyEvent* event
    ) override;

private:
    struct CachedFrame {
        QImage image;
        double timestamp = 0.0;
        qint64 frameNumber = 0;
    };

    struct AudioChunk {
        QByteArray data;
        qsizetype offset = 0;
    };

    static constexpr int DecodeBatchFrames = 3;
    static constexpr int BufferRefillThreshold = 1;
    static constexpr int AbsoluteMaxHistoryFrames = 120;
    static constexpr int TimelineResolution = 10000;

    void createMenus();
    void showKeyboardShortcuts();

    void chooseFile();

    void openFile(
        const QString& path
    );

    void requestDecode(
        int frameCount =
            DecodeBatchFrames
    );

    void nextFrame();
    void previousFrame();

    void startForwardScrub();
    void startBackwardScrub();
    void stopFrameScrub();

    void consumePendingFrame(
        bool render = true
    );

    void startPlayback();
    void pausePlayback();
    void hardStopForSeek();
    void togglePlayback();

    void playbackTick();

    void seekTo(
        double seconds,
        bool resumePlayback = false
    );

    void seekRelative(
        double seconds
    );

    void displayCachedFrame();

    void updateInfo();
    void updateLoopUi();

    void setupAudio();
    void resetAudio();

    bool startAudio();

    void pumpAudio();

    void setLoopStart();
    void setLoopEnd();
    void clearLoop();

    void saveCurrentFrame();

    void changeSpeed(
        int direction
    );

    QString formatTime(
        double seconds
    ) const;

    DecoderWorker* worker_ = nullptr;
    QThread decoderThread_;

    VideoWidget* videoWidget_ = nullptr;

    QLabel* infoLabel_ = nullptr;
    QLabel* timeLabel_ = nullptr;

    QPushButton* previousButton_ = nullptr;
    QPushButton* playButton_ = nullptr;
    QPushButton* nextButton_ = nullptr;

    QPushButton* loopStartButton_ = nullptr;
    QPushButton* loopEndButton_ = nullptr;
    QPushButton* clearLoopButton_ = nullptr;

    LoopSlider* timeline_ = nullptr;
    QComboBox* speedBox_ = nullptr;

    /*
        QAction shortcuts work at the window
        level even when a child widget has focus.
    */
    QAction* openAction_ = nullptr;
    QAction* saveFrameAction_ = nullptr;
    QAction* exitAction_ = nullptr;

    QAction* playPauseAction_ = nullptr;
    QAction* previousFrameAction_ = nullptr;
    QAction* nextFrameAction_ = nullptr;

    QAction* seekBackAction_ = nullptr;
    QAction* seekForwardAction_ = nullptr;

    QAction* seekBackFineAction_ = nullptr;
    QAction* seekForwardFineAction_ = nullptr;

    QAction* speedDownAction_ = nullptr;
    QAction* speedUpAction_ = nullptr;

    QAction* loopStartAction_ = nullptr;
    QAction* loopEndAction_ = nullptr;
    QAction* clearLoopAction_ = nullptr;

    QAction* fullscreenAction_ = nullptr;
    QAction* shortcutsAction_ = nullptr;

    QTimer playbackTimer_;
    QTimer audioPumpTimer_;
    QTimer frameStepTimer_;

    QElapsedTimer playbackClock_;

    QAudioSink* audioSink_ = nullptr;
    QIODevice* audioDevice_ = nullptr;

    std::deque<CachedFrame>
        frameHistory_;

    std::deque<CachedFrame>
        pendingFrames_;

    std::deque<AudioChunk>
        audioQueue_;

    int historyIndex_ = -1;

    int maxHistoryFrames_ = 24;

    int frameStepDirection_ = 0;

    int sourceWidth_ = 0;
    int sourceHeight_ = 0;

    QImage currentFrame_;

    qint64 currentFrameNumber_ = 0;

    qsizetype queuedAudioBytes_ = 0;

    double currentTimestamp_ = 0.0;
    double fps_ = 30.0;
    double duration_ = 0.0;

    double playbackSpeed_ = 1.0;

    double playbackAnchorTimestamp_ =
        0.0;

    double loopStart_ = -1.0;
    double loopEnd_ = -1.0;

    qint64 audioProcessedAtStart_ = 0;

    quint64 generation_ = 0;

    bool decoderOpen_ = false;
    bool decodeInFlight_ = false;

    bool playing_ = false;

    bool hasAudio_ = false;
    bool useAudioClock_ = false;

    bool awaitingFirstFrame_ = false;
    bool manualStepWaiting_ = false;

    bool audioNeedsResync_ = false;

    bool resumeAfterSeek_ = false;

    bool timelineDragging_ = false;
};