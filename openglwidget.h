#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H

#include <stdint.h>

#include <mutex>

#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QPixmap>

class OpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    OpenGLWidget(QWidget *parent = nullptr);
    ~OpenGLWidget();

    void readYuvPic(const char *picPath, int picWidth, int picHeight);
    void onPicure(const QPixmap &pix);

    enum class FrameFormat
    {
        UNKNOWN = 0,
        YUV420,
        BGRA,
        RGBA
    };

    void setVideoFrameFormat(const FrameFormat &format);
    OpenGLWidget::FrameFormat videoFrameFormat() const;

public slots:
    // 传入下一待渲染帧
    // void onFrame(const vframe_s &frame);

protected:
    virtual void initializeGL() override;
    virtual void paintGL() override;
    virtual void resizeGL(int w, int h) override;

private:
    bool createShaders(
        const QString &vertexSourcePath, const QString &fragmentSourcePath);
    bool createShaders(const char *vertexSource, const char *fragmentSource);
    void reinitNecessaryResource();
    void recreateTextures();
    void clearData();

private:
    bool m_initialized;
    int m_width;
    int m_height;
    int m_videoWidth;
    int m_videoHeight;
    int m_strideY;
    int m_strideU;
    int m_strideV;
    unsigned char *m_pData[3];

    // For m_pData and m_needRender
    std::mutex m_mutex;
    FrameFormat m_frameFormat;
    bool m_needRender;

    // Count Render stats
    int64_t m_frameCount;
    int64_t m_frameDropped;
    int64_t m_frameRendered;

    // Vertex Array Object
    QOpenGLVertexArrayObject m_vao;
    // Vertex Buffer Object
    QOpenGLBuffer m_vbo;
    // Element Buffer Object or Index Buffer Object
    QOpenGLBuffer m_ibo;
    QOpenGLTexture *m_textures[3];
    QOpenGLShader *m_vertexShader;
    QOpenGLShader *m_fragmentShader;
    QOpenGLShaderProgram m_program;
};

#endif  // OPENGLWIDGET_H
