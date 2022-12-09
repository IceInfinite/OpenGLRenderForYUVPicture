#ifndef VIDEOCODEC_H
#define VIDEOCODEC_H

#include <stdint.h>

enum class VideoCodecType
{
    kVideoCodecMJPEG = 0,
    kVideoCodecH264,
};

struct VideoCodecH264
{
    bool operator==(const VideoCodecH264 &other) const;
    bool operator!=(const VideoCodecH264 &other) const;

    int m_keyFrameInterval;
};

struct VideoCodecMJPEG
{
    // todo
};

union VideoCodecUnion
{
    VideoCodecH264 h264;
    VideoCodecMJPEG mjpeg;
};

class VideoCodec
{
public:
    VideoCodec();

    int m_width;
    int m_height;
    VideoCodecType m_codecType;
    uint32_t m_frameRate;
    uint32_t m_targetBitrate;

    uint32_t m_qpMax;

    bool m_frameDropEnabled;

    const VideoCodecMJPEG &MJPEG() const;
    const VideoCodecH264 &H264() const;

private:
    VideoCodecUnion m_codecSpecific;
};

#endif  // VIDEOCODEC_H
