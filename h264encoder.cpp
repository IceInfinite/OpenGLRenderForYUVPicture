#include "h264encoder.h"

#include <string.h>

#include <QDebug>

extern "C"
{
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}  // extern "C"

namespace
{
constexpr char kH264NVENCName[] = {"h264_nvenc"};
constexpr char kH264QSVName[] = {"h264_qsv"};
constexpr char kX264Name[] = {"libx264"};
constexpr int kMaxFrameQueueSize = 5;
}  // namespace

H264Encoder::H264Encoder()
    : m_thread(nullptr),
      m_isStarting(false),
      m_encodedImageCallback(nullptr),
      m_encoderType(H264EncoderType::kX264),
      m_pts(0),
      m_encodeCtx(nullptr),
      m_avCodec(nullptr),
      m_hwDeviceCtx(nullptr),
      m_swsCtx(nullptr),
      m_avFrame(nullptr),
      m_swsFrame(nullptr),
      m_swsBuffer(nullptr),
      m_packet(nullptr)
{
    if (isEncoderSupport(kH264NVENCName))
    {
        m_encoderType = H264EncoderType::kH264NVENC;
    }
    else if (isEncoderSupport(kH264QSVName))
    {
        m_encoderType = H264EncoderType::kH264QSV;
    }
    else if (isEncoderSupport(kX264Name))
    {
        m_encoderType = H264EncoderType::kX264;
    }
    else
    {
        m_encoderType = H264EncoderType::kUnsupport;
    }
}

H264Encoder::~H264Encoder()
{
    release();
}

void H264Encoder::release()
{
    if (m_encodeCtx)
    {
        avcodec_close(m_encodeCtx);
        avcodec_free_context(&m_encodeCtx);
        m_encodeCtx = nullptr;
    }

    if (m_hwDeviceCtx)
    {
        av_buffer_unref(&m_hwDeviceCtx);
        m_hwDeviceCtx = nullptr;
    }

    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }

    if (m_avFrame)
    {
        av_frame_free(&m_avFrame);
        m_avFrame = nullptr;
    }

    if (m_swsFrame)
    {
        av_frame_free(&m_swsFrame);
        m_swsFrame = nullptr;
    }

    if (m_swsBuffer)
    {
        av_free(m_swsBuffer);
        m_swsBuffer = nullptr;
    }

    if (m_packet)
    {
        av_packet_free(&m_packet);
        m_packet = nullptr;
    }

    while (!m_frameQueue.empty())
        m_frameQueue.pop();
}

bool H264Encoder::initEncoder(const VideoCodec &codec)
{
    release();

    // 目前仅支持YUV420P帧输入
    m_srcPixFmt = AV_PIX_FMT_YUV420P;
    m_swsPixFmt = AV_PIX_FMT_YUV420P;
    m_codec = codec;
    m_encodedImage.setEncodedBuffer(EncodedImageBuffer::create());
    m_encodedImage.m_encodedWidth = m_codec.m_width;
    m_encodedImage.m_encodedHeight = m_codec.m_height;

    switch (m_encoderType)
    {
        case H264EncoderType::kH264NVENC:
        {
            m_avCodec = avcodec_find_encoder_by_name(kH264NVENCName);
            m_swsPixFmt = AV_PIX_FMT_YUV420P;
            qDebug() << "InitEncoder: use " << kH264NVENCName << " encoder";
            break;
        }
        case H264EncoderType::kH264QSV:
        {
            m_avCodec = avcodec_find_encoder_by_name(kH264QSVName);
            m_swsPixFmt = AV_PIX_FMT_NV12;
            int ret = av_hwdevice_ctx_create(
                &m_hwDeviceCtx, AV_HWDEVICE_TYPE_QSV, nullptr, nullptr, 0);
            if (ret < 0)
            {
                qWarning("Failed to create a QSV device. Error code: %d", ret);
                return false;
            }
            qDebug() << "InitEncoder: use  " << kH264QSVName << " encoder";
            break;
        }
        case H264EncoderType::kX264:
        {
            m_avCodec = avcodec_find_encoder_by_name(kX264Name);
            m_swsPixFmt = AV_PIX_FMT_YUV420P;
            qDebug() << "InitEncoder: use " << kX264Name << " encoder";
            break;
        }
        case H264EncoderType::kUnsupport:
        {
            m_avCodec = nullptr;
            break;
        }
    }
    if (!m_avCodec)
    {
        qWarning() << "Can't find requested codec, codec type: "
                   << static_cast<int>(m_encoderType);
        return false;
    }

    m_encodeCtx = avcodec_alloc_context3(m_avCodec);
    if (!m_encodeCtx)
    {
        qWarning() << "Can't alloc codec context";
        return false;
    }

    if (m_encoderType == H264EncoderType::kH264QSV)
    {
        if (!setHwframeCtx())
            return false;
    }

    m_encodeCtx->width = m_codec.m_width;
    m_encodeCtx->height = m_codec.m_height;
    m_encodeCtx->gop_size = m_codec.H264()->m_keyFrameInterval;
    m_encodeCtx->time_base = av_make_q(1, m_codec.m_frameRate);
    m_encodeCtx->framerate = av_make_q(m_codec.m_frameRate, 1);
    m_encodeCtx->bit_rate = 0;
    m_encodeCtx->rc_buffer_size = 0;
    m_encodeCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    switch (m_encoderType)
    {
        case H264EncoderType::kH264QSV:
            m_encodeCtx->pix_fmt = AV_PIX_FMT_QSV;
            // -preset     <int>   (from 1 to 7) (default medium)
            // veryfast      7
            // faster        6
            // fast          5
            // medium        4
            // slow          3
            // slower        2
            // veryslow      1
            av_opt_set(m_encodeCtx->priv_data, "preset", "veryfast", 0);

            // -profile    <int> (default unknown)
            //  unknown     0
            //  baseline    66
            //  main        77
            //  high        100
            av_opt_set(m_encodeCtx->priv_data, "profile", "main", 0);
            // av_opt_set_int(m_encodeCtx->priv_data, "max_qp_i", m_codec.m_qp,
            // 0); av_opt_set_int(m_encodeCtx->priv_data, "min_qp_i",
            // m_codec.m_qp, 0); av_opt_set_int(m_encodeCtx->priv_data,
            // "max_qp_p", m_codec.m_qp, 0);
            // av_opt_set_int(m_encodeCtx->priv_data, "min_qp_p", m_codec.m_qp,
            // 0); av_opt_set_int(m_encodeCtx->priv_data, "max_qp_b",
            // m_codec.m_qp, 0); av_opt_set_int(m_encodeCtx->priv_data,
            // "min_qp_b", m_codec.m_qp, 0);

            // For CQP Mode
            // m_encodeCtx->flags |= AV_CODEC_FLAG_QSCALE;
            // m_encodeCtx->global_quality = m_codec.m_qp * FF_QP2LAMBDA;

            // For ICQ Mode
            m_encodeCtx->global_quality = m_codec.m_qp;
            m_encodeCtx->bit_rate = m_codec.m_targetBitrate;
            m_encodeCtx->rc_buffer_size = m_codec.m_targetBitrate;
            m_encodeCtx->max_b_frames = 1;
            // m_encodeCtx->rc_max_rate = m_codec.m_targetBitrate;
            // m_encodeCtx->rc_min_rate = m_codec.m_targetBitrate;
            break;
        case H264EncoderType::kH264NVENC:
            m_encodeCtx->pix_fmt = m_swsPixFmt;
            // -preset    <int>
            //  default     0
            //  slow        1      hq 2 passes
            //  medium      2      hq 1 pass
            //  fast        3      hp 1 pass
            //  hp          4
            //  hq          5
            //  bd          6
            //  ll          7      low latency
            //  llhq        8      low latency hq
            //  llhp        9      low latency hp
            //  lossless    10
            //  losslesshp  11

            av_opt_set(m_encodeCtx->priv_data, "preset", "hq", 0);
            // -tune      <int>    (default hq)
            //  hq          1      High quality
            //  ll          2      Low latency
            //  ull         3      Ultra low latency
            //  lossless    4      Lossless
            av_opt_set(m_encodeCtx->priv_data, "tune", "hq", 0);
            av_opt_set(m_encodeCtx->priv_data, "profile", "high", 0);
            // -multipass <int> (default disabled)
            //  disabled   0    Single Pass
            //  qres       1    Two Pass encoding is enabled
            //                  where first Pass is quarter resolution
            //  fullres    2    Two Pass encoding is enabled
            //                  where first Pass is full resolution
            av_opt_set_int(m_encodeCtx->priv_data, "multipass", 2, 0);
            av_opt_set_int(m_encodeCtx->priv_data, "cbr", false, 0);
            av_opt_set_int(m_encodeCtx->priv_data, "qp", m_codec.m_qp, 0);
            m_encodeCtx->max_b_frames = 2;
            break;
        case H264EncoderType::kX264:
            m_encodeCtx->pix_fmt = m_swsPixFmt;
            // 编码速度越快，编码出的帧越大
            // preset: ultrafast, superfast, veryfast, faster, fast,
            // medium, slow, slower, veryslow, placebo
            av_opt_set(m_encodeCtx->priv_data, "preset", "ultrafast", 0);

            // tune: film, animation, grain, stillimage, psnr,
            // ssim, fastdecode, zerolatency
            //  av_opt_set(m_encodeCtx->priv_data, "tune", "zerolatency", 0);

            // profile: baseline, main, high, high10, high422, high444
            av_opt_set(m_encodeCtx->priv_data, "profile", "baseline", 0);
            av_opt_set_double(m_encodeCtx->priv_data, "crf", m_codec.m_crf, 0);
            m_encodeCtx->max_b_frames = 0;
            break;
        case H264EncoderType::kUnsupport:
            return false;
    }

    int ret = avcodec_open2(m_encodeCtx, m_avCodec, nullptr);
    if (ret < 0)
    {
        qWarning() << "Can't open codec: " << ret;
        return false;
    }

    m_avFrame = av_frame_alloc();
    if (!m_avFrame)
    {
        qWarning() << "Can't allocate video frame";
        return false;
    }

    m_avFrame->format = m_srcPixFmt;
    m_avFrame->width = m_codec.m_width;
    m_avFrame->height = m_codec.m_height;
    ret = av_frame_get_buffer(m_avFrame, 0);
    if (ret < 0)
    {
        qWarning() << "Can't allocate the video frame data";
        return false;
    }

    // Make sure the frame data is writable.
    ret = av_frame_make_writable(m_avFrame);
    if (ret < 0)
    {
        qWarning() << "Can't make avFrame writable!";
        return false;
    }

    m_packet = av_packet_alloc();
    if (!m_packet)
    {
        qWarning() << "Can't allocate video packet";
        return false;
    }

    if (m_srcPixFmt != m_swsPixFmt)
    {
        m_swsCtx = sws_getContext(
            m_codec.m_width, m_codec.m_height, m_srcPixFmt, m_codec.m_width,
            m_codec.m_height, m_swsPixFmt, SWS_BICUBIC, nullptr, nullptr,
            nullptr);
        if (!m_swsCtx)
        {
            qWarning() << "Failed to get sws context!";
            return false;
        }

        m_swsFrame = av_frame_alloc();
        if (!m_swsFrame)
        {
            qWarning() << "Can't allocate sws frame";
            return false;
        }
        m_swsFrame->format = m_swsPixFmt;
        m_swsFrame->width = m_codec.m_width;
        m_swsFrame->height = m_codec.m_height;
        int nbytes = av_image_get_buffer_size(
            m_swsPixFmt, m_codec.m_width, m_codec.m_height, 1);
        m_swsBuffer = static_cast<uint8_t *>(av_malloc(nbytes));
        av_image_fill_arrays(
            m_swsFrame->data, m_swsFrame->linesize, m_swsBuffer, m_swsPixFmt,
            m_codec.m_width, m_codec.m_height, 1);
    }

    return true;
}

bool H264Encoder::setHwframeCtx()
{
    AVBufferRef *hwFramesCtx = nullptr;
    AVHWFramesContext *framesCtx = nullptr;

    if (!(hwFramesCtx = av_hwframe_ctx_alloc(m_hwDeviceCtx)))
    {
        qWarning("Failed to create qsv hardware frame context.");
        return false;
    }
    framesCtx = reinterpret_cast<AVHWFramesContext *>(hwFramesCtx->data);
    framesCtx->format = AV_PIX_FMT_QSV;
    framesCtx->sw_format = AV_PIX_FMT_NV12;
    framesCtx->width = m_codec.m_width;
    framesCtx->height = m_codec.m_height;
    framesCtx->initial_pool_size = 20;
    if (av_hwframe_ctx_init(hwFramesCtx) < 0)
    {
        qWarning("Failed to initialize qsv hardware frame context.");
        av_buffer_unref(&hwFramesCtx);
        return false;
    }
    m_encodeCtx->hw_frames_ctx = av_buffer_ref(hwFramesCtx);
    if (!(m_encodeCtx->hw_frames_ctx))
    {
        qWarning("Failed to set hwframe context.");
        return false;
    }

    av_buffer_unref(&hwFramesCtx);
    return true;
}

void H264Encoder::encode(const VideoFrame &frame)
{
    static int64_t dropFrameCount = 0;
    if (!m_isStarting)
        return;
    std::unique_lock<std::mutex> locker(m_frameMutex);
    // 如果编码速率跟不上,最多保存 kMaxFrameQueueSize 帧在待编码队列中
    // TODO(hcb):实现一个多线程安全的队列
    if (m_frameQueue.size() > kMaxFrameQueueSize)
    {
        m_frameQueue.pop();
        ++dropFrameCount;
        if (dropFrameCount % 30 == 0)
        {
            qWarning() << "Encode: drop frame count=" << dropFrameCount;
        }
        ++m_pts;
    }

    m_frameQueue.emplace(frame);
    m_cv.notify_one();
}

void H264Encoder::registerEncodeCompleteCallback(EncodedImageCallback *callback)
{
    m_encodedImageCallback = callback;
}

void H264Encoder::start()
{
    m_isStarting = true;
    m_thread.reset(new std::thread([&]() { threadFunc(); }));
}

void H264Encoder::stop()
{
    m_isStarting = false;
    m_cv.notify_all();

    if (m_thread)
        m_thread.get()->join();

    m_thread.reset(nullptr);
}

const AVCodecContext *H264Encoder::encodeContext() const
{
    return m_encodeCtx;
}

bool H264Encoder::isEncoderSupport(const char *encoderName)
{
    AVCodec *codec = nullptr;
    AVCodecContext *codecCtx = nullptr;

    codec = avcodec_find_encoder_by_name(encoderName);
    if (!codec)
    {
        qWarning("Can't find encoder %s, enc is not support", encoderName);
        return false;
    }

    codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx)
    {
        qWarning("%s hardware Enc is not support!!", encoderName);
        return false;
    }

    codecCtx->codec_id = codec->id;
    codecCtx->time_base = av_make_q(1, 30);
    codecCtx->pix_fmt = *(codec->pix_fmts);
    codecCtx->width = 1920;
    codecCtx->height = 1080;
    codecCtx->bit_rate = 1024 * 1000;

    int ret = avcodec_open2(codecCtx, codec, nullptr);
    if (ret < 0)
    {
        qWarning("%s hardware Enc is not support!!", encoderName);
        avcodec_free_context(&codecCtx);
        codecCtx = nullptr;
        return false;
    }
    avcodec_close(codecCtx);
    avcodec_free_context(&codecCtx);
    codecCtx = nullptr;
    return true;
}

void H264Encoder::threadFunc()
{
    VideoFrame frame;
    while (m_isStarting)
    {
        {
            std::unique_lock<std::mutex> locker(m_frameMutex);
            m_cv.wait(
                locker,
                [this]() { return !m_frameQueue.empty() || !m_isStarting; });
        }
        while (true)
        {
            {
                std::unique_lock<std::mutex> locker(m_frameMutex);
                if (m_frameQueue.empty())
                    break;
                frame = m_frameQueue.front();
                m_frameQueue.pop();
                m_avFrame->pts = m_pts++;
            }
            auto i420Buffer = frame.videoFrameBuffer()->toI420();
            if (!i420Buffer)
                continue;
            memcpy(
                m_avFrame->data[0], i420Buffer->dataY(),
                i420Buffer->strideY() * i420Buffer->height());
            memcpy(
                m_avFrame->data[1], i420Buffer->dataU(),
                i420Buffer->strideU() * i420Buffer->chromaHeight());
            memcpy(
                m_avFrame->data[2], i420Buffer->dataV(),
                i420Buffer->strideV() * i420Buffer->chromaHeight());

            doEncode(m_avFrame);
        }
    }

    while (true)
    {
        {
            std::unique_lock<std::mutex> locker(m_frameMutex);
            if (m_frameQueue.empty())
                break;
            frame = m_frameQueue.front();
            m_frameQueue.pop();
            m_avFrame->pts = m_pts++;
        }
        auto i420Buffer = frame.videoFrameBuffer()->toI420();
        if (!i420Buffer)
            continue;
        memcpy(
            m_avFrame->data[0], i420Buffer->dataY(),
            i420Buffer->strideY() * i420Buffer->height());
        memcpy(
            m_avFrame->data[1], i420Buffer->dataU(),
            i420Buffer->strideU() * i420Buffer->chromaHeight());
        memcpy(
            m_avFrame->data[2], i420Buffer->dataV(),
            i420Buffer->strideV() * i420Buffer->chromaHeight());

        doEncode(m_avFrame);
    }

    // flush the encoder
    encodeInternal(nullptr);
}

bool H264Encoder::doEncode(const AVFrame *frame)
{
    if (!frame)
        return true;

    bool ret = false;
    const AVFrame *dstFrame = frame;

    if (m_srcPixFmt != m_swsPixFmt)
    {
        if (sws_scale(
                m_swsCtx, static_cast<const uint8_t *const *>(frame->data),
                frame->linesize, 0, frame->height, m_swsFrame->data,
                m_swsFrame->linesize) < 0)
        {
            qWarning() << "sws_scale frame failed!";
            return false;
        }
        m_swsFrame->pts = frame->pts;
        dstFrame = m_swsFrame;
    }

    if (m_encoderType == H264EncoderType::kH264QSV)
    {
        // TODO(hcb): 统计消耗的时间
        AVFrame *hwFrame = av_frame_alloc();
        if (av_hwframe_get_buffer(m_encodeCtx->hw_frames_ctx, hwFrame, 0) < 0)
        {
            qWarning() << "Get hardware frame failed";
            return false;
        }

        if (!hwFrame->hw_frames_ctx)
        {
            qWarning() << "hw_frame doesn't have hw_frame_ctx";
            av_frame_free(&hwFrame);
            return false;
        }

        if (av_hwframe_transfer_data(hwFrame, dstFrame, 0) < 0)
        {
            qWarning(
                "QSV: Error while transferring frame data to "
                "surface.");
            av_frame_free(&hwFrame);
            return false;
        }

        hwFrame->pts = dstFrame->pts;
        ret = encodeInternal(hwFrame);

        av_frame_free(&hwFrame);
    }
    else
    {
        ret = encodeInternal(dstFrame);
    }
    return ret;
}

bool H264Encoder::encodeInternal(const AVFrame *frame)
{
    int ret = -1;
    ret = avcodec_send_frame(m_encodeCtx, frame);
    if (ret < 0)
    {
        qWarning() << "Error sending a frame for encoding";
        return false;
    }

    while (ret >= 0)
    {
        ret = avcodec_receive_packet(m_encodeCtx, m_packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            break;
        }
        else if (ret < 0)
        {
            qWarning() << "Error during encoding";
            return false;
        }

        // 暂不使用此接口，目前无需转换，避免拷贝
        // auto encodedBuffer =
        //     EncodedImageBuffer::create(m_packet->data,
        //     m_packet->size);
        // m_encodedImage.setEncodedBuffer(encodedBuffer);
        // m_encodedImage.m_encodedWidth = m_encodeCtx->width;
        // m_encodedImage.m_encodedHeight = m_encodeCtx->height;
        // m_encodedImage.m_pts = m_packet->pts;
        // m_encodedImage.m_dts = m_packet->dts;
        // m_encodedImage.m_streamIndex = m_packet->stream_index;
        // m_encodedImage.m_isKeyFrame = (m_packet->flags & AV_PKT_FLAG_KEY);

        if (m_encodedImageCallback)
            m_encodedImageCallback->onEncodedImage(
                m_packet, m_encodeCtx->width, m_encodeCtx->height);
        // m_encodedImageCallback->onEncodedImage(m_encodedImage);

        av_packet_unref(m_packet);
    }
    return true;
}
