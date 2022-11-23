#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLExtraFunctions>
#include <QString>

class OpenGLWidget : public QOpenGLWidget, protected QOpenGLExtraFunctions
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
    bool checkCompileError(GLuint shader, bool isVertexShader);
    bool createShaders(const QString &vertexSourcePath, const QString &fragmentSourcePath);

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

    // Vertex Array Object
    GLuint m_vao;
    // Vertex Buffer Object
    GLuint m_vbo;
    // Element Buffer Object
    GLuint m_ebo;
    GLuint m_textures[3];
    GLuint m_vertexShader;
    GLuint m_fragmentShader;
    GLuint m_program;
};
#endif // OPENGLWIDGET_H
