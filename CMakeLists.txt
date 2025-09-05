cmake_minimum_required(VERSION 3.10)
project(ScreenIT)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Temporarily enable console for debugging (TDD verification)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:CONSOLE")

# Define UNICODE for proper Windows API usage
add_definitions(-DUNICODE -D_UNICODE)

# Find required packages
find_package(OpenCV REQUIRED)

# Find FFmpeg
find_path(FFMPEG_INCLUDE_DIR libavcodec/avcodec.h)
find_library(FFMPEG_LIBAVCODEC avcodec)
find_library(FFMPEG_LIBAVFORMAT avformat)
find_library(FFMPEG_LIBAVUTIL avutil)
find_library(FFMPEG_LIBSWSCALE swscale)
find_library(FFMPEG_LIBSWRESAMPLE swresample)

if(NOT FFMPEG_INCLUDE_DIR OR NOT FFMPEG_LIBAVCODEC OR NOT FFMPEG_LIBAVFORMAT OR NOT FFMPEG_LIBAVUTIL OR NOT FFMPEG_LIBSWSCALE OR NOT FFMPEG_LIBSWRESAMPLE)
    message(STATUS "FFmpeg not found, will download and build")
    set(FFMPEG_FOUND FALSE)
else()
    message(STATUS "FFmpeg found")
    set(FFMPEG_FOUND TRUE)
endif()

# Create and configure resource file - temporarily commented out
# set(RESOURCE_FILES "src/icons.rc")

# Add executable
add_executable(${PROJECT_NAME} WIN32
    src/main.cpp
    src/screen_recorder.cpp
    src/screen_ui.cpp
    src/ffmpeg_encoder.cpp
    src/audio_capture.cpp
    src/webcam_capture.cpp
    # ${RESOURCE_FILES}
)

# Include directories
include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/include
)

# Add FFmpeg include directories if found
if(FFMPEG_FOUND)
    include_directories(${FFMPEG_INCLUDE_DIR})
    add_definitions(-DHAVE_FFMPEG)
endif()

# Link libraries
target_link_libraries(${PROJECT_NAME} 
    ${OpenCV_LIBS}
    gdi32
    user32
    shell32
    comctl32
    ole32
    winmm
)

# Link FFmpeg libraries if found
if(FFMPEG_FOUND)
    target_link_libraries(${PROJECT_NAME}
        ${FFMPEG_LIBAVCODEC}
        ${FFMPEG_LIBAVFORMAT}
        ${FFMPEG_LIBAVUTIL}
        ${FFMPEG_LIBSWSCALE}
        ${FFMPEG_LIBSWRESAMPLE}
    )
endif()

# Copy icon files to the output directory
# Temporarily commented out until we have the real icon files
# configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.ico ${CMAKE_CURRENT_BINARY_DIR}/icon.ico COPYONLY)
# configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resources/recording.ico ${CMAKE_CURRENT_BINARY_DIR}/recording.ico COPYONLY)
# configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resources/idle.ico ${CMAKE_CURRENT_BINARY_DIR}/idle.ico COPYONLY) 