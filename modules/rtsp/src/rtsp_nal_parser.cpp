#include "rtsp_nal_parser.h"

H264NalType parse_h264_nal_type(const uint8_t* data, uint32_t len) {
    if (len < 1) return H264NalType::OTHER;
    uint8_t nal_type = data[0] & 0x1F;
    switch (nal_type) {
        case 1:  return H264NalType::NON_IDR;
        case 5:  return H264NalType::IDR;
        case 7:  return H264NalType::SPS;
        case 8:  return H264NalType::PPS;
        default: return H264NalType::OTHER;
    }
}

H265NalType parse_h265_nal_type(const uint8_t* data, uint32_t len) {
    if (len < 2) return H265NalType::OTHER;
    uint8_t nal_type = (data[0] >> 1) & 0x3F;
    switch (nal_type) {
        case 32: return H265NalType::VPS;
        case 33: return H265NalType::SPS;
        case 34: return H265NalType::PPS;
        case 19: return H265NalType::IDR_W_RADL;
        case 20: return H265NalType::IDR_N_LP;
        default: return H265NalType::OTHER;
    }
}

static uint32_t start_code_len(const uint8_t* p, const uint8_t* end) {
    if (p + 3 <= end && p[0] == 0 && p[1] == 0 && p[2] == 1) {
        return 3;
    }
    if (p + 4 <= end && p[0] == 0 && p[1] == 0 && p[2] == 0 && p[3] == 1) {
        return 4;
    }
    return 0;
}

static void classify_nal(const uint8_t* nal_data, uint32_t nal_len,
                         CodecType codec, bool frame_is_idr,
                         ParsedNal& out) {
    out.is_idr = false;
    out.is_vps = false;
    out.is_sps = false;
    out.is_pps = false;

    if (codec == CodecType::H264) {
        H264NalType t = parse_h264_nal_type(nal_data, nal_len);
        switch (t) {
            case H264NalType::IDR:  out.is_idr = true; break;
            case H264NalType::SPS:  out.is_sps = true; break;
            case H264NalType::PPS:  out.is_pps = true; break;
            default: break;
        }
    } else if (codec == CodecType::H265) {
        H265NalType t = parse_h265_nal_type(nal_data, nal_len);
        switch (t) {
            case H265NalType::IDR_W_RADL:
            case H265NalType::IDR_N_LP:
                out.is_idr = true;
                break;
            case H265NalType::VPS:  out.is_vps = true; break;
            case H265NalType::SPS:  out.is_sps = true; break;
            case H265NalType::PPS:  out.is_pps = true; break;
            default: break;
        }
    }
}

void split_annex_b_nals(const uint8_t* data, uint32_t len,
                        CodecType codec, uint64_t timestamp, bool is_idr,
                        std::vector<ParsedNal>& out) {
    if (len == 0) return;

    std::vector<uint32_t> nal_offsets;
    std::vector<uint32_t> nal_sc_lens;

    uint32_t i = 0;
    while (i < len) {
        uint32_t sc_len = start_code_len(data + i, data + len);
        if (sc_len > 0) {
            nal_offsets.push_back(i + sc_len);
            nal_sc_lens.push_back(sc_len);
            i += sc_len;
            continue;
        }
        ++i;
    }

    if (nal_offsets.empty()) {
        ParsedNal nal;
        nal.data.assign(data, data + len);
        nal.timestamp = timestamp;
        classify_nal(nal.data.data(), static_cast<uint32_t>(nal.data.size()),
                     codec, is_idr, nal);
        out.push_back(std::move(nal));
        return;
    }

    for (size_t n = 0; n < nal_offsets.size(); ++n) {
        uint32_t start = nal_offsets[n];
        uint32_t end = (n + 1 < nal_offsets.size())
                           ? nal_offsets[n + 1] - nal_sc_lens[n + 1]
                           : len;
        if (end <= start) continue;

        ParsedNal nal;
        nal.data.assign(data + start, data + end);
        nal.timestamp = timestamp;
        classify_nal(nal.data.data(), static_cast<uint32_t>(nal.data.size()),
                     codec, is_idr, nal);
        out.push_back(std::move(nal));
    }
}
