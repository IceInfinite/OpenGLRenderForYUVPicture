#include "videoframebuffer.h"

#include "i420buffer.h"

const I420BufferInterface *VideoFrameBuffer::getI420() const
{
    return nullptr;
}

VideoFrameBuffer::Type I420BufferInterface::type() const
{
    return Type::kI420;
}

int I420BufferInterface::chromaWidth() const
{
    return (width() + 1) / 2;
}

int I420BufferInterface::chromaHeight() const
{
    return (height() + 1) / 2;
}

std::shared_ptr<I420BufferInterface> I420BufferInterface::toI420()
{
    return shared_from_this();
}

const I420BufferInterface *I420BufferInterface::getI420() const
{
    return this;
}
