#include "MainWindow.h"

#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
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

    auto* central = new QWidget;

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

    setCentralWidget(central);

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
    )");

    playbackTimer_.setTimerType(
        Qt::PreciseTimer
    );

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
            stopPlayback();
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
            stopPlayback();
            nextFrame();
        }
    );

    connect(
        &playbackTimer_,
        &QTimer::timeout,
        this,
        [this]() {
            nextFrame();
        }
    );

    connect(
        timeline_,
        &QSlider::sliderPressed,
        this,
        [this]() {
            timelineDragging_ = true;
        }
    );

    connect(
        timeline_,
        &QSlider::sliderReleased,
        this,
        [this]() {
            timelineDragging_ = false;

            if (!decoder_.isOpen()) {
                return;
            }

            double fraction =
                static_cast<double>(
                    timeline_->value()
                ) /
                TimelineResolution;

            seekTo(
                fraction *
                decoder_.duration()
            );
        }
    );

    connect(
        speedBox_,
        &QComboBox::currentIndexChanged,
        this,
        [this](int index) {
            playbackSpeed_ =
                speedBox_->itemData(
                    index
                ).toDouble();

            if (playing_) {
                updatePlaybackTimer();
            }
        }
    );
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
    stopPlayback();

    QString error;

    if (!decoder_.open(
            path,
            &error
        )) {

        QMessageBox::critical(
            this,
            "Could not open video",
            error
        );

        return;
    }

    frameCache_.clear();
    cacheIndex_ = -1;

    currentFrame_ = QImage();

    currentTimestamp_ = 0.0;
    currentFrameNumber_ = 0;

    timeline_->setValue(0);

    setWindowTitle(
        QString(
            "VidView - %1"
        ).arg(
            QFileInfo(path).fileName()
        )
    );

    nextFrame();
}

void MainWindow::nextFrame() {
    if (!decoder_.isOpen()) {
        return;
    }

    if (
        cacheIndex_ + 1 <
        static_cast<int>(
            frameCache_.size()
        )
    ) {
        ++cacheIndex_;

        displayCachedFrame();
        return;
    }

    QImage frame;
    double timestamp = 0.0;

    if (!decoder_.nextFrame(
            frame,
            timestamp
        )) {

        stopPlayback();
        return;
    }

    cacheDecodedFrame(
        std::move(frame),
        timestamp
    );

    displayCachedFrame();
}

void MainWindow::previousFrame() {
    if (
        frameCache_.empty() ||
        cacheIndex_ <= 0
    ) {
        return;
    }

    --cacheIndex_;

    displayCachedFrame();
}

void MainWindow::cacheDecodedFrame(
    QImage image,
    double timestamp
) {
    qint64 frameNumber = 1;

    if (!frameCache_.empty()) {
        frameNumber =
            frameCache_.back().frameNumber + 1;
    } else {
        frameNumber =
            std::max<qint64>(
                1,
                static_cast<qint64>(
                    std::llround(
                        timestamp *
                        decoder_.fps()
                    )
                ) + 1
            );
    }

    if (
        frameCache_.size() >=
        MaxCachedFrames
    ) {
        frameCache_.pop_front();

        if (cacheIndex_ > 0) {
            --cacheIndex_;
        }
    }

    frameCache_.push_back({
        std::move(image),
        timestamp,
        frameNumber
    });

    cacheIndex_ =
        static_cast<int>(
            frameCache_.size()
        ) - 1;
}

void MainWindow::displayCachedFrame() {
    if (
        cacheIndex_ < 0 ||
        cacheIndex_ >=
            static_cast<int>(
                frameCache_.size()
            )
    ) {
        return;
    }

    const CachedFrame& frame =
        frameCache_[cacheIndex_];

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
        decoder_.duration() > 0.0
    ) {
        double fraction =
            currentTimestamp_ /
            decoder_.duration();

        fraction =
            std::clamp(
                fraction,
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

void MainWindow::seekTo(
    double seconds
) {
    if (!decoder_.isOpen()) {
        return;
    }

    stopPlayback();

    QImage frame;
    double timestamp = 0.0;

    if (!decoder_.seekTo(
            seconds,
            frame,
            timestamp
        )) {

        return;
    }

    frameCache_.clear();
    cacheIndex_ = -1;

    cacheDecodedFrame(
        std::move(frame),
        timestamp
    );

    displayCachedFrame();
}

void MainWindow::seekRelative(
    double seconds
) {
    seekTo(
        currentTimestamp_ +
        seconds
    );
}

void MainWindow::togglePlayback() {
    if (!decoder_.isOpen()) {
        return;
    }

    if (playing_) {
        stopPlayback();
        return;
    }

    playing_ = true;

    playButton_->setText(
        "Pause"
    );

    updatePlaybackTimer();
}

void MainWindow::updatePlaybackTimer() {
    if (!playing_) {
        return;
    }

    double effectiveFps =
        std::max(
            decoder_.fps() *
                playbackSpeed_,
            1.0
        );

    int interval =
        static_cast<int>(
            std::round(
                1000.0 /
                effectiveFps
            )
        );

    playbackTimer_.start(
        std::max(
            interval,
            1
        )
    );
}

void MainWindow::stopPlayback() {
    playbackTimer_.stop();

    playing_ = false;

    playButton_->setText(
        "Play"
    );
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
            Qt::SmoothTransformation
        )
    );
}

void MainWindow::updateInfo() {
    infoLabel_->setText(
        QString(
            "Frame %1   |   %2 FPS   |   %3 x %4"
        )
            .arg(
                currentFrameNumber_
            )
            .arg(
                decoder_.fps(),
                0,
                'f',
                3
            )
            .arg(
                decoder_.width()
            )
            .arg(
                decoder_.height()
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
                    decoder_.duration()
                )
            )
    );
}

QString MainWindow::formatTime(
    double seconds
) const {
    seconds =
        std::max(
            0.0,
            seconds
        );

    int minutes =
        static_cast<int>(
            seconds / 60.0
        );

    int wholeSeconds =
        static_cast<int>(
            seconds
        ) % 60;

    int milliseconds =
        static_cast<int>(
            std::round(
                (
                    seconds -
                    std::floor(seconds)
                ) *
                1000.0
            )
        );

    if (milliseconds >= 1000) {
        milliseconds = 999;
    }

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
        event->mimeData()->hasUrls()
    ) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(
    QDropEvent* event
) {
    if (
        !event->mimeData()->hasUrls()
    ) {
        return;
    }

    const auto urls =
        event->mimeData()->urls();

    if (urls.isEmpty()) {
        return;
    }

    QString path =
        urls.first().toLocalFile();

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
            stopPlayback();
            nextFrame();
            return;

        case Qt::Key_Comma:
            stopPlayback();
            previousFrame();
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

    QMainWindow::keyPressEvent(
        event
    );
}

void MainWindow::resizeEvent(
    QResizeEvent* event
) {
    QMainWindow::resizeEvent(event);

    renderCurrentFrame();
}