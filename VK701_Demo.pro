# ----------------------------
# Qt Modules
# ----------------------------
QT += core gui printsupport sql serialbus concurrent
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets 

CONFIG += c++17

# 禁用 Qt 过时 API（如需要，取消注释）
# DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000

# ----------------------------
# 编译选项
# ----------------------------
QMAKE_CXXFLAGS += -fpermissive
msvc {
    QMAKE_CXXFLAGS += -utf-8
}

# ----------------------------
# 源文件
# ----------------------------
SOURCES += \
    main.cpp \
    mainwindow.cpp \
    src/mdbprocess.cpp \
    src/mdbtcp.cpp \
    src/motorpage.cpp \
    src/vk701nsd.cpp \  # vk701 DAQ
    src/qcustomplot.cpp \
    src/vk701page.cpp \
    src/zmotionpage.cpp \
    src/zmcaux.cpp \
    src/statemachine.cpp \
    src/autodrilling.cpp \
    src/drillingstate.cpp \
    src/motioncontroller.cpp    \
    src/DebugTestMotion.cpp \
    src/DrillingController.cpp \
    src/DrillingParameters.cpp \
    src/StateMachineWorker.cpp
    

# ----------------------------
# 头文件
# ----------------------------
HEADERS += \
    mainwindow.h \
    inc/Global.h \
    inc/VK70xNMC_DAQ2.h \
    inc/zmotion.h \
    inc/mdbprocess.h \
    inc/mdbtcp.h \
    inc/vk701nsd.h \
    inc/qcustomplot.h \
    inc/motorpage.h \
    inc/vk701page.h \
    inc/zmotionpage.h \
    inc/zmcaux.h \
    inc/statemachine.h \
    inc/autodrilling.h \
    inc/drillingstate.h \
    inc/motioncontroller.h \
    inc/DebugTestMotion.h   \
    #inc/MotionParameters.h \
    inc/DrillingController.h \
    inc/DrillingParameters.h \
    inc/StateMachineWorker.h

# ----------------------------
# UI 界面文件
# ----------------------------
FORMS += \
    mainwindow.ui \
    mdbtcp.ui \
    motorpage.ui \
    vk701page.ui \
    zmotionpage.ui

# ----------------------------
# 头文件 & 库路径
# ----------------------------
INCLUDEPATH += \
    $$PWD/build \
    $$PWD/src \
    $$PWD/inc

# Windows Python 依赖
win32 {
    INCLUDEPATH += C:/Users/YMH/anaconda3/include
    LIBS += -LC:/Users/YMH/anaconda3/libs -lpython310

    CONFIG(debug, debug|release) {
        QMAKE_CXXFLAGS_DEBUG -= -MDd
        QMAKE_LFLAGS_DEBUG -= -MDd
        LIBS += -LC:/Users/YMH/anaconda3/libs -lpython310
    }
}

unix:!macx: INCLUDEPATH += -I /usr/include/python3.8
unix:!macx: LIBS += -L /usr//lib -lpython3.8

# ----------------------------
# VK701N 库
# ----------------------------
win32 {
    LIBS += -L$$PWD/lib/ -lVK70XNMC_DAQ2
}

unix:!macx {
    LIBS += -L$$PWD/lib/ -lVK70XNMC_DAQ_SHARED
    INCLUDEPATH += $$PWD/lib
    DEPENDPATH += $$PWD/lib
    INCLUDEPATH += -I/usr/include/python3.8
    LIBS += -L/usr/lib -lpython3.8
}

# ----------------------------
# Zmotion 库
# ----------------------------
win32 {
    LIBS += -L$$PWD/release/ -lzmotion
    LIBS += -L$$PWD/release/ -lzauxdll
}

unix:!macx {
    LIBS += -L$$PWD/lib/ -lzmotion
}

# ----------------------------
# 目标文件夹
# ----------------------------
win32:CONFIG(release, debug|release) {
    DESTDIR = $$PWD/release
    UI_DIR = $$PWD/tmp/release/ui
    MOC_DIR = $$PWD/tmp/release/moc
    OBJECTS_DIR = $$PWD/tmp/release/obj
    RCC_DIR = $$PWD/tmp/release/rcc
} else:win32:CONFIG(debug, debug|release) {
    DESTDIR = $$PWD/debug
    UI_DIR = $$PWD/tmp/debug/ui
    MOC_DIR = $$PWD/tmp/debug/moc
    OBJECTS_DIR = $$PWD/tmp/debug/obj
    RCC_DIR = $$PWD/tmp/debug/rcc
}

# ----------------------------
# 部署规则
# ----------------------------
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# ----------------------------
# ONNX (可选，取消注释启用)
# ----------------------------
# win32 {
#     HEADERS += $$files($$PWD/inc/include/*.h)
#     LIBS += -L$$PWD/lib/ -lonnxruntime -lonnxruntime_providers_cuda -lonnxruntime_providers_shared
# }
