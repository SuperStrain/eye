#include "rtsp_frame_queue.h"
#include "rtsp_stream_source.h"
#include "rtsp_server.h"
#include "stream_frame.h"

#include <BasicUsageEnvironment.hh>
#include <MediaSink.hh>
#include <cassert>
#include <cstdint>
#include <deque>
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

    std::deque<RtspNalUnit> first;
    first.push_back(make_nal(0x11, 100));
    first.push_back(make_nal(0x12, 100));
    queue.push_access_unit(std::move(first));

    std::deque<RtspNalUnit> second;
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

struct DeliveryResult {
    bool called;
    unsigned frame_size;
    char volatile* watch_variable;
};

static void after_getting_frame(void* client_data, unsigned frame_size,
                                unsigned, timeval, unsigned) {
    DeliveryResult* result = static_cast<DeliveryResult*>(client_data);
    result->called = true;
    result->frame_size = frame_size;
    if (result->watch_variable) {
        *result->watch_variable = 1;
    }
}

static void timeout_handler(void* clientData) {
    *((char volatile*)clientData) = 1;
}

static void test_stream_source_waits_for_idr_on_first_play() {
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);
    std::shared_ptr<RtspFrameQueue> queue(new RtspFrameQueue(4));

    RtspStreamSource* source = RtspStreamSource::createNew(
        *env, StreamType::VIDEO_MAIN, CodecType::H265, queue);
    uint8_t output[16] = {};
    DeliveryResult result = {false, 0, nullptr};
    char volatile watch_variable = 0;

    // --- First PLAY: queue only has P NAL, should not deliver ---
    RtspNalUnit p_nal = make_nal(0x01, 100);
    p_nal.is_idr = false;
    queue->push_nal_unit(std::move(p_nal));

    source->getNextFrame(output, sizeof(output), after_getting_frame, &result,
                         NULL, NULL);
    assert(!result.called);

    // Push IDR, notify, drive event loop with 500ms timeout
    RtspNalUnit idr_nal = make_nal(0x26, 200);
    idr_nal.is_idr = true;
    queue->push_nal_unit(std::move(idr_nal));

    result = {false, 0, &watch_variable};
    queue->notify_active_sources();

    scheduler->scheduleDelayedTask(
        500000, timeout_handler, (void*)&watch_variable);
    scheduler->doEventLoop(&watch_variable);

    assert(result.called);
    assert(result.frame_size == 1);
    assert(output[0] == 0x26);

    // --- Re-PLAY: stop, queue exhausted + async notification ---
    source->stopGettingFrames();

    RtspNalUnit resumed_p = make_nal(0x01, 300);
    resumed_p.is_idr = false;
    queue->push_nal_unit(std::move(resumed_p));

    output[0] = 0;
    result = {false, 0, nullptr};
    source->getNextFrame(output, sizeof(output), after_getting_frame, &result,
                         NULL, NULL);
    assert(!result.called);

    RtspNalUnit resumed_idr = make_nal(0x26, 400);
    resumed_idr.is_idr = true;
    queue->push_nal_unit(std::move(resumed_idr));

    watch_variable = 0;
    result = {false, 0, &watch_variable};
    queue->notify_active_sources();

    scheduler->scheduleDelayedTask(
        500000, timeout_handler, (void*)&watch_variable);
    scheduler->doEventLoop(&watch_variable);

    assert(result.called);
    assert(output[0] == 0x26);

    Medium::close(source);
    env->reclaim();
    delete scheduler;
}

static void test_server_accepts_large_video_nals() {
    const unsigned original_max_size = OutPacketBuffer::maxSize;
    OutPacketBuffer::maxSize = 60000;

    RtspServer& server = RtspServer::instance();
    assert(server.start(18554));
    assert(OutPacketBuffer::maxSize >= 1024 * 1024);
    server.stop();

    OutPacketBuffer::maxSize = original_max_size;
}

int main() {
    test_queue_drops_whole_access_units();
    test_stream_frame_timestamp_is_non_zero_and_monotonic();
    test_stream_source_waits_for_idr_on_first_play();
    test_server_accepts_large_video_nals();
    return 0;
}
