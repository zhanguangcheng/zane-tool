cmake_minimum_required(VERSION 3.16)
project(ZaneTool VERSION 0.1.1 LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

string(APPEND CMAKE_CXX_FLAGS_RELEASE " -ffunction-sections -fdata-sections")
string(APPEND CMAKE_C_FLAGS_RELEASE " -ffunction-sections -fdata-sections")
string(APPEND CMAKE_CXX_FLAGS_MINSIZEREL " -ffunction-sections -fdata-sections")
string(APPEND CMAKE_C_FLAGS_MINSIZEREL " -ffunction-sections -fdata-sections")

string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " -Wl,--gc-sections -s")
string(APPEND CMAKE_EXE_LINKER_FLAGS_MINSIZEREL " -Wl,--gc-sections -s")

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/image_tool.cpp
    src/video_tool.cpp
    src/audio_tool.cpp
    src/colorpickerpage.cpp
    src/ffmpegprocess.cpp
    src/imageprocessor.cpp
    src/videoprocessor.cpp
    src/audioprocessor.cpp
    src/colorpicker.cpp
    src/windowpicker.cpp
    src/screenshotpicker.cpp
    src/pinwindow.cpp
    src/screenshottool.cpp
    src/transparencytool.cpp
    src/stopwatchtimer.cpp
    src/utils.cpp
    src/calculator.cpp
    src/resources.qrc
    src/third_party/qrcodegen.cpp
    src/third_party/quirc.c
    src/third_party/decode.c
    src/third_party/identify.c
    src/third_party/version_db.c
)

set(RESOURCES
    src/app.rc
)

set(HEADERS
    src/mainwindow.h
    src/image_tool.h
    src/video_tool.h
    src/audio_tool.h
    src/colorpickerpage.h
    src/ffmpegprocess.h
    src/imageprocessor.h
    src/videoprocessor.h
    src/audioprocessor.h
    src/colorpicker.h
    src/windowpicker.h
    src/screenshotpicker.h
    src/pinwindow.h
    src/screenshottool.h
    src/transparencytool.h
    src/stopwatchtimer.h
    src/utils.h
    src/calculator.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${RESOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Network)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME "ZaneTool"
)
