@echo off
REM ScreenIT Launcher - Sets up DLL paths and runs the application

REM Set up paths for OpenCV and FFmpeg DLLs
set VCP=%USERPROFILE%\vcpkg\installed\x64-windows
set OCV=C:\Users\TM-9X\Downloads\opencv\build
set PATH=%VCP%\bin;%OCV%\x64\vc16\bin;%PATH%

REM Launch ScreenIT
echo Starting ScreenIT...
.\build\Release\ScreenIT.exe

REM Keep window open if there's an error
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Application exited with error code %ERRORLEVEL%
    pause
)
