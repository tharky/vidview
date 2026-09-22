#pragma once

#include "DecoderWorker.h"

#include <QElapsedTimer>
#include <QImage>
#include <QMainWindow>
#include <QThread>
#include <QTimer>

#include <deque>

class QLabel;
class QPushButton;
class QSlider;
class QComboBox;
class QAudioSink;
class QIODevice;

class QDragEnterEvent;
class QDropEvent;
class QKeyEvent;
class QResizeEvent;

class MainWindow : public QMainWindow {
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

    void resizeEvent(
        QResizeEvent* event
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

    void chooseFile();
    void openFile(const QString& path);

    void requestDecode(
        int frameCount = DecodeBatchFrames
    );

    void nextFrame();
    void previousFrame();

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

    void renderCurrentFrame();
    void updateInfo();

    void setupAudio();
    void resetAudio();
    bool startAudio();
    void pumpAudio();

    QString formatTime(
        double seconds
    ) const;

    DecoderWorker* worker_ = nullptr;
    QThread decoderThread_;

    QLabel* videoLabel_ = nullptr;
    QLabel* infoLabel_ = nullptr;
    QLabel* timeLabel_ = nullptr;

    QPushButton* openButton_ = nullptr;
    QPushButton* previousButton_ = nullptr;
    QPushButton* playButton_ = nullptr;
    QPushButton* nextButton_ = nullptr;

    QSlider* timeline_ = nullptr;
    QComboBox* speedBox_ = nullptr;

    QTimer playbackTimer_;
    QTimer audioPumpTimer_;

    // NEW: held , / . frame stepping
    QTimer frameStepTimer_;
    int frameStepDirection_ = 0;

    QElapsedTimer playbackClock_;

    QAudioSink* audioSink_ = nullptr;
    QIODevice* audioDevice_ = nullptr;

    std::deque<CachedFrame> frameHistory_;
    std::deque<CachedFrame> pendingFrames_;

    std::deque<AudioChunk> audioQueue_;

    int historyIndex_ = -1;
    int maxHistoryFrames_ = 30;

    QImage currentFrame_;

    qint64 currentFrameNumber_ = 0;

    double currentTimestamp_ = 0.0;
    double fps_ = 30.0;
    double duration_ = 0.0;

    double playbackSpeed_ = 1.0;

    double playbackAnchorTimestamp_ = 0.0;
    qint64 audioProcessedAtStart_ = 0;

    quint64 generation_ = 0;

    bool decoderOpen_ = false;
    bool decodeInFlight_ = false;

    bool playing_ = false;
    bool hasAudio_ = false;
    bool useAudioClock_ = false;
    qsizetype queuedAudioBytes_ = 0;

    bool awaitingFirstFrame_ = false;
    bool manualStepWaiting_ = false;

    bool audioNeedsResync_ = false;

    bool resumeAfterSeek_ = false;
    bool wasPlayingBeforeScrub_ = false;

    bool timelineDragging_ = false;
};