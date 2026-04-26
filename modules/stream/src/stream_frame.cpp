#include "stream_frame.h"
#include <algorithm>

StreamFrame::StreamFrame(VencChannel chn, StreamType type, CodecType codec, const FrameData& data)
    : channel_(chn), type_(type), codec_type_(codec), frame_data_(data) {
    timestamp_ = 0;  // FrameData 中无 pts 字段，暂设为0

    // 深拷贝 pack 数据，避免上游释放 buffer 后产生悬空指针
    if (frame_data_.pack_count > 0) {
        pack_data_.reserve(frame_data_.pack_count);
        for (uint32_t i = 0; i < frame_data_.pack_count; ++i) {
            const auto& pk = frame_data_.packs[i];
            if (pk.data != nullptr && pk.len > 0) {
                pack_data_.emplace_back(pk.data, pk.data + pk.len);
                frame_data_.packs[i].data = pack_data_.back().data();
            } else {
                pack_data_.emplace_back();
                frame_data_.packs[i].data = nullptr;
            }
        }
    }
}

StreamFrame::~StreamFrame() {
}

bool StreamFrame::is_idr() const {
    if (codec_type_ == CodecType::MJPEG) {
        return true;
    }
    uint32_t count = frame_data_.pack_count;
    if (count > FrameData::MAX_PACKS) {
        count = FrameData::MAX_PACKS;
    }
    for (uint32_t i = 0; i < count; ++i) {
        if (frame_data_.packs[i].nalu_type == NaluType::IDR_SLICE) {
            return true;
        }
    }
    return false;
}
