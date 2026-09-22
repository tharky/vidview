#pragma once

#include <QImage>
#include <QString>

struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

class VideoDecoder {
public:
    VideoDecoder() = default;
    ~VideoDecoder();

    VideoDecoder(const VideoDecoder&) = delete;
    VideoDecoder& operator=(const VideoDecoder&) = delete;

    bool open(const QString& path, QString* errorMessage = nullptr);
    void close();

    bool nextFrame(QImage& image, double& timestampSeconds);

    bool isOpen() const;

    double fps() const;
    double duration() const;

    int width() const;
    int height() const;

private:
    bool convertCurrentFrame(QImage& image, double& timestampSeconds);

    AVFormatContext* formatContext_ = nullptr;
    AVCodecContext* codecContext_ = nullptr;

    AVFrame* frame_ = nullptr;
    AVPacket* packet_ = nullptr;

    SwsContext* swsContext_ = nullptr;

    int videoStreamIndex_ = -1;

    double fps_ = 30.0;
    double duration_ = 0.0;

    int width_ = 0;
    int height_ = 0;
};