#ifndef VIDEOFRAMEBUFFER_H
#define VIDEOFRAMEBUFFER_H

#include <memory>

class I420BufferInterface;
// class I420ABufferInterface;
// class I422BufferInterface;
// class I444BufferInterface;
// class I010BufferInterface;
// class I210BufferInterface;
// class NV12BufferInterface;

class VideoFrameBuffer
{
public:
    enum class Type
    {
        kNative,
        kI420,
        kI420A,
        kI422,
        kI444,
        kI010,
        kI210,
        kNV12,
    };

    // This function specifies in what pixel format the data is stored in.
    virtual Type type() const = 0;

    // The resolution of the frame in pixels. For formats where some planes are
    // subsampled, this is the highest-resolution plane.
    virtual int width() const = 0;
    virtual int height() const = 0;

    // Returns a memory-backed frame buffer in I420 format. If the pixel data is
    // in another format, a conversion will take place. All implementations must
    // provide a fallback to I420 for compatibility with e.g. the internal
    // WebRTC software encoders. Conversion may fail, for example if reading the
    // pixel data from a texture fails. If the conversion fails, nullptr is
    // returned.
    virtual std::shared_ptr<I420BufferInterface> toI420() = 0;

    // getI420() methods should return I420 buffer if conversion is trivial, i.e
    // no change for binary data is needed. Otherwise these methods should
    // return nullptr. One example of buffer with that property is
    // WebrtcVideoFrameAdapter in Chrome - it's I420 buffer backed by a shared
    // memory buffer. Therefore it must have type kNative. Yet, toI420()
    // doesn't affect binary data at all. Another example is any I420A buffer.
    // TODO(https://crbug.com/webrtc/12021): Make this method non-virtual and
    // behave as the other GetXXX methods below.
    virtual const I420BufferInterface *getI420() const;

    // These functions should only be called if type() is of the correct type.
    // Calling with a different type will result in a crash.
    // const I420ABufferInterface *GetI420A() const;
    // const I422BufferInterface *GetI422() const;
    // const I444BufferInterface *GetI444() const;
    // const I010BufferInterface *GetI010() const;
    // const I210BufferInterface *GetI210() const;
    // const NV12BufferInterface *GetNV12() const;

    virtual ~VideoFrameBuffer() {}
};

// This interface represents planar formats.
class PlanarYuvBuffer : public VideoFrameBuffer
{
public:
    virtual int chromaWidth() const = 0;
    virtual int chromaHeight() const = 0;

    // Returns the number of steps(in terms of Data*() return type) between
    // successive rows for a given plane.
    virtual int strideY() const = 0;
    virtual int strideU() const = 0;
    virtual int strideV() const = 0;

    virtual ~PlanarYuvBuffer() override {}
};

// This interface represents 8-bit color depth formats: Type::kI420,
// Type::kI420A, Type::kI422 and Type::kI444.
class PlanarYuv8Buffer : public PlanarYuvBuffer
{
public:
    // Returns pointer to the pixel data for a given plane. The memory is owned
    // by the VideoFrameBuffer object and must not be freed by the caller.
    virtual const uint8_t *dataY() const = 0;
    virtual const uint8_t *dataU() const = 0;
    virtual const uint8_t *dataV() const = 0;

    virtual ~PlanarYuv8Buffer() override {}
};

class I420BufferInterface
    : public PlanarYuv8Buffer,
      public std::enable_shared_from_this<I420BufferInterface>
{
public:
    virtual Type type() const override;

    virtual int chromaWidth() const final;
    virtual int chromaHeight() const final;

    virtual std::shared_ptr<I420BufferInterface> toI420() final;
    virtual const I420BufferInterface *getI420() const final;

    virtual ~I420BufferInterface() override {}
};

#endif  // VIDEOFRAMEBUFFER_H
