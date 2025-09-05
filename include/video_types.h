#pragma once

/**
 * File: include/video_types.h
 * Purpose: Defines video codec and quality preset enums shared across recording components
 */

enum class VideoCodec {
    H264_HARDWARE,  // NVENC/AMD/Intel QSV H.264
    H264_SOFTWARE,  // Software H.264
    HEVC_HARDWARE,  // NVENC/AMD/Intel QSV H.265
    HEVC_SOFTWARE,  // Software H.265
    AV1_HARDWARE,   // Hardware AV1 (newer GPUs)
    AV1_SOFTWARE,   // Software AV1
    MJPEG          // Fallback uncompressed
};

enum class QualityPreset {
    ULTRA_TINY,     // Extreme compression, may sacrifice readability
    SMALL_SHARP,    // Small file size, sharp quality for UI/text
    BALANCED,       // Good balance of size and quality
    HIGH_QUALITY,   // Larger files, better quality
    LOSSLESS        // Maximum quality, largest files
};
