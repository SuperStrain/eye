#ifndef RTSP_NAL_PARSER_H
#define RTSP_NAL_PARSER_H

#include <cstdint>
#include <vector>
#include "common_types.h"

enum class H264NalType : uint8_t {
    NON_IDR = 1,
    IDR = 5,
    SPS = 7,
    PPS = 8,
    OTHER = 0xFF
};

enum class H265NalType : uint8_t {
    VPS = 32,
    SPS = 33,
    PPS = 34,
    IDR_W_RADL = 19,
    IDR_N_LP = 20,
    OTHER = 0xFF
};

struct ParsedNal {
    std::vector<uint8_t> data;
    uint64_t timestamp;
    bool is_idr;
    bool is_vps;
    bool is_sps;
    bool is_pps;
};

H264NalType parse_h264_nal_type(const uint8_t* data, uint32_t len);
H265NalType parse_h265_nal_type(const uint8_t* data, uint32_t len);

void split_annex_b_nals(const uint8_t* data, uint32_t len,
                        CodecType codec, uint64_t timestamp, bool is_idr,
                        std::vector<ParsedNal>& out);

#endif
