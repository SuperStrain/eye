#include "stream_frame.h"
#include <algorithm>

StreamFrame::StreamFrame(VencChannel chn, StreamType type, CodecType codec, const FrameData& data)
    : channel_(chn), type_(type), codec_type_(codec), frame_data_(data) {
    timestamp_ = 0;  // FrameData 中无 pts 字段，暂设为0
}

StreamFrame::~StreamFrame() {
}

bool StreamFrame::is_idr() const {
    if (codec_type_ == CodecType::MJPEG) {
        return true;
    }
    for (uint32_t i = 0; i < frame_data_.pack_count; ++i) {
        if (frame_data_.packs[i].nalu_type == NaluType::IDR_SLICE) {
            return true;
        }
    }
    return false;
}
