#ifndef H264ENCODER_H
#define H264ENCODER_H

#include <stdint.h>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

#include "videoencoder.h"

extern "C"
{
#include <libavcodec/avcodec.h>
}  // extern "C"

class H264Encoder : public VideoEncoder
{
public:
    enum class H264EncoderType
    {
        kH264NVENC = 0,
        kH264QSV,
        kX264,
    };

    H264Encoder();
    ~H264Encoder() override;

    bool initEncoder(const VideoCodec &codec) override;
    void release() override;
    void encode(const VideoFrame &frame) override;
    void registerEncodeCompleteCallback(
        EncodedImageCallback *callback) override;

    // Start encoder thread
    void start() override;
    void stop() override;

private:
    bool isEncoderSupport(const char *encoderName);
    void encodeInternal(const AVFrame *frame);
    void threadFunc();

private:
    std::condition_variable m_cv;
    std::unique_ptr<std::thread> m_thread;
    std::mutex m_frameMutex;
    std::queue<VideoFrame> m_inFrames;
    std::atomic_bool m_isStarting;

    VideoCodec m_codec;
    EncodedImage m_encodedImage;
    EncodedImageCallback *m_encodedImageCallback;

    H264EncoderType m_encoderType;

    int64_t m_pts;

    AVCodecContext *m_codecCtx;
    AVCodec *m_avCodec;
    AVBufferRef *m_hwDeviceCtx;
    // 软编码时使用的帧, 无论是否软编均需要 YUV420P
    AVFrame *m_avFrame;
    // 硬编码时需使用的帧, 由m_swFrame转化成 NV12
    AVFrame *m_hwFrame;
    AVPacket *m_packet;
    //    AVOutputFormat *m_oFmt;
    //    AVFormatContext *m_oFmtCtx;
    //    AVStream *m_videoStream;
    //    AVDictionary *m_option;

    //    SwsContext *m_swsCtx;
    //    uint8_t *m_buf;
    //    AVPixelFormat m_swsPixFmt;
    //    AVPixelFormat m_srcPixFmt;
};

#endif  // H264ENCODER_H
