#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <cstdint>

enum class VencChannel { CHN0 = 0, CHN1 = 1, CHN2 = 2 };

enum class StreamType {
    VIDEO_MAIN,
    VIDEO_SUB,
    VIDEO_MJPEG,
    AUDIO_AAC
};

enum class CodecType { H264, H265, MJPEG };

struct Size {
    uint32_t width;
    uint32_t height;
};

enum class NaluType : uint8_t {
    IDR_SLICE,
    P_SLICE,
    B_SLICE,
    OTHER,
};

struct FramePack {
    const uint8_t* data;
    uint32_t len;
    NaluType nalu_type;
};

struct FrameData {
    static constexpr uint32_t MAX_PACKS = 16;
    FramePack packs[MAX_PACKS];
    uint32_t pack_count;
    uint32_t seq;
};

#endif
