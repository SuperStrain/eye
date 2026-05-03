#ifndef RTSP_FRAME_QUEUE_H
#define RTSP_FRAME_QUEUE_H

#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

class RtspStreamSource;

struct RtspNalUnit {
    std::vector<uint8_t> data;
    uint64_t timestamp;
    bool is_idr;
};

class RtspFrameQueue {
public:
    explicit RtspFrameQueue(size_t max_queue_size);
    ~RtspFrameQueue();

    void push_nal_unit(RtspNalUnit nal);
    bool pop_nal_unit(RtspNalUnit& nal);
    void register_source(RtspStreamSource* source);
    void unregister_source(RtspStreamSource* source);
    void notify_active_sources();
    void clear();

private:
    std::deque<RtspNalUnit> queue_;
    std::vector<RtspStreamSource*> active_sources_;
    std::mutex mutex_;
    size_t max_queue_size_;
};

#endif
