#pragma once

#include <string>
#include <memory>
#include <opencv2/opencv.hpp>

#ifdef HAVE_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}
#endif

#include "video_types.h"

class FFmpegEncoder {
public:
    FFmpegEncoder();
    ~FFmpegEncoder();
    
    // Initialize encoder with specified parameters
    bool init(const std::string& filename, int width, int height, int fps, 
              VideoCodec codec, QualityPreset quality);
    
    // Encode a single frame
    bool encodeFrame(const cv::Mat& frame);
    
    // Finish encoding and close file
    void finish();
    
    // Check if encoder is initialized
    bool isInitialized() const { return initialized; }
    
    // Get expected bitrate for given settings
    static int getTargetBitrate(int width, int height, VideoCodec codec, QualityPreset quality);

private:
    bool initialized;
    
#ifdef HAVE_FFMPEG
    // FFmpeg context structures
    AVFormatContext* formatContext;
    AVCodecContext* codecContext;
    AVStream* videoStream;
    AVFrame* frame;
    AVPacket* packet;
    SwsContext* swsContext;
    
    // Frame counter
    int64_t frameCount;
    
    // Helper methods
    bool setupCodec(VideoCodec codec, QualityPreset quality, int width, int height, int fps);
    bool setupFormat(const std::string& filename);
    bool writeFrame(AVFrame* frame);
#endif
};
