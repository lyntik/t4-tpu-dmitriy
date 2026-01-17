TEMPLATE      = lib
CONFIG       += plugin
QT           += widgets sql charts printsupport
TARGET        = FiltrationVolOpTool
DESTDIR       = $$OUT_PWD/../../../../bin/Plugins

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

SOURCES += $$PWD/FiltrationVolOpTool.cpp \
    FiltrationWidget.cpp

HEADERS += $$PWD/FiltrationVolOpTool.h \
    FiltrationWidget.h

FORMS += $$PWD/FiltrationWidget.ui \
    ControlPanel.ui

RESOURCES += \
    resource.qrc

