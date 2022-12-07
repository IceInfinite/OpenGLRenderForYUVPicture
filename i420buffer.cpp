#include "i420buffer.h"

#include <assert.h>

// Aligning pointer to 64 bytes for improved performance, e.g. use SIMD.
static const int kBufferAlignment = 64;

namespace
{
size_t I420DataSize(int height, int stride_y, int stride_u, int stride_v)
{
    assert(height >= 0);
    assert(stride_y >= 0);
    assert(stride_u >= 0);
    assert(stride_v >= 0);

    return static_cast<size_t>(
        stride_y * height + (stride_u + stride_v) * ((height + 1) / 2));
}

}  // namespace

I420Buffer::I420Buffer(int width, int height)
    : I420Buffer(width, height, width, (width + 1) / 2, (width + 1) / 2)
{
}

I420Buffer::I420Buffer(
    int width, int height, int strideY, int strideU, int strideV)
    : m_width(width),
      m_height(height),
      m_size(I420DataSize(height, strideY, strideU, strideV)),
      m_strideY(strideY),
      m_strideU(strideU),
      m_strideV(strideV),
      m_data(static_cast<uint8_t *>(alignedMalloc(m_size, kBufferAlignment)))
{
    assert(width > 0);
    assert(height > 0);
    assert(strideY >= width);
    assert(strideU >= (height + 1) / 2);
    assert(strideV >= (height + 1) / 2);
}

I420Buffer::~I420Buffer() {}

// static 
std::shared_ptr<I420Buffer> I420Buffer::create(int width, int height)
{
    return std::make_shared<I420Buffer>(width, height);
}

// static
std::shared_ptr<I420Buffer> I420Buffer::create(
    int width, int height, int strideY, int strideU, int strideV)
{
    return std::make_shared<I420Buffer>(
        width, height, strideY, strideU, strideV);
}

// static
// std::shared_ptr<I420Buffer> I420Buffer::Copy(const I420BufferInterface
// &source)
//{
//    return Copy(
//        source.width(), source.height(), source.dataY(), source.strideY(),
//        source.dataU(), source.strideU(), source.dataV(), source.strideV());
//}

// static
// std::shared_ptr<I420Buffer> I420Buffer::Copy(
//    int width, int height, const uint8_t *dataY, int strideY,
//    const uint8_t *dataU, int strideU, const uint8_t *dataV, int strideV)
//{
//    std::shared_ptr<I420Buffer> buffer = create(width, height);
//
//    assert(
//        0 == libyuv::I420Copy(
//                 dataY, strideY, dataU, strideU, dataV, strideV,
//                 buffer->mutableDataY(), buffer->strideY(),
//                 buffer->mutableDataU(), buffer->strideU(),
//                 buffer->mutableDataV(), buffer->strideV(), width, height));
//
//    return buffer;
//}

// static
// void I420Buffer::SetBlack(I420Buffer *buffer) {}

void I420Buffer::initializeData()
{
    memset(m_data.get(), 0, m_size);
}

int I420Buffer::width() const
{
    return m_width;
}

int I420Buffer::height() const
{
    return m_height;
}

size_t I420Buffer::size() const
{
    return m_size;
}

const uint8_t *I420Buffer::dataY() const
{
    return m_data.get();
}

const uint8_t *I420Buffer::dataU() const
{
    return m_data.get() + m_strideY * m_height;
}

const uint8_t *I420Buffer::dataV() const
{
    return m_data.get() + m_strideY * m_height +
           m_strideU * ((m_height + 1) / 2);
}

int I420Buffer::strideY() const
{
    return m_strideY;
}

int I420Buffer::strideU() const
{
    return m_strideU;
}

int I420Buffer::strideV() const
{
    return m_strideV;
}

uint8_t *I420Buffer::mutableDataY()
{
    return const_cast<uint8_t *>(dataY());
}

uint8_t *I420Buffer::mutableDataU()
{
    return const_cast<uint8_t *>(dataU());
}

uint8_t *I420Buffer::mutableDataV()
{
    return const_cast<uint8_t *>(dataV());
}
