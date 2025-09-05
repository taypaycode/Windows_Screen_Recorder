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
#include <libavutil/channel_layout.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
#endif

#include "video_types.h"

// Forward declaration
struct AudioSample;

class FFmpegEncoder {
public:
    FFmpegEncoder();
    ~FFmpegEncoder();
    
    // Initialize encoder with specified parameters
    bool init(const std::string& filename, int width, int height, int fps, 
              VideoCodec codec, QualityPreset quality, bool enableAudio = false);
    
    // Encode a single frame
    bool encodeFrame(const cv::Mat& frame);
    
    // Encode audio samples
    bool encodeAudio(const AudioSample& sample, bool isMicrophone = true);
    
    // Finish encoding and close file
    void finish();
    
    // Check if encoder is initialized
    bool isInitialized() const { return initialized; }
    
    // Get expected bitrate for given settings
    static int getTargetBitrate(int width, int height, VideoCodec codec, QualityPreset quality);

private:
    bool initialized;
    bool audioEnabled;
    
#ifdef HAVE_FFMPEG
    // FFmpeg context structures
    AVFormatContext* formatContext;
    AVCodecContext* codecContext;
    AVStream* videoStream;
    AVFrame* frame;
    AVPacket* packet;
    SwsContext* swsContext;
    
    // Audio encoding structures
    AVCodecContext* micCodecContext;
    AVCodecContext* systemCodecContext;
    AVStream* micAudioStream;
    AVStream* systemAudioStream;
    AVFrame* micAudioFrame;
    AVFrame* systemAudioFrame;
    SwrContext* micSwrContext;
    SwrContext* systemSwrContext;
    
    // Frame counters
    int64_t frameCount;
    int64_t micAudioFrameCount;
    int64_t systemAudioFrameCount;

    // Incoming audio FIFOs (interleaved float samples)
    std::vector<float> micInputFifo;
    std::vector<float> systemInputFifo;

    // Source audio characteristics (discovered at runtime)
    int micSourceSampleRate = 0;
    int systemSourceSampleRate = 0;
    
    // Helper methods
    bool setupCodec(VideoCodec codec, QualityPreset quality, int width, int height, int fps);
    bool setupFormat(const std::string& filename);
    bool setupAudioCodec(AVCodecContext*& codecContext, AVStream*& stream, SwrContext*& swrContext, AVFrame*& audioFrame);
    bool writeFrame(AVFrame* frame);
    bool writeAudioFrame(AVFrame* audioFrame, AVCodecContext* codecContext, AVStream* stream);
    void flushEncoder(AVCodecContext* codecContext);
    bool ensureResamplerConfigured(SwrContext*& swr,
                                   const AVChannelLayout& dstLayout,
                                   AVSampleFormat dstFmt,
                                   int dstRate,
                                   const AVChannelLayout& srcLayout,
                                   int srcRate);
#endif
};
