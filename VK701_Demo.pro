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
    $$PWD/inc \
    $$PWD # For generated ui_*.h files in build directory

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

# For UI files processing, ensure UI_DIR is set early or use default behavior
# For tests, ui_vk701page.h needs to be generated.
# If UI_DIR is set as above, then INCLUDEPATH for tests might need to include it.
# Or, more simply, ensure generated UI headers are findable from $$PWD (build dir).

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

#-------------------------------------------------
# Unit Test Configuration for tst_vk701nsd
# This creates an executable named tst_vk701nsd when CONFIG+=run_tst_vk701nsd
#-------------------------------------------------
run_tst_vk701nsd {
    TARGET = tst_vk701nsd
    CONFIG += testcase
    CONFIG -= app_bundle # Ensure it's a console app
    QT += testlib

    SOURCES = tests/testvk701nsd.cpp \
              src/vk701nsd.cpp # Class under test
    # Headers for tst_vk701nsd
    # Global.h and VK70xNMC_DAQ2.h are already in global HEADERS.
    # inc/vk701nsd.h is also in global HEADERS.
    HEADERS_TEST_NSD = tests/testvk701nsd.cpp # For MOC processing
    HEADERS += $$HEADERS_TEST_NSD

    # Link external libraries (VK701N)
    win32 { LIBS += -L$$PWD/lib/ -lVK70XNMC_DAQ2 }
    unix:!macx { LIBS += -L$$PWD/lib/ -lVK70XNMC_DAQ_SHARED }

    # Define a 'check_vk701nsd' make target
    TEST_NSD_EXECUTABLE_PATH = $$OUT_PWD/$$TARGET
    unix {
        test_runner_nsd.target = check_vk701nsd
        test_runner_nsd.commands = $$TEST_NSD_EXECUTABLE_PATH
        QMAKE_EXTRA_TARGETS += test_runner_nsd
        PRE_TARGETDEPS += $$TEST_NSD_EXECUTABLE_PATH
    }
    win32 {
        TEST_NSD_EXECUTABLE_WIN_PATH = $$shell_path($$TEST_NSD_EXECUTABLE_PATH)
        test_runner_nsd.target = check_vk701nsd
        test_runner_nsd.commands = $$TEST_NSD_EXECUTABLE_WIN_PATH
        QMAKE_EXTRA_TARGETS += test_runner_nsd
        PRE_TARGETDEPS += $$TEST_NSD_EXECUTABLE_WIN_PATH
    }
}

#-------------------------------------------------
# Unit Test Configuration for tst_vk701page
# This creates an executable named tst_vk701page when CONFIG+=run_tst_vk701page
#-------------------------------------------------
run_tst_vk701page {
    TARGET = tst_vk701page
    CONFIG += testcase
    CONFIG -= app_bundle # Ensure it's a console app
    QT += testlib sql widgets concurrent # Modules needed by vk701page

    # Sources for tst_vk701page
    SOURCES = tests/testvk701page.cpp \
              src/vk701page.cpp \
              src/vk701nsd.cpp \
              src/qcustomplot.cpp
    
    # Headers for tst_vk701page
    # inc/vk701page.h, inc/vk701nsd.h, inc/qcustomplot.h, inc/Global.h, inc/VK70xNMC_DAQ2.h
    # are already in the global HEADERS list from the main application part.
    # We need to ensure ui_vk701page.h is generated and accessible.
    HEADERS_TEST_PAGE = tests/testvk701page.cpp # For MOC processing
    HEADERS += $$HEADERS_TEST_PAGE

    # UI form for vk701page - this ensures ui_vk701page.h is generated
    FORMS += vk701page.ui

    # Link external libraries (VK701N)
    win32 { LIBS += -L$$PWD/lib/ -lVK70XNMC_DAQ2 }
    unix:!macx { LIBS += -L$$PWD/lib/ -lVK70XNMC_DAQ_SHARED }

    # Define a 'check_vk701page' make target
    TEST_PAGE_EXECUTABLE_PATH = $$OUT_PWD/$$TARGET
    unix {
        test_runner_page.target = check_vk701page
        test_runner_page.commands = $$TEST_PAGE_EXECUTABLE_PATH
        QMAKE_EXTRA_TARGETS += test_runner_page
        PRE_TARGETDEPS += $$TEST_PAGE_EXECUTABLE_PATH
    }
    win32 {
        TEST_PAGE_EXECUTABLE_WIN_PATH = $$shell_path($$TEST_PAGE_EXECUTABLE_PATH)
        test_runner_page.target = check_vk701page
        test_runner_page.commands = $$TEST_PAGE_EXECUTABLE_WIN_PATH
        QMAKE_EXTRA_TARGETS += test_runner_page
        PRE_TARGETDEPS += $$TEST_PAGE_EXECUTABLE_WIN_PATH
    }
}

# Note on building multiple test executables:
# The approach above uses custom CONFIG flags (run_tst_vk701nsd, run_tst_vk701page)
# to select which test executable to build.
# Example: qmake CONFIG+=run_tst_vk701nsd && make
# Example: qmake CONFIG+=run_tst_vk701page && make
# If no such CONFIG is passed, it builds the main application.
# This avoids TARGET conflicts and recompiles only necessary sources for each target.
# The global SOURCES, HEADERS, FORMS are used by the main app.
# Inside each 'run_tst_...' scope, SOURCES, HEADERS, FORMS are redefined for that specific test build.
# This is a more robust way to handle multiple executables in a single .pro file
# than relying on a single global CONFIG += testcase.
# The previous global "CONFIG += testcase" and its associated "TARGET = tst_vk701nsd"
# have been removed and replaced by the scoped "run_tst_vk701nsd" block.
# The `HEADERS += tests/testvk701nsd.cpp` was moved inside its scope.
# Added `INCLUDEPATH += $$PWD` to global to help find generated ui_*.h files from build dir.
