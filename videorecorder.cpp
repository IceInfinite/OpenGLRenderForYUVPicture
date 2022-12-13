#include <QDebug>

#include "videorecorder.h"

VideoRecorder::VideoRecorder()
    : m_isStarting(false),
      m_outPutFmtCtx(nullptr),
      m_outStream(nullptr),
      m_outPacket(nullptr)
{
}

VideoRecorder::~VideoRecorder()
{
    if (m_outPacket)
        av_packet_free(&m_outPacket);
    if (m_outPutFmtCtx)
        avformat_free_context(m_outPutFmtCtx);
}

bool VideoRecorder::onEncodedImage(const EncodedImage &encodedImage)
{
    if (!m_outPacket || !m_outPutFmtCtx)
    {
        qWarning() << "Set record params failed, could not save encoded image!";
        return false;
    }
    // TODO(hcb): put packet sidedata to encodedImage
    m_outPacket->stream_index = encodedImage.m_streamIndex;
    m_outPacket->pts = encodedImage.m_pts;
    m_outPacket->dts = encodedImage.m_dts;
    m_outPacket->data = const_cast<uint8_t *>(encodedImage.data());

    av_packet_rescale_ts(m_outPacket, m_encodeTimeBase, m_outStream->time_base);
    int ret = av_interleaved_write_frame(m_outPutFmtCtx, m_outPacket);
    if (ret < 0)
    {
        qWarning() << "Write frame to file failed!";
        return false;
    }

    return true;
}

bool VideoRecorder::onEncodedImage(AVPacket *packet)
{
    if (!packet || !m_outPutFmtCtx)
    {
        qWarning() << "Set record params failed, could not save encoded image!";
        return false;
    }
    av_packet_rescale_ts(packet, m_encodeTimeBase, m_outStream->time_base);
    int ret = av_interleaved_write_frame(m_outPutFmtCtx, packet);
    if (ret < 0)
    {
        qWarning() << "Write frame to file failed!";
        return false;
    }

    return true;
}

void VideoRecorder::setRecordParams(
    const char *filePath, const AVCodecContext *encodeCtx, int fps)
{
    int ret = -1;
    ret = avformat_alloc_output_context2(&m_outPutFmtCtx, NULL, NULL, filePath);
    if (ret < 0)
    {
        qWarning() << "Alloc output context failed!";
        return;
    }

    m_outStream = avformat_new_stream(m_outPutFmtCtx, NULL);
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

    // TODO(hcb): 移入start()流程中
    ret = avio_open2(
        &m_outPutFmtCtx->pb, filePath, AVIO_FLAG_WRITE,
        &m_outPutFmtCtx->interrupt_callback, NULL);
    if (ret < 0)
    {
        qWarning() << "Open output video file failed";
        return;
    }
}

void VideoRecorder::setFilePath(const QString &filePath)
{
    //    m_filePath = filePath;
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

    int ret = avformat_write_header(m_outPutFmtCtx, NULL);
    if (ret < 0)
    {
        qWarning() << "Write output video file header failed!";
        return;
    }

    m_outPacket = av_packet_alloc();
    m_isStarting = true;

}

void VideoRecorder::stop()
{
    if (m_isStarting && m_outPutFmtCtx)
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
