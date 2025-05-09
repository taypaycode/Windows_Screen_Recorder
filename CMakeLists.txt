cmake_minimum_required(VERSION 3.10)
project(ScreenIT)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set WIN32 flag to create a Windows GUI application (no console)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")

# Define UNICODE for proper Windows API usage
add_definitions(-DUNICODE -D_UNICODE)

# Find required packages
find_package(OpenCV REQUIRED)

# Create and configure resource file - temporarily commented out
# set(RESOURCE_FILES "src/icons.rc")

# Add executable
add_executable(${PROJECT_NAME} WIN32
    src/main.cpp
    src/screen_recorder.cpp
    src/screen_ui.cpp
    # ${RESOURCE_FILES}
)

# Include directories
include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(${PROJECT_NAME} 
    ${OpenCV_LIBS}
    gdi32
    user32
    shell32
    comctl32
)

# Copy icon files to the output directory
# Temporarily commented out until we have the real icon files
# configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.ico ${CMAKE_CURRENT_BINARY_DIR}/icon.ico COPYONLY)
# configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resources/recording.ico ${CMAKE_CURRENT_BINARY_DIR}/recording.ico COPYONLY)
# configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resources/idle.ico ${CMAKE_CURRENT_BINARY_DIR}/idle.ico COPYONLY) 