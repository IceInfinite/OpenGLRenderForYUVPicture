#ifndef I420BUFFER_H
#define I420BUFFER_H

#include <memory>

#include "alignedmalloc.h"

#include "videoframebuffer.h"

class I420Buffer : public I420BufferInterface
{
public:
    static std::shared_ptr<I420Buffer> create(int width, int height);
    static std::shared_ptr<I420Buffer> create(
        int width, int height, int strideY, int strideU, int strideV);
    // create a new buffer and copy the pixel data.
    //static std::shared_ptr<I420Buffer> Copy(
    //    const I420BufferInterface &source);

    //static std::shared_ptr<I420Buffer> Copy(
    //    int width, int height, const uint8_t *dataY, int strideY,
    //    const uint8_t *dataU, int strideU, const uint8_t *dataV, int strideV);

    // Sets the buffer to all black.
    //static void SetBlack(I420Buffer *buffer);

    I420Buffer(int width, int height);
    I420Buffer(int width, int height, int strideY, int strideU, int strideV);
    ~I420Buffer() override;

    void initializeData();

    int width() const override;
    int height() const override;
    // Data size
    size_t size() const;
    const uint8_t *dataY() const override;
    const uint8_t *dataU() const override;
    const uint8_t *dataV() const override;

    int strideY() const override;
    int strideU() const override;
    int strideV() const override;

    uint8_t *mutableDataY();
    uint8_t *mutableDataU();
    uint8_t *mutableDataV();


private:
    const int m_width;
    const int m_height;
    const size_t m_size;
    const int m_strideY;
    const int m_strideU;
    const int m_strideV;
    const std::unique_ptr<uint8_t, AlignedFreeDeleter> m_data;
};

#endif  // I420BUFFER_H
