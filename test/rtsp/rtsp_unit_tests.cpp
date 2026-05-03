#include "rtsp_frame_queue.h"
#include "stream_frame.h"

#include <cassert>
#include <cstdint>
#include <vector>

static RtspNalUnit make_nal(uint8_t value, uint64_t timestamp) {
    RtspNalUnit nal;
    nal.data.push_back(value);
    nal.timestamp = timestamp;
    nal.is_idr = false;
    return nal;
}

static void test_queue_drops_whole_access_units() {
    RtspFrameQueue queue(1);

    std::vector<RtspNalUnit> first;
    first.push_back(make_nal(0x11, 100));
    first.push_back(make_nal(0x12, 100));
    queue.push_access_unit(std::move(first));

    std::vector<RtspNalUnit> second;
    second.push_back(make_nal(0x21, 200));
    second.push_back(make_nal(0x22, 200));
    queue.push_access_unit(std::move(second));

    RtspNalUnit nal;
    assert(queue.pop_nal_unit(nal));
    assert(nal.data.size() == 1 && nal.data[0] == 0x21);
    assert(queue.pop_nal_unit(nal));
    assert(nal.data.size() == 1 && nal.data[0] == 0x22);
    assert(!queue.pop_nal_unit(nal));
}

static void test_stream_frame_timestamp_is_non_zero_and_monotonic() {
    uint8_t data[] = {0x65, 0x88};
    FrameData frame_data = {};
    frame_data.pack_count = 1;
    frame_data.packs[0].data = data;
    frame_data.packs[0].len = sizeof(data);
    frame_data.packs[0].nalu_type = NaluType::IDR_SLICE;

    StreamFrame first(VencChannel::CHN0, StreamType::VIDEO_MAIN,
                      CodecType::H265, frame_data);
    StreamFrame second(VencChannel::CHN0, StreamType::VIDEO_MAIN,
                       CodecType::H265, frame_data);

    assert(first.timestamp() > 0);
    assert(second.timestamp() > first.timestamp());
}

int main() {
    test_queue_drops_whole_access_units();
    test_stream_frame_timestamp_is_non_zero_and_monotonic();
    return 0;
}
