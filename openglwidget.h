#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H

#include <stdint.h>

#include <memory>
#include <mutex>
#include <queue>

#include <QByteArray>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QPixmap>

#include "videoframe.h"

class OpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    OpenGLWidget(QWidget *parent = nullptr);
    ~OpenGLWidget();

    enum class FrameFormat
    {
        UNKNOWN = -1,
        YUV420P,
        RGBA,
        BGRA,
        RGB
    };

    void setVideoParams(const FrameFormat &format, int width, int height);
    void setVideoFrameFormat(const FrameFormat &format);
    OpenGLWidget::FrameFormat videoFrameFormat() const;

    void startPlay();
    void stopPlay();
    // TODO(hcb): 图像缩放策略
    void setScale(int scale);

    bool checkOpenGLVersion(float requireMinVersion);

public slots:
    // 传入下一待渲染帧
    void onFrame(const VideoFrame &videoFrame);
    // 输出渲染相关信息
    void printRenderStats() const;

protected:
    virtual void initializeGL() override;
    virtual void paintGL() override;
    virtual void resizeGL(int w, int h) override;

private:
    bool createShaders(
        const QString &vertexSourcePath, const QString &fragmentSourcePath);
    bool createShaders(QByteArray vertexSource, QByteArray fragmentSource);
    void recreateTextures();

    bool init();
    void clearFrame();

private:
    bool m_initialized;
    bool m_starting;
    int m_videoWidth;
    int m_videoHeight;
    int m_strideY;
    int m_strideU;
    int m_strideV;
    int m_scale;
    std::queue<VideoFrame> m_frameQueue;

    // For frame and m_needRender
    std::mutex m_mutex;
    FrameFormat m_frameFormat;
    bool m_needRender;
    bool m_recvFirstFrame;

    // Count Render stats
    mutable int64_t m_times = 1;
    int64_t m_frameCount;
    int64_t m_frameDropped;
    int64_t m_frameRendered;

    // Vertex Array Object
    QOpenGLVertexArrayObject m_vao;
    // Vertex Buffer Object
    QOpenGLBuffer m_vbo;
    // Element Buffer Object or Index Buffer Object
    QOpenGLBuffer m_ibo;
    const int m_posLocation;
    const int m_texCoordsLocation;
    // TODO(hcb):将指针改为Qt智能指针
    QOpenGLTexture *m_textures[3];
    QOpenGLShaderProgram *m_program;
};

#endif  // OPENGLWIDGET_H
