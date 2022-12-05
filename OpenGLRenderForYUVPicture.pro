QT       += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    openglwidget.cpp

HEADERS += \
    openglwidget.h

FORMS +=

INCLUDEPATH += $$PWD/third_party/ffmpeg/include

# Now only support x64
contains(QT_ARCH, i386) {
    message("32-bit")
    LIBS += -L$$PWD/third_party/ffmpeg/lib/x86
} else {
    message("64-bit")
    LIBS += -L$$PWD/third_party/ffmpeg/lib/x64
}

LIBS += -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lpostproc -lswresample -lswscale

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    res/fragmentshader.frag \
    res/vertexshader.vert

# Copies the given files to the destination directory
#defineTest(copyToDestdir) {
#    files = $$1  dir = $$2

#    for(FILE, files) {
#        DDIR = $$OUT_PWD

#        # Replace slashes in paths with backslashes for Windows
#        win32:FILE ~= s,/,\\,g
#        win32:DDIR ~= s,/,\\,g

#        QMAKE_POST_LINK += $$QMAKE_COPY $$quote($$FILE) $$quote($$DDIR)$$escape_expand(\\n\\t)
#    }
#    export(QMAKE_POST_LINK)
#}
#copyToDestdir($$PWD/res/fragmentshader.frag $$PWD/res/vertexshader.vert $$PWD/res/thewitcher3.yuv)

#bin need to copy
EXTRA_BINFILES += \
    $$PWD/res/thewitcher3_1920x1080.yuv \
    $$PWD/res/thewitcher3_1907x1080.yuv \
    $$PWD/res/thewitcher3_1889x1073.yuv
#        $${THIRDPARTY_PATH}/gstreamer-0.10/linux/plugins/libgstrtp.so \
#        $${THIRDPARTY_PATH}/gstreamer-0.10/linux/plugins/libgstvideo4linux2.so

#res need to copy
EXTRA_RESOURCES += \
    $$PWD/res/vertexshader.vert \
    $$PWD/res/fragmentshader.frag

linux-g++{
    for(FILE, EXTRA_BINFILES){
        QMAKE_POST_LINK += $$quote(cp $$FILE $$OUT_PWD$$escape_expand(\n\t))
    }
    QMAKE_POST_LINK += $$quote(cmd /c [ -d $$OUT_PWD/res ] && echo exist || mkdir $$OUT_PWD/res$$escape_expand(\n\t))
    for(RESOURCE, EXTRA_RESOURCES){
        QMAKE_POST_LINK += $$quote(cp $$RESOURCE $$OUT_PWD/res$$escape_expand(\n\t))
    }
}

win32 {
    EXTRA_BINFILES_WIN = $$EXTRA_BINFILES
    EXTRA_BINFILES_WIN ~= s,/,\\,g
    DESTDIR_WIN = $$OUT_PWD
    DESTDIR_WIN ~= s,/,\\,g
    for(FILE, EXTRA_BINFILES_WIN){
        QMAKE_POST_LINK += $$quote(cmd /c copy /y $$FILE $$DESTDIR_WIN$$escape_expand(\n\t))
    }
    EXTRA_RESOURCES_WIN = $$EXTRA_RESOURCES
    EXTRA_RESOURCES_WIN ~= s,/,\\,g
    RES_DESTDIR_WIN = $$OUT_PWD/res
    RES_DESTDIR_WIN ~= s,/,\\,g
    QMAKE_POST_LINK += $$quote(cmd /c if not exist $$RES_DESTDIR_WIN md $$RES_DESTDIR_WIN$$escape_expand(\n\t))
    for(RESOURCE, EXTRA_RESOURCES_WIN){
        QMAKE_POST_LINK += $$quote(cmd /c copy /y $$RESOURCE $$RES_DESTDIR_WIN$$escape_expand(\n\t))
    }
}

