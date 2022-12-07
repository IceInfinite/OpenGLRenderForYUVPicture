#include <fstream>

#include <QApplication>

#include "openglwidget.h"
#include "videoframe.h"
#include "videoframebuffer.h"

void readYuvPic(
    const char *picPath, int picWidth, int picHeight,
    std::shared_ptr<VideoFrame> &frame)
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

    frame = std::make_shared<VideoFrame>(buffer, 0);

    // free resource
    picFile.close();
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    OpenGLWidget w;
    w.show();
    //w.startPlay();
    //std::shared_ptr<VideoFrame> frame = nullptr;
    //readYuvPic("D:/test_video/thewitcher3_1889x1073.yuv", 1889, 1073, frame);
    //w.onFrame(frame);

    return a.exec();
}
