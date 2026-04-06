TEMPLATE = lib
CONFIG += plugin

QT += widgets sql charts printsupport

TARGET = AutoDenoiseTool

DESTDIR = C:/tomograph4/Plugins

include($$(T4LIB_ROOT)/depends/opencv.pri)
include($$(T4LIB_ROOT)/depends/vtk.pri)
include($$(T4LIB_ROOT)/depends/LibTiff.pri)

INCLUDEPATH += $$(T4LIB_ROOT)/include/Common
INCLUDEPATH += $$(T4LIB_ROOT)/include/Common/t4
INCLUDEPATH += $$(T4LIB_ROOT)/include/Common/UtilFuncs
INCLUDEPATH += $$(T4LIB_ROOT)/include/Image
INCLUDEPATH += $$(T4LIB_ROOT)/include/Math
INCLUDEPATH += $$(T4LIB_ROOT)/include/Widgets
INCLUDEPATH += $$(T4LIB_ROOT)/include/Widgets/geo

LIBS += -L$$(T4LIB_ROOT)/lib -lt4Common -lt4Widgets -lt4Math -lt4Image

SOURCES += \
    $$PWD/autodenoisetool.cpp \
    $$PWD/denoisewidget.cpp

HEADERS += \
    $$PWD/autodenoisetool.h \
    $$PWD/denoisewidget.h

FORMS += \
    $$PWD/DenoiseTool.ui \
    $$PWD/Layout.ui

RESOURCES += \
    resource.qrc