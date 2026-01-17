cmake_minimum_required(VERSION 3.16)

project(FiltrationVolOpTool LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/Plugins)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/Plugins)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Charts PrintSupport)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Charts PrintSupport)

add_library(${PROJECT_NAME} SHARED
    FiltrationVolOpTool.h
    FiltrationWidget.h

    FiltrationVolOpTool.cpp
    FiltrationWidget.cpp

    ControlPanel.ui
    FiltrationWidget.ui

    resource.qrc
)

target_link_libraries(${PROJECT_NAME}  Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Charts Qt${QT_VERSION_MAJOR}::PrintSupport)
target_compile_definitions(${PROJECT_NAME} PRIVATE LOGGINGVOLOPTOOL_CMAKE_LIBRARY)

include($ENV{T4LIB_ROOT}/depends/opencv.cmake.in)
include($ENV{T4LIB_ROOT}/depends/vtk.cmake.in)
include($ENV{T4LIB_ROOT}/depends/libtiff.cmake.in)

include_directories($ENV{T4LIB_ROOT}/include/Common)
include_directories($ENV{T4LIB_ROOT}/include/Common/t4)
include_directories($ENV{T4LIB_ROOT}/include/Common/funcs)
include_directories($ENV{T4LIB_ROOT}/include/Image)
include_directories($ENV{T4LIB_ROOT}/include/Math)
include_directories($ENV{T4LIB_ROOT}/include/Widgets)
include_directories($ENV{T4LIB_ROOT}/include/Widgets/geo)
find_library(result_t4Common NAMES t4Common PATHS $ENV{T4LIB_ROOT}/lib)
find_library(result_t4Widgets NAMES t4Widgets PATHS $ENV{T4LIB_ROOT}/lib)
find_library(result_t4Math NAMES t4Math PATHS $ENV{T4LIB_ROOT}/lib)
find_library(result_t4Image NAMES t4Image PATHS $ENV{T4LIB_ROOT}/lib)
target_link_libraries(${PROJECT_NAME} ${result_t4Common} ${result_t4Widgets} ${result_t4Math} ${result_t4Image})





