#ifndef VIDEORECORDER_H
#define VIDEORECORDER_H

#include <QString>
//#include <QFile>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "videoencoder.h"

class VideoRecorder : public EncodedImageCallback
{
public:
    VideoRecorder();
    ~VideoRecorder();

    bool onEncodedImage(const EncodedImage &encodedImage) override;
    bool onEncodedImage(AVPacket *packet) override;
    void setRecordParams(
        const char *filePath, const AVCodecContext *encodeCtx, int fps);
    void setFilePath(const QString &filePath);
    void start();
    void stop();
    void pause();

private:
    bool m_isStarting;
    //    QString m_filePath;
    //    QFile m_file;
    AVFormatContext *m_outPutFmtCtx;
    AVStream *m_outStream;
    AVPacket *m_outPacket;
    AVRational m_encodeTimeBase;
};

#endif  // VIDEORECORDER_H
