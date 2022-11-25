#include "openglwidget.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    OpenGLWidget w;
    w.show();
    QPixmap pix("D:/test_video/yuvplay-myyuvrender.png");
    w.onPicure(pix);
    return a.exec();
}
