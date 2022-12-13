#ifndef VIDEOENCODER_H
#define VIDEOENCODER_H

#include <memory>

extern "C"
{
#include <libavcodec/avcodec.h>
}

#include "encodedimage.h"
#include "videocodec.h"
#include "videoframe.h"

class EncodedImageCallback
{
public:
    virtual ~EncodedImageCallback() {}
    // Callback function which is called when an image has been encoded.
    virtual bool onEncodedImage(
        const EncodedImage &encodedImage
        /*const CodecSpecificInfo* codec_specific_info*/) = 0;
    virtual bool onEncodedImage(/* const */ AVPacket *packet) = 0;
};

class VideoEncoder
{
public:
    virtual ~VideoEncoder(){};

    // Initialize the encoder with the information from the codecSettings
    virtual bool initEncoder(const VideoCodec &codec) = 0;

    virtual void start() = 0;

    virtual void stop() = 0;

    virtual AVCodecContext *encodeContext() { return nullptr; }
    // Encode an image (as a part of a video stream). The encoded image
    // will be returned to the user through the encode complete callback.
    virtual void encode(const VideoFrame &frame) = 0;

    // Free encoder memory.
    virtual void release() = 0;

    // Register an encode complete callback object.
    virtual void registerEncodeCompleteCallback(
        EncodedImageCallback *callback) = 0;
};

#endif  // VIDEOENCODER_H
