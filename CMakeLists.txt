cmake_minimum_required(VERSION 3.16)
project(ZaneTool VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/ffmpegprocess.cpp
    src/imageprocessor.cpp
    src/videoprocessor.cpp
    src/audioprocessor.cpp
    src/colorpicker.cpp
    src/windowpicker.cpp
    src/utils.cpp
    src/resources.qrc
)

set(RESOURCES
    src/app.rc
)

set(HEADERS
    src/mainwindow.h
    src/ffmpegprocess.h
    src/imageprocessor.h
    src/videoprocessor.h
    src/audioprocessor.h
    src/colorpicker.h
    src/windowpicker.h
    src/utils.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${RESOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME "ZaneTool"
)
