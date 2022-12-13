#include "openglwidget.h"

#include <assert.h>

//#include <fstream>
#include <string>

#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QTimer>

namespace
{
// clang-format off
const GLfloat kVertices[] = {
    // pos              // texture coords
    -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, // bottom left
     1.0f, -1.0f, 0.0f, 1.0f, 1.0f, // bottom right
     1.0f,  1.0f, 0.0f, 1.0f, 0.0f, // top right
    -1.0f,  1.0f, 0.0f, 0.0f, 0.0f  // top left
};

const GLuint kIndices[] = {
    0, 1, 2, // first triangle
    0, 2, 3  // second triangle
};

const char kVertexSource[] = {
    "attribute vec3 a_Position;\n"
    "attribute vec2 a_TexCoords;\n"
    "\n"
    "varying vec2 v_TexCoords;\n"
    "\n"
    "void main()\n"
    "{\n"
        "gl_Position = vec4(a_Position, 1.0);\n"
        "v_TexCoords = a_TexCoords;\n"
    "}\n\0"
};

// for I420(16-255) convert to rgb
const char kFragmentSource[] = {
    "varying vec2 v_TexCoords;\n"
    "\n"
    "uniform sampler2D u_Texture0;\n"
    "uniform sampler2D u_Texture1;\n"
    "uniform sampler2D u_Texture2;\n"
    "\n"
    "void main()\n"
    "{\n"
        "float y = texture(u_Texture0, v_TexCoords).r - 0.063;\n"
        "float u = texture(u_Texture1, v_TexCoords).r - 0.5;\n"
        "float v = texture(u_Texture2, v_TexCoords).r - 0.5;\n"
        "\n"
        "float r = 1.164 * y + 1.596 * v;\n"
        "float g = 1.164 * y - 0.392 * u - 0.813 * v;\n"
        "float b = 1.164 * y + 2.017 * u;\n"
        "\n"
        "gl_FragColor = vec4(r, g, b, 1.0);\n"
    "}\n\0"
};

const char kRGBXFragmentSource[] = {
    "varying vec2 v_TexCoords;\n"
    "\n"
    "uniform sampler2D u_Texture0;\n"
    "\n"
    "void main()\n"
    "{\n"
        "gl_FragColor = texture(u_Texture0, v_TexCoords);\n"
    "}\n\0"
};
// clang-format on

OpenGLWidget::FrameFormat convertToFrameFormat(
    VideoFrameBuffer::Type type)
{
    switch (type)
    {
        case VideoFrameBuffer::Type::kI420:
            return OpenGLWidget::FrameFormat::YUV420P;
        case VideoFrameBuffer::Type::kNative:
        case VideoFrameBuffer::Type::kI420A:
        case VideoFrameBuffer::Type::kI422:
        case VideoFrameBuffer::Type::kI444:
        case VideoFrameBuffer::Type::kI010:
        case VideoFrameBuffer::Type::kI210:
        case VideoFrameBuffer::Type::kNV12:
        default:
            return OpenGLWidget::FrameFormat::UNKNOWN;
    }

    return OpenGLWidget::FrameFormat::UNKNOWN;
}

// OpenGLHelper function
int GLSLVersion()
{
    static int v = -1;
    if (v >= 0)
        return v;
    if (!QOpenGLContext::currentContext())
    {
        qWarning("%s: current context is null", __FUNCTION__);
        return 0;
    }
    const char *vs =
        (const char *)(QOpenGLContext::currentContext()
                           ->functions()
                           ->glGetString(GL_SHADING_LANGUAGE_VERSION));
    int major = 0, minor = 0;
    // es: "OpenGL ES GLSL ES 1.00 (ANGLE 2.1.99...)" can use ""%*[ a-zA-Z]
    // %d.%d" in sscanf, desktop: "2.1"
    // QRegExp rx("(\\d+)\\.(\\d+)");
    if (strncmp(vs, "OpenGL ES GLSL ES ", 18) == 0)
        vs += 18;
    if (sscanf(vs, "%d.%d", &major, &minor) == 2)
    {
        v = major * 100 + minor;
    }
    else
    {
        qWarning(
            "Failed to detect glsl version using GL_SHADING_LANGUAGE_VERSION!");
        v = 110;
    }
    qDebug("GLSL Version: %d", v);
    return v;
}

// current shader works fine for gles 2~3 only with commonShaderHeader(). It's
// mainly for desktop core profile
QByteArray commonShaderHeader(QOpenGLShader::ShaderType type)
{
    QByteArray h;
    h += "#define highp\n"
         "#define mediump\n"
         "#define lowp\n";

    if (type == QOpenGLShader::Fragment)
    {
        // >=1.30: texture(sampler2DRect,...). 'texture' is defined in header
        // we can't check GLSLVersion() here because it the actually version
        // used can be defined by "#version"
        h += "#if __VERSION__ < 130\n"
             "#define texture texture2D\n"
             "#else\n"
             "#define texture2D texture\n"
             "#endif // < 130\n";
    }
    return h;
}

QByteArray compatibleShaderHeader(QOpenGLShader::ShaderType type)
{
    QByteArray h;
    // #version directive must occur in a compilation unit before anything else,
    // except for comments and white spaces. Default is 100 if not set
    h.append("#version ").append(QByteArray::number(GLSLVersion()));
    h += "\n";
    h += commonShaderHeader(type);
    if (GLSLVersion() >= 130)
    {  // gl 3
        if (type == QOpenGLShader::Vertex)
        {
            h += "#define attribute in\n"
                 "#define varying out\n";
        }
        else if (type == QOpenGLShader::Fragment)
        {
            h +=
                "#define varying in\n"
                "#define gl_FragColor out_color\n"  // can not starts with 'gl_'
                "out vec4 gl_FragColor;\n";
        }
    }
    return h;
}

}  // namespace

OpenGLWidget::OpenGLWidget(QWidget *parent)
    : QOpenGLWidget(parent),
      m_initialized(false),
      m_starting(false),
      m_videoWidth(0),
      m_videoHeight(0),
      m_strideY(0),
      m_strideU(0),
      m_strideV(0),
      m_scale(1),
      m_frameFormat(FrameFormat::YUV420P),
      m_needRender(false),
      m_recvFirstFrame(false),
      m_frameCount(0),
      m_frameDropped(0),
      m_frameRendered(0),
      m_vbo(QOpenGLBuffer::VertexBuffer),
      m_ibo(QOpenGLBuffer::IndexBuffer),
      m_posLocation(0),
      m_texCoordsLocation(1),
      m_program(nullptr)
{
    for (unsigned int i = 0; i < sizeof(m_textures) / sizeof(QOpenGLTexture *);
         ++i)
    {
        m_textures[i] = nullptr;
    }
    // 输出统计信息
    QTimer *timer = new QTimer(this);
    connect(
        timer, &QTimer::timeout, this,
        QOverload<>::of(&OpenGLWidget::printRenderStats));
    timer->start(1000);
}

OpenGLWidget::~OpenGLWidget()
{
    makeCurrent();

    if (m_program)
    {
        m_program->removeAllShaders();
        delete m_program;
        m_program = nullptr;
    }

    for (unsigned int i = 0; i < sizeof(m_textures) / sizeof(QOpenGLTexture *);
         ++i)
    {
        if (m_textures[i])
        {
            delete m_textures[i];
            m_textures[i] = nullptr;
        }
    }

    if (m_ibo.isCreated())
        m_ibo.destroy();
    if (m_vbo.isCreated())
        m_vbo.destroy();
    if (m_vao.isCreated())
        m_vao.destroy();

    clearFrame();

    doneCurrent();
}

void OpenGLWidget::onFrame(const VideoFrame &videoFrame)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized || !m_starting)
        return;
    ++m_frameCount;
    FrameFormat frameFormat = convertToFrameFormat(videoFrame.type());
    // 前一帧未渲染完成时，丢弃后来的一帧
    if (m_needRender ||
        (videoFrame.width() <= 0 && videoFrame.height() <= 0) ||
        frameFormat == FrameFormat::UNKNOWN)
    {
        ++m_frameDropped;
        return;
    }

    if (m_videoWidth != videoFrame.width() ||
        m_videoHeight != videoFrame.height() || m_frameFormat != frameFormat)
    {
        m_videoWidth = videoFrame.width();
        m_videoHeight = videoFrame.height();
        m_frameFormat = frameFormat;
        if (!init())
        {
            qDebug() << "Reinit failed!";
            return;
        }
        qDebug() << "Stride Y: " << m_strideY << ", stride uv: " << m_strideU
                 << ", width x height: " << m_videoWidth << "x" << m_videoHeight
                 << "format: " << static_cast<int>(m_frameFormat);
    }

    m_recvFirstFrame = true;
    m_frameQueue.emplace(videoFrame);
    m_needRender = true;
    update();
}

void OpenGLWidget::printRenderStats() const
{
    static int64_t lastTimeRenderedFrames = 0;
    if (!m_starting)
        return;
    qDebug() << "Time " << m_times << ", Frame Count: " << m_frameCount
             << ", Frame Rendered: " << m_frameRendered
             << ", Frame Dropped: " << m_frameDropped
             << ", FPS: " << m_frameRendered - lastTimeRenderedFrames;
    lastTimeRenderedFrames = m_frameRendered;
    ++m_times;
}

bool OpenGLWidget::createShaders(
    const QString &vertexSourcePath, const QString &fragmentSourcePath)
{
    QFile vertexSourceFile(vertexSourcePath);
    if (!vertexSourceFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Open vertex source file failed";
        return false;
    }
    QByteArray vertexSource = vertexSourceFile.readAll();
    vertexSourceFile.close();

    QFile fragmentSourceFile(fragmentSourcePath);
    if (!fragmentSourceFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Open fragrament source file failed";
        return false;
    }
    QByteArray fragmentSource = fragmentSourceFile.readAll();
    fragmentSourceFile.close();

    return createShaders(vertexSource, fragmentSource);
}

bool OpenGLWidget::createShaders(
    QByteArray vertexSource, QByteArray fragmentSource)
{
    if (!m_program)
        m_program = new QOpenGLShaderProgram();

    if (m_program->isLinked())
    {
        qWarning() << "Shader program is already linked";
        // TODO(bug):
        // 部分电脑上存在无法使用原ShaderProgram的问题,必须在此处重新创建,暂时这样规避
        // 可能是由于创建了多个OpenGLWidget实例导致的？
        m_program->removeAllShaders();
        delete m_program;
        m_program = nullptr;
        m_program = new QOpenGLShaderProgram();
    }

    m_program->removeAllShaders();
    vertexSource.prepend(compatibleShaderHeader(QOpenGLShader::Vertex));
    fragmentSource.prepend(compatibleShaderHeader(QOpenGLShader::Fragment));
    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexSource);
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentSource);

    m_program->bindAttributeLocation("a_Position", m_posLocation);
    m_program->bindAttributeLocation("a_texCoords", m_texCoordsLocation);
    qDebug("Bind attribute: %s => %d", "a_Position", m_posLocation);
    qDebug("Bind attribute: %s => %d", "a_texCoords", m_texCoordsLocation);

    if (!m_program->link())
    {
        qDebug() << "Shader program compile failed";
        return false;
    }
    return true;
}

void OpenGLWidget::recreateTextures()
{
    qDebug() << "RecreateTextures";
    for (unsigned int i = 0; i < sizeof(m_textures) / sizeof(QOpenGLTexture *);
         ++i)
    {
        if (m_textures[i])
        {
            delete m_textures[i];
            m_textures[i] = nullptr;
        }
    }

    switch (m_frameFormat)
    {
        case FrameFormat::YUV420P:
            for (unsigned int i = 0;
                 i < sizeof(m_textures) / sizeof(QOpenGLTexture *); ++i)
            {
                m_textures[i] = new QOpenGLTexture(QOpenGLTexture::Target2D);
                // set the texture wrapping parameters
                m_textures[i]->setWrapMode(
                    QOpenGLTexture::CoordinateDirection::DirectionS,
                    QOpenGLTexture::WrapMode::Repeat);
                m_textures[i]->setWrapMode(
                    QOpenGLTexture::CoordinateDirection::DirectionT,
                    QOpenGLTexture::WrapMode::Repeat);
                m_textures[i]->setMinMagFilters(
                    QOpenGLTexture::Filter::Linear,
                    QOpenGLTexture::Filter::Linear);
                m_textures[i]->setFormat(
                    QOpenGLTexture::TextureFormat::R8_UNorm);
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
                m_textures[i]->allocateStorage(
                    QOpenGLTexture::PixelFormat::Red,
                    QOpenGLTexture::PixelType::UInt8);
                m_textures[i]->generateMipMaps();
            }
            break;
        case FrameFormat::RGBA:
        case FrameFormat::BGRA:
            m_textures[0] = new QOpenGLTexture(QOpenGLTexture::Target2D);
            m_textures[0]->setWrapMode(
                QOpenGLTexture::CoordinateDirection::DirectionS,
                QOpenGLTexture::WrapMode::Repeat);
            m_textures[0]->setWrapMode(
                QOpenGLTexture::CoordinateDirection::DirectionT,
                QOpenGLTexture::WrapMode::Repeat);
            m_textures[0]->setMinMagFilters(
                QOpenGLTexture::Filter::Linear, QOpenGLTexture::Filter::Linear);
            m_textures[0]->setFormat(
                QOpenGLTexture::TextureFormat::RGBA8_UNorm);
            m_textures[0]->setSize(m_videoWidth, m_videoHeight);
            m_textures[0]->allocateStorage();
            m_textures[0]->generateMipMaps();
            break;
        case FrameFormat::RGB:
            m_textures[0] = new QOpenGLTexture(QOpenGLTexture::Target2D);
            m_textures[0]->setWrapMode(
                QOpenGLTexture::CoordinateDirection::DirectionS,
                QOpenGLTexture::WrapMode::Repeat);
            m_textures[0]->setWrapMode(
                QOpenGLTexture::CoordinateDirection::DirectionT,
                QOpenGLTexture::WrapMode::Repeat);
            m_textures[0]->setMinMagFilters(
                QOpenGLTexture::Filter::Linear, QOpenGLTexture::Filter::Linear);
            m_textures[0]->setFormat(QOpenGLTexture::TextureFormat::RGB8_UNorm);
            m_textures[0]->setSize(m_videoWidth, m_videoHeight);
            m_textures[0]->allocateStorage();
            m_textures[0]->generateMipMaps();
            break;
        case FrameFormat::UNKNOWN:
            qDebug() << "RecreateTextures: unsupport video format!";
            break;
    }
}

void OpenGLWidget::setScale(int scale)
{
    m_scale = scale;
}

bool OpenGLWidget::checkOpenGLVersion(float requireMinVersion)
{
    if (!QOpenGLContext::currentContext())
    {
        qWarning("%s: current context is null", __FUNCTION__);
        return false;
    }
    const char *vs = reinterpret_cast<const char *>(
        QOpenGLContext::currentContext()->functions()->glGetString(GL_VERSION));
    float currentVersion = 0.0f;
    int major = 0, minor = 0;
    if (sscanf(vs, "%d.%d", &major, &minor) == 2)
    {
        currentVersion = double(major) + (double)(minor / 10.0f);
    }
    else
    {
        qWarning("Failed to detect opengl version using GL_VERSION!");
        currentVersion = 1.0;
    }
    qDebug("OpenGL Version: %f", currentVersion);

    if (currentVersion < requireMinVersion)
    {
        qWarning(
            "Current OpenGL version %f does not meet the required min version "
            "%f",
            currentVersion, requireMinVersion);
        return false;
    }

    return true;
}

void OpenGLWidget::setVideoParams(
    const FrameFormat &format, int width, int height)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_frameFormat != format || m_videoWidth != width || m_videoHeight != height)
    {
        m_frameFormat = format;
        m_videoWidth = width;
        m_videoHeight = height;
        if (!init())
        {
            qDebug() << "Set video frame format failed!";
        }
        m_needRender = false;
    }
}

void OpenGLWidget::setVideoFrameFormat(const FrameFormat &new_format)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_frameFormat == new_format)
        return;
    m_frameFormat = new_format;
    if (!init())
    {
        qDebug() << "Set video frame format failed!";
    }
    m_needRender = false;
}

OpenGLWidget::FrameFormat OpenGLWidget::videoFrameFormat() const
{
    return m_frameFormat;
}

void OpenGLWidget::startPlay()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_starting = true;
}

void OpenGLWidget::stopPlay()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_starting = false;
}

void OpenGLWidget::initializeGL()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    qDebug() << "InitializeGL";
    assert(!m_initialized);

    initializeOpenGLFunctions();
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    const GLubyte *vendorName = f->glGetString(GL_VENDOR);
    const GLubyte *renderer = f->glGetString(GL_RENDERER);
    const GLubyte *openGLVersion = f->glGetString(GL_VERSION);
    qDebug(
        "Vendor: %s\n Renderer: %s\n OpenGL Version: %s\n", vendorName,
        renderer, openGLVersion);

    // 由于OpenGL内部是4字节对齐的, 为避免像素值不符合要求导致的错误,
    // 设定不足字节对齐的位数按照1字节取出
     f->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    if (!init())
    {
        qDebug() << "OpenGLWidget init failed";
        return;
    }

    m_initialized = true;
}

void OpenGLWidget::paintGL()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    // 无需渲染时，直接返回
    if (!m_needRender)
    {
        return;
    }

    // 未初始化/未启动/未接收到首帧时，重置m_needRender状态，无需渲染
    if (!m_initialized || !m_starting || !m_recvFirstFrame)
    {
        m_needRender = false;
        return;
    }

    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    f->glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    f->glClear(GL_COLOR_BUFFER_BIT);

    std::shared_ptr<I420BufferInterface> frameBuffer;
    if (m_frameQueue.empty())
    {
        frameBuffer = nullptr;
    }
    else
    {
        frameBuffer = m_frameQueue.front().videoFrameBuffer()->toI420();
    }

    m_program->bind();

    // bind textures on corresponding texture units
    switch (m_frameFormat)
    {
        case FrameFormat::YUV420P:
            m_program->setUniformValue("u_Texture0", 0);
            m_program->setUniformValue("u_Texture1", 1);
            m_program->setUniformValue("u_Texture2", 2);
            m_textures[0]->bind(0);
            m_textures[1]->bind(1);
            m_textures[2]->bind(2);
            if (frameBuffer)
            {
                m_textures[0]->setData(
                    0, QOpenGLTexture::PixelFormat::Red,
                    QOpenGLTexture::PixelType::UInt8, frameBuffer->dataY());
                m_textures[1]->setData(
                    0, QOpenGLTexture::PixelFormat::Red,
                    QOpenGLTexture::PixelType::UInt8, frameBuffer->dataU());
                m_textures[2]->setData(
                    0, QOpenGLTexture::PixelFormat::Red,
                    QOpenGLTexture::PixelType::UInt8, frameBuffer->dataV());
            }
            break;
        case FrameFormat::RGBA:
        case FrameFormat::BGRA:
        case FrameFormat::RGB:
        case FrameFormat::UNKNOWN:
            return;
    }

    m_vao.bind();
    f->glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    m_vao.release();
    m_program->release();

    if (!m_frameQueue.empty())
    {
        m_frameQueue.pop();
        ++m_frameRendered;
    }

    m_needRender = false;
}

void OpenGLWidget::resizeGL(int w, int h)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    // qDebug() << "ResizeGL";
    glViewport(0, 0, w, h);
    m_needRender = true;
}

bool OpenGLWidget::init()
{
    clearFrame();
    QByteArray vertexSource;
    QByteArray fragmentSource;
    switch (m_frameFormat)
    {
        case FrameFormat::YUV420P:
            m_strideY = m_videoWidth;
            m_strideU = m_strideV = (m_videoWidth + 1) / 2;
            vertexSource =
                QByteArray(kVertexSource, sizeof(kVertexSource) / sizeof(char));
            fragmentSource = QByteArray(
                kFragmentSource, sizeof(kFragmentSource) / sizeof(char));
            break;
        case FrameFormat::RGBA:
        case FrameFormat::BGRA:
            m_strideY = 0;
            m_strideU = m_strideV = 0;
            vertexSource =
                QByteArray(kVertexSource, sizeof(kVertexSource) / sizeof(char));
            fragmentSource = QByteArray(
                kRGBXFragmentSource,
                sizeof(kRGBXFragmentSource) / sizeof(char));
            break;
        case FrameFormat::RGB:
            m_strideY = 0;
            m_strideU = m_strideV = 0;
            vertexSource =
                QByteArray(kVertexSource, sizeof(kVertexSource) / sizeof(char));
            fragmentSource = QByteArray(
                kRGBXFragmentSource,
                sizeof(kRGBXFragmentSource) / sizeof(char));
            break;
        case FrameFormat::UNKNOWN:
            qWarning() << "Unsupport video format!";
            return false;
    }

    if (!createShaders(vertexSource, fragmentSource))
        return false;

    if (!m_vao.isCreated())
    {
        m_vao.create();
        m_vao.bind();
    }

    if (!m_vbo.isCreated())
    {
        m_vbo.create();
        m_vbo.bind();
        m_vbo.setUsagePattern(QOpenGLBuffer::UsagePattern::StaticDraw);
        m_vbo.allocate(kVertices, sizeof(kVertices));
    }

    if (!m_ibo.isCreated())
    {
        m_ibo.create();
        m_ibo.bind();
        m_ibo.setUsagePattern(QOpenGLBuffer::UsagePattern::StaticDraw);
        m_ibo.allocate(kIndices, sizeof(kIndices));
    }

    // position attribute
    m_program->setAttributeBuffer(
        m_posLocation, GL_FLOAT, 0, 3, 5 * sizeof(GLfloat));
    m_program->enableAttributeArray(m_posLocation);
    // texture coord attribute
    m_program->setAttributeBuffer(
        m_texCoordsLocation, GL_FLOAT, 3 * sizeof(GLfloat), 2,
        5 * sizeof(GLfloat));
    m_program->enableAttributeArray(m_texCoordsLocation);

    // create textures
    recreateTextures();
    m_vao.release();
    return true;
}

void OpenGLWidget::clearFrame()
{
    while (!m_frameQueue.empty())
        m_frameQueue.pop();
}