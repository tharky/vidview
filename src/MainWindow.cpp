#include "MainWindow.h"

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
#include <QLabel>
#include <QMediaDevices>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QResizeEvent>
#include <QSlider>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <limits>

MainWindow::MainWindow()
    : videoLabel_(new QLabel),
      infoLabel_(new QLabel),
      timeLabel_(new QLabel),
      openButton_(new QPushButton("Open")),
      previousButton_(new QPushButton("<  Previous")),
      playButton_(new QPushButton("Play")),
      nextButton_(new QPushButton("Next  >")),
      timeline_(new QSlider(Qt::Horizontal)),
      speedBox_(new QComboBox) {

    setWindowTitle(
        "VidView - Video Inspector"
    );

    resize(1150, 780);
    setAcceptDrops(true);

    auto* central =
        new QWidget;

    auto* mainLayout =
        new QVBoxLayout(central);

    auto* controls =
        new QHBoxLayout;

    auto* timelineRow =
        new QHBoxLayout;

    videoLabel_->setAlignment(
        Qt::AlignCenter
    );

    videoLabel_->setMinimumSize(
        640,
        360
    );

    videoLabel_->setStyleSheet(
        "background-color: black;"
    );

    infoLabel_->setText(
        "Drop a video here or press Ctrl+O"
    );

    timeLabel_->setText(
        "00:00.000 / 00:00.000"
    );

    timeline_->setRange(
        0,
        TimelineResolution
    );

    speedBox_->addItem("0.25x", 0.25);
    speedBox_->addItem("0.5x", 0.5);
    speedBox_->addItem("1x", 1.0);
    speedBox_->addItem("1.5x", 1.5);
    speedBox_->addItem("2x", 2.0);
    speedBox_->addItem("4x", 4.0);

    speedBox_->setCurrentIndex(2);

    timelineRow->addWidget(
        timeline_,
        1
    );

    timelineRow->addWidget(
        timeLabel_
    );

    controls->addWidget(
        openButton_
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

    controls->addStretch();

    controls->addWidget(
        speedBox_
    );

    mainLayout->addWidget(
        videoLabel_,
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

        QPushButton,
        QComboBox {
            background: #292d35;
            border: 1px solid #444b57;
            padding: 8px 14px;
            border-radius: 5px;
        }

        QPushButton:hover,
        QComboBox:hover {
            background: #353b46;
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

    /*
        Playback timing is now separate
        from decoding.

        Decoder thread fills queues.
        UI timer decides WHEN to display.
    */

    playbackTimer_.setInterval(16);
    playbackTimer_.setTimerType(
        Qt::PreciseTimer
    );

    audioPumpTimer_.setInterval(10);
    audioPumpTimer_.setTimerType(
        Qt::PreciseTimer
    );

    frameStepTimer_.setInterval(120);

    frameStepTimer_.setTimerType(
        Qt::PreciseTimer
    );

    connect(
        &frameStepTimer_,
        &QTimer::timeout,
        this,
        [this]() {
            if (frameStepDirection_ > 0) {
                nextFrame();
            } else if (
                frameStepDirection_ < 0
            ) {
                previousFrame();
            }
        }
    );

    /*
        Create the decoder worker with no
        parent, then move ownership of its
        work to the decoder thread.
    */

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

            decodeInFlight_ = false;

            if (!success) {
                decoderOpen_ = false;

                QMessageBox::critical(
                    this,
                    "Could not open video",
                    error
                );

                return;
            }

            decoderOpen_ = true;

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

            /*
                keep ~180MB frame history max
            */
            constexpr qint64 targetHistoryBytes =
                180LL * 1024LL * 1024LL;

            constexpr int MaxPreviewWidth = 1920;
            constexpr int MaxPreviewHeight = 1080;

            double previewScale =
                std::min({
                    1.0,
                    static_cast<double>(
                        MaxPreviewWidth
                    ) / width,
                    static_cast<double>(
                        MaxPreviewHeight
                    ) / height
                });

            int previewWidth =
                std::max(
                    1,
                    static_cast<int>(
                        std::lround(
                            width *
                            previewScale
                        )
                    )
                );

            int previewHeight =
                std::max(
                    1,
                    static_cast<int>(
                        std::lround(
                            height *
                            previewScale
                        )
                    )
                );

            qint64 approximateFrameBytes =
                static_cast<qint64>(
                    previewWidth
                ) *
                static_cast<qint64>(
                    previewHeight
                ) *
                3LL;

            if (approximateFrameBytes > 0) {
                maxHistoryFrames_ =
                    std::clamp(
                        static_cast<int>(
                            targetHistoryBytes /
                            approximateFrameBytes
                        ),
                        6,
                        AbsoluteMaxHistoryFrames
                    );
            }

            hasAudio_ =
                hasAudio;

            setupAudio();

            awaitingFirstFrame_ = true;

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
                    ) + 1
                );

            pendingFrames_.push_back({
                std::move(image),
                timestamp,
                estimatedFrame
            });

            if (awaitingFirstFrame_) {
                awaitingFirstFrame_ = false;

                consumePendingFrame();
            } else if (
                manualStepWaiting_
            ) {
                manualStepWaiting_ = false;

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
                playbackSpeed_ != 1.0
            ) {
                return;
            }

            /*
                audio buffer cap
                48kHz * stereo * int16
                = 192,000 bytes/sec, keep 2 seconds at most
            */
            constexpr qsizetype MaxAudioQueueBytes =
                48000 * 2 * 2 * 2;

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

            decodeInFlight_ = false;

            if (
                resumeAfterSeek_ &&
                !awaitingFirstFrame_
            ) {
                resumeAfterSeek_ = false;

                startPlayback();
                return;
            }

            if (
                pendingFrames_.size() <
                BufferRefillThreshold
            ) {
                requestDecode();
            }
        }
    );

    decoderThread_.start();

    connect(
        openButton_,
        &QPushButton::clicked,
        this,
        [this]() {
            chooseFile();
        }
    );

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
        timeline_,
        &QSlider::sliderPressed,
        this,
        [this]() {
            timelineDragging_ = true;

            /*
                VidView is an analysis tool:
                touching the timeline always stops
                playback before seeking.
            */
            hardStopForSeek();
        }
    );

    connect(
        timeline_,
        &QSlider::sliderReleased,
        this,
        [this]() {
            timelineDragging_ = false;

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
                speedBox_->itemData(
                    index
                ).toDouble();

            /*
                Until we add true time-stretch,
                non-1x playback is intentionally
                silent.
            */

            audioNeedsResync_ = true;

            if (wasPlaying) {
                if (
                    playbackSpeed_ == 1.0 &&
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

MainWindow::~MainWindow() {
    playbackTimer_.stop();
    audioPumpTimer_.stop();

    if (audioSink_) {
        audioSink_->reset();
    }

    if (
        worker_ &&
        decoderThread_.isRunning()
    ) {
        QMetaObject::invokeMethod(
            worker_,
            [worker = worker_]() {
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
        QFileDialog::getOpenFileName(
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
    pausePlayback();

    ++generation_;

    worker_->setDesiredGeneration(
        generation_
    );

    decoderOpen_ = false;
    decodeInFlight_ = false;

    frameHistory_.clear();
    pendingFrames_.clear();

    historyIndex_ = -1;

    currentFrame_ = QImage();

    currentTimestamp_ = 0.0;
    currentFrameNumber_ = 0;

    duration_ = 0.0;

    resetAudio();

    timeline_->setValue(0);

    videoLabel_->clear();

    videoLabel_->setText(
        "Loading..."
    );

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

    decodeInFlight_ = true;

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
    if (pendingFrames_.empty()) {
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

        if (historyIndex_ > 0) {
            --historyIndex_;
        }
    }

    historyIndex_ =
        static_cast<int>(
            frameHistory_.size()
        ) - 1;

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

    audioNeedsResync_ = true;

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

    if (!pendingFrames_.empty()) {
        consumePendingFrame();
        return;
    }

    manualStepWaiting_ = true;

    requestDecode(2);
}

void MainWindow::previousFrame() {
    if (!decoderOpen_) {
        return;
    }

    pausePlayback();

    audioNeedsResync_ = true;

    if (historyIndex_ <= 0) {
        return;
    }

    --historyIndex_;

    displayCachedFrame();
}

void MainWindow::togglePlayback() {
    if (!decoderOpen_) {
        return;
    }

    if (playing_) {
        pausePlayback();
        return;
    }

    /*
        Frame stepping changes the displayed
        position without consuming matching
        audio. Re-seek once before resuming
        normal 1x playback.
    */

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
        resumeAfterSeek_ = true;

        requestDecode();

        return;
    }

    playing_ = true;

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
        pendingFrames_.size() < BufferRefillThreshold
    ) {
        requestDecode();
    }
}

void MainWindow::pausePlayback() {
    playbackTimer_.stop();
    audioPumpTimer_.stop();

    playing_ = false;
    useAudioClock_ = false;

    playButton_->setText(
        "Play"
    );

    /*
        Do NOT suspend the existing audio stream.

        Fully discard it instead. Keeping a
        suspended QAudioSink/QIODevice alive
        was creating a different state from
        our timeline hard-stop path.
    */
    if (audioSink_) {
        audioSink_->reset();
    }

    audioDevice_ = nullptr;

    audioQueue_.clear();
    queuedAudioBytes_ = 0;

    /*
        If playback starts again from this
        paused frame, we need to rebuild audio
        beginning at the current timestamp.
    */
    if (
        hasAudio_ &&
        playbackSpeed_ == 1.0
    ) {
        audioNeedsResync_ = true;
    }
}

void MainWindow::hardStopForSeek() {
    /*
        First use the exact same shutdown path
        as a normal manual pause.
    */
    pausePlayback();

    /*
        Seeking also terminates any held
        frame-step operation.
    */
    frameStepTimer_.stop();
    frameStepDirection_ = 0;

    /*
        seekTo() is about to establish a new
        decoder/audio position, so it will not
        need the pause-resume resync path.
    */
    audioNeedsResync_ = false;
}

void MainWindow::playbackTick() {
    if (!playing_) {
        return;
    }

    double elapsedSeconds = 0.0;

    if (
        useAudioClock_ &&
        audioSink_
    ) {
        qint64 processedDelta =
            audioSink_->processedUSecs() -
            audioProcessedAtStart_;

        elapsedSeconds =
            std::max<qint64>(
                processedDelta,
                0
            ) /
            1000000.0;
    } else {
        elapsedSeconds =
            playbackClock_.elapsed() /
            1000.0;
    }

    double targetTimestamp =
        playbackAnchorTimestamp_ +
        elapsedSeconds *
            playbackSpeed_;

    /*
        Advance through every frame that
        should already have been displayed,
        but DO NOT render each intermediate
        frame.

        We render only the final/latest one.
    */
    bool advanced = false;

    while (true) {
        double nextTimestamp =
            std::numeric_limits<double>::
                infinity();

        bool fromHistory = false;

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

            fromHistory = true;
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
            consumePendingFrame(false);
        }

        advanced = true;
    }

    /*
        ONE expensive QImage -> QPixmap ->
        scaling operation per GUI tick.
    */
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

    decodeInFlight_ = false;

    pendingFrames_.clear();
    frameHistory_.clear();

    historyIndex_ = -1;

    manualStepWaiting_ = false;
    awaitingFirstFrame_ = true;

    resumeAfterSeek_ =
        resumePlayback;

    currentTimestamp_ =
        seconds;

    resetAudio();

    audioNeedsResync_ = false;

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

    renderCurrentFrame();
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

        audioSink_ = nullptr;
        audioDevice_ = nullptr;
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
        /*
            Video still works if the system
            cannot output our chosen PCM
            format.
        */

        hasAudio_ = false;
        return;
    }

    audioSink_ =
        new QAudioSink(
            device,
            format,
            this
        );

    /*
        Roughly half a second of
        48kHz stereo 16-bit audio.
    */

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
    queuedAudioBytes_ = 0;

    if (audioSink_) {
        audioSink_->reset();

        audioDevice_ = nullptr;
    }
}

bool MainWindow::startAudio() {
    if (
        !hasAudio_ ||
        !audioSink_ ||
        playbackSpeed_ != 1.0
    ) {
        return false;
    }

    if (!audioDevice_) {
        audioDevice_ =
            audioSink_->start();
    } else {
        audioSink_->resume();
    }

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

void MainWindow::renderCurrentFrame() {
    if (currentFrame_.isNull()) {
        return;
    }

    videoLabel_->setPixmap(
        QPixmap::fromImage(
            currentFrame_
        ).scaled(
            videoLabel_->size(),
            Qt::KeepAspectRatio,
            Qt::FastTransformation
        )
    );
}

void MainWindow::updateInfo() {
    QString audioText;

    if (!hasAudio_) {
        audioText = "No audio";
    } else if (
        playbackSpeed_ != 1.0
    ) {
        audioText =
            "Audio muted @ non-1x";
    } else {
        audioText = "Audio";
    }

    infoLabel_->setText(
        QString(
            "Frame ~%1   |   %2 FPS   |   %3   |   buffered: %4 frames"
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
                audioText
            )
            .arg(
                pendingFrames_
                    .size()
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

    int totalMilliseconds =
        static_cast<int>(
            std::llround(
                seconds *
                1000.0
            )
        );

    int milliseconds =
        totalMilliseconds %
        1000;

    int totalSeconds =
        totalMilliseconds /
        1000;

    int minutes =
        totalSeconds /
        60;

    int wholeSeconds =
        totalSeconds %
        60;

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
        event->acceptProposedAction();
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
    if (event->isAutoRepeat()) {
        return;
    }

    if (
        event->key() == Qt::Key_O &&
        event->modifiers() &
            Qt::ControlModifier
    ) {
        chooseFile();
        return;
    }

    switch (event->key()) {
        case Qt::Key_Space:
            togglePlayback();
            return;

        case Qt::Key_Period:
            pausePlayback();

            frameStepDirection_ = 1;

            nextFrame();

            frameStepTimer_.start();
            return;

        case Qt::Key_Comma:
            pausePlayback();

            frameStepDirection_ = -1;

            previousFrame();

            frameStepTimer_.start();
            return;

        case Qt::Key_Left:
            seekRelative(-5.0);
            return;

        case Qt::Key_Right:
            seekRelative(5.0);
            return;

        default:
            break;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(
    QKeyEvent* event
) {
    if (event->isAutoRepeat()) {
        return;
    }

    bool stopRepeating = false;

    if (
        event->key() == Qt::Key_Period &&
        frameStepDirection_ > 0
    ) {
        stopRepeating = true;
    }

    if (
        event->key() == Qt::Key_Comma &&
        frameStepDirection_ < 0
    ) {
        stopRepeating = true;
    }

    if (stopRepeating) {
        frameStepTimer_.stop();

        frameStepDirection_ = 0;

        return;
    }

    QMainWindow::keyReleaseEvent(event);
}

void MainWindow::resizeEvent(
    QResizeEvent* event
) {
    QMainWindow::resizeEvent(
        event
    );

    renderCurrentFrame();
}