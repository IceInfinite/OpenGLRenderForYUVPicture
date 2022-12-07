#ifndef VIDEOFRAME_H
#define VIDEOFRAME_H

#include <stdint.h>

#include <memory>

//#include <QMetaType>

#include "i420buffer.h"
#include "videoframebuffer.h"

class VideoFrame
{
public:
    VideoFrame(
        const std::shared_ptr<VideoFrameBuffer> &buffer, int64_t timestampUs = 0);
    ~VideoFrame();

    VideoFrame(const VideoFrame &) = default;
    VideoFrame(VideoFrame &&) = default;
    VideoFrame &operator=(const VideoFrame &) = default;
    VideoFrame &operator=(VideoFrame &&) = default;

    int width() const;
    int height() const;
    VideoFrameBuffer::Type type() const;
    // Get frame size in pixels.
    uint32_t size() const;

    // System monotonic clock, same timebase as rtc::TimeMicros().
    int64_t timestampUs() const;
    void setTimestampUs(int64_t timestampUs);

    std::shared_ptr<VideoFrameBuffer> videoFrameBuffer() const;

    void setVideoFrameBuffer(const std::shared_ptr<VideoFrameBuffer> &buffer);

    // Return true if the frame is stored in a texture.
    bool isTexture() const
    {
        return videoFrameBuffer()->type() == VideoFrameBuffer::Type::kNative;
    }

private:
    std::shared_ptr<VideoFrameBuffer> m_videoFrameBuffer;
    int64_t m_timestampUs; // pts
    // uint16_t m_id;
    // color space
    // rotation
};

//Q_DECLARE_METATYPE(std::shared_ptr<VideoFrame>);

#endif  // VIDEOFRAME_H
