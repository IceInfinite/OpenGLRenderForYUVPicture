#include "h264encoder.h"

#include <string.h>

#include <QDebug>

extern "C"
{
#include <libavutil/opt.h>

}  // extern "C"

namespace
{
constexpr char kH264NVENCName[] = {"h264_nvenc"};
constexpr char kH264QSVName[] = {"h264_qsv"};
constexpr char kX264Name[] = {"libx264"};
}  // namespace

H264Encoder::H264Encoder()
    : m_thread(nullptr),
      m_isStarting(false),
      m_encodedImageCallback(nullptr),
      m_encoderType(H264EncoderType::kX264),
      m_pts(0),
      m_codecCtx(nullptr),
      m_avCodec(nullptr),
      m_hwDeviceCtx(nullptr),
      m_avFrame(nullptr),
      m_hwFrame(nullptr),
      m_packet(nullptr)
{
}

H264Encoder::~H264Encoder()
{
    release();
}

bool H264Encoder::initEncoder(const VideoCodec &codec)
{
    release();

    m_codec = codec;
    m_encodedImage.setEncodedBuffer(EncodedImageBuffer::create());
    m_encodedImage.setWidth(m_codec.m_width);
    m_encodedImage.setHeight(m_codec.m_height);
    m_encodedImage.setSize(0);
    switch (m_encoderType)
    {
        case H264EncoderType::kH264NVENC:
            break;
        case H264EncoderType::kH264QSV:
            break;
        case H264EncoderType::kX264:
            m_avCodec =
                const_cast<AVCodec *>(avcodec_find_encoder_by_name(kX264Name));
            break;
    }
    if (!m_avCodec)
    {
        qWarning() << "Can't find requested codec, codec type: "
                   << static_cast<int>(m_encoderType);
        return false;
    }
    m_codecCtx = avcodec_alloc_context3(m_avCodec);

    if (!m_codecCtx)
    {
        qWarning() << "Can't alloc codec context";
        return false;
    }

    m_codecCtx->qmin = 16;
    m_codecCtx->qmax = m_codec.m_qpMax;
    m_codecCtx->width = m_codec.m_width;
    m_codecCtx->height = m_codec.m_height;
    m_codecCtx->gop_size = 10 /*m_codec.H264().m_keyFrameInterval*/;
    m_codecCtx->time_base.den = 1;
    m_codecCtx->time_base.num = m_codec.m_frameRate;
    m_codecCtx->framerate.den = 1;
    m_codecCtx->framerate.num = m_codec.m_frameRate;
    m_codecCtx->max_b_frames = 1;
    // TODO(hcb): 根据编码器而定, QSV->NV12
    m_codecCtx->pix_fmt = AV_PIX_FMT_YUV420P;

    if (m_avCodec->id == AV_CODEC_ID_H264)
        av_opt_set(m_codecCtx->priv_data, "preset", "slow", 0);

    int ret = avcodec_open2(m_codecCtx, m_avCodec, NULL);

    if (ret < 0)
    {
        qWarning() << "Can't open codec: " << ret;
    }

    m_avFrame = av_frame_alloc();
    if (!m_avFrame)
    {
        qWarning() << "Can't allocate video frame";
        return false;
    }

    m_avFrame->format = m_codecCtx->pix_fmt;
    m_avFrame->width = m_codecCtx->width;
    m_avFrame->height = m_codecCtx->height;
    ret = av_frame_get_buffer(m_avFrame, 0);
    if (ret < 0)
    {
        qWarning() << "Can't allocate the video frame data";
        return false;
    }

    // Make sure the frame data is writable.
    ret = av_frame_make_writable(m_avFrame);
    if (ret < 0)
    {
        qWarning() << "Can't make avFrame writable!";
        return false;
    }

    m_packet = av_packet_alloc();
    if (!m_packet)
    {
        qWarning() << "Can't allocate video packet";
        return false;
    }

    return true;
}

void H264Encoder::release()
{
    if (m_codecCtx)
    {
        avcodec_close(m_codecCtx);
        avcodec_free_context(&m_codecCtx);
        m_codecCtx = nullptr;
    }

    if (m_hwDeviceCtx)
    {
        av_buffer_unref(&m_hwDeviceCtx);
        m_hwDeviceCtx = nullptr;
    }

    if (m_avFrame)
    {
        av_frame_free(&m_avFrame);
        m_avFrame = nullptr;
    }

    if (m_hwFrame)
    {
        av_frame_free(&m_hwFrame);
        m_hwFrame = nullptr;
    }

    if (m_packet)
    {
        av_packet_free(&m_packet);
        m_packet = nullptr;
    }

    while (!m_inFrames.empty())
        m_inFrames.pop();
}

void H264Encoder::encode(const VideoFrame &frame)
{
    if (!m_isStarting)
        return;
    std::lock_guard<std::mutex> locker(m_frameMutex);
    m_inFrames.emplace(frame);
    m_cv.notify_one();
}

void H264Encoder::registerEncodeCompleteCallback(EncodedImageCallback *callback)
{
    m_encodedImageCallback = callback;
}

void H264Encoder::start()
{
    m_isStarting = true;

    m_thread.reset(new std::thread([&]() { threadFunc(); }));
}

void H264Encoder::stop()
{
    m_isStarting = false;
    m_cv.notify_all();

    if (m_thread)
        m_thread.get()->join();

    release();
}

bool H264Encoder::isEncoderSupport(const char *encoderName)
{
    return false;
}

void H264Encoder::encodeInternal(const AVFrame *frame)
{
    int ret = -1;
    ret = avcodec_send_frame(m_codecCtx, frame);
    if (ret < 0)
    {
        qWarning() << "Error sending a frame for encoding";
        return;
    }

    while (ret >= 0)
    {
        ret = avcodec_receive_packet(m_codecCtx, m_packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            break;
        }
        else if (ret < 0)
        {
            qWarning() << "Error during encoding";
            return;
        }

        auto encodedBuffer =
            EncodedImageBuffer::create(m_packet->data, m_packet->size);
        m_encodedImage.setEncodedBuffer(encodedBuffer);
        m_encodedImage.setWidth(m_codecCtx->width);
        m_encodedImage.setHeight(m_codecCtx->height);
        if (m_encodedImageCallback)
            m_encodedImageCallback->onEncodedImage(m_encodedImage);

        av_packet_unref(m_packet);
    }
}

void H264Encoder::threadFunc()
{
    while (m_isStarting)
    {
        std::unique_lock<std::mutex> locker(m_frameMutex);
        m_cv.wait(
            locker, [this]() { return !m_inFrames.empty() || !m_isStarting; });

        if (!m_inFrames.empty())
        {
            VideoFrame frame = m_inFrames.front();
            m_inFrames.pop();
            auto i420Buffer = frame.videoFrameBuffer()->toI420();
            if (!i420Buffer)
                continue;

            m_avFrame->pts = ++m_pts;
            memcpy(
                m_avFrame->data[0], i420Buffer->dataY(),
                i420Buffer->strideY() * i420Buffer->height());
            memcpy(
                m_avFrame->data[1], i420Buffer->dataU(),
                i420Buffer->strideU() * i420Buffer->chromaHeight());
            memcpy(
                m_avFrame->data[2], i420Buffer->dataV(),
                i420Buffer->strideV() * i420Buffer->chromaHeight());

            encodeInternal(m_avFrame);
        }
    }

    // flush the encoder
    encodeInternal(nullptr);
}
