#include "videoframe.h"

#include <assert.h>

VideoFrame::VideoFrame(
    const std::shared_ptr<VideoFrameBuffer> &buffer, int64_t timestampUs)
    : m_videoFrameBuffer(buffer), m_timestampUs(timestampUs)
{
}

VideoFrame::~VideoFrame() {}

int VideoFrame::width() const
{
    return m_videoFrameBuffer ? m_videoFrameBuffer->width() : 0;
}

int VideoFrame::height() const
{
    return m_videoFrameBuffer ? m_videoFrameBuffer->height() : 0;
}

VideoFrameBuffer::Type VideoFrame::type() const
{
    return m_videoFrameBuffer->type();
}

uint32_t VideoFrame::size() const
{
    return height() * width();
}

int64_t VideoFrame::timestampUs() const
{
    return m_timestampUs;
}

void VideoFrame::setTimestampUs(int64_t timestampUs)
{
    m_timestampUs = timestampUs;
}

std::shared_ptr<VideoFrameBuffer> VideoFrame::videoFrameBuffer() const
{
    return m_videoFrameBuffer;
}

void VideoFrame::setVideoFrameBuffer(
    const std::shared_ptr<VideoFrameBuffer> &buffer)
{
    assert(buffer);
    m_videoFrameBuffer = buffer;
}
