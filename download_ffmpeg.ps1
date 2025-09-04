# Download FFmpeg for Windows
# This script downloads pre-compiled FFmpeg binaries for development

$ffmpegUrl = "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl-shared.zip"
$ffmpegZip = "ffmpeg.zip"
$ffmpegDir = "ffmpeg"

Write-Host "Downloading FFmpeg..."
Invoke-WebRequest -Uri $ffmpegUrl -OutFile $ffmpegZip

Write-Host "Extracting FFmpeg..."
Expand-Archive -Path $ffmpegZip -DestinationPath "temp" -Force

# Move the extracted folder to ffmpeg
$extractedDir = Get-ChildItem -Path "temp" -Directory | Select-Object -First 1
Move-Item -Path $extractedDir.FullName -Destination $ffmpegDir -Force

# Clean up
Remove-Item -Path $ffmpegZip -Force
Remove-Item -Path "temp" -Recurse -Force

Write-Host "FFmpeg downloaded and extracted to: $ffmpegDir"
Write-Host ""
Write-Host "To build with FFmpeg support, run:"
Write-Host "cmake -DOpenCV_DIR=C:/Users/TM-9X/Downloads/opencv/build -DFFMPEG_INCLUDE_DIR=./ffmpeg/include -DFFMPEG_LIBAVCODEC=./ffmpeg/lib/avcodec.lib -DFFMPEG_LIBAVFORMAT=./ffmpeg/lib/avformat.lib -DFFMPEG_LIBAVUTIL=./ffmpeg/lib/avutil.lib -DFFMPEG_LIBSWSCALE=./ffmpeg/lib/swscale.lib .."
