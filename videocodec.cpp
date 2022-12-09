#include "videocodec.h"

#include <assert.h>

VideoCodec::VideoCodec()
    : m_width(0),
      m_height(0),
      m_codecType(VideoCodecType::kVideoCodecH264),
      m_frameRate(0),
      m_targetBitrate(0),
      m_qpMax(0),
      m_frameDropEnabled(false)
{
}

const VideoCodecMJPEG &VideoCodec::MJPEG() const
{
    assert(m_codecType == VideoCodecType::kVideoCodecMJPEG);
    return m_codecSpecific.mjpeg;
}

const VideoCodecH264 &VideoCodec::H264() const
{
    assert(m_codecType == VideoCodecType::kVideoCodecH264);
    return m_codecSpecific.h264;
}

bool VideoCodecH264::operator==(const VideoCodecH264 &other) const
{
    return (m_keyFrameInterval == other.m_keyFrameInterval);
}

bool VideoCodecH264::operator!=(const VideoCodecH264 &other) const
{
    return !(*this == other);
}
