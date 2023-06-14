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
#include <libswscale/swscale.h>
}  // extern "C"

class H264Encoder : public VideoEncoder
{
public:
    enum class H264EncoderType
    {
        kUnsupport = -1,
        kH264NVENC,
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

    const AVCodecContext *encodeContext() const override;

private:
    bool isEncoderSupport(const char *encoderName);
    bool doEncode(const AVFrame *frame);
    bool encodeInternal(const AVFrame *frame);
    void threadFunc();
    // Use for h264_qsv encoder
    bool setHwframeCtx();

private:
    std::condition_variable m_cv;
    std::unique_ptr<std::thread> m_thread;
    std::mutex m_frameMutex;
    std::queue<VideoFrame> m_frameQueue;
    std::atomic_bool m_isStarting;

    VideoCodec m_codec;
    EncodedImage m_encodedImage;
    EncodedImageCallback *m_encodedImageCallback;

    H264EncoderType m_encoderType;

    int64_t m_pts;

    AVPixelFormat m_srcPixFmt;
    AVPixelFormat m_swsPixFmt;

    // TODO(hcb): 改造成智能指针
    AVCodecContext *m_encodeCtx;
    const AVCodec *m_avCodec;
    AVBufferRef *m_hwDeviceCtx;
    SwsContext *m_swsCtx;
    AVFrame *m_avFrame;
    // 用于格式转换的帧
    AVFrame *m_swsFrame;
    uint8_t *m_swsBuffer;
    AVPacket *m_packet;
};

#endif  // H264ENCODER_H
