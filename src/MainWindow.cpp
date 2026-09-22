#include "MainWindow.h"
#include "LoopSlider.h"

#include <QAction>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIODevice>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMediaDevices>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <QStyle>

#include <algorithm>
#include <cmath>
#include <limits>

MainWindow::MainWindow()
    : videoWidget_(
        new VideoWidget
      ),
      infoLabel_(
        new QLabel
      ),
      timeLabel_(
        new QLabel
      ),
      previousButton_(
        new QPushButton("< Previous")
      ),
      playButton_(
        new QPushButton("Play")
      ),
      nextButton_(
        new QPushButton("Next >")
      ),
      loopStartButton_(
        new QPushButton("Loop Start")
      ),
      loopEndButton_(
        new QPushButton("Loop End")
      ),
      clearLoopButton_(
        new QPushButton("Clear Loop")
      ),
      timeline_(
        new LoopSlider(
            Qt::Horizontal
        )
    ),
      speedBox_(
        new QComboBox
      ) {

    setWindowTitle(
        "VidView - Video Inspector"
    );

    resize(
        1200,
        800
    );

    setAcceptDrops(true);

    createMenus();

    auto* central =
        new QWidget;

    auto* mainLayout =
        new QVBoxLayout(
            central
        );

    auto* timelineRow =
        new QHBoxLayout;

    auto* controls =
        new QHBoxLayout;

    timeline_->setRange(
        0,
        TimelineResolution
    );

    infoLabel_->setText(
        "Drop a video here or use File > Open"
    );

    timeLabel_->setText(
        "00:00.000 / 00:00.000"
    );

    speedBox_->addItem(
        "0.25x",
        0.25
    );

    speedBox_->addItem(
        "0.5x",
        0.5
    );

    speedBox_->addItem(
        "1x",
        1.0
    );

    speedBox_->addItem(
        "1.5x",
        1.5
    );

    speedBox_->addItem(
        "2x",
        2.0
    );

    speedBox_->addItem(
        "4x",
        4.0
    );

    speedBox_->setCurrentIndex(2);

    previousButton_->setFocusPolicy(
        Qt::NoFocus
    );

    playButton_->setFocusPolicy(
        Qt::NoFocus
    );

    nextButton_->setFocusPolicy(
        Qt::NoFocus
    );

    loopStartButton_->setFocusPolicy(
        Qt::NoFocus
    );

    loopEndButton_->setFocusPolicy(
        Qt::NoFocus
    );

    clearLoopButton_->setFocusPolicy(
        Qt::NoFocus
    );

    speedBox_->setFocusPolicy(
        Qt::NoFocus
    );

    timelineRow->addWidget(
        timeline_,
        1
    );

    timelineRow->addWidget(
        timeLabel_
    );

    controls->addWidget(
        loopStartButton_
    );

    controls->addWidget(
        loopEndButton_
    );

    controls->addWidget(
        clearLoopButton_
    );

    controls->addStretch();

    controls->addWidget(
        previousButton_
    );

    controls->addWidget(
        playButton_
    );

    controls->addWidget(
        nextButton_
    );

    controls->addWidget(
        speedBox_
    );

    mainLayout->addWidget(
        videoWidget_,
        1
    );

    mainLayout->addWidget(
        infoLabel_
    );

    mainLayout->addLayout(
        timelineRow
    );

    mainLayout->addLayout(
        controls
    );

    setCentralWidget(
        central
    );

    setStyleSheet(R"(
        QMainWindow {
            background: #16181d;
        }

        QWidget {
            color: #e6e6e6;
            font-size: 14px;
        }

        QMenuBar {
            background: #1d2026;
            color: #e6e6e6;
        }

        QMenuBar::item:selected {
            background: #353b46;
        }

        QMenu {
            background: #242830;
            color: #e6e6e6;
            border: 1px solid #444b57;
        }

        QMenu::item:selected {
            background: #3a414d;
        }

        QPushButton,
        QComboBox {
            background: #292d35;
            border: 1px solid #444b57;
            padding: 7px 11px;
            border-radius: 5px;
        }

        QPushButton:hover,
        QComboBox:hover {
            background: #353b46;
        }

        QPushButton[loopState="start"] {
            background: #123f3b;
            border: 1px solid #2dd4bf;
            color: #ccfbf1;
            font-weight: 600;
        }

        QPushButton[loopState="start"]:hover {
            background: #18554f;
        }

        QPushButton[loopState="end"] {
            background: #4a3210;
            border: 1px solid #f59e0b;
            color: #fef3c7;
            font-weight: 600;
        }

        QPushButton[loopState="end"]:hover {
            background: #624514;
        }

        QLabel {
            color: #dddddd;
        }

        QSlider::groove:horizontal {
            height: 5px;
            background: #353a43;
        }

        QSlider::handle:horizontal {
            width: 14px;
            margin: -5px 0;
            border-radius: 7px;
            background: #e2e2e2;
        }
    )");

    playbackTimer_.setInterval(
        16
    );

    playbackTimer_.setTimerType(
        Qt::PreciseTimer
    );

    audioPumpTimer_.setInterval(
        10
    );

    audioPumpTimer_.setTimerType(
        Qt::PreciseTimer
    );

    frameStepTimer_.setInterval(
        60
    );

    frameStepTimer_.setTimerType(
        Qt::PreciseTimer
    );

    worker_ =
        new DecoderWorker;

    worker_->moveToThread(
        &decoderThread_
    );

    connect(
        &decoderThread_,
        &QThread::finished,
        worker_,
        &QObject::deleteLater
    );

    connect(
        worker_,
        &DecoderWorker::opened,
        this,
        [this](
            bool success,
            QString error,
            double fps,
            double duration,
            int width,
            int height,
            bool hasAudio,
            quint64 generation
        ) {
            if (
                generation !=
                generation_
            ) {
                return;
            }

            decodeInFlight_ =
                false;

            if (!success) {
                decoderOpen_ =
                    false;

                QMessageBox::critical(
                    this,
                    "Could not open video",
                    error
                );

                return;
            }

            decoderOpen_ =
                true;

            sourceWidth_ =
                width;

            sourceHeight_ =
                height;

            fps_ =
                std::max(
                    fps,
                    1.0
                );

            duration_ =
                std::max(
                    duration,
                    0.0
                );

            constexpr qint64
                TargetHistoryBytes =
                    160LL *
                    1024LL *
                    1024LL;

            constexpr int
                MaxPreviewWidth =
                    1920;

            constexpr int
                MaxPreviewHeight =
                    1080;

            double scale =
                std::min({
                    1.0,
                    static_cast<double>(
                        MaxPreviewWidth
                    ) /
                    width,
                    static_cast<double>(
                        MaxPreviewHeight
                    ) /
                    height
                });

            int previewWidth =
                std::max(
                    1,
                    static_cast<int>(
                        std::lround(
                            width *
                            scale
                        )
                    )
                );

            int previewHeight =
                std::max(
                    1,
                    static_cast<int>(
                        std::lround(
                            height *
                            scale
                        )
                    )
                );

            qint64 frameBytes =
                static_cast<qint64>(
                    previewWidth
                ) *
                previewHeight *
                4LL;

            if (
                frameBytes > 0
            ) {
                maxHistoryFrames_ =
                    std::clamp(
                        static_cast<int>(
                            TargetHistoryBytes /
                            frameBytes
                        ),
                        6,
                        AbsoluteMaxHistoryFrames
                    );
            }

            hasAudio_ =
                hasAudio;

            setupAudio();

            awaitingFirstFrame_ =
                true;

            requestDecode();
        }
    );

    connect(
        worker_,
        &DecoderWorker::videoFrameReady,
        this,
        [this](
            QImage image,
            double timestamp,
            quint64 generation
        ) {
            if (
                generation !=
                generation_
            ) {
                return;
            }

            qint64 estimatedFrame =
                std::max<qint64>(
                    1,
                    static_cast<qint64>(
                        std::llround(
                            timestamp *
                            fps_
                        )
                    ) +
                    1
                );

            pendingFrames_.push_back({
                std::move(image),
                timestamp,
                estimatedFrame
            });

            if (
                awaitingFirstFrame_
            ) {
                awaitingFirstFrame_ =
                    false;

                consumePendingFrame();
            } else if (
                manualStepWaiting_
            ) {
                manualStepWaiting_ =
                    false;

                consumePendingFrame();
            }
        }
    );

    connect(
        worker_,
        &DecoderWorker::audioChunkReady,
        this,
        [this](
            QByteArray pcm,
            double,
            quint64 generation
        ) {
            if (
                generation !=
                    generation_ ||
                playbackSpeed_ !=
                    1.0
            ) {
                return;
            }

            constexpr qsizetype
                MaxAudioQueueBytes =
                    48000 *
                    2 *
                    2 *
                    2;

            if (
                queuedAudioBytes_ +
                    pcm.size() <=
                MaxAudioQueueBytes
            ) {
                queuedAudioBytes_ +=
                    pcm.size();

                audioQueue_.push_back({
                    std::move(pcm),
                    0
                });
            }

            if (playing_) {
                pumpAudio();
            }
        }
    );

    connect(
        worker_,
        &DecoderWorker::seekFinished,
        this,
        [this](
            double,
            quint64 generation
        ) {
            if (
                generation !=
                generation_
            ) {
                return;
            }

            requestDecode();
        }
    );

    connect(
        worker_,
        &DecoderWorker::batchFinished,
        this,
        [this](
            quint64 generation
        ) {
            if (
                generation !=
                generation_
            ) {
                return;
            }

            decodeInFlight_ =
                false;

            if (
                resumeAfterSeek_ &&
                !awaitingFirstFrame_
            ) {
                resumeAfterSeek_ =
                    false;

                startPlayback();

                return;
            }

            if (
                playing_ &&
                pendingFrames_.size() <
                    BufferRefillThreshold
            ) {
                requestDecode();
            }
        }
    );

    decoderThread_.start();

    connect(
        previousButton_,
        &QPushButton::clicked,
        this,
        [this]() {
            previousFrame();
        }
    );

    connect(
        playButton_,
        &QPushButton::clicked,
        this,
        [this]() {
            togglePlayback();
        }
    );

    connect(
        nextButton_,
        &QPushButton::clicked,
        this,
        [this]() {
            nextFrame();
        }
    );

    connect(
        loopStartButton_,
        &QPushButton::clicked,
        this,
        [this]() {
            setLoopStart();
        }
    );

    connect(
        loopEndButton_,
        &QPushButton::clicked,
        this,
        [this]() {
            setLoopEnd();
        }
    );

    connect(
        clearLoopButton_,
        &QPushButton::clicked,
        this,
        [this]() {
            clearLoop();
        }
    );

    connect(
        &playbackTimer_,
        &QTimer::timeout,
        this,
        [this]() {
            playbackTick();
        }
    );

    connect(
        &audioPumpTimer_,
        &QTimer::timeout,
        this,
        [this]() {
            pumpAudio();
        }
    );

    connect(
        &frameStepTimer_,
        &QTimer::timeout,
        this,
        [this]() {
            if (
                frameStepDirection_ > 0
            ) {
                nextFrame();
            } else if (
                frameStepDirection_ < 0
            ) {
                previousFrame();
            }
        }
    );

    connect(
        timeline_,
        &QSlider::sliderPressed,
        this,
        [this]() {
            timelineDragging_ =
                true;

            hardStopForSeek();
        }
    );

    connect(
        timeline_,
        &QSlider::sliderReleased,
        this,
        [this]() {
            timelineDragging_ =
                false;

            if (!decoderOpen_) {
                return;
            }

            double fraction =
                static_cast<double>(
                    timeline_->value()
                ) /
                TimelineResolution;

            seekTo(
                fraction *
                    duration_,
                false
            );
        }
    );

    connect(
        speedBox_,
        &QComboBox::currentIndexChanged,
        this,
        [this](int index) {
            bool wasPlaying =
                playing_;

            pausePlayback();

            playbackSpeed_ =
                speedBox_
                    ->itemData(
                        index
                    )
                    .toDouble();

            audioNeedsResync_ =
                true;

            if (wasPlaying) {
                if (
                    playbackSpeed_ ==
                        1.0 &&
                    hasAudio_
                ) {
                    seekTo(
                        currentTimestamp_,
                        true
                    );
                } else {
                    startPlayback();
                }
            }

            updateInfo();
        }
    );
}

void MainWindow::createMenus() {
    /*
        FILE
    */
    QMenu* fileMenu =
        menuBar()->addMenu(
            "&File"
        );

    openAction_ =
        new QAction(
            "&Open...",
            this
        );

    openAction_->setShortcut(
        QKeySequence::Open
    );

    connect(
        openAction_,
        &QAction::triggered,
        this,
        [this]() {
            chooseFile();
        }
    );

    fileMenu->addAction(
        openAction_
    );

    saveFrameAction_ =
        new QAction(
            "&Save Current Frame...",
            this
        );

    saveFrameAction_->setShortcut(
        QKeySequence(
            "Ctrl+S"
        )
    );

    connect(
        saveFrameAction_,
        &QAction::triggered,
        this,
        [this]() {
            saveCurrentFrame();
        }
    );

    fileMenu->addAction(
        saveFrameAction_
    );

    fileMenu->addSeparator();

    exitAction_ =
        new QAction(
            "E&xit",
            this
        );

    exitAction_->setShortcut(
        QKeySequence::Quit
    );

    connect(
        exitAction_,
        &QAction::triggered,
        this,
        &QWidget::close
    );

    fileMenu->addAction(
        exitAction_
    );

    /*
        PLAYBACK
    */
    QMenu* playbackMenu =
        menuBar()->addMenu(
            "&Playback"
        );

    playPauseAction_ =
        new QAction(
            "Play / Pause",
            this
        );

    playPauseAction_->setShortcut(
        QKeySequence(
            Qt::Key_Space
        )
    );

    connect(
        playPauseAction_,
        &QAction::triggered,
        this,
        [this]() {
            togglePlayback();
        }
    );

    playbackMenu->addAction(
        playPauseAction_
    );

    playbackMenu->addSeparator();

    previousFrameAction_ =
        new QAction(
            "Previous Frame",
            this
        );

    previousFrameAction_->setShortcut(
        QKeySequence(
            Qt::Key_K
        )
    );

    connect(
        previousFrameAction_,
        &QAction::triggered,
        this,
        [this]() {
            previousFrame();
        }
    );

    playbackMenu->addAction(
        previousFrameAction_
    );

    nextFrameAction_ =
        new QAction(
            "Next Frame",
            this
        );

    nextFrameAction_->setShortcut(
        QKeySequence(
            Qt::Key_L
        )
    );

    connect(
        nextFrameAction_,
        &QAction::triggered,
        this,
        [this]() {
            nextFrame();
        }
    );

    playbackMenu->addAction(
        nextFrameAction_
    );

    playbackMenu->addSeparator();

    seekBackAction_ =
        new QAction(
            "Seek Back 5 Seconds",
            this
        );

    seekBackAction_->setShortcut(
        QKeySequence(
            Qt::Key_Left
        )
    );

    connect(
        seekBackAction_,
        &QAction::triggered,
        this,
        [this]() {
            seekRelative(-5.0);
        }
    );

    playbackMenu->addAction(
        seekBackAction_
    );

    seekForwardAction_ =
        new QAction(
            "Seek Forward 5 Seconds",
            this
        );

    seekForwardAction_->setShortcut(
        QKeySequence(
            Qt::Key_Right
        )
    );

    connect(
        seekForwardAction_,
        &QAction::triggered,
        this,
        [this]() {
            seekRelative(5.0);
        }
    );

    playbackMenu->addAction(
        seekForwardAction_
    );

    seekBackFineAction_ =
        new QAction(
            "Seek Back 1 Second",
            this
        );

    seekBackFineAction_->setShortcut(
        QKeySequence(
            "Shift+Left"
        )
    );

    connect(
        seekBackFineAction_,
        &QAction::triggered,
        this,
        [this]() {
            seekRelative(-1.0);
        }
    );

    playbackMenu->addAction(
        seekBackFineAction_
    );

    seekForwardFineAction_ =
        new QAction(
            "Seek Forward 1 Second",
            this
        );

    seekForwardFineAction_->setShortcut(
        QKeySequence(
            "Shift+Right"
        )
    );

    connect(
        seekForwardFineAction_,
        &QAction::triggered,
        this,
        [this]() {
            seekRelative(1.0);
        }
    );

    playbackMenu->addAction(
        seekForwardFineAction_
    );

    playbackMenu->addSeparator();

    speedDownAction_ =
        new QAction(
            "Decrease Playback Speed",
            this
        );

    speedDownAction_->setShortcut(
        QKeySequence("-")
    );

    connect(
        speedDownAction_,
        &QAction::triggered,
        this,
        [this]() {
            changeSpeed(-1);
        }
    );

    playbackMenu->addAction(
        speedDownAction_
    );

    speedUpAction_ =
        new QAction(
            "Increase Playback Speed",
            this
        );

    speedUpAction_->setShortcut(
        QKeySequence("=")
    );

    connect(
        speedUpAction_,
        &QAction::triggered,
        this,
        [this]() {
            changeSpeed(1);
        }
    );

    playbackMenu->addAction(
        speedUpAction_
    );

    /*
        LOOP
    */
    QMenu* loopMenu =
        menuBar()->addMenu(
            "&Loop"
        );

    loopStartAction_ =
        new QAction(
            "Set Loop Start",
            this
        );

    loopStartAction_->setShortcut(
        QKeySequence("[")
    );

    connect(
        loopStartAction_,
        &QAction::triggered,
        this,
        [this]() {
            setLoopStart();
        }
    );

    loopMenu->addAction(
        loopStartAction_
    );

    loopEndAction_ =
        new QAction(
            "Set Loop End",
            this
        );

    loopEndAction_->setShortcut(
        QKeySequence("]")
    );

    connect(
        loopEndAction_,
        &QAction::triggered,
        this,
        [this]() {
            setLoopEnd();
        }
    );

    loopMenu->addAction(
        loopEndAction_
    );

    clearLoopAction_ =
        new QAction(
            "Clear Loop",
            this
        );

    clearLoopAction_->setShortcut(
        QKeySequence("\\")
    );

    connect(
        clearLoopAction_,
        &QAction::triggered,
        this,
        [this]() {
            clearLoop();
        }
    );

    loopMenu->addAction(
        clearLoopAction_
    );

    /*
        VIEW
    */
    QMenu* viewMenu =
        menuBar()->addMenu(
            "&View"
        );

    fullscreenAction_ =
        new QAction(
            "Toggle Fullscreen",
            this
        );

    fullscreenAction_->setShortcut(
        QKeySequence(
            Qt::Key_F
        )
    );

    connect(
        fullscreenAction_,
        &QAction::triggered,
        this,
        [this]() {
            if (isFullScreen()) {
                showNormal();
            } else {
                showFullScreen();
            }
        }
    );

    viewMenu->addAction(
        fullscreenAction_
    );

    /*
        HELP
    */
    QMenu* helpMenu =
        menuBar()->addMenu(
            "&Help"
        );

    shortcutsAction_ =
        new QAction(
            "Keyboard Shortcuts",
            this
        );

    shortcutsAction_->setShortcut(
        QKeySequence("F1")
    );

    connect(
        shortcutsAction_,
        &QAction::triggered,
        this,
        [this]() {
            showKeyboardShortcuts();
        }
    );

    helpMenu->addAction(
        shortcutsAction_
    );
}

void MainWindow::showKeyboardShortcuts() {
    QMessageBox::information(
        this,
        "VidView Keyboard Shortcuts",
        R"(
<b>Playback</b><br>
Space — Play / pause<br>
K — Previous frame<br>
L — Next frame<br>
Hold , — Scrub backward through frames<br>
Hold . — Scrub forward through frames<br>
Left — Seek back 5 seconds<br>
Right — Seek forward 5 seconds<br>
Shift + Left — Seek back 1 second<br>
Shift + Right — Seek forward 1 second<br>
- — Decrease playback speed<br>
= — Increase playback speed<br>
<br>
<b>Loop</b><br>
[ — Set loop start<br>
] — Set loop end<br>
\ — Clear loop<br>
<br>
<b>File / View</b><br>
Ctrl + O — Open video<br>
Ctrl + S — Save current frame<br>
F — Fullscreen<br>
Esc — Exit fullscreen<br>
F1 — Show this window
)"
    );
}

MainWindow::~MainWindow() {
    playbackTimer_.stop();
    audioPumpTimer_.stop();
    frameStepTimer_.stop();

    if (audioSink_) {
        audioSink_->reset();
    }

    if (
        worker_ &&
        decoderThread_.isRunning()
    ) {
        QMetaObject::invokeMethod(
            worker_,
            [
                worker = worker_
            ]() {
                worker->closeFile();
            },
            Qt::BlockingQueuedConnection
        );
    }

    decoderThread_.quit();
    decoderThread_.wait();
}

void MainWindow::chooseFile() {
    QString path =
        QFileDialog::
            getOpenFileName(
                this,
                "Open Video",
                QString(),
                "Video Files (*.mp4 *.mov *.mkv *.avi *.webm *.m4v);;All Files (*.*)"
            );

    if (!path.isEmpty()) {
        openFile(path);
    }
}

void MainWindow::openFile(
    const QString& path
) {
    hardStopForSeek();

    ++generation_;

    worker_->setDesiredGeneration(
        generation_
    );

    decoderOpen_ =
        false;

    decodeInFlight_ =
        false;

    frameHistory_.clear();
    pendingFrames_.clear();

    historyIndex_ = -1;

    currentFrame_ =
        QImage();

    currentTimestamp_ =
        0.0;

    currentFrameNumber_ =
        0;

    duration_ =
        0.0;

    sourceWidth_ = 0;
    sourceHeight_ = 0;

    loopStart_ = -1.0;
    loopEnd_ = -1.0;
    updateLoopUi();

    resetAudio();

    timeline_->setValue(0);

    videoWidget_->clearFrame();

    setWindowTitle(
        QString(
            "VidView - %1"
        ).arg(
            QFileInfo(path)
                .fileName()
        )
    );

    quint64 generation =
        generation_;

    QMetaObject::invokeMethod(
        worker_,
        [
            worker = worker_,
            path,
            generation
        ]() {
            worker->openFile(
                path,
                generation
            );
        },
        Qt::QueuedConnection
    );
}

void MainWindow::requestDecode(
    int frameCount
) {
    if (
        !decoderOpen_ ||
        decodeInFlight_
    ) {
        return;
    }

    decodeInFlight_ =
        true;

    quint64 generation =
        generation_;

    QMetaObject::invokeMethod(
        worker_,
        [
            worker = worker_,
            frameCount,
            generation
        ]() {
            worker->decodeBatch(
                frameCount,
                generation
            );
        },
        Qt::QueuedConnection
    );
}

void MainWindow::consumePendingFrame(
    bool render
) {
    if (
        pendingFrames_.empty()
    ) {
        return;
    }

    CachedFrame frame =
        std::move(
            pendingFrames_.front()
        );

    pendingFrames_.pop_front();

    if (
        historyIndex_ + 1 <
        static_cast<int>(
            frameHistory_.size()
        )
    ) {
        frameHistory_.erase(
            frameHistory_.begin() +
                historyIndex_ + 1,
            frameHistory_.end()
        );
    }

    frameHistory_.push_back(
        std::move(frame)
    );

    while (
        frameHistory_.size() >
        static_cast<size_t>(
            maxHistoryFrames_
        )
    ) {
        frameHistory_.pop_front();

        if (
            historyIndex_ > 0
        ) {
            --historyIndex_;
        }
    }

    historyIndex_ =
        static_cast<int>(
            frameHistory_.size()
        ) -
        1;

    if (render) {
        displayCachedFrame();
    }

    if (
        pendingFrames_.size() <
        BufferRefillThreshold
    ) {
        requestDecode();
    }
}

void MainWindow::nextFrame() {
    if (!decoderOpen_) {
        return;
    }

    pausePlayback();

    audioNeedsResync_ =
        true;

    if (
        historyIndex_ + 1 <
        static_cast<int>(
            frameHistory_.size()
        )
    ) {
        ++historyIndex_;

        displayCachedFrame();

        return;
    }

    if (
        !pendingFrames_.empty()
    ) {
        consumePendingFrame();

        return;
    }

    manualStepWaiting_ =
        true;

    requestDecode(2);
}

void MainWindow::previousFrame() {
    if (!decoderOpen_) {
        return;
    }

    pausePlayback();

    audioNeedsResync_ =
        true;

    if (
        historyIndex_ <= 0
    ) {
        return;
    }

    --historyIndex_;

    displayCachedFrame();
}

void MainWindow::startForwardScrub() {
    pausePlayback();

    frameStepDirection_ =
        1;

    nextFrame();

    frameStepTimer_.start();
}

void MainWindow::startBackwardScrub() {
    pausePlayback();

    frameStepDirection_ =
        -1;

    previousFrame();

    frameStepTimer_.start();
}

void MainWindow::stopFrameScrub() {
    frameStepTimer_.stop();

    frameStepDirection_ =
        0;
}

void MainWindow::togglePlayback() {
    if (!decoderOpen_) {
        return;
    }

    if (playing_) {
        pausePlayback();

        return;
    }

    if (
        hasAudio_ &&
        playbackSpeed_ == 1.0 &&
        audioNeedsResync_
    ) {
        seekTo(
            currentTimestamp_,
            true
        );

        return;
    }

    startPlayback();
}

void MainWindow::startPlayback() {
    if (
        playing_ ||
        !decoderOpen_
    ) {
        return;
    }

    if (
        historyIndex_ + 1 >=
            static_cast<int>(
                frameHistory_.size()
            ) &&
        pendingFrames_.empty()
    ) {
        resumeAfterSeek_ =
            true;

        requestDecode();

        return;
    }

    playing_ =
        true;

    playButton_->setText(
        "Pause"
    );

    playbackAnchorTimestamp_ =
        currentTimestamp_;

    playbackClock_.restart();

    useAudioClock_ =
        startAudio();

    if (
        useAudioClock_ &&
        audioSink_
    ) {
        audioProcessedAtStart_ =
            audioSink_
                ->processedUSecs();
    }

    playbackTimer_.start();

    if (
        pendingFrames_.size() <
        BufferRefillThreshold
    ) {
        requestDecode();
    }
}

void MainWindow::pausePlayback() {
    playbackTimer_.stop();
    audioPumpTimer_.stop();

    playing_ =
        false;

    useAudioClock_ =
        false;

    playButton_->setText(
        "Play"
    );

    if (audioSink_) {
        audioSink_->reset();
    }

    audioDevice_ =
        nullptr;

    audioQueue_.clear();

    queuedAudioBytes_ =
        0;

    if (
        hasAudio_ &&
        playbackSpeed_ == 1.0
    ) {
        audioNeedsResync_ =
            true;
    }
}

void MainWindow::hardStopForSeek() {
    pausePlayback();

    stopFrameScrub();

    audioNeedsResync_ =
        false;
}

void MainWindow::playbackTick() {
    if (!playing_) {
        return;
    }

    double elapsedSeconds =
        0.0;

    if (
        useAudioClock_ &&
        audioSink_
    ) {
        qint64 processedDelta =
            audioSink_
                ->processedUSecs() -
            audioProcessedAtStart_;

        elapsedSeconds =
            std::max<qint64>(
                processedDelta,
                0
            ) /
            1000000.0;
    } else {
        elapsedSeconds =
            playbackClock_
                .elapsed() /
            1000.0;
    }

    double targetTimestamp =
        playbackAnchorTimestamp_ +
        elapsedSeconds *
            playbackSpeed_;

    if (
        loopStart_ >= 0.0 &&
        loopEnd_ > loopStart_ &&
        targetTimestamp >=
            loopEnd_
    ) {
        seekTo(
            loopStart_,
            true
        );

        return;
    }

    bool advanced =
        false;

    while (true) {
        double nextTimestamp =
            std::numeric_limits<
                double
            >::infinity();

        bool fromHistory =
            false;

        if (
            historyIndex_ + 1 <
            static_cast<int>(
                frameHistory_.size()
            )
        ) {
            nextTimestamp =
                frameHistory_[
                    historyIndex_ + 1
                ].timestamp;

            fromHistory =
                true;
        } else if (
            !pendingFrames_.empty()
        ) {
            nextTimestamp =
                pendingFrames_
                    .front()
                    .timestamp;
        }

        if (
            !std::isfinite(
                nextTimestamp
            ) ||
            nextTimestamp >
                targetTimestamp +
                0.001
        ) {
            break;
        }

        if (fromHistory) {
            ++historyIndex_;
        } else {
            consumePendingFrame(
                false
            );
        }

        advanced =
            true;
    }

    if (advanced) {
        displayCachedFrame();
    }

    if (
        pendingFrames_.size() <
        BufferRefillThreshold
    ) {
        requestDecode();
    }
}

void MainWindow::seekTo(
    double seconds,
    bool resumePlayback
) {
    if (!decoderOpen_) {
        return;
    }

    hardStopForSeek();

    seconds =
        std::clamp(
            seconds,
            0.0,
            duration_
        );

    ++generation_;

    worker_->setDesiredGeneration(
        generation_
    );

    decodeInFlight_ =
        false;

    pendingFrames_.clear();
    frameHistory_.clear();

    historyIndex_ =
        -1;

    manualStepWaiting_ =
        false;

    awaitingFirstFrame_ =
        true;

    resumeAfterSeek_ =
        resumePlayback;

    currentTimestamp_ =
        seconds;

    resetAudio();

    audioNeedsResync_ =
        false;

    if (
        duration_ > 0.0 &&
        !timelineDragging_
    ) {
        timeline_->setValue(
            static_cast<int>(
                (
                    seconds /
                    duration_
                ) *
                TimelineResolution
            )
        );
    }

    quint64 generation =
        generation_;

    QMetaObject::invokeMethod(
        worker_,
        [
            worker = worker_,
            seconds,
            generation
        ]() {
            worker->seek(
                seconds,
                generation
            );
        },
        Qt::QueuedConnection
    );
}

void MainWindow::seekRelative(
    double seconds
) {
    seekTo(
        currentTimestamp_ +
        seconds
    );
}

void MainWindow::displayCachedFrame() {
    if (
        historyIndex_ < 0 ||
        historyIndex_ >=
            static_cast<int>(
                frameHistory_.size()
            )
    ) {
        return;
    }

    const CachedFrame& frame =
        frameHistory_[
            historyIndex_
        ];

    currentFrame_ =
        frame.image;

    currentTimestamp_ =
        frame.timestamp;

    currentFrameNumber_ =
        frame.frameNumber;

    videoWidget_->setFrame(
        currentFrame_
    );

    updateInfo();

    if (
        !timelineDragging_ &&
        duration_ > 0.0
    ) {
        double fraction =
            std::clamp(
                currentTimestamp_ /
                    duration_,
                0.0,
                1.0
            );

        timeline_->setValue(
            static_cast<int>(
                fraction *
                TimelineResolution
            )
        );
    }
}

void MainWindow::setupAudio() {
    resetAudio();

    if (audioSink_) {
        delete audioSink_;

        audioSink_ =
            nullptr;

        audioDevice_ =
            nullptr;
    }

    if (!hasAudio_) {
        return;
    }

    QAudioFormat format;

    format.setSampleRate(
        48000
    );

    format.setChannelCount(
        2
    );

    format.setSampleFormat(
        QAudioFormat::Int16
    );

    QAudioDevice device =
        QMediaDevices::
            defaultAudioOutput();

    if (
        !device.isFormatSupported(
            format
        )
    ) {
        hasAudio_ =
            false;

        return;
    }

    audioSink_ =
        new QAudioSink(
            device,
            format,
            this
        );

    audioSink_->setBufferSize(
        48000 *
        2 *
        2 /
        2
    );
}

void MainWindow::resetAudio() {
    audioPumpTimer_.stop();

    audioQueue_.clear();

    queuedAudioBytes_ =
        0;

    if (audioSink_) {
        audioSink_->reset();
    }

    audioDevice_ =
        nullptr;
}

bool MainWindow::startAudio() {
    if (
        !hasAudio_ ||
        !audioSink_ ||
        playbackSpeed_ != 1.0
    ) {
        return false;
    }

    audioDevice_ =
        audioSink_->start();

    if (!audioDevice_) {
        return false;
    }

    audioPumpTimer_.start();

    pumpAudio();

    return true;
}

void MainWindow::pumpAudio() {
    if (
        !audioSink_ ||
        !audioDevice_
    ) {
        return;
    }

    qsizetype available =
        audioSink_->bytesFree();

    while (
        available > 0 &&
        !audioQueue_.empty()
    ) {
        AudioChunk& chunk =
            audioQueue_.front();

        qsizetype remaining =
            chunk.data.size() -
            chunk.offset;

        qsizetype amount =
            std::min(
                available,
                remaining
            );

        qint64 written =
            audioDevice_->write(
                chunk.data.constData() +
                    chunk.offset,
                amount
            );

        if (written <= 0) {
            break;
        }

        chunk.offset +=
            written;

        available -=
            written;

        if (
            chunk.offset >=
            chunk.data.size()
        ) {
            queuedAudioBytes_ -=
                chunk.data.size();

            audioQueue_.pop_front();
        }
    }
}

void MainWindow::setLoopStart() {
    if (!decoderOpen_) {
        return;
    }

    loopStart_ =
        currentTimestamp_;

    // if existing end is now invalid, clear it and wait for a new end.

    if (
        loopEnd_ >= 0.0 &&
        loopEnd_ <= loopStart_
    ) {
        loopEnd_ = -1.0;
    }

    updateLoopUi();
    updateInfo();
}

void MainWindow::setLoopEnd() {
    if (
        !decoderOpen_ ||
        loopStart_ < 0.0
    ) {
        return;
    }

    if (
        currentTimestamp_ <=
        loopStart_
    ) {
        return;
    }

    loopEnd_ =
        currentTimestamp_;

    updateLoopUi();
    updateInfo();
}

void MainWindow::clearLoop() {
    loopStart_ = -1.0;
    loopEnd_ = -1.0;

    updateLoopUi();
    updateInfo();
}

void MainWindow::saveCurrentFrame() {
    if (
        currentFrame_.isNull()
    ) {
        return;
    }

    QString defaultName =
        QString(
            "vidview-frame-%1.png"
        ).arg(
            currentFrameNumber_
        );

    QString path =
        QFileDialog::
            getSaveFileName(
                this,
                "Save Current Frame",
                defaultName,
                "PNG Image (*.png);;JPEG Image (*.jpg *.jpeg)"
            );

    if (path.isEmpty()) {
        return;
    }

    currentFrame_.save(
        path
    );
}

void MainWindow::changeSpeed(
    int direction
) {
    int newIndex =
        speedBox_->currentIndex() +
        direction;

    newIndex =
        std::clamp(
            newIndex,
            0,
            speedBox_->count() - 1
        );

    speedBox_->setCurrentIndex(
        newIndex
    );
}

void MainWindow::updateLoopUi() {
    loopStartButton_->setProperty(
        "loopState",
        loopStart_ >= 0.0
            ? "start"
            : ""
    );

    loopEndButton_->setProperty(
        "loopState",
        loopEnd_ >= 0.0
            ? "end"
            : ""
    );

    auto refreshStyle =
        [](QPushButton* button) {
            button->style()->unpolish(
                button
            );

            button->style()->polish(
                button
            );

            button->update();
        };

    refreshStyle(
        loopStartButton_
    );

    refreshStyle(
        loopEndButton_
    );

    // update timeline markers
    int startMarker = -1;
    int endMarker = -1;

    if (duration_ > 0.0) {
        if (loopStart_ >= 0.0) {
            startMarker =
                static_cast<int>(
                    std::llround(
                        (
                            loopStart_ /
                            duration_
                        ) *
                        TimelineResolution
                    )
                );
        }

        if (loopEnd_ >= 0.0) {
            endMarker =
                static_cast<int>(
                    std::llround(
                        (
                            loopEnd_ /
                            duration_
                        ) *
                        TimelineResolution
                    )
                );
        }
    }

    timeline_->setLoopMarkers(
        startMarker,
        endMarker
    );
}

void MainWindow::updateInfo() {
    QString audioText;

    if (!hasAudio_) {
        audioText =
            "No audio";
    } else if (
        playbackSpeed_ !=
        1.0
    ) {
        audioText =
            "Audio muted";
    } else {
        audioText =
            "Audio";
    }

    QString loopText =
        "Loop off";

    if (
        loopStart_ >= 0.0 &&
        loopEnd_ > loopStart_
    ) {
        loopText =
            QString(
                "Loop %1 -> %2"
            )
                .arg(
                    formatTime(
                        loopStart_
                    )
                )
                .arg(
                    formatTime(
                        loopEnd_
                    )
                );
    } else if (
        loopStart_ >= 0.0
    ) {
        loopText =
            QString(
                "Loop start %1"
            ).arg(
                formatTime(
                    loopStart_
                )
            );
    }

    infoLabel_->setText(
        QString(
            "Frame ~%1 | %2 FPS | Source %3x%4 | Preview %5x%6 | %7 | %8 | buffer %9"
        )
            .arg(
                currentFrameNumber_
            )
            .arg(
                fps_,
                0,
                'f',
                3
            )
            .arg(
                sourceWidth_
            )
            .arg(
                sourceHeight_
            )
            .arg(
                currentFrame_.width()
            )
            .arg(
                currentFrame_.height()
            )
            .arg(
                audioText
            )
            .arg(
                loopText
            )
            .arg(
                pendingFrames_.size()
            )
    );

    timeLabel_->setText(
        QString(
            "%1 / %2"
        )
            .arg(
                formatTime(
                    currentTimestamp_
                )
            )
            .arg(
                formatTime(
                    duration_
                )
            )
    );
}

QString MainWindow::formatTime(
    double seconds
) const {
    seconds =
        std::max(
            seconds,
            0.0
        );

    qint64 totalMilliseconds =
        static_cast<qint64>(
            std::llround(
                seconds *
                1000.0
            )
        );

    int milliseconds =
        static_cast<int>(
            totalMilliseconds %
            1000
        );

    qint64 totalSeconds =
        totalMilliseconds /
        1000;

    qint64 minutes =
        totalSeconds /
        60;

    int wholeSeconds =
        static_cast<int>(
            totalSeconds %
            60
        );

    return QString(
        "%1:%2.%3"
    )
        .arg(
            minutes,
            2,
            10,
            QChar('0')
        )
        .arg(
            wholeSeconds,
            2,
            10,
            QChar('0')
        )
        .arg(
            milliseconds,
            3,
            10,
            QChar('0')
        );
}

void MainWindow::dragEnterEvent(
    QDragEnterEvent* event
) {
    if (
        event->mimeData()
            ->hasUrls()
    ) {
        event
            ->acceptProposedAction();
    }
}

void MainWindow::dropEvent(
    QDropEvent* event
) {
    if (
        !event->mimeData()
            ->hasUrls()
    ) {
        return;
    }

    const auto urls =
        event->mimeData()
            ->urls();

    if (urls.isEmpty()) {
        return;
    }

    QString path =
        urls.first()
            .toLocalFile();

    if (!path.isEmpty()) {
        openFile(path);
    }
}

void MainWindow::keyPressEvent(
    QKeyEvent* event
) {
    if (
        event->isAutoRepeat()
    ) {
        return;
    }

    if (
        event->key() ==
        Qt::Key_Period
    ) {
        startForwardScrub();

        return;
    }

    if (
        event->key() ==
        Qt::Key_Comma
    ) {
        startBackwardScrub();

        return;
    }

    if (
        event->key() ==
            Qt::Key_Escape &&
        isFullScreen()
    ) {
        showNormal();

        return;
    }

    QMainWindow::keyPressEvent(
        event
    );
}

void MainWindow::keyReleaseEvent(
    QKeyEvent* event
) {
    if (
        event->isAutoRepeat()
    ) {
        return;
    }

    if (
        event->key() ==
            Qt::Key_Period &&
        frameStepDirection_ > 0
    ) {
        stopFrameScrub();

        return;
    }

    if (
        event->key() ==
            Qt::Key_Comma &&
        frameStepDirection_ < 0
    ) {
        stopFrameScrub();

        return;
    }

    QMainWindow::keyReleaseEvent(
        event
    );
}