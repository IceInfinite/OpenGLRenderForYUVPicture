#include "videocodec.h"

#include <assert.h>

#include <QTextStream>

VideoCodec::VideoCodec()
    : m_width(0),
      m_height(0),
      m_codecType(VideoCodecType::kVideoCodecH264),
      m_frameRate(0),
      m_targetBitrate(0),
      m_qpMin(0),
      m_qpMax(0),
      m_qp(0),
      m_crf(0.0),
      m_frameDropEnabled(false)
{
}

VideoCodecMJPEG *VideoCodec::MJPEG()
{
    assert(m_codecType == VideoCodecType::kVideoCodecMJPEG);
    return &m_codecSpecific.mjpeg;
}

const VideoCodecMJPEG &VideoCodec::MJPEG() const
{
    assert(m_codecType == VideoCodecType::kVideoCodecMJPEG);
    return m_codecSpecific.mjpeg;
}

VideoCodecH264 *VideoCodec::H264()
{
    assert(m_codecType == VideoCodecType::kVideoCodecH264);
    return &m_codecSpecific.h264;
}

const VideoCodecH264 &VideoCodec::H264() const
{
    assert(m_codecType == VideoCodecType::kVideoCodecH264);
    return m_codecSpecific.h264;
}

QString VideoCodec::toString()
{
    QString s;
    QTextStream ts(&s);
    ts << "Video Codec: height=" << m_height << ", width=" << m_width
       << ", fps=" << m_frameRate << ", bit_rate=" << m_targetBitrate
       << ", qp_min=" << m_qpMin << ", qp_max=" << m_qpMax
       << ", frame_drop=" << (m_frameDropEnabled ? "true" : "false")
       << ", type="
       << (m_codecType == VideoCodecType::kVideoCodecH264 ? "h264" : "other");
    return ts.readAll();
}

bool VideoCodecH264::operator==(const VideoCodecH264 &other) const
{
    return (m_keyFrameInterval == other.m_keyFrameInterval);
}

bool VideoCodecH264::operator!=(const VideoCodecH264 &other) const
{
    return !(*this == other);
}
