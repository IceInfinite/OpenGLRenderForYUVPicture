#ifndef VIDEORECORDER_H
#define VIDEORECORDER_H

#include <memory>

#include <QString>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "videoencoder.h"
#include "videoframe.h"

//  Example
// 1. Use Internal Encoder
//    VideoRecorder recorder;
//    VideoCodec codec;
//    ... set codec ...
//    recoder.setSaveFilePath(path);
//    recoder.setInternalEncoder(codec);
//    reocder.start();
//    while(get(frame))
//      reocder.onFrame(frame);
//    recorder.stop();
//
// 2. Use External Encoder
//    VideoRecorder recorder;
//    VideoEncoder encoder;
//    VideoCodec codec;
//    ... set codec ...
//    encoder.initEncoder(codec);
//    encoder.registerEncodeCompleteCallback(&recoder);
//    recoder.setSaveFilePath(path);
//    recoder.setRecordParams(encoder.encodeContext());
//    reocder.start();
//    encoder.start();
//    while(get(frame))
//      encoder.encode(frame);
//    encoder.stop();
//    recorder.stop();
//    encoder.registerEncodeCompleteCallback(nullptr);

class VideoRecorder : public EncodedImageCallback
{
public:
    VideoRecorder();
    ~VideoRecorder();

    // Decoded Frame
    void onFrame(const VideoFrame &frame);

    // Encoded Image
    bool onEncodedImage(const EncodedImage &encodedImage) override;
    bool onEncodedImage(AVPacket *packet, int width, int height) override;

    void setSaveFilePath(const QString &filePath);

    // Set this when use external encoder
    void initRecorder(const AVCodecContext *encodeCtx);

    void setInternalEncoder(const VideoCodec &codec);

    void start();
    void stop();
    void pause();

private:
    bool m_isStarting;
    QString m_saveFilePath;
    int m_width;
    int m_height;
    AVFormatContext *m_outPutFmtCtx;
    AVStream *m_outStream;
    AVPacket *m_outPacket;
    AVRational m_encodeTimeBase;

    // default h264 encoder
    std::unique_ptr<VideoEncoder> m_encoder;
};

#endif  // VIDEORECORDER_H
