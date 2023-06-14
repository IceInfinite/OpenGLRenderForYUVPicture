#include "encodedimage.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

std::shared_ptr<EncodedImageBuffer> EncodedImageBuffer::create(size_t size)
{
    return std::make_shared<EncodedImageBuffer>(size);
}

std::shared_ptr<EncodedImageBuffer> EncodedImageBuffer::create(
    const uint8_t *data, size_t size)
{
    return std::make_shared<EncodedImageBuffer>(data, size);
}

const uint8_t *EncodedImageBuffer::data() const
{
    return m_buffer;
}

size_t EncodedImageBuffer::size() const
{
    return m_size;
}

void EncodedImageBuffer::reallocBuffer(size_t size)
{
    assert(size > 0);
    m_buffer = static_cast<uint8_t *>(realloc(m_buffer, size));
    m_size = size;
}

EncodedImageBuffer::EncodedImageBuffer(size_t size)
{
    m_buffer = static_cast<uint8_t *>(malloc(size));
    m_size = size;
}

EncodedImageBuffer::EncodedImageBuffer(const uint8_t *data, size_t size)
    : EncodedImageBuffer(size)
{
    memcpy(m_buffer, data, size);
}

EncodedImageBuffer::~EncodedImageBuffer()
{
    free(m_buffer);
}

EncodedImage::EncodedImage()
    : m_streamIndex(0),
      m_encodedWidth(0),
      m_encodedHeight(0),
      m_isKeyFrame(false),
      m_pts(0),
      m_encodedBuffer(nullptr),
      m_size(0)
{
}

size_t EncodedImage::size() const
{
    return m_size;
}

void EncodedImage::setEncodedBuffer(
    std::shared_ptr<EncodedImageBufferInterface> buffer)
{
    m_encodedBuffer = buffer;
    m_size = buffer->size();
}

std::shared_ptr<EncodedImageBufferInterface> EncodedImage::encodedBuffer() const
{
    return m_encodedBuffer;
}

void EncodedImage::clearEncodedBuffer()
{
    m_encodedBuffer = nullptr;
    m_size = 0;
}

const uint8_t *EncodedImage::data() const
{
    return m_encodedBuffer ? m_encodedBuffer->data() : nullptr;
}

void EncodedImage::setSize(size_t newSize)
{
    // Allow set_size(0) even if we have no buffer.
    assert(newSize <= (newSize == 0 ? 0 : capacity()));
    m_size = newSize;
}

size_t EncodedImage::capacity()
{
    return m_encodedBuffer ? m_encodedBuffer->size() : 0;
}
