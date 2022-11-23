#include "openglwidget.h"

#include <fstream>
#include <string>

#include <QDebug>
#include <QFile>
#include <QTextStream>

namespace
{
    int i420DataSize(int height, int strideY, int strideU, int strideV)
    {
        return strideY * height + (strideU + strideV) * ((height + 1) / 2);
    }
} // namespace

static const GLfloat kVertices[] = {
    // pos              // texture coords
    -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, // bottom left
    1.0f,  -1.0f, 0.0f, 1.0f, 1.0f, // bottom right
    1.0f,  1.0f,  0.0f, 1.0f, 0.0f, // top right
    -1.0f, 1.0f,  0.0f, 0.0f, 0.0f  // top left
};

static const GLuint kIndices[] = {
    0, 1, 2, // first triangle
    0, 2, 3  // second triangle
};

OpenGLWidget::OpenGLWidget(QWidget *parent) :
    QOpenGLWidget(parent),
    m_initialized(false),
    m_width(0),
    m_height(0),
    m_picWidth(0),
    m_picHeight(0),
    m_strideY(0),
    m_strideU(0),
    m_strideV(0),
    m_vbo(QOpenGLBuffer::VertexBuffer),
    m_ibo(QOpenGLBuffer::IndexBuffer),
    m_vertexShader(nullptr),
    m_fragmentShader(nullptr)
{
    for (unsigned int i = 0; i < sizeof(m_pData) / sizeof(unsigned char *); ++i)
    {
        m_pData[i] = nullptr;
    }
    for (unsigned int i = 0; i < sizeof(m_textures) / sizeof(QOpenGLTexture *); ++i)
    {
        m_textures[i] = nullptr;
    }
    //    readYuvPic("thewitcher3_1907x1080.yuv", 1907, 1080);
    //    readYuvPic("thewitcher3_1920x1080.yuv", 1920, 1080);
    readYuvPic("thewitcher3_1889x1073.yuv", 1889, 1073);
}

OpenGLWidget::~OpenGLWidget()
{
    makeCurrent();

    for (unsigned int i = 0; i < sizeof(m_textures) / sizeof(QOpenGLTexture *); ++i)
    {
        if (m_textures[i])
            delete m_textures[i];
    }

    if (m_ibo.isCreated())
        m_ibo.destroy();
    if (m_vbo.isCreated())
        m_vbo.destroy();
    if (m_vao.isCreated())
        m_vao.destroy();

    clearData();

    doneCurrent();
}

void OpenGLWidget::readYuvPic(const char *picPath, int picWidth, int picHeight)
{
    std::ifstream picFile(picPath, std::ios::in | std::ios::binary);
    if (!picFile)
    {
        qDebug() << "Open yuv pic failed!";
        return;
    }

    if (m_picWidth != picWidth || m_picHeight != picHeight)
    {
        m_picWidth = picWidth;
        m_picHeight = picHeight;
        m_strideY = m_picWidth;
        m_strideU = m_strideV = (m_picWidth + 1) / 2;
        qDebug() << "Stride Y: " << m_strideY << ", stride uv: " << m_strideU << ", width x height: " << m_picWidth << "x" << m_picHeight;
        clearData();
        m_pData[0] = new unsigned char[m_strideY * m_picHeight];
        m_pData[1] = new unsigned char[m_strideU * ((m_picHeight + 1) / 2)];
        m_pData[2] = new unsigned char[m_strideV * ((m_picHeight + 1) / 2)];
    }

    int dataSize = i420DataSize(m_picHeight, m_strideY, m_strideU, m_strideV);
    char *data = new char[dataSize];
    qDebug() << "Y size: " << m_strideY * m_picHeight << ", UV size: " << m_strideU * ((m_picHeight + 1) / 2)
             << ", total size: " << dataSize;
    // read all pic data
    picFile.read(data, dataSize);

    // Y
    memcpy(m_pData[0], data, m_strideY * m_picHeight);

    // U
    memcpy(m_pData[1], data + m_strideY * m_picHeight, m_strideU * ((m_picHeight + 1) / 2));

    // V
    memcpy(m_pData[2], data + m_strideY * m_picHeight + m_strideU * ((m_picHeight + 1) / 2), m_strideV * ((m_picHeight + 1) / 2));

    // free resource
    delete[] data;
    picFile.close();
}

void OpenGLWidget::initializeGL()
{
    if (!m_initialized)
    {
        initializeOpenGLFunctions();
        if (!createShaders("res/vertexshader.vert", "res/fragmentshader.frag"))
            return;
        m_vao.create();
        m_vao.bind();

        m_vbo.create();
        m_vbo.bind();
        m_vbo.setUsagePattern(QOpenGLBuffer::UsagePattern::StaticDraw);
        m_vbo.allocate(kVertices, sizeof(kVertices));

        m_ibo.create();
        m_ibo.bind();
        m_ibo.setUsagePattern(QOpenGLBuffer::UsagePattern::StaticDraw);
        m_ibo.allocate(kIndices, sizeof(kIndices));

        // position attribute
        m_program.setAttributeBuffer(0, GL_FLOAT, 0, 3, 5 * sizeof(GLfloat));
        m_program.enableAttributeArray(0);
        // texture coord attribute
        m_program.setAttributeBuffer(1, GL_FLOAT, 3 * sizeof(GLfloat), 2, 5 * sizeof(GLfloat));
        m_program.enableAttributeArray(1);

        // create textures
        for (unsigned int i = 0; i < 3; ++i)
        {
            m_textures[i] = new QOpenGLTexture(QOpenGLTexture::Target2D);
            // set the texture wrapping parameters
            m_textures[i]->setWrapMode(QOpenGLTexture::CoordinateDirection::DirectionS, QOpenGLTexture::WrapMode::Repeat);
            m_textures[i]->setWrapMode(QOpenGLTexture::CoordinateDirection::DirectionT, QOpenGLTexture::WrapMode::Repeat);
            m_textures[i]->setMinMagFilters(QOpenGLTexture::Filter::Linear, QOpenGLTexture::Filter::Linear);
            m_textures[i]->setFormat(QOpenGLTexture::TextureFormat::R8_UNorm);
            if (i == 0)
            {
                m_textures[i]->setSize(m_strideY, m_picHeight);
            }
            else if (i == 1)
            {
                m_textures[i]->setSize(m_strideU, (m_picHeight + 1) / 2);
            }
            else if (i == 2)
            {
                m_textures[i]->setSize(m_strideV, (m_picHeight + 1) / 2);
            }
            m_textures[i]->allocateStorage(QOpenGLTexture::PixelFormat::Red, QOpenGLTexture::PixelType::UInt8);
            m_textures[i]->generateMipMaps();
            // 由于OpenGL内部是4字节对齐的, 为避免像素值不符合要求导致的错误, 设定不足字节对齐的位数按照1字节取出
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        }
        m_vao.release();
        m_initialized = true;
    }
}

void OpenGLWidget::paintGL()
{
    if (!m_initialized)
        return;
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    m_program.bind();

    m_program.setUniformValue("yTexture", 0);
    m_program.setUniformValue("uTexture", 1);
    m_program.setUniformValue("vTexture", 2);
    // bind textures on corresponding texture units
    for (unsigned int i = 0; i < 3; ++i)
    {
        m_textures[i]->bind(i);
        m_textures[i]->setData(
            0, QOpenGLTexture::PixelFormat::Red, QOpenGLTexture::PixelType::UInt8, static_cast<const void *>(m_pData[i]));
    }

    m_vao.bind();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void OpenGLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    m_width = w;
    m_height = h;
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

    m_vertexShader = new QOpenGLShader(QOpenGLShader::Vertex);
    m_vertexShader->compileSourceCode(vertexSource);
    if (!m_vertexShader->isCompiled())
    {
        qDebug() << "Vertex shader compile failed";
        return false;
    }

    m_fragmentShader = new QOpenGLShader(QOpenGLShader::Fragment);
    m_fragmentShader->compileSourceCode(fragmentSource);
    if (!m_fragmentShader->isCompiled())
    {
        qDebug() << "Fragment shader compile failed";
        return false;
    }
    m_program.addShader(m_vertexShader);
    m_program.addShader(m_fragmentShader);
    m_program.link();
    if (!m_program.isLinked())
    {
        qDebug() << "Shader program compile failed";
        return false;
    }

    delete m_vertexShader;
    delete m_fragmentShader;
    m_vertexShader = nullptr;
    m_fragmentShader = nullptr;
    return true;
}

void OpenGLWidget::clearData()
{
    for (unsigned int i = 0; i < sizeof(m_pData) / sizeof(unsigned char *); ++i)
    {
        if (m_pData[i] != nullptr)
        {
            delete[] m_pData[i];
            m_pData[i] = nullptr;
        }
    }
}
