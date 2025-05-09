# ScreenIT - Simple Screen Recording Application

A lightweight C++ application for recording your screen on Windows platforms.

## Features

- Record your entire screen
- Set custom frame rates
- Define recording duration
- Simple graphical user interface
- System tray icon indicating recording status

## Requirements

- C++17 compatible compiler 
- CMake 3.10 or higher
- OpenCV library
- Windows operating system

## Building

1. Install OpenCV (if not already installed)
   ```
   You can download pre-built binaries from https://opencv.org/releases/ 
   or build from source.
   ```

2. Clone this repository
   ```
   git clone https://github.com/yourusername/ScreenIT.git
   cd ScreenIT
   ```

3. Create icon files
   The application needs three icon files:
   - icon.ico - Main application icon
   - recording.ico - System tray icon when recording
   - idle.ico - System tray icon when idle
   
   Place these files in the resources directory.

4. Create a build directory and configure with CMake
   ```
   mkdir build
   cd build
   cmake -DOpenCV_DIR=path/to/opencv/build ..
   ```

5. Build the project
   ```
   cmake --build . --config Release
   ```

## Usage

1. Launch the ScreenIT application
2. Set your desired parameters:
   - Output filename
   - Frame rate (FPS)
   - Duration (0 for recording until manually stopped)
3. Click "Start Recording" button
4. The application window will hide, and a system tray icon will appear
5. To stop recording, either:
   - Right-click the system tray icon and select "Show ScreenIT"
   - Double-click the system tray icon to show the application
   - Then click the "Stop Recording" button
6. Once recording is complete, the application will show a success message with the path to the saved file

## License

This project is licensed under the MIT License - see the LICENSE file for details. 