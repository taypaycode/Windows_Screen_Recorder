#include "../include/ffmpeg_encoder.h"
#include <iostream>

FFmpegEncoder::FFmpegEncoder() : initialized(false) {
#ifdef HAVE_FFMPEG
    formatContext = nullptr;
    codecContext = nullptr;
    videoStream = nullptr;
    frame = nullptr;
    packet = nullptr;
    swsContext = nullptr;
    frameCount = 0;
#endif
}

FFmpegEncoder::~FFmpegEncoder() {
    finish();
}

bool FFmpegEncoder::init(const std::string& filename, int width, int height, int fps, 
                        VideoCodec codec, QualityPreset quality) {
#ifdef HAVE_FFMPEG
    std::cout << "Initializing FFmpeg encoder:" << std::endl;
    std::cout << "- Filename: " << filename << std::endl;
    std::cout << "- Resolution: " << width << "x" << height << std::endl;
    std::cout << "- FPS: " << fps << std::endl;
    
    // Allocate format context
    if (avformat_alloc_output_context2(&formatContext, nullptr, nullptr, filename.c_str()) < 0) {
        std::cerr << "Failed to allocate output context" << std::endl;
        return false;
    }
    
    // Setup codec
    if (!setupCodec(codec, quality, width, height, fps)) {
        std::cerr << "Failed to setup codec" << std::endl;
        return false;
    }
    
    // Set video stream parameters
    codecContext->width = width;
    codecContext->height = height;
    codecContext->time_base = {1, fps};
    codecContext->framerate = {fps, 1};
    codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    
    // Log encoder control mode depending on codec implementation
    if (strstr(codecContext->codec->name, "h264_mf") || strstr(codecContext->codec->name, "h264_amf") || strstr(codecContext->codec->name, "h264_qsv")) {
        std::cout << "- Using CBR mode for sharp UI" << std::endl;
    } else if (strstr(codecContext->codec->name, "x264") || strstr(codecContext->codec->name, "h264")) {
        std::cout << "- Using CRF mode for optimal compression" << std::endl;
    }
    
    // Open codec
    if (avcodec_open2(codecContext, codecContext->codec, nullptr) < 0) {
        std::cerr << "Failed to open codec" << std::endl;
        return false;
    }
    
    // Create video stream and align timing to constant frame rate
    videoStream = avformat_new_stream(formatContext, nullptr);
    if (!videoStream) {
        std::cerr << "Failed to create video stream" << std::endl;
        return false;
    }
    
    videoStream->time_base = codecContext->time_base;           // 1/fps
    videoStream->avg_frame_rate = {fps, 1};                      // nominal frame rate
    videoStream->r_frame_rate = {fps, 1};
    
    // Copy codec parameters to stream
    if (avcodec_parameters_from_context(videoStream->codecpar, codecContext) < 0) {
        std::cerr << "Failed to copy codec parameters" << std::endl;
        return false;
    }
    
    // Open output file
    if (!(formatContext->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&formatContext->pb, filename.c_str(), AVIO_FLAG_WRITE) < 0) {
            std::cerr << "Failed to open output file" << std::endl;
            return false;
        }
    }
    
    // Write header
    if (avformat_write_header(formatContext, nullptr) < 0) {
        std::cerr << "Failed to write header" << std::endl;
        return false;
    }
    
    // Allocate frame
    frame = av_frame_alloc();
    if (!frame) {
        std::cerr << "Failed to allocate frame" << std::endl;
        return false;
    }
    
    frame->format = codecContext->pix_fmt;
    frame->width = codecContext->width;
    frame->height = codecContext->height;
    
    if (av_frame_get_buffer(frame, 0) < 0) {
        std::cerr << "Failed to allocate frame buffer" << std::endl;
        return false;
    }
    
    // Allocate packet
    packet = av_packet_alloc();
    if (!packet) {
        std::cerr << "Failed to allocate packet" << std::endl;
        return false;
    }
    
    // Setup scale context for BGR to YUV420P conversion
    swsContext = sws_getContext(
        width, height, AV_PIX_FMT_BGR24,
        width, height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    
    if (!swsContext) {
        std::cerr << "Failed to create scale context" << std::endl;
        return false;
    }
    
    frameCount = 0;
    initialized = true;
    
    std::cout << "✓ FFmpeg encoder initialized successfully" << std::endl;
    return true;
    
#else
    std::cerr << "FFmpeg not available, cannot initialize encoder" << std::endl;
    return false;
#endif
}

bool FFmpegEncoder::encodeFrame(const cv::Mat& cvFrame) {
#ifdef HAVE_FFMPEG
    if (!initialized) {
        return false;
    }
    
    // Convert BGR to YUV420P
    const uint8_t* srcData[1] = { cvFrame.data };
    int srcLinesize[1] = { static_cast<int>(cvFrame.step[0]) };
    
    sws_scale(swsContext, srcData, srcLinesize, 0, cvFrame.rows,
              frame->data, frame->linesize);
    
    // PTS in stream time_base (1/fps). Using frameCount increments by exactly one per frame.
    frame->pts = frameCount++;
    
    return writeFrame(frame);
#else
    return false;
#endif
}

void FFmpegEncoder::finish() {
#ifdef HAVE_FFMPEG
    if (!initialized) {
        return;
    }
    
    // Flush encoder
    writeFrame(nullptr);
    
    // Write trailer
    if (formatContext) {
        av_write_trailer(formatContext);
    }
    
    // Cleanup
    if (codecContext) {
        avcodec_free_context(&codecContext);
    }
    
    if (frame) {
        av_frame_free(&frame);
    }
    
    if (packet) {
        av_packet_free(&packet);
    }
    
    if (swsContext) {
        sws_freeContext(swsContext);
    }
    
    if (formatContext) {
        if (!(formatContext->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&formatContext->pb);
        }
        avformat_free_context(formatContext);
    }
    
    initialized = false;
    std::cout << "FFmpeg encoder finished" << std::endl;
#endif
}

int FFmpegEncoder::getTargetBitrate(int width, int height, VideoCodec codec, QualityPreset quality) {
    // Calculate based on resolution (reference: 1920x1080 = 1080p)
    double pixelCount = width * height;
    double referencePixels = 1920.0 * 1080.0;
    double resolutionFactor = pixelCount / referencePixels;
    
    int baseBitrate = 0;
    
    switch (codec) {
        case VideoCodec::H264_HARDWARE:
        case VideoCodec::H264_SOFTWARE:
            switch (quality) {
                case QualityPreset::SMALL_SHARP: baseBitrate = 800; break;   // Very aggressive
                case QualityPreset::BALANCED: baseBitrate = 1500; break;
                case QualityPreset::HIGH_QUALITY: baseBitrate = 3000; break;
                case QualityPreset::LOSSLESS: baseBitrate = 8000; break;
            }
            break;
            
        case VideoCodec::HEVC_HARDWARE:
        case VideoCodec::HEVC_SOFTWARE:
            switch (quality) {
                case QualityPreset::SMALL_SHARP: baseBitrate = 500; break;   // HEVC efficiency
                case QualityPreset::BALANCED: baseBitrate = 1000; break;
                case QualityPreset::HIGH_QUALITY: baseBitrate = 2000; break;
                case QualityPreset::LOSSLESS: baseBitrate = 5000; break;
            }
            break;
            
        case VideoCodec::AV1_HARDWARE:
            switch (quality) {
                case QualityPreset::SMALL_SHARP: baseBitrate = 400; break;   // AV1 efficiency
                case QualityPreset::BALANCED: baseBitrate = 800; break;
                case QualityPreset::HIGH_QUALITY: baseBitrate = 1500; break;
                case QualityPreset::LOSSLESS: baseBitrate = 4000; break;
            }
            break;
            
        default:
            baseBitrate = 1500; // Safe default
            break;
    }
    
    return static_cast<int>(baseBitrate * resolutionFactor);
}

#ifdef HAVE_FFMPEG
bool FFmpegEncoder::setupCodec(VideoCodec codec, QualityPreset quality, int width, int height, int fps) {
    const AVCodec* avCodec = nullptr;
    
    switch (codec) {
        case VideoCodec::H264_HARDWARE:
            // Try AMD hardware encoder first (detected AMD Ryzen system)
            avCodec = avcodec_find_encoder_by_name("h264_amf");    // AMD AMF (best for Radeon)
            if (!avCodec) avCodec = avcodec_find_encoder_by_name("libx264");     // Software fallback (reliable)
            if (!avCodec) avCodec = avcodec_find_encoder_by_name("h264_nvenc");  // NVIDIA 
            if (!avCodec) avCodec = avcodec_find_encoder_by_name("h264_qsv");    // Intel
            if (!avCodec) avCodec = avcodec_find_encoder(AV_CODEC_ID_H264);      // Final fallback
            break;
            
        case VideoCodec::H264_SOFTWARE:
            avCodec = avcodec_find_encoder_by_name("libx264");
            if (!avCodec) avCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
            break;
            
        case VideoCodec::HEVC_HARDWARE:
            // Prioritize AMD HEVC encoding for Ryzen system
            avCodec = avcodec_find_encoder_by_name("hevc_amf");    // AMD AMF HEVC
            if (!avCodec) avCodec = avcodec_find_encoder_by_name("libx265");     // Software fallback
            if (!avCodec) avCodec = avcodec_find_encoder_by_name("hevc_nvenc");  // NVIDIA
            if (!avCodec) avCodec = avcodec_find_encoder_by_name("hevc_qsv");    // Intel
            if (!avCodec) avCodec = avcodec_find_encoder(AV_CODEC_ID_HEVC);      // Final fallback
            break;
            
        case VideoCodec::HEVC_SOFTWARE:
            avCodec = avcodec_find_encoder_by_name("libx265");
            if (!avCodec) avCodec = avcodec_find_encoder(AV_CODEC_ID_HEVC);
            break;
            
        case VideoCodec::AV1_HARDWARE:
            avCodec = avcodec_find_encoder_by_name("av1_nvenc");
            if (!avCodec) avCodec = avcodec_find_encoder_by_name("av1_amf");
            if (!avCodec) avCodec = avcodec_find_encoder(AV_CODEC_ID_AV1);
            break;
            
        default:
            avCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
            break;
    }
    
    if (!avCodec) {
        std::cerr << "Codec not found" << std::endl;
        return false;
    }
    
    std::cout << "Using codec: " << avCodec->name << std::endl;
    
    codecContext = avcodec_alloc_context3(avCodec);
    if (!codecContext) {
        std::cerr << "Failed to allocate codec context" << std::endl;
        return false;
    }
    
    // Set quality-specific options for maximum compression
    if (strstr(avCodec->name, "h264_mf") || strstr(avCodec->name, "h264_amf") || strstr(avCodec->name, "h264_qsv")) {
        // Hardware H.264 (Media Foundation/AMF/QSV): use bitrate-based control for sharp UI
        // Compute target bitrate ~ 5 Mbps @ 1920x1080 scaled by resolution
        double pixelCount = width * height;
        double referencePixels = 1920.0 * 1080.0;
        double scale = std::max(0.5, pixelCount / referencePixels);
        int targetKbps = static_cast<int>(5000 * scale); // kbps
        targetKbps = std::min(std::max(targetKbps, 2500), 12000);

        codecContext->bit_rate = static_cast<int64_t>(targetKbps) * 1000;
        av_opt_set(codecContext->priv_data, "rc_mode", "cbr", 0);
        av_opt_set_int(codecContext->priv_data, "b", codecContext->bit_rate, 0);
        av_opt_set_int(codecContext->priv_data, "maxrate", codecContext->bit_rate, 0);
        av_opt_set_int(codecContext->priv_data, "bufsize", codecContext->bit_rate * 2, 0);
        av_opt_set_int(codecContext->priv_data, "g", fps * 2, 0); // GOP
        av_opt_set_int(codecContext->priv_data, "bf", 0, 0);
        av_opt_set(codecContext->priv_data, "profile", "high", 0);
        av_opt_set(codecContext->priv_data, "level", "4.2", 0);
        av_opt_set(codecContext->priv_data, "coder", "cabac", 0);
        av_opt_set(codecContext->priv_data, "tune", "stillimage", 0);

        std::cout << "- Using MF/QSV/AMF CBR ~" << targetKbps << " kbps for sharp UI" << std::endl;
    }
    else if (strstr(avCodec->name, "x264") || strstr(avCodec->name, "h264")) {
        // Software x264: use CRF for excellent UI quality
        int crf = 23; // Default
        switch (quality) {
            case QualityPreset::ULTRA_TINY: crf = 24; break;    // Still small but readable
            case QualityPreset::SMALL_SHARP: crf = 19; break;   // Good quality, readable text
            case QualityPreset::BALANCED: crf = 17; break;
            case QualityPreset::HIGH_QUALITY: crf = 15; break;
            case QualityPreset::LOSSLESS: crf = 0; break;
        }
        
        av_opt_set_int(codecContext->priv_data, "crf", crf, 0);
        av_opt_set(codecContext->priv_data, "preset", "veryslow", 0);  // Best compression
        av_opt_set(codecContext->priv_data, "tune", "stillimage", 0);  // Optimize for screen content
        
        // Screen recording optimizations
        av_opt_set_int(codecContext->priv_data, "keyint", 30, 0);     // GOP size
        av_opt_set_int(codecContext->priv_data, "min-keyint", 30, 0); // Force regular keyframes
        av_opt_set_int(codecContext->priv_data, "scenecut", 0, 0);    // No scene cut detection
        av_opt_set_int(codecContext->priv_data, "me-range", 8, 0);    // Smaller motion estimation
        av_opt_set_int(codecContext->priv_data, "subme", 2, 0);       // Faster subpixel ME
        av_opt_set_int(codecContext->priv_data, "ref", 1, 0);         // Only 1 reference frame
        av_opt_set_int(codecContext->priv_data, "b-adapt", 0, 0);     // No adaptive B-frames
        av_opt_set_int(codecContext->priv_data, "bframes", 0, 0);     // No B-frames for screen content
        
        std::cout << "- Using CRF " << crf << " with screen recording optimizations" << std::endl;
    }
    else if (strstr(avCodec->name, "x265") || strstr(avCodec->name, "hevc")) {
        // HEVC settings for even better compression
        int crf = 28;
        switch (quality) {
            case QualityPreset::ULTRA_TINY: crf = 30; break;    // Extreme HEVC compression but readable
            case QualityPreset::SMALL_SHARP: crf = 26; break;   // Good HEVC compression, readable text
            case QualityPreset::BALANCED: crf = 23; break;
            case QualityPreset::HIGH_QUALITY: crf = 20; break;
            case QualityPreset::LOSSLESS: crf = 0; break;
        }
        
        av_opt_set_int(codecContext->priv_data, "crf", crf, 0);
        av_opt_set(codecContext->priv_data, "preset", "veryslow", 0);
        av_opt_set(codecContext->priv_data, "tune", "stillimage", 0);
        av_opt_set_int(codecContext->priv_data, "keyint", 30, 0);
        
        std::cout << "- Using HEVC CRF " << crf << " for superior compression" << std::endl;
    }
    else if (strstr(avCodec->name, "nvenc")) {
        // NVIDIA NVENC hardware encoder settings
        int cq = 32;
        switch (quality) {
            case QualityPreset::SMALL_SHARP: cq = 42; break;   // Higher CQ = smaller files
            case QualityPreset::BALANCED: cq = 35; break;
            case QualityPreset::HIGH_QUALITY: cq = 28; break;
            case QualityPreset::LOSSLESS: cq = 0; break;
        }
        
        // NVENC-specific settings (different parameter names)
        av_opt_set_int(codecContext->priv_data, "cq", cq, 0);
        av_opt_set(codecContext->priv_data, "preset", "p7", 0);           // NVENC preset (p1=fastest, p7=slowest/best)
        av_opt_set(codecContext->priv_data, "tune", "hq", 0);             // High quality tune
        av_opt_set_int(codecContext->priv_data, "spatial_aq", 1, 0);      // Spatial adaptive quantization
        av_opt_set_int(codecContext->priv_data, "aq-strength", 8, 0);     // AQ strength for better quality
        av_opt_set_int(codecContext->priv_data, "rc", 0, 0);              // Rate control mode: CQ
        
        // Screen content optimizations for NVENC
        av_opt_set_int(codecContext->priv_data, "gpu", 0, 0);             // Use first GPU
        av_opt_set_int(codecContext->priv_data, "delay", 0, 0);           // No B-frame delay
        av_opt_set_int(codecContext->priv_data, "zerolatency", 1, 0);     // Low latency
        
        std::cout << "- Using NVENC CQ " << cq << " with preset p7 (best compression)" << std::endl;
    }
    else if (strstr(avCodec->name, "amf")) {
        // AMD AMF hardware encoder settings
        int cqp = 32;
        switch (quality) {
            case QualityPreset::SMALL_SHARP: cqp = 42; break;
            case QualityPreset::BALANCED: cqp = 35; break;
            case QualityPreset::HIGH_QUALITY: cqp = 28; break;
            case QualityPreset::LOSSLESS: cqp = 0; break;
        }
        
        // AMD AMF specific settings (different parameter names)
        av_opt_set_int(codecContext->priv_data, "cqp", cqp, 0);
        av_opt_set(codecContext->priv_data, "quality", "quality", 0);     // Quality mode
        av_opt_set(codecContext->priv_data, "usage", "ultralowlatency", 0); // Low latency for screen recording
        av_opt_set_int(codecContext->priv_data, "enforce_hrd", 1, 0);     // Better rate control
        av_opt_set_int(codecContext->priv_data, "filler_data", 0, 0);     // No filler for smaller files
        av_opt_set_int(codecContext->priv_data, "frame_skipping", 0, 0);  // No frame skipping
        
        std::cout << "- Using AMD AMF CQP " << cqp << " optimized for Radeon Graphics" << std::endl;
    }
    else if (strstr(avCodec->name, "qsv")) {
        // Intel QSV hardware encoder settings  
        int cqp = 32;
        switch (quality) {
            case QualityPreset::SMALL_SHARP: cqp = 42; break;
            case QualityPreset::BALANCED: cqp = 35; break;
            case QualityPreset::HIGH_QUALITY: cqp = 28; break;
            case QualityPreset::LOSSLESS: cqp = 0; break;
        }
        
        av_opt_set_int(codecContext->priv_data, "global_quality", cqp, 0);
        av_opt_set(codecContext->priv_data, "preset", "veryslow", 0);
        
        std::cout << "- Using Intel QSV CQP " << cqp << " for hardware acceleration" << std::endl;
    }
    
    return true;
}

bool FFmpegEncoder::writeFrame(AVFrame* frame) {
    int ret = avcodec_send_frame(codecContext, frame);
    if (ret < 0) {
        std::cerr << "Error sending frame to encoder" << std::endl;
        return false;
    }
    
    while (ret >= 0) {
        ret = avcodec_receive_packet(codecContext, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            std::cerr << "Error encoding frame" << std::endl;
            return false;
        }
        
        // Scale packet timestamps
        av_packet_rescale_ts(packet, codecContext->time_base, videoStream->time_base);
        packet->stream_index = videoStream->index;
        
        // Write packet
        ret = av_interleaved_write_frame(formatContext, packet);
        av_packet_unref(packet);
        
        if (ret < 0) {
            std::cerr << "Error writing packet" << std::endl;
            return false;
        }
    }
    
    return true;
}
#endif
