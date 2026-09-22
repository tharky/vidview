#pragma once

#include <QByteArray>
#include <QImage>
#include <QObject>
#include <QString>

#include <atomic>

struct AVCodecContext;
struct AVFormatContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;
struct SwrContext;

class DecoderWorker final : public QObject {
    Q_OBJECT

public:
    explicit DecoderWorker(QObject* parent = nullptr);
    ~DecoderWorker() override;

    // Safe to call directly from the GUI thread: this only updates an atomic.
    void setDesiredGeneration(quint64 generation) noexcept {
        desiredGeneration_.store(generation, std::memory_order_relaxed);
    }

    void openFile(const QString& path, quint64 generation);
    void decodeBatch(int maxVideoFrames, quint64 generation);
    void seek(double seconds, quint64 generation);
    void closeFile();

signals:
    void opened(
        bool success,
        QString errorMessage,
        double fps,
        double duration,
        int width,
        int height,
        bool hasAudio,
        quint64 generation
    );

    void videoFrameReady(QImage image, double timestamp, quint64 generation);
    void audioChunkReady(QByteArray pcm, double timestamp, quint64 generation);
    void seekFinished(double seconds, quint64 generation);
    void batchFinished(quint64 generation);
    void endOfStream(quint64 generation);

private:
    bool openCodec(
        int streamIndex,
        AVCodecContext*& codecContext,
        QString& errorMessage
    );

    bool isDesiredGeneration(quint64 generation) const noexcept {
        return generation ==
            desiredGeneration_.load(std::memory_order_relaxed);
    }

    QImage convertVideoFrame();
    void processVideoFrames(int& producedFrames, quint64 generation);
    void processAudioFrames(quint64 generation);
    double timestampForFrame(const AVFrame* frame, int streamIndex) const;
    void resetResampler();

    AVFormatContext* formatContext_ = nullptr;
    AVCodecContext* videoCodecContext_ = nullptr;
    AVCodecContext* audioCodecContext_ = nullptr;
    AVFrame* videoFrame_ = nullptr;
    AVFrame* audioFrame_ = nullptr;
    AVPacket* packet_ = nullptr;
    SwsContext* swsContext_ = nullptr;
    SwrContext* swrContext_ = nullptr;

    int videoStreamIndex_ = -1;
    int audioStreamIndex_ = -1;
    int width_ = 0;
    int height_ = 0;

    double fps_ = 30.0;
    double duration_ = 0.0;
    double discardVideoBefore_ = -1.0;
    double discardAudioBefore_ = -1.0;

    quint64 activeGeneration_ = 0;
    std::atomic<quint64> desiredGeneration_{0};

    bool eof_ = false;

    static constexpr int AudioSampleRate = 48000;
    static constexpr int AudioChannels = 2;
};
