#include "stream_frame.h"

#include <algorithm>
#include <chrono>
#include <mutex>

static uint64_t next_monotonic_timestamp_ms() {
    using namespace std::chrono;

    static std::mutex timestamp_mutex;
    static uint64_t last_timestamp = 0;

    uint64_t now = static_cast<uint64_t>(
        duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()).count());

    std::lock_guard<std::mutex> lock(timestamp_mutex);
    if (now <= last_timestamp) {
        now = last_timestamp + 1;
    }
    last_timestamp = now;
    return now;
}

StreamFrame::StreamFrame(VencChannel chn, StreamType type, CodecType codec, const FrameData& data)
    : channel_(chn), type_(type), codec_type_(codec), frame_data_(data) {
    timestamp_ = next_monotonic_timestamp_ms();

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
