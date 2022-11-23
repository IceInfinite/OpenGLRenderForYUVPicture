#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H

#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>

class OpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    OpenGLWidget(QWidget *parent = nullptr);
    ~OpenGLWidget();

    void readYuvPic(const char *picPath, int picWidth, int picHeight);

protected:
    virtual void initializeGL() override;
    virtual void paintGL() override;
    virtual void resizeGL(int w, int h) override;

private:
    bool createShaders(const QString &vertexSourcePath, const QString &fragmentSourcePath);

    void clearData();

private:
    bool m_initialized;
    int m_width;
    int m_height;
    int m_picWidth;
    int m_picHeight;
    int m_strideY;
    int m_strideU;
    int m_strideV;
    unsigned char *m_pData[3];

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
#endif // OPENGLWIDGET_H
