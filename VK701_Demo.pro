QT       += core gui printsupport sql serialbus

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

QMAKE_CXXFLAGS += -fpermissive
msvc{
QMAKE_CXXFLAGS += -utf-8
}


SOURCES += \
    main.cpp \
    mainwindow.cpp	\
    src/mdbprocess.cpp \
    src/mdbtcp.cpp \
    src/motorpage.cpp	\
    src/vk701nsd.cpp	\		#vk701 DAQ
    src/qcustomplot.cpp	\
    src/vk701page.cpp \
    src/zmotionpage.cpp \
    src/zmcaux.cpp


HEADERS += \
    mainwindow.h	\
    inc/Global.h \
    inc/VK70xNMC_DAQ2.h \
    inc/zmotion.h	\
    inc/mdbprocess.h \
    inc/mdbtcp.h \
    inc/vk701nsd.h	\
    inc/qcustomplot.h	\
    inc/motorpage.h	\
    inc/vk701page.h    \
    inc/zmotionpage.h   \
    inc/zmcaux.h


win32:INCLUDEPATH += C:/Users/YMH/anaconda3/include
win32:LIBS += -LC:/Users/YMH/anaconda3/libs -lpython310
win32:CONFIG(debug, debug|release) {
    QMAKE_CXXFLAGS_DEBUG -= -MDd
    QMAKE_LFLAGS_DEBUG -= -MDd
    LIBS += -LC:/Users/YMH/anaconda3/libs -lpython310
}

INCLUDEPATH += $$PWD/build

FORMS += \
    mainwindow.ui \
    mdbtcp.ui \
    motorpage.ui \
    vk701page.ui \
    zmotionpage.ui


# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# [VK701N lib]
# Windows
win32: LIBS += -L$$PWD/lib/ -lVK70XNMC_DAQ2
INCLUDEPATH += $$PWD/.
DEPENDPATH += $$PWD/.
# Linux
unix:!macx: LIBS += -L$$PWD/lib/ -lVK70XNMC_DAQ_SHARED
unix:!macx: INCLUDEPATH += $$PWD/lib
unix:!macx: DEPENDPATH += $$PWD/lib
unix:!macx: INCLUDEPATH += -I /usr/include/python3.8
unix:!macx: LIBS += -L /usr//lib -lpython3.8
#INCLUDEPATH += /home/hui/miniconda/envs/py3.9/include/python3.9
#LIBS += -L/home/hui/miniconda/envs/py3.9/lib -lpython3.9
# [Zmotion lib]
# Linux
win32: LIBS += -L$$PWD/./release/ -lzmotion
win32: LIBS += -L$$PWD/./release/ -lzauxdll
unix:!macx: LIBS += -L$$PWD/lib/ -lzmotion

win32:CONFIG(release, debug|release):{
    DESTDIR =$$PWD/release
    UI_DIR = $$PWD/tmp/release/ui
    MOC_DIR = $$PWD/tmp/release/moc
    OBJECTS_DIR = $$PWD/tmp/release/obj
    RCC_DIR = $$PWD/tmp/release/rcc
}
else:win32:CONFIG(debug, debug|release):{
    DESTDIR =$$PWD/debug
    UI_DIR = $$PWD/tmp/debug/ui
    MOC_DIR = $$PWD/tmp/debug/moc
    OBJECTS_DIR = $$PWD/tmp/debug/obj
    RCC_DIR = $$PWD/tmp/debug/rcc
}
# # ONNX windows

#win32:HEADERS += $$files($$PWD/inc/include/*.h)
#win32:LIBS += -L$$PWD/lib/ -lonnxruntime -lonnxruntime_providers_cuda -lonnxruntime_providers_shared

