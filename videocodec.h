#ifndef VIDEOCODEC_H
#define VIDEOCODEC_H

#include <QString>

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
    int m_frameRate;
    int m_targetBitrate;

    int m_qpMin;
    int m_qpMax;
    int m_qp;
    float m_crf;

    bool m_frameDropEnabled;

    VideoCodecMJPEG *MJPEG();
    const VideoCodecMJPEG &MJPEG() const;

    VideoCodecH264 *H264();
    const VideoCodecH264 &H264() const;

    QString toString();

private:
    VideoCodecUnion m_codecSpecific;
};

#endif  // VIDEOCODEC_H
