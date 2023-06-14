#include "videorecorder.h"

#include <QDebug>

#include "h264encoder.h"

VideoRecorder::VideoRecorder()
    : m_isStarting(false),
      m_width(0),
      m_height(0),
      m_outPutFmtCtx(nullptr),
      m_outStream(nullptr),
      m_outPacket(nullptr),
      m_encodeTimeBase({0, 0}),
      m_encoder(nullptr)
{
}

VideoRecorder::~VideoRecorder()
{
    if (m_outPacket)
        av_packet_free(&m_outPacket);

    if (m_outPutFmtCtx)
        avformat_free_context(m_outPutFmtCtx);
}

void VideoRecorder::onFrame(const VideoFrame &frame)
{
    if (!m_isStarting || !m_encoder)
        return;

    if (m_width != frame.width() || m_height != frame.height())
    {
        // TODO(hcb):rescale frame to dest resolution
    }
    else
    {
        m_encoder->encode(frame);
    }
}

bool VideoRecorder::onEncodedImage(const EncodedImage &encodedImage)
{
    if (!m_isStarting)
    {
        qWarning()
            << "Could not save encoded image, recorder have not started!";
        return false;
    }

    // 分辨率切换后，不再写入新分辨率的帧
    // TODO(hcb):将帧缩放至所需分辨率
    if (m_width != encodedImage.m_encodedWidth ||
        m_height != encodedImage.m_encodedHeight)
        return false;

    // TODO(hcb):check availbale space
    m_outPacket->stream_index = encodedImage.m_streamIndex;
    m_outPacket->pts = encodedImage.m_pts;
    m_outPacket->data = const_cast<uint8_t *>(encodedImage.data());
    m_outPacket->size = static_cast<int>(encodedImage.size());
    if (encodedImage.m_isKeyFrame)
        m_outPacket->flags |= AV_PKT_FLAG_KEY;

    av_packet_rescale_ts(m_outPacket, m_encodeTimeBase, m_outStream->time_base);
    int ret = av_interleaved_write_frame(m_outPutFmtCtx, m_outPacket);
    if (ret < 0)
    {
        qWarning() << "Write frame to file failed!";
        return false;
    }

    return true;
}

bool VideoRecorder::onEncodedImage(AVPacket *packet, int width, int height)
{
    if (!m_isStarting)
    {
        qWarning()
            << "Could not save encoded image, recorder have not started!";
        return false;
    }

    // 分辨率切换后，不再写入新分辨率的帧
    // TODO(hcb):将帧缩放至所需分辨率
    if (m_width != width || m_height != height)
        return false;

    // TODO(hcb):check availbale space
    av_packet_rescale_ts(packet, m_encodeTimeBase, m_outStream->time_base);
    int ret = av_interleaved_write_frame(m_outPutFmtCtx, packet);
    if (ret < 0)
    {
        qWarning() << "Write frame to file failed!";
        return false;
    }

    return true;
}

void VideoRecorder::initRecorder(const AVCodecContext *encodeCtx)
{
    m_isStarting = false;
    m_width = encodeCtx->width;
    m_height = encodeCtx->height;

    if (m_saveFilePath.isEmpty())
    {
        qWarning() << "initRecorder failed, invalid save file path!";
        return;
    }
    std::string stdFilePath = m_saveFilePath.toStdString();
    const char *filePath = stdFilePath.c_str();

    int ret = -1;
    ret = avformat_alloc_output_context2(
        &m_outPutFmtCtx, nullptr, nullptr, filePath);
    if (ret < 0)
    {
        qWarning() << "Alloc output context failed!";
        return;
    }

    m_outStream = avformat_new_stream(m_outPutFmtCtx, nullptr);
    if (!m_outStream)
    {
        qWarning() << "Output stream create failed!";
        return;
    }
    m_encodeTimeBase = encodeCtx->time_base;
    m_outStream->time_base = m_encodeTimeBase;
    ret = avcodec_parameters_from_context(m_outStream->codecpar, encodeCtx);
    if (ret < 0)
    {
        qWarning() << "Get codec params from encode context failed!";
        return;
    }
}

void VideoRecorder::setInternalEncoder(const VideoCodec &codec)
{
    m_isStarting = false;
    if (codec.m_codecType != VideoCodecType::kVideoCodecH264)
    {
        qWarning() << "Now only support h264 internal encoder";
        return;
    }

    m_encoder.reset(new H264Encoder());
    if (!m_encoder->initEncoder(codec))
    {
        qWarning() << "Internal encoder init failed";
        return;
    }
    initRecorder(m_encoder->encodeContext());
    m_encoder->registerEncodeCompleteCallback(this);
}

void VideoRecorder::setSaveFilePath(const QString &filePath)
{
    m_saveFilePath = filePath;
}

void VideoRecorder::start()
{
    if (m_isStarting)
    {
        qWarning() << "Video recorder is already started";
        return;
    }
    if (!m_outPutFmtCtx)
        return;

    if (m_saveFilePath.isEmpty())
    {
        qWarning() << "Start failed, invalid save file path!";
        return;
    }
    std::string stdFilePath = m_saveFilePath.toStdString();
    const char *filePath = stdFilePath.c_str();

    int ret = -1;
    ret = avio_open2(
        &m_outPutFmtCtx->pb, filePath, AVIO_FLAG_WRITE,
        &m_outPutFmtCtx->interrupt_callback, nullptr);
    if (ret < 0)
    {
        qWarning() << "Open output video file failed";
        return;
    }

    ret = avformat_write_header(m_outPutFmtCtx, nullptr);
    if (ret < 0)
    {
        qWarning() << "Write output video file header failed!";
        return;
    }

    m_outPacket = av_packet_alloc();
    if (!m_outPacket)
    {
        qWarning() << "Output Packet alloc failed";
    }

    if (m_encoder)
        m_encoder->start();

    m_isStarting = true;
}

void VideoRecorder::stop()
{
    if (m_encoder)
    {
        m_encoder->stop();
        m_encoder->release();
    }

    if (m_isStarting)
    {
        av_write_trailer(m_outPutFmtCtx);
        avio_close(m_outPutFmtCtx->pb);
    }
    m_isStarting = false;
}

void VideoRecorder::pause()
{
    m_isStarting = false;
}
