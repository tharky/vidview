#include "MainWindow.h"

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
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>

MainWindow::MainWindow()
    : videoLabel_(new QLabel),
      infoLabel_(new QLabel),
      openButton_(new QPushButton("Open")),
      playButton_(new QPushButton("Play")),
      nextButton_(new QPushButton("Next Frame  .")) {

    setWindowTitle(
        "VidView"
    );

    resize(1100, 750);

    setAcceptDrops(true);

    auto* central = new QWidget;
    auto* mainLayout = new QVBoxLayout(central);
    auto* controls = new QHBoxLayout;

    videoLabel_->setAlignment(Qt::AlignCenter);
    videoLabel_->setMinimumSize(640, 360);

    videoLabel_->setStyleSheet(
        "background-color: black;"
    );

    infoLabel_->setText(
        "Drop a video here or press Ctrl+O"
    );

    controls->addWidget(openButton_);
    controls->addWidget(playButton_);
    controls->addWidget(nextButton_);
    controls->addStretch();

    mainLayout->addWidget(
        videoLabel_,
        1
    );

    mainLayout->addWidget(infoLabel_);
    mainLayout->addLayout(controls);

    setCentralWidget(central);

    setStyleSheet(R"(
        QMainWindow {
            background: #16181d;
        }

        QWidget {
            color: #e6e6e6;
            font-size: 14px;
        }

        QPushButton {
            background: #292d35;
            border: 1px solid #444b57;
            padding: 8px 14px;
            border-radius: 5px;
        }

        QPushButton:hover {
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

    currentPath_ = path;
    frameNumber_ = 0;
    currentTimestamp_ = 0.0;

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

    QImage frame;
    double timestamp = 0.0;

    if (!decoder_.nextFrame(
            frame,
            timestamp
        )) {

        stopPlayback();

        infoLabel_->setText(
            "End of video"
        );

        return;
    }

    currentFrame_ = frame;
    currentTimestamp_ = timestamp;
    ++frameNumber_;

    renderCurrentFrame();

    infoLabel_->setText(
        QString(
            "Frame %1   |   %2 s   |   %3 FPS   |   %4 x %5"
        )
            .arg(frameNumber_)
            .arg(
                currentTimestamp_,
                0,
                'f',
                3
            )
            .arg(
                decoder_.fps(),
                0,
                'f',
                3
            )
            .arg(decoder_.width())
            .arg(decoder_.height())
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
    playButton_->setText("Pause");

    double fps =
        std::max(
            decoder_.fps(),
            1.0
        );

    int interval =
        static_cast<int>(
            std::round(
                1000.0 / fps
            )
        );

    playbackTimer_.start(
        std::max(interval, 1)
    );
}

void MainWindow::stopPlayback() {
    playbackTimer_.stop();

    playing_ = false;

    playButton_->setText("Play");
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

void MainWindow::dragEnterEvent(
    QDragEnterEvent* event
) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(
    QDropEvent* event
) {
    if (!event->mimeData()->hasUrls()) {
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

    if (event->key() == Qt::Key_Space) {
        togglePlayback();
        return;
    }

    if (event->key() == Qt::Key_Period) {
        stopPlayback();
        nextFrame();
        return;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::resizeEvent(
    QResizeEvent* event
) {
    QMainWindow::resizeEvent(event);

    renderCurrentFrame();
}