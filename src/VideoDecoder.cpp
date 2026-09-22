#include "VideoDecoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <QByteArray>

namespace {

QString ffmpegError(int errorCode) {
    char buffer[AV_ERROR_MAX_STRING_SIZE]{};

    av_strerror(
        errorCode,
        buffer,
        sizeof(buffer)
    );

    return QString::fromUtf8(buffer);
}

}

VideoDecoder::~VideoDecoder() {
    close();
}

bool VideoDecoder::open(
    const QString& path,
    QString* errorMessage
) {
    close();

    QByteArray encodedPath = path.toUtf8();

    int result = avformat_open_input(
        &formatContext_,
        encodedPath.constData(),
        nullptr,
        nullptr
    );

    if (result < 0) {
        if (errorMessage) {
            *errorMessage =
                "Could not open file: " +
                ffmpegError(result);
        }

        return false;
    }

    result = avformat_find_stream_info(
        formatContext_,
        nullptr
    );

    if (result < 0) {
        if (errorMessage) {
            *errorMessage =
                "Could not read stream information: " +
                ffmpegError(result);
        }

        close();
        return false;
    }

    const AVCodec* decoder = nullptr;

    videoStreamIndex_ = av_find_best_stream(
        formatContext_,
        AVMEDIA_TYPE_VIDEO,
        -1,
        -1,
        &decoder,
        0
    );

    if (videoStreamIndex_ < 0 || decoder == nullptr) {
        if (errorMessage) {
            *errorMessage = "No video stream found.";
        }

        close();
        return false;
    }

    AVStream* stream =
        formatContext_->streams[videoStreamIndex_];

    codecContext_ = avcodec_alloc_context3(decoder);

    if (!codecContext_) {
        if (errorMessage) {
            *errorMessage =
                "Could not allocate codec context.";
        }

        close();
        return false;
    }

    result = avcodec_parameters_to_context(
        codecContext_,
        stream->codecpar
    );

    if (result < 0) {
        if (errorMessage) {
            *errorMessage =
                "Could not copy codec parameters.";
        }

        close();
        return false;
    }

    result = avcodec_open2(
        codecContext_,
        decoder,
        nullptr
    );

    if (result < 0) {
        if (errorMessage) {
            *errorMessage =
                "Could not open decoder: " +
                ffmpegError(result);
        }

        close();
        return false;
    }

    frame_ = av_frame_alloc();
    packet_ = av_packet_alloc();

    if (!frame_ || !packet_) {
        if (errorMessage) {
            *errorMessage =
                "Could not allocate FFmpeg frame/packet.";
        }

        close();
        return false;
    }

    width_ = codecContext_->width;
    height_ = codecContext_->height;

    AVRational guessedRate =
        av_guess_frame_rate(
            formatContext_,
            stream,
            nullptr
        );

    if (guessedRate.num > 0 &&
        guessedRate.den > 0) {

        fps_ = av_q2d(guessedRate);
    }

    if (fps_ <= 0.0) {
        fps_ = 30.0;
    }

    if (formatContext_->duration != AV_NOPTS_VALUE) {
        duration_ =
            static_cast<double>(formatContext_->duration)
            / AV_TIME_BASE;
    }

    return true;
}

bool VideoDecoder::nextFrame(
    QImage& image,
    double& timestampSeconds
) {
    if (!codecContext_ ||
        !formatContext_) {

        return false;
    }

    while (true) {
        int result =
            avcodec_receive_frame(
                codecContext_,
                frame_
            );

        if (result == 0) {
            return convertCurrentFrame(
                image,
                timestampSeconds
            );
        }

        if (result == AVERROR_EOF) {
            return false;
        }

        if (result != AVERROR(EAGAIN)) {
            return false;
        }

        result =
            av_read_frame(
                formatContext_,
                packet_
            );

        if (result < 0) {
            avcodec_send_packet(
                codecContext_,
                nullptr
            );

            continue;
        }

        if (packet_->stream_index !=
            videoStreamIndex_) {

            av_packet_unref(packet_);
            continue;
        }

        result =
            avcodec_send_packet(
                codecContext_,
                packet_
            );

        av_packet_unref(packet_);

        if (result < 0 &&
            result != AVERROR(EAGAIN)) {

            return false;
        }
    }
}

bool VideoDecoder::convertCurrentFrame(
    QImage& image,
    double& timestampSeconds
) {
    width_ = frame_->width;
    height_ = frame_->height;

    image = QImage(
        width_,
        height_,
        QImage::Format_RGB888
    );

    if (image.isNull()) {
        return false;
    }

    swsContext_ =
        sws_getCachedContext(
            swsContext_,
            frame_->width,
            frame_->height,
            static_cast<AVPixelFormat>(
                frame_->format
            ),
            width_,
            height_,
            AV_PIX_FMT_RGB24,
            SWS_BILINEAR,
            nullptr,
            nullptr,
            nullptr
        );

    if (!swsContext_) {
        return false;
    }

    uint8_t* destinationData[4] = {
        image.bits(),
        nullptr,
        nullptr,
        nullptr
    };

    int destinationLinesize[4] = {
        static_cast<int>(
            image.bytesPerLine()
        ),
        0,
        0,
        0
    };

    sws_scale(
        swsContext_,
        frame_->data,
        frame_->linesize,
        0,
        frame_->height,
        destinationData,
        destinationLinesize
    );

    AVStream* stream =
        formatContext_->streams[
            videoStreamIndex_
        ];

    if (frame_->best_effort_timestamp !=
        AV_NOPTS_VALUE) {

        timestampSeconds =
            frame_->best_effort_timestamp *
            av_q2d(stream->time_base);

    } else {
        timestampSeconds = 0.0;
    }

    return true;
}

void VideoDecoder::close() {
    if (swsContext_) {
        sws_freeContext(swsContext_);
        swsContext_ = nullptr;
    }

    if (packet_) {
        av_packet_free(&packet_);
    }

    if (frame_) {
        av_frame_free(&frame_);
    }

    if (codecContext_) {
        avcodec_free_context(
            &codecContext_
        );
    }

    if (formatContext_) {
        avformat_close_input(
            &formatContext_
        );
    }

    videoStreamIndex_ = -1;
    width_ = 0;
    height_ = 0;
    duration_ = 0.0;
    fps_ = 30.0;
}

bool VideoDecoder::isOpen() const {
    return formatContext_ != nullptr;
}

double VideoDecoder::fps() const {
    return fps_;
}

double VideoDecoder::duration() const {
    return duration_;
}

int VideoDecoder::width() const {
    return width_;
}

int VideoDecoder::height() const {
    return height_;
}