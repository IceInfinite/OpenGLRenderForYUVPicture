#include "openglwidget.h"

#include <fstream>
#include <string>

#include <QDebug>
#include <QFile>
#include <QTextStream>

static const GLfloat kVertices[] = {
    // pos              // texture coords
    -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, // bottom left
     1.0f, -1.0f, 0.0f, 1.0f, 1.0f, // bottom right
     1.0f,  1.0f, 0.0f, 1.0f, 0.0f, // top right
    -1.0f,  1.0f, 0.0f, 0.0f, 0.0f  // top left
};

static const GLuint kIndices[] = {
    0, 1, 2, // 第一个三角形
    0, 2, 3  // 第二个三角形
};

void splitYuv420p(const char *inputPath, int width, int height)
{

    std::ifstream picFile(inputPath, std::ios::in | std::ios::binary);
    if (!picFile)
    {
        qDebug() << "Open yuv pic failed!";
        return;
    }
    std::ofstream picYfile("the_witcher3_1920x1080.y", std::ios::out | std::ios::binary);
    std::ofstream picUfile("the_witcher3_960x540.u", std::ios::out | std::ios::binary);
    std::ofstream picVfile("the_witcher3_960x540.v", std::ios::out | std::ios::binary);


    // 因为一个y(8)+u/4(2) + v/4(2) = 12bit = 1.5个字节
    char *data = new char[width * height * 3 / 2];

    // 整张图片读进来
    picFile.read(data, width * height * 3 / 2);

    // Y
    picYfile.write(data, width * height);

    // U
    picUfile.write(data + width * height, width * height / 4);

    // V
    picVfile.write(data + width * height * 5 / 4, width * height / 4);

    // 释放资源
    delete[] data;

    picFile.close();
    picYfile.close();
    picUfile.close();
    picVfile.close();
}

OpenGLWidget::OpenGLWidget(QWidget *parent) :
    QOpenGLWidget(parent),
    m_initialized(false),
    m_width(0),
    m_height(0),
    m_videoWidth(0),
    m_videoHeight(0),
    m_vao(0),
    m_vbo(0),
    m_ebo(0)
{
    for (unsigned int i = 0; i < sizeof(m_pData) / sizeof(unsigned char *); ++i)
    {
        m_pData[i] = nullptr;
    }

    for (unsigned int i = 0; i < sizeof(m_textures) / sizeof(GLuint); ++i)
    {
        m_textures[i] = 0;
    }
    readYuvPic("thewitcher3.yuv", 1920, 1080);
}

OpenGLWidget::~OpenGLWidget()
{
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_vbo);
    glDeleteBuffers(1, &m_ebo);
    glDeleteProgram(m_program);
}

void OpenGLWidget::readYuvPic(const char *picPath, int picWidth, int picHeight)
{
    std::ifstream picFile(picPath, std::ios::in | std::ios::binary);
    if (!picFile)
    {
        qDebug() << "Open yuv pic failed!";
        return;
    }

    if (m_videoWidth != picWidth || m_videoHeight != picHeight)
    {
        m_videoWidth = picWidth;
        m_videoHeight = picHeight;
        for (unsigned int i = 0; i < sizeof(m_pData) / sizeof(unsigned char *); ++i)
        {
            if (m_pData[i] != nullptr)
            {
                delete[] m_pData[i];
                m_pData[i] = nullptr;
            }
        }
        m_pData[0] = new unsigned char[m_videoWidth * m_videoHeight];
        m_pData[1] = new unsigned char[m_videoWidth * m_videoHeight / 4];
        m_pData[2] = new unsigned char[m_videoWidth * m_videoHeight / 4];
    }
    // y(8bit) + u/4(2bit) + v/4(2bit) = 12 bit = 1.5 Byte
    char *data = new char[picWidth * picHeight * 3 / 2];

    // 整张图片读进来
    picFile.read(data, picWidth * picHeight * 3 / 2);

    // Y
    memcpy(m_pData[0], data, picWidth * picHeight);

    // U
    memcpy(m_pData[1], data + picWidth * picHeight, picWidth * picHeight / 4);

    // V
    memcpy(m_pData[2], data + picWidth * picHeight * 5 / 4, picWidth * picHeight / 4);

    // 释放资源
    delete[] data;
    picFile.close();
}

void OpenGLWidget::initializeGL()
{
    if (!m_initialized)
    {
        initializeOpenGLFunctions();
        // TODO(hcb): 将Shader代码文件整合到/res路径下
        if (!createShaders("vertexshader.vert", "fragmentshader.frag"))
            return;

        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glGenBuffers(1, &m_ebo);
        // 1. 绑定顶点数组对象
        glBindVertexArray(m_vao);
        // 2. 把我们的顶点数组复制到一个顶点缓冲中，供OpenGL使用
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(kVertices), kVertices, GL_STATIC_DRAW);
        // 3. 复制我们的索引数组到一个索引缓冲中，供OpenGL使用
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kIndices), kIndices, GL_STATIC_DRAW);
        // 4. 设定顶点属性指针
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void *)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void *)(3 * sizeof(GLfloat)));
        glEnableVertexAttribArray(1);
        // 创建纹理对象
        glGenBuffers(3, m_textures);
        for (unsigned int i = 0; i < sizeof(m_textures) / sizeof(GLuint); ++i)
        {
            // 将当前上下文纹理对象设置成m_textures[i]
            glBindTexture(GL_TEXTURE_2D, m_textures[i]);
            // 设置纹理采样时超出与缩小图像时的采样方式
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            // 设置纹理多级渐远
            glGenerateMipmap(GL_TEXTURE_2D);
            // 解除绑定
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        glBindVertexArray(0);
        m_initialized = true;
    }
}

void OpenGLWidget::paintGL()
{
    if (!m_initialized)
        return;
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(m_program);
    // Y
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_videoWidth, m_videoHeight, 0, GL_RED, GL_UNSIGNED_BYTE, m_pData[0]);
    glUniform1i(glGetUniformLocation(m_program, "yTexture"), 0);

    // U
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_textures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_videoWidth / 2 , m_videoHeight / 2, 0, GL_RED, GL_UNSIGNED_BYTE, m_pData[1]);
    glUniform1i(glGetUniformLocation(m_program, "uTexture"), 1);

    // V
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_textures[2]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_videoWidth / 2, m_videoHeight / 2, 0, GL_RED, GL_UNSIGNED_BYTE, m_pData[2]);
    glUniform1i(glGetUniformLocation(m_program, "vTexture"), 2);

    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void OpenGLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    m_width = w;
    m_height = h;
}

bool OpenGLWidget::checkCompileError(GLuint shader, bool isVertexShader)
{
    int success;
    char infoLog[512];

    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        qDebug() << "Shader compile error, type: " << (isVertexShader ? "vertex shader" : "fragment shader") << "\n" << infoLog;
        return true;
    }
    return false;
}

bool OpenGLWidget::createShaders(const QString &vertexSourcePath, const QString &fragmentSourcePath)
{
    QFile vertexSourceFile(vertexSourcePath);
    if (!vertexSourceFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Open vertex source file failed";
        return false;
    }
    QTextStream vertexSourceText(&vertexSourceFile);
    QString vertexSourceStr = vertexSourceText.readAll();

    QFile fragmentSourceFile(fragmentSourcePath);
    if (!fragmentSourceFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Open fragrament source file failed";
        return false;
    }
    QTextStream fragmentSourceText(&fragmentSourceFile);
    QString fragmentSourceStr = fragmentSourceText.readAll();

    std::string vertexSourceStdStr = vertexSourceStr.toStdString();
    std::string fragmentSourceStdStr = fragmentSourceStr.toStdString();
    const char *vertexSource = vertexSourceStdStr.c_str();
    const char *fragmentSource = fragmentSourceStdStr.c_str();
    m_vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(m_vertexShader, 1, &vertexSource, NULL);
    glCompileShader(m_vertexShader);
    if (checkCompileError(m_vertexShader, true))
        return false;

    m_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(m_fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(m_fragmentShader);
    if (checkCompileError(m_fragmentShader, false))
        return false;

    m_program = glCreateProgram();
    glAttachShader(m_program, m_vertexShader);
    glAttachShader(m_program, m_fragmentShader);
    glLinkProgram(m_program);

    int success;
    char infoLog[512];
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(m_program, 512, NULL, infoLog);
        qDebug() << "Program link error!\n" << infoLog;
        return false;
    }

    glDeleteShader(m_vertexShader);
    glDeleteShader(m_fragmentShader);
    return true;
}
