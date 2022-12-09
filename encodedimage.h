#ifndef ENCODEDIMAGE_H
#define ENCODEDIMAGE_H

#include <stdint.h>

#include <memory>

class EncodedImageBufferInterface
{
public:
    virtual const uint8_t *data() const = 0;
    virtual size_t size() const = 0;

    virtual ~EncodedImageBufferInterface() {}
};

// Basic implementation of EncodedImageBufferInterface.
class EncodedImageBuffer : public EncodedImageBufferInterface
{
public:
    static std::shared_ptr<EncodedImageBuffer> create() { return create(0); }
    static std::shared_ptr<EncodedImageBuffer> create(size_t size);
    static std::shared_ptr<EncodedImageBuffer> create(
        const uint8_t *data, size_t size);

    const uint8_t *data() const override;
    // uint8_t *data() override;
    size_t size() const override;
    void reallocBuffer(size_t size);

    // protected:
    //  Not recommend use, prefer use create()
    explicit EncodedImageBuffer(size_t size);
    EncodedImageBuffer(const uint8_t *data, size_t size);
    ~EncodedImageBuffer() override;

    size_t m_size;
    uint8_t *m_buffer;
};

class EncodedImage
{
public:
    EncodedImage() = default;
    EncodedImage(EncodedImage &&) = default;
    EncodedImage(const EncodedImage &) = default;

    ~EncodedImage() = default;

    EncodedImage &operator=(EncodedImage &&) = default;
    EncodedImage &operator=(const EncodedImage &) = default;

    void setTimestamp(int64_t timestamp);
    int64_t timestamp() const;

    void setStreamIndex(int index);
    int streamIndex() const;

    void setSize(size_t new_size);
    size_t size() const;

    void setEncodedBuffer(std::shared_ptr<EncodedImageBufferInterface> buffer);
    std::shared_ptr<EncodedImageBufferInterface> encodedBuffer() const;
    void clearEncodedBuffer();

    const uint8_t *data() const;

    void setWidth(int width);
    void setHeight(int height);

    int width() const;
    int height() const;

private:
    size_t capacity();

    std::shared_ptr<EncodedImageBufferInterface> m_encodedBuffer;
    // buffer size
    size_t m_size;
    int m_streamIndex;
    int64_t m_timestampUs;
    int m_encodedWidth;
    int m_encodedHeight;
};

#endif  // ENCODEDIMAGE_H
