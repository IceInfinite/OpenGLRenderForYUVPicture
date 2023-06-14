#include <stdint.h>
#include <stdio.h>

#include <fstream>
#include <thread>

#include <QApplication>
#include <QDebug>
#include <QString>

#include "h264encoder.h"
#include "openglwidget.h"
#include "videoframe.h"
#include "videoframebuffer.h"
#include "videorecorder.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
}

static void readYuvPic(
    const char *picPath, int picWidth, int picHeight, VideoFrame &frame)
{
    std::ifstream picFile(picPath, std::ios::in | std::ios::binary);
    if (!picFile)
    {
        qDebug() << "Open yuv pic failed!";
        return;
    }

    auto buffer = I420Buffer::create(picWidth, picHeight);
    // read all pic data
    picFile.read(
        reinterpret_cast<char *>(buffer->mutableDataY()), buffer->size());

    frame = VideoFrame(buffer, 0);

    // free resource
    picFile.close();
}

// static void readYUVVideo(
//    const std::string &yuvFilePath, int width, int height, OpenGLWidget &w)
//{
//    const char mp4Path[] = "D:/test_video/john_test.mp4";
//    qDebug() << "File Path: " << yuvFilePath.c_str()
//             << ", save file path: " << mp4Path;
//    AVFormatContext *ofmt_ctx = nullptr;
//    AVCodecContext *enc_ctx = nullptr;
//    AVFrame *frame = nullptr;
//    AVPacket *pkt = nullptr;
//    int64_t pts_i = 0;
//    int ret = avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, mp4Path);
//    if (ret < 0)
//    {
//        qWarning("avformat_alloc_output_context2 fail");
//        return;
//    }
//    const AVCodec *enc = avcodec_find_encoder_by_name("libx264");
//    if (!dec)
//    {
//        qWarning("avcodec_find_encoder fail");
//        return;
//    }
//    enc_ctx = avcodec_alloc_context3(enc);
//    enc_ctx->width = width;
//    enc_ctx->height = height;
//    enc_ctx->framerate = av_make_q(60, 1);
//    enc_ctx->time_base = av_make_q(1, 60);
//    enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
//    enc_ctx->gop_size = 10;
//    enc_ctx->bit_rate = 8 * 1000 * 1000;
//    enc_ctx->max_b_frames = 0;
//    if (enc_ctx->codec_id == AV_CODEC_ID_H264)
//    {
//        av_opt_set(enc_ctx->priv_data, "preset", "veryfast", 0);
//    }
//
//    ret = avio_open(&ofmt_ctx->pb, mp4Path, AVIO_FLAG_WRITE);
//    if (ret < 0)
//    {
//        qWarning("avio_open fail");
//        return;
//    }
//    ret = avcodec_open2(enc_ctx, enc, NULL);
//    if (ret < 0)
//    {
//        qWarning("avcodec_open2 fail");
//        return;
//    }
//    AVStream *st = avformat_new_stream(ofmt_ctx, enc);
//    ret = avcodec_parameters_from_context(st->codecpar, enc_ctx);
//    if (ret < 0)
//    {
//        qWarning("avcodec_parameters_from_context fail");
//        return;
//    }
//    st->time_base = enc_ctx->time_base;
//
//    ret = avformat_write_header(ofmt_ctx, NULL);
//    if (ret < 0)
//    {
//        qWarning("avformat_write_header fail");
//        return;
//    }
//
//    frame = av_frame_alloc();
//    if (!frame)
//    {
//        qWarning("av_frame_alloc fail");
//        return;
//    }
//
//    frame->format = enc_ctx->pix_fmt;
//    frame->width = enc_ctx->width;
//    frame->height = enc_ctx->height;
//    ret = av_frame_get_buffer(frame, 0);
//    if (ret < 0)
//    {
//        qWarning("av_frame_get_buffer fail");
//        return;
//    }
//
//    pkt = av_packet_alloc();
//    if (!pkt)
//    {
//        qWarning("av_packet_alloc fail");
//        return;
//    }
//
//    FILE *yuvFile = fopen(yuvFilePath.c_str(), "rb+");
//    if (!yuvFile)
//    {
//        qDebug() << "Open YUV file failed";
//        return;
//    }
//
//    while (!feof(yuvFile))
//    {
//        auto i420Buffer = I420Buffer::create(width, height);
//        // 由于I420Buffer YUV数据段是连续的，因此可以直接这样读取
//        size_t size =
//            fread(i420Buffer->mutableDataY(), 1, i420Buffer->size(), yuvFile);
//        if (size < i420Buffer->size())
//        {
//            qDebug() << "Read frame failed!";
//            break;
//        }
//        VideoFrame videoFrame(i420Buffer);
//        w.onFrame(videoFrame);
//
//        frame->pts = pts_i++;
//        memcpy(
//            frame->data[0], i420Buffer->dataY(),
//            i420Buffer->strideY() * i420Buffer->height());
//        memcpy(
//            frame->data[1], i420Buffer->dataU(),
//            i420Buffer->strideU() * i420Buffer->chromaHeight());
//        memcpy(
//            frame->data[2], i420Buffer->dataV(),
//            i420Buffer->strideV() * i420Buffer->chromaHeight());
//
//        ret = avcodec_send_frame(enc_ctx, frame);
//        if (ret < 0)
//        {
//            qWarning("avcodec_send_frame fail");
//            break;
//        }
//        while (ret >= 0)
//        {
//            ret = avcodec_receive_packet(enc_ctx, pkt);
//            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
//            {
//                break;
//            }
//            else if (ret < 0)
//            {
//                qWarning("avcodec_receive_packet fail");
//                break;
//            }
//
//            av_packet_rescale_ts(pkt, enc_ctx->time_base, st->time_base);
//            ret = av_interleaved_write_frame(ofmt_ctx, pkt);
//            if (ret < 0)
//            {
//                qWarning("av_interleaved_write_frame fail");
//                break;
//            }
//            av_packet_unref(pkt);
//        }
//        Sleep(10);
//    }
//
//    ret = avcodec_send_frame(enc_ctx, NULL);
//    if (ret < 0)
//    {
//        qWarning("avcodec_send_frame fail");
//        return;
//    }
//    while (ret >= 0)
//    {
//        ret = avcodec_receive_packet(enc_ctx, pkt);
//        if (ret == AVERROR(EINVAL) || ret == AVERROR_EOF)
//        {
//            break;
//        }
//        else if (ret < 0)
//        {
//            qWarning("avcodec_receive_packet fail");
//            break;
//        }
//
//        av_packet_rescale_ts(pkt, enc_ctx->time_base, st->time_base);
//        ret = av_interleaved_write_frame(ofmt_ctx, pkt);
//        if (ret < 0)
//        {
//            qWarning("av_interleaved_write_frame fail");
//            break;
//        }
//        av_packet_unref(pkt);
//    }
//
//    ret = av_write_trailer(ofmt_ctx);
//    if (ret < 0)
//    {
//        qWarning("av_write_trailer fail");
//    }
//
//    if (ofmt_ctx && ofmt_ctx->pb)
//    {
//        avio_close(ofmt_ctx->pb);
//    }
//    if (enc_ctx)
//    {
//        avcodec_close(enc_ctx);
//    }
//    if (ofmt_ctx)
//    {
//        avformat_free_context(ofmt_ctx);
//    }
//    if (frame)
//    {
//        av_frame_free(&frame);
//    }
//    if (pkt)
//    {
//        av_packet_free(&pkt);
//    }
//
//    qDebug() << "Play YUV File done";
//}

static void readYUVVideo(
    const std::string &yuvFilePath, int width, int height, OpenGLWidget &w)
{
    FILE *yuvFile = fopen(yuvFilePath.c_str(), "rb+");
    if (!yuvFile)
    {
        qDebug() << "Open YUV file failed";
        return;
    }

    QString saveFilePath = "D:/test_video/Johnny/Johnny_1280x720_60_qsv.mp4";
    // QString saveFilePath =
    // "D:/test_video/actual_sence_1920x1080_60_x264.mp4";
    // QString saveFilePath =
    // "D:/test_video/Johnny/Johnny_1280x720_60_nvenc.mp4";
    qDebug() << "File Path: " << yuvFilePath.c_str()
             << ", save file path: " << saveFilePath;

    //VideoCodec codec;
    //codec.m_qpMin = 16;
    //codec.m_qpMax = 30;
    //codec.m_width = width;
    //codec.m_height = height;
    //if (codec.m_width * codec.m_height > 1920 * 1080)
    //{
    //    codec.m_crf = 26.0;
    //    codec.m_qp = 33;
    //}
    //else
    //{
    //    codec.m_crf = 23.0;
    //    codec.m_qp = 28;
    //}
    //codec.m_frameRate = 60;
    //codec.m_codecType = VideoCodecType::kVideoCodecH264;
    //codec.H264()->m_keyFrameInterval = 60 * 5;
    //codec.m_targetBitrate = 86 * 1000 * 1000;

    //VideoRecorder recoder;
    //recoder.setSaveFilePath(saveFilePath);
    //recoder.setInternalEncoder(codec);
    // recoder.initRecorder(encoder.encodeContext());
    //recoder.start();

    while (!feof(yuvFile))
    {
        auto i420Buffer = I420Buffer::create(width, height);
        // 由于I420Buffer YUV数据段是连续的，因此可以直接这样读取
        size_t size =
            fread(i420Buffer->mutableDataY(), 1, i420Buffer->size(), yuvFile);
        if (size < i420Buffer->size())
        {
            qDebug() << "Read frame failed!";
            break;
        }
        VideoFrame videoFrame(i420Buffer);
        w.onFrame(videoFrame);
        //recoder.onFrame(videoFrame);
        Sleep(15);
    }
    //recoder.stop();

    qDebug() << "Play YUV File done";
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    OpenGLWidget w;
    w.show();
    // w.setVideoParams(OpenGLWidget::FrameFormat::YUV420P, 1920, 1080);
    w.setVideoParams(OpenGLWidget::FrameFormat::YUV420P, 1280, 720);
    w.startPlay();
    // VideoFrame frame(nullptr);
    // readYuvPic("D:/test_video/thewitcher3_1889x1073.yuv", 1889, 1073, frame);
    // w.onFrame(frame);
    // 开启一个线程读取文件并播放
    av_log_set_level(AV_LOG_DEBUG);
    std::thread *videoThread = new std::thread(
        [&]()
        {
            readYUVVideo(
                "D:/test_video/Johnny/Johnny_1280x720_60.yuv", 1280, 720, w);
            // readYUVVideo(
            //"D:/test_video/actual_sence_1920x1080_60.yuv", 1920, 1080, w);
        });

    return a.exec();
}
