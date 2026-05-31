#include "rtsp_frame_queue.h"
#include "stream_frame.h"

#include <cassert>
#include <cstdint>
#include <deque>
#include <vector>

static RtspNalUnit make_nal(uint8_t value, uint64_t timestamp, bool is_idr = false) {
    RtspNalUnit nal;
    nal.data.push_back(value);
    nal.timestamp = timestamp;
    nal.is_idr = is_idr;
    return nal;
}

static void test_queue_drops_whole_access_units() {
    RtspFrameQueue queue(1);

    std::deque<RtspNalUnit> first;
    first.push_back(make_nal(0x11, 100));
    first.push_back(make_nal(0x12, 100));
    queue.push_access_unit(std::move(first));

    std::deque<RtspNalUnit> second;
    second.push_back(make_nal(0x21, 200, true));
    second.push_back(make_nal(0x22, 200));
    queue.push_access_unit(std::move(second));

    RtspNalUnit nal;
    assert(queue.pop_nal_unit(nal));
    assert(nal.data.size() == 1 && nal.data[0] == 0x21);
    assert(queue.pop_nal_unit(nal));
    assert(nal.data.size() == 1 && nal.data[0] == 0x22);
    assert(!queue.pop_nal_unit(nal));
}

static void test_queue_skips_non_idr_after_overflow_until_next_idr() {
    RtspFrameQueue queue(1);

    std::deque<RtspNalUnit> first;
    first.push_back(make_nal(0x11, 100, false));
    queue.push_access_unit(std::move(first));

    std::deque<RtspNalUnit> second;
    second.push_back(make_nal(0x21, 200, false));
    queue.push_access_unit(std::move(second));

    std::deque<RtspNalUnit> third;
    third.push_back(make_nal(0x31, 300, false));
    queue.push_access_unit(std::move(third));

    RtspNalUnit nal;
    assert(!queue.pop_nal_unit(nal));

    std::deque<RtspNalUnit> idr;
    idr.push_back(make_nal(0x41, 400, true));
    idr.push_back(make_nal(0x42, 400, false));
    queue.push_access_unit(std::move(idr));

    assert(queue.pop_nal_unit(nal));
    assert(nal.data.size() == 1 && nal.data[0] == 0x41);
    assert(queue.pop_nal_unit(nal));
    assert(nal.data.size() == 1 && nal.data[0] == 0x42);
    assert(!queue.pop_nal_unit(nal));
}

static void test_queue_discards_stale_p_frames_when_new_idr_overflows() {
    RtspFrameQueue queue(3);

    std::deque<RtspNalUnit> old_idr;
    old_idr.push_back(make_nal(0x11, 100, true));
    queue.push_access_unit(std::move(old_idr));

    std::deque<RtspNalUnit> old_p1;
    old_p1.push_back(make_nal(0x21, 200, false));
    queue.push_access_unit(std::move(old_p1));

    std::deque<RtspNalUnit> old_p2;
    old_p2.push_back(make_nal(0x31, 300, false));
    queue.push_access_unit(std::move(old_p2));

    std::deque<RtspNalUnit> new_idr;
    new_idr.push_back(make_nal(0x41, 400, true));
    queue.push_access_unit(std::move(new_idr));

    RtspNalUnit nal;
    assert(queue.pop_nal_unit(nal));
    assert(nal.data.size() == 1 && nal.data[0] == 0x41);
    assert(!queue.pop_nal_unit(nal));
}

static void test_queue_does_not_request_idr_when_overflow_frame_is_idr() {
    RtspFrameQueue queue(1);
    int overflow_count = 0;
    queue.set_overflow_callback([&overflow_count] { ++overflow_count; });

    std::deque<RtspNalUnit> first;
    first.push_back(make_nal(0x11, 100, true));
    queue.push_access_unit(std::move(first));
    assert(overflow_count == 0);

    std::deque<RtspNalUnit> second;
    second.push_back(make_nal(0x21, 200, true));
    queue.push_access_unit(std::move(second));
    assert(overflow_count == 0);
}

static void test_queue_notifies_overflow_even_when_new_frame_is_not_idr() {
    RtspFrameQueue queue(1);
    int overflow_count = 0;
    queue.set_overflow_callback([&overflow_count] { ++overflow_count; });

    std::deque<RtspNalUnit> first;
    first.push_back(make_nal(0x11, 100, true));
    queue.push_access_unit(std::move(first));

    std::deque<RtspNalUnit> second;
    second.push_back(make_nal(0x21, 200, false));
    queue.push_access_unit(std::move(second));

    RtspNalUnit nal;
    assert(!queue.pop_nal_unit(nal));
    assert(overflow_count == 1);

    RtspQueueStats stats = queue.stats();
    assert(stats.queue_depth == 0);
    assert(stats.max_queue_size == 1);
    assert(stats.peak_queue_depth == 1);
    assert(stats.dropped == 1);
    assert(stats.skipped == 1);
}

static void test_stream_frame_metadata() {
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
    assert(second.frame_id() == first.frame_id() + 1);
    assert(first.frame_type() == NaluType::IDR_SLICE);
    assert(first.frame_size() == sizeof(data));
}

int main() {
    test_queue_drops_whole_access_units();
    test_queue_skips_non_idr_after_overflow_until_next_idr();
    test_queue_discards_stale_p_frames_when_new_idr_overflows();
    test_queue_does_not_request_idr_when_overflow_frame_is_idr();
    test_queue_notifies_overflow_even_when_new_frame_is_not_idr();
    test_stream_frame_metadata();
    return 0;
}
