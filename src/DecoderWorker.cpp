#include "DecoderWorker.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <QByteArray>

#include <algorithm>
#include <cmath>

namespace {

QString ffmpegError(
    int errorCode
) {
    char buffer[
        AV_ERROR_MAX_STRING_SIZE
    ]{};

    av_strerror(
        errorCode,
        buffer,
        sizeof(buffer)
    );

    return QString::fromUtf8(
        buffer
    );
}

}

DecoderWorker::DecoderWorker(
    QObject* parent
)
    : QObject(parent) {
}

DecoderWorker::~DecoderWorker() {
    closeFile();
}

bool DecoderWorker::openCodec(
    int streamIndex,
    AVCodecContext*& codecContext,
    QString& errorMessage
) {
    AVStream* stream =
        formatContext_->streams[
            streamIndex
        ];

    const AVCodec* codec =
        avcodec_find_decoder(
            stream->codecpar->codec_id
        );

    if (!codec) {
        errorMessage =
            "Could not find decoder.";

        return false;
    }

    codecContext =
        avcodec_alloc_context3(
            codec
        );

    if (!codecContext) {
        errorMessage =
            "Could not allocate codec context.";

        return false;
    }

    int result =
        avcodec_parameters_to_context(
            codecContext,
            stream->codecpar
        );

    if (result < 0) {
        errorMessage =
            "Could not copy codec parameters: " +
            ffmpegError(result);

        avcodec_free_context(
            &codecContext
        );

        return false;
    }

    if (
        codec->type ==
        AVMEDIA_TYPE_VIDEO
    ) {
        codecContext->thread_count = 0;

        codecContext->thread_type =
            FF_THREAD_FRAME |
            FF_THREAD_SLICE;
    }

    result =
        avcodec_open2(
            codecContext,
            codec,
            nullptr
        );

    if (result < 0) {
        errorMessage =
            "Could not open decoder: " +
            ffmpegError(result);

        avcodec_free_context(
            &codecContext
        );

        return false;
    }

    return true;
}

void DecoderWorker::openFile(
    const QString& path,
    quint64 generation
) {
    closeFile();

    activeGeneration_ =
        generation;

    desiredGeneration_.store(
        generation,
        std::memory_order_relaxed
    );

    QByteArray encodedPath =
        path.toUtf8();

    int result =
        avformat_open_input(
            &formatContext_,
            encodedPath.constData(),
            nullptr,
            nullptr
        );

    if (result < 0) {
        emit opened(
            false,
            "Could not open file: " +
                ffmpegError(result),
            0.0,
            0.0,
            0,
            0,
            false,
            generation
        );

        return;
    }

    result =
        avformat_find_stream_info(
            formatContext_,
            nullptr
        );

    if (result < 0) {
        emit opened(
            false,
            "Could not read stream information.",
            0.0,
            0.0,
            0,
            0,
            false,
            generation
        );

        closeFile();

        return;
    }

    videoStreamIndex_ =
        av_find_best_stream(
            formatContext_,
            AVMEDIA_TYPE_VIDEO,
            -1,
            -1,
            nullptr,
            0
        );

    if (
        videoStreamIndex_ < 0
    ) {
        emit opened(
            false,
            "No video stream found.",
            0.0,
            0.0,
            0,
            0,
            false,
            generation
        );

        closeFile();

        return;
    }

    QString errorMessage;

    if (
        !openCodec(
            videoStreamIndex_,
            videoCodecContext_,
            errorMessage
        )
    ) {
        emit opened(
            false,
            errorMessage,
            0.0,
            0.0,
            0,
            0,
            false,
            generation
        );

        closeFile();

        return;
    }

    width_ =
        videoCodecContext_->width;

    height_ =
        videoCodecContext_->height;

    AVStream* videoStream =
        formatContext_->streams[
            videoStreamIndex_
        ];

    AVRational guessedFrameRate =
        av_guess_frame_rate(
            formatContext_,
            videoStream,
            nullptr
        );

    if (
        guessedFrameRate.num > 0 &&
        guessedFrameRate.den > 0
    ) {
        fps_ =
            av_q2d(
                guessedFrameRate
            );
    }

    if (fps_ <= 0.0) {
        fps_ = 30.0;
    }

    if (
        formatContext_->duration !=
        AV_NOPTS_VALUE
    ) {
        duration_ =
            static_cast<double>(
                formatContext_->duration
            ) /
            AV_TIME_BASE;
    }

    videoFrame_ =
        av_frame_alloc();

    packet_ =
        av_packet_alloc();

    if (
        !videoFrame_ ||
        !packet_
    ) {
        emit opened(
            false,
            "Could not allocate FFmpeg frame/packet.",
            0.0,
            0.0,
            0,
            0,
            false,
            generation
        );

        closeFile();

        return;
    }

    audioStreamIndex_ =
        av_find_best_stream(
            formatContext_,
            AVMEDIA_TYPE_AUDIO,
            -1,
            -1,
            nullptr,
            0
        );

    bool hasAudio = false;

    if (
        audioStreamIndex_ >= 0
    ) {
        QString audioError;

        if (
            openCodec(
                audioStreamIndex_,
                audioCodecContext_,
                audioError
            )
        ) {
            audioFrame_ =
                av_frame_alloc();

            AVChannelLayout stereo =
                AV_CHANNEL_LAYOUT_STEREO;

            result =
                swr_alloc_set_opts2(
                    &swrContext_,
                    &stereo,
                    AV_SAMPLE_FMT_S16,
                    AudioSampleRate,
                    &audioCodecContext_
                        ->ch_layout,
                    audioCodecContext_
                        ->sample_fmt,
                    audioCodecContext_
                        ->sample_rate,
                    0,
                    nullptr
                );

            av_channel_layout_uninit(
                &stereo
            );

            if (
                result >= 0 &&
                audioFrame_ &&
                swr_init(
                    swrContext_
                ) >= 0
            ) {
                hasAudio = true;
            } else {
                if (audioFrame_) {
                    av_frame_free(
                        &audioFrame_
                    );
                }

                if (swrContext_) {
                    swr_free(
                        &swrContext_
                    );
                }

                if (
                    audioCodecContext_
                ) {
                    avcodec_free_context(
                        &audioCodecContext_
                    );
                }

                audioStreamIndex_ = -1;
            }
        }
    }

    eof_ = false;

    emit opened(
        true,
        QString(),
        fps_,
        duration_,
        width_,
        height_,
        hasAudio,
        generation
    );
}

double DecoderWorker::timestampForFrame(
    const AVFrame* frame,
    int streamIndex
) const {
    if (
        !frame ||
        frame->best_effort_timestamp ==
            AV_NOPTS_VALUE
    ) {
        return -1.0;
    }

    AVStream* stream =
        formatContext_->streams[
            streamIndex
        ];

    return
        frame->best_effort_timestamp *
        av_q2d(
            stream->time_base
        );
}

QImage DecoderWorker::convertVideoFrame() {
    //ui preview capped at 1080
    constexpr int MaxPreviewWidth =
        1920;

    constexpr int MaxPreviewHeight =
        1080;

    const int sourceWidth =
        videoFrame_->width;

    const int sourceHeight =
        videoFrame_->height;

    double scale =
        std::min({
            1.0,
            static_cast<double>(
                MaxPreviewWidth
            ) /
            sourceWidth,
            static_cast<double>(
                MaxPreviewHeight
            ) /
            sourceHeight
        });

    int outputWidth =
        std::max(
            1,
            static_cast<int>(
                std::lround(
                    sourceWidth *
                    scale
                )
            )
        );

    int outputHeight =
        std::max(
            1,
            static_cast<int>(
                std::lround(
                    sourceHeight *
                    scale
                )
            )
        );

    QImage image(
        outputWidth,
        outputHeight,
        QImage::Format_RGBA8888
    );

    if (image.isNull()) {
        return {};
    }

    swsContext_ =
        sws_getCachedContext(
            swsContext_,
            sourceWidth,
            sourceHeight,
            static_cast<AVPixelFormat>(
                videoFrame_->format
            ),
            outputWidth,
            outputHeight,
            AV_PIX_FMT_RGBA,
            SWS_FAST_BILINEAR,
            nullptr,
            nullptr,
            nullptr
        );

    if (!swsContext_) {
        return {};
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
        videoFrame_->data,
        videoFrame_->linesize,
        0,
        sourceHeight,
        destinationData,
        destinationLinesize
    );

    return image;
}

void DecoderWorker::processVideoFrames(
    int& producedFrames,
    quint64 generation
) {
    while (true) {
        if (
            generation !=
            desiredGeneration_.load(
                std::memory_order_relaxed
            )
        ) {
            return;
        }

        int result =
            avcodec_receive_frame(
                videoCodecContext_,
                videoFrame_
            );

        if (
            result ==
                AVERROR(EAGAIN) ||
            result ==
                AVERROR_EOF
        ) {
            return;
        }

        if (result < 0) {
            return;
        }

        double timestamp =
            timestampForFrame(
                videoFrame_,
                videoStreamIndex_
            );

        if (
            discardVideoBefore_ >= 0.0 &&
            timestamp >= 0.0 &&
            timestamp + 0.000001 <
                discardVideoBefore_
        ) {
            av_frame_unref(
                videoFrame_
            );

            continue;
        }

        discardVideoBefore_ =
            -1.0;

        QImage image =
            convertVideoFrame();

        if (!image.isNull()) {
            emit videoFrameReady(
                std::move(image),
                std::max(
                    timestamp,
                    0.0
                ),
                generation
            );

            ++producedFrames;
        }

        av_frame_unref(
            videoFrame_
        );
    }
}

void DecoderWorker::processAudioFrames(
    quint64 generation
) {
    if (
        !audioCodecContext_ ||
        !swrContext_
    ) {
        return;
    }

    while (true) {
        if (
            generation !=
            desiredGeneration_.load(
                std::memory_order_relaxed
            )
        ) {
            return;
        }

        int result =
            avcodec_receive_frame(
                audioCodecContext_,
                audioFrame_
            );

        if (
            result ==
                AVERROR(EAGAIN) ||
            result ==
                AVERROR_EOF
        ) {
            return;
        }

        if (result < 0) {
            return;
        }

        double timestamp =
            timestampForFrame(
                audioFrame_,
                audioStreamIndex_
            );

        if (
            discardAudioBefore_ >= 0.0 &&
            timestamp >= 0.0 &&
            timestamp + 0.000001 <
                discardAudioBefore_
        ) {
            av_frame_unref(
                audioFrame_
            );

            continue;
        }

        discardAudioBefore_ =
            -1.0;

        int outputSamples =
            static_cast<int>(
                av_rescale_rnd(
                    swr_get_delay(
                        swrContext_,
                        audioCodecContext_
                            ->sample_rate
                    ) +
                        audioFrame_
                            ->nb_samples,
                    AudioSampleRate,
                    audioCodecContext_
                        ->sample_rate,
                    AV_ROUND_UP
                )
            );

        int bytesPerSample =
            av_get_bytes_per_sample(
                AV_SAMPLE_FMT_S16
            );

        QByteArray pcm;

        pcm.resize(
            outputSamples *
            AudioChannels *
            bytesPerSample
        );

        uint8_t* outputData[1] = {
            reinterpret_cast<
                uint8_t*
            >(
                pcm.data()
            )
        };

        const uint8_t* const*
            inputData =
                audioFrame_
                    ->extended_data;

        int convertedSamples =
            swr_convert(
                swrContext_,
                outputData,
                outputSamples,
                inputData,
                audioFrame_
                    ->nb_samples
            );

        if (
            convertedSamples > 0
        ) {
            pcm.resize(
                convertedSamples *
                AudioChannels *
                bytesPerSample
            );

            emit audioChunkReady(
                std::move(pcm),
                std::max(
                    timestamp,
                    0.0
                ),
                generation
            );
        }

        av_frame_unref(
            audioFrame_
        );
    }
}

void DecoderWorker::decodeBatch(
    int maxVideoFrames,
    quint64 generation
) {
    if (
        generation !=
            activeGeneration_ ||
        generation !=
            desiredGeneration_.load(
                std::memory_order_relaxed
            ) ||
        !formatContext_ ||
        eof_
    ) {
        emit batchFinished(
            generation
        );

        return;
    }

    int producedFrames = 0;

    while (
        producedFrames <
        maxVideoFrames
    ) {
        if (
            generation !=
            desiredGeneration_.load(
                std::memory_order_relaxed
            )
        ) {
            emit batchFinished(
                generation
            );

            return;
        }

        int result =
            av_read_frame(
                formatContext_,
                packet_
            );

        if (result < 0) {
            avcodec_send_packet(
                videoCodecContext_,
                nullptr
            );

            processVideoFrames(
                producedFrames,
                generation
            );

            if (
                audioCodecContext_
            ) {
                avcodec_send_packet(
                    audioCodecContext_,
                    nullptr
                );

                processAudioFrames(
                    generation
                );
            }

            eof_ = true;

            emit endOfStream(
                generation
            );

            break;
        }

        if (
            packet_->stream_index ==
            videoStreamIndex_
        ) {
            result =
                avcodec_send_packet(
                    videoCodecContext_,
                    packet_
                );

            if (
                result >= 0 ||
                result ==
                    AVERROR(EAGAIN)
            ) {
                processVideoFrames(
                    producedFrames,
                    generation
                );
            }
        } else if (
            audioCodecContext_ &&
            packet_->stream_index ==
                audioStreamIndex_
        ) {
            result =
                avcodec_send_packet(
                    audioCodecContext_,
                    packet_
                );

            if (
                result >= 0 ||
                result ==
                    AVERROR(EAGAIN)
            ) {
                processAudioFrames(
                    generation
                );
            }
        }

        av_packet_unref(
            packet_
        );
    }

    emit batchFinished(
        generation
    );
}

void DecoderWorker::resetResampler() {
    if (!swrContext_) {
        return;
    }

    swr_close(
        swrContext_
    );

    swr_init(
        swrContext_
    );
}

void DecoderWorker::seek(
    double seconds,
    quint64 generation
) {
    if (!formatContext_) {
        return;
    }

    desiredGeneration_.store(
        generation,
        std::memory_order_relaxed
    );

    activeGeneration_ =
        generation;

    seconds =
        std::max(
            0.0,
            seconds
        );

    if (duration_ > 0.0) {
        seconds =
            std::min(
                seconds,
                duration_
            );
    }

    AVStream* stream =
        formatContext_->streams[
            videoStreamIndex_
        ];

    int64_t targetTimestamp =
        av_rescale_q(
            static_cast<int64_t>(
                std::llround(
                    seconds *
                    AV_TIME_BASE
                )
            ),
            AV_TIME_BASE_Q,
            stream->time_base
        );

    int result =
        av_seek_frame(
            formatContext_,
            videoStreamIndex_,
            targetTimestamp,
            AVSEEK_FLAG_BACKWARD
        );

    if (result < 0) {
        emit seekFinished(
            seconds,
            generation
        );

        return;
    }

    avformat_flush(
        formatContext_
    );

    avcodec_flush_buffers(
        videoCodecContext_
    );

    if (
        audioCodecContext_
    ) {
        avcodec_flush_buffers(
            audioCodecContext_
        );
    }

    if (packet_) {
        av_packet_unref(
            packet_
        );
    }

    resetResampler();

    discardVideoBefore_ =
        seconds;

    discardAudioBefore_ =
        seconds;

    eof_ = false;

    emit seekFinished(
        seconds,
        generation
    );
}

void DecoderWorker::closeFile() {
    if (swsContext_) {
        sws_freeContext(
            swsContext_
        );

        swsContext_ = nullptr;
    }

    if (swrContext_) {
        swr_free(
            &swrContext_
        );
    }

    if (packet_) {
        av_packet_free(
            &packet_
        );
    }

    if (videoFrame_) {
        av_frame_free(
            &videoFrame_
        );
    }

    if (audioFrame_) {
        av_frame_free(
            &audioFrame_
        );
    }

    if (
        videoCodecContext_
    ) {
        avcodec_free_context(
            &videoCodecContext_
        );
    }

    if (
        audioCodecContext_
    ) {
        avcodec_free_context(
            &audioCodecContext_
        );
    }

    if (formatContext_) {
        avformat_close_input(
            &formatContext_
        );
    }

    videoStreamIndex_ = -1;
    audioStreamIndex_ = -1;

    width_ = 0;
    height_ = 0;

    fps_ = 30.0;
    duration_ = 0.0;

    discardVideoBefore_ = -1.0;
    discardAudioBefore_ = -1.0;

    activeGeneration_ = 0;

    eof_ = false;
}