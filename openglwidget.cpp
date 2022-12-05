#include "OpenGLWidget.h"

#include <assert.h>

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

    // OpenGLWidget::FrameFormat convertReadFormat2FrameFormat(const read_format_e &readFormat)
    //{
    //    switch (readFormat)
    //    {
    //        case read_format_e::RD_FMT_YUV420P:
    //            return OpenGLWidget::FrameFormat::YUV420;
    //        case read_format_e::RD_FMT_BGRA:
    //            return OpenGLWidget::FrameFormat::BGRA;
    //        case read_format_e::RD_FMT_RGBA:
    //            return OpenGLWidget::FrameFormat::RGBA;
    //        case read_format_e::RD_FMT_NV12:
    //        case read_format_e::RD_FMT_GRAY8:
    //        case read_format_e::RD_FMT_CODEC_H264:
    //        case read_format_e::RD_FMT_CODEC_MJPEG:
    //        case read_format_e::RD_FMT_YUVJ420P:
    //        case read_format_e::RD_FMT_RGB24:
    //        case read_format_e::RD_FMT_BGR24:
    //        case read_format_e::RD_FMT_ARGB:
    //        case read_format_e::RD_FMT_ABGR:
    //        case read_format_e::RD_FMT_NOT_SUPPORT:
    //            return OpenGLWidget::FrameFormat::UNKNOWN;
    //    };
    //}
} // namespace

// clang-format off
static const GLfloat kVertices[] = {
    // pos              // texture coords
    -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, // bottom left
     1.0f, -1.0f, 0.0f, 1.0f, 1.0f, // bottom right
     1.0f,  1.0f, 0.0f, 1.0f, 0.0f, // top right
    -1.0f,  1.0f, 0.0f, 0.0f, 0.0f  // top left
};

static const GLuint kIndices[] = {
    0, 1, 2, // first triangle
    0, 2, 3  // second triangle
};

static const char kVertexSource[] = {
    "#version 330 core\n"
    "layout (location = 0) in vec3 pos;\n"
    "layout (location = 1) in vec2 textureCoord;\n"
    "\n"
    "out vec2 texCoord;\n"
    "\n"
    "void main()\n"
    "{\n"
        "gl_Position = vec4(pos, 1.0);\n"
        "texCoord = textureCoord;\n"
    "}\n\0"
};

static const char kFragmentSource[] = {
    "#version 330 core\n"
    "out vec4 fragColor;\n"
    "in vec2 texCoord;\n"
    "\n"
    "uniform sampler2D yTexture;\n"
    "uniform sampler2D uTexture;\n"
    "uniform sampler2D vTexture;\n"
    "\n"
    "void main()\n"
    "{\n"
        "float y = texture(yTexture, texCoord).r - 0.063;\n"
        "float u = texture(uTexture, texCoord).r - 0.5;\n"
        "float v = texture(vTexture, texCoord).r - 0.5;\n"
        "\n"
        "float r = 1.164 * y + 1.596 * v;\n"
        "float g = 1.164 * y - 0.392 * u - 0.813 * v;\n"
        "float b = 1.164 * y + 2.017 * u;\n"
        "\n"
        "fragColor = vec4(r, g, b, 1.0);\n"
    "}\n\0"
};

static const char kBGRAFragmentSource[] = {
    "#version 330 core\n"
    "out vec4 fragColor;\n"
    "in vec2 texCoord;\n"
    "\n"
    "uniform sampler2D myTexture;\n"
    "\n"
    "void main()\n"
    "{\n"
        "fragColor = texture(myTexture, texCoord);\n"
    "}\n\0"
};
// clang-format on

OpenGLWidget::OpenGLWidget(QWidget *parent) :
    QOpenGLWidget(parent),
    m_initialized(false),
    m_width(0),
    m_height(0),
    m_videoWidth(0),
    m_videoHeight(0),
    m_strideY(0),
    m_strideU(0),
    m_strideV(0),
    m_frameFormat(FrameFormat::YUV420),
    m_needRender(false),
    m_frameCount(0),
    m_frameDropped(0),
    m_frameRendered(0),
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
    readYuvPic("thewitcher3_1889x1073.yuv", 1889, 1073);
}

OpenGLWidget::~OpenGLWidget()
{
    makeCurrent();

    for (unsigned int i = 0; i < sizeof(m_textures) / sizeof(QOpenGLTexture *); ++i)
    {
        if (m_textures[i])
        {
            delete m_textures[i];
            m_textures[i] = nullptr;
        }
    }

    if (m_vertexShader)
    {
        delete m_vertexShader;
        m_vertexShader = nullptr;
    }
    if (m_fragmentShader)
    {
        delete m_fragmentShader;
        m_fragmentShader = nullptr;
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

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_videoWidth != picWidth || m_videoHeight != picHeight)
    {
        m_videoWidth = picWidth;
        m_videoHeight = picHeight;
        m_strideY = m_videoWidth;
        m_strideU = m_strideV = (m_videoWidth + 1) / 2;
        qDebug() << "Stride Y: " << m_strideY << ", stride uv: " << m_strideU << ", width x height: " << m_videoWidth << "x"
                 << m_videoHeight;
        clearData();
        m_pData[0] = new unsigned char[m_strideY * m_videoHeight];
        m_pData[1] = new unsigned char[m_strideU * ((m_videoHeight + 1) / 2)];
        m_pData[2] = new unsigned char[m_strideV * ((m_videoHeight + 1) / 2)];
        // Need to recreate textures
        // recreateTextures();
    }

    int dataSize = i420DataSize(m_videoHeight, m_strideY, m_strideU, m_strideV);
    char *data = new char[dataSize];
    qDebug() << "Y size: " << m_strideY * m_videoHeight << ", UV size: " << m_strideU * ((m_videoHeight + 1) / 2)
             << ", total size: " << dataSize;
    // read all pic data
    picFile.read(data, dataSize);

    // Y
    memcpy(m_pData[0], data, m_strideY * m_videoHeight);

    // U
    memcpy(m_pData[1], data + m_strideY * m_videoHeight, m_strideU * ((m_videoHeight + 1) / 2));

    // V
    memcpy(m_pData[2], data + m_strideY * m_videoHeight + m_strideU * ((m_videoHeight + 1) / 2), m_strideV * ((m_videoHeight + 1) / 2));

    // free resource
    delete[] data;
    picFile.close();
    m_needRender = true;
}

void OpenGLWidget::onPicure(const QPixmap &pix)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_frameCount;
    if (m_needRender)
    {
        ++m_frameDropped;
        return;
    }

    QImage img = pix.toImage();
    img = img.convertToFormat(QImage::Format::Format_RGBA8888);
    if (m_videoWidth != img.width() || m_videoHeight != img.height() || m_frameFormat != FrameFormat::RGBA)
    {
        m_videoWidth = img.width();
        m_videoHeight = img.height();
        m_frameFormat = FrameFormat::RGBA;
        reinitNecessaryResource();
        qDebug() << "Stride Y: " << m_strideY << ", stride uv: " << m_strideU << ", width x height: " << m_videoWidth << "x"
                 << m_videoHeight << "format: " << static_cast<int>(m_frameFormat);
    }
    qDebug() << "Img size: " << img.size() << ", img bytesPerLine: " << img.bytesPerLine();
    memcpy(m_pData[0], img.bits(), m_videoWidth * m_videoHeight * 4);
    m_needRender = true;
    update();
}

void OpenGLWidget::reinitNecessaryResource()
{
    qDebug() << "ReinitNecessaryResource";
    clearData();
    switch (m_frameFormat)
    {
        case FrameFormat::YUV420:
            m_strideY = m_videoWidth;
            m_strideU = m_strideV = (m_videoWidth + 1) / 2;
            m_pData[0] = new unsigned char[m_strideY * m_videoHeight];
            m_pData[1] = new unsigned char[m_strideU * ((m_videoHeight + 1) / 2)];
            m_pData[2] = new unsigned char[m_strideV * ((m_videoHeight + 1) / 2)];
            createShaders(kVertexSource, kFragmentSource);
            break;
        case FrameFormat::RGBA:
            // createShaders(kVertexSource, kRGBAFragmentSource);
        case FrameFormat::BGRA:
            m_strideY = 0;
            m_strideU = m_strideV = 0;
            m_pData[0] = new unsigned char[m_videoWidth * m_videoHeight * 4];
            createShaders(kVertexSource, kBGRAFragmentSource);
            break;
        case FrameFormat::UNKNOWN:
            qDebug() << "Unsupport video format!";
            break;
    }
    // Recreate the textures
    recreateTextures();
}

void OpenGLWidget::recreateTextures()
{
    qDebug() << "RecreateTextures";
    for (unsigned int i = 0; i < sizeof(m_textures) / sizeof(QOpenGLTexture *); ++i)
    {
        if (m_textures[i])
        {
            delete m_textures[i];
            m_textures[i] = nullptr;
        }
    }

    switch (m_frameFormat)
    {
        case FrameFormat::YUV420:
            for (unsigned int i = 0; i < sizeof(m_textures) / sizeof(QOpenGLTexture *); ++i)
            {
                m_textures[i] = new QOpenGLTexture(QOpenGLTexture::Target2D);
                // set the texture wrapping parameters
                m_textures[i]->setWrapMode(QOpenGLTexture::CoordinateDirection::DirectionS, QOpenGLTexture::WrapMode::Repeat);
                m_textures[i]->setWrapMode(QOpenGLTexture::CoordinateDirection::DirectionT, QOpenGLTexture::WrapMode::Repeat);
                m_textures[i]->setMinMagFilters(QOpenGLTexture::Filter::Linear, QOpenGLTexture::Filter::Linear);
                m_textures[i]->setFormat(QOpenGLTexture::TextureFormat::R8_UNorm);
                if (i == 0)
                {
                    m_textures[i]->setSize(m_strideY, m_videoHeight);
                }
                else if (i == 1)
                {
                    m_textures[i]->setSize(m_strideU, (m_videoHeight + 1) / 2);
                }
                else if (i == 2)
                {
                    m_textures[i]->setSize(m_strideV, (m_videoHeight + 1) / 2);
                }
                m_textures[i]->allocateStorage(QOpenGLTexture::PixelFormat::Red, QOpenGLTexture::PixelType::UInt8);
                m_textures[i]->generateMipMaps();
            }
            break;
        case FrameFormat::RGBA:
            // m_textures[0] = new QOpenGLTexture(QOpenGLTexture::Target2D);
            // m_textures[0]->setWrapMode(QOpenGLTexture::CoordinateDirection::DirectionS, QOpenGLTexture::WrapMode::Repeat);
            // m_textures[0]->setWrapMode(QOpenGLTexture::CoordinateDirection::DirectionT, QOpenGLTexture::WrapMode::Repeat);
            // m_textures[0]->setMinMagFilters(QOpenGLTexture::Filter::Linear, QOpenGLTexture::Filter::Linear);
            // m_textures[0]->setFormat(QOpenGLTexture::TextureFormat::RGBA8_UNorm);
            // m_textures[0]->setSize(m_videoWidth, m_videoHeight);
            // m_textures[0]->allocateStorage(QOpenGLTexture::PixelFormat::RGBA, QOpenGLTexture::PixelType::UInt8);
            // m_textures[0]->generateMipMaps();
            // break;
        case FrameFormat::BGRA:
            m_textures[0] = new QOpenGLTexture(QOpenGLTexture::Target2D);
            m_textures[0]->setWrapMode(QOpenGLTexture::CoordinateDirection::DirectionS, QOpenGLTexture::WrapMode::Repeat);
            m_textures[0]->setWrapMode(QOpenGLTexture::CoordinateDirection::DirectionT, QOpenGLTexture::WrapMode::Repeat);
            m_textures[0]->setMinMagFilters(QOpenGLTexture::Filter::Linear, QOpenGLTexture::Filter::Linear);
            m_textures[0]->setFormat(QOpenGLTexture::TextureFormat::RGBA8_UNorm);
            m_textures[0]->setSize(m_videoWidth, m_videoHeight);
            // m_textures[0]->allocateStorage(QOpenGLTexture::PixelFormat::BGRA, QOpenGLTexture::PixelType::UInt8);
            m_textures[0]->allocateStorage();
            m_textures[0]->generateMipMaps();
            break;
        case FrameFormat::UNKNOWN:
            qDebug() << "RecreateTextures: unsupport video format!";
            break;
    }
}

// void OpenGLWidget::onFrame(const vframe_s &frame)
//{
//    std::lock_guard<std::mutex> lock(m_mutex);
//    ++m_frameCount;
//    FrameFormat frameFormat = convertReadFormat2FrameFormat(frame.fmt);
//    if (m_needRender || frameFormat == FrameFormat::UNKNOWN)
//    {
//        ++m_frameDropped;
//        return;
//    }
//
//    if (m_videoWidth != frame.width || m_videoHeight != frame.height || m_frameFormat != frameFormat)
//    {
//        m_videoWidth = frame.width;
//        m_videoHeight = frame.height;
//        m_frameFormat = frameFormat;
//        reinitNecessaryResource();
//        qDebug() << "Stride Y: " << m_strideY << ", stride uv: " << m_strideU << ", width x height: " << m_videoWidth << "x"
//                 << m_videoHeight << "format: " << static_cast<int>(m_frameFormat);
//    }
//
//    switch (m_frameFormat)
//    {
//        // TODO(hcb): 确定数据是这样存储的吗？
//        case FrameFormat::YUV420:
//            // Y
//            memcpy(m_pData[0], frame.data[0], m_strideY * m_videoHeight);
//            // U
//            memcpy(m_pData[1], frame.data[1], m_strideU * ((m_videoHeight + 1) / 2));
//            // V
//            memcpy(m_pData[2], frame.data[2], m_strideV * ((m_videoHeight + 1) / 2));
//            break;
//        case FrameFormat::RGBA:
//        case FrameFormat::BGRA:
//            memcpy(m_pData[0], frame.data[0], m_videoWidth * m_videoHeight * 4);
//            break;
//        case FrameFormat::UNKNOWN:
//            qDebug() << "Unsupport video format!";
//            break;
//    }
//    m_needRender = true;
//    update();
//}

void OpenGLWidget::setVideoFrameFormat(const FrameFormat &new_format)
{
    if (m_frameFormat == new_format)
        return;
    m_frameFormat = new_format;
    std::lock_guard<std::mutex> lock(m_mutex);
    reinitNecessaryResource();
    m_needRender = false;
}

OpenGLWidget::FrameFormat OpenGLWidget::videoFrameFormat() const
{
    return m_frameFormat;
}

void OpenGLWidget::initializeGL()
{
    qDebug() << "InitializeGL";
    // if (m_initialized)
    // return;
    assert(!m_initialized);

    initializeOpenGLFunctions();
    if (!createShaders(kVertexSource, kFragmentSource))
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
    recreateTextures();
    // 由于OpenGL内部是4字节对齐的, 为避免像素值不符合要求导致的错误, 设定不足字节对齐的位数按照1字节取出
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    m_vao.release();

    m_initialized = true;
}

void OpenGLWidget::paintGL()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    //if (!m_needRender)
    //    return;
    if (!m_initialized)
    {
        ++m_frameDropped;
        return;
    }

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    m_program.bind();

    // bind textures on corresponding texture units
    switch (m_frameFormat)
    {
        case FrameFormat::YUV420:
            m_program.setUniformValue("yTexture", 0);
            m_program.setUniformValue("uTexture", 1);
            m_program.setUniformValue("vTexture", 2);
            for (unsigned int i = 0; i < sizeof(m_textures) / sizeof(QOpenGLTexture *); ++i)
            {
                m_textures[i]->bind(i);
                if (m_pData[i])
                    m_textures[i]->setData(
                        0, QOpenGLTexture::PixelFormat::Red, QOpenGLTexture::PixelType::UInt8, static_cast<const void *>(m_pData[i]));
            }
            break;
        case FrameFormat::RGBA:
            m_program.setUniformValue("myTexture", 0);
            m_textures[0]->bind(0);
            if (m_pData[0])
                m_textures[0]->setData(
                    0, QOpenGLTexture::PixelFormat::RGBA, QOpenGLTexture::PixelType::UInt8, static_cast<const void *>(m_pData[0]));
            break;
        case FrameFormat::BGRA:
            m_program.setUniformValue("myTexture", 0);
            m_textures[0]->bind(0);
            if (m_pData[0])
                m_textures[0]->setData(
                    0, QOpenGLTexture::PixelFormat::BGRA, QOpenGLTexture::PixelType::UInt8, static_cast<const void *>(m_pData[0]));
            break;
    }

    m_vao.bind();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    clearData();
    qDebug() << "PaintGL";
    m_needRender = false;
    ++m_frameRendered;
}

void OpenGLWidget::resizeGL(int w, int h)
{
    qDebug() << "ResizeGL";
    glViewport(0, 0, w, h);
    m_width = w;
    m_height = h;
    // std::lock_guard<std::mutex> lock(m_mutex);
    // m_needRender = true;
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

    return createShaders(vertexSource, fragmentSource);
}

bool OpenGLWidget::createShaders(const char *vertexSource, const char *fragmentSource)
{
    m_program.removeAllShaders();
    if (m_vertexShader)
    {
        delete m_vertexShader;
        m_vertexShader = nullptr;
    }
    m_vertexShader = new QOpenGLShader(QOpenGLShader::Vertex);
    m_vertexShader->compileSourceCode(vertexSource);
    if (!m_vertexShader->isCompiled())
    {
        qDebug() << "Vertex shader compile failed";
        return false;
    }

    if (m_fragmentShader)
    {
        delete m_fragmentShader;
        m_fragmentShader = nullptr;
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

    // delete m_vertexShader;
    // delete m_fragmentShader;
    // m_vertexShader = nullptr;
    // m_fragmentShader = nullptr;
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
