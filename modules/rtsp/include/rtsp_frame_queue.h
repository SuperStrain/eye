#ifndef RTSP_FRAME_QUEUE_H
#define RTSP_FRAME_QUEUE_H

#include <cstdint>
#include <chrono>
#include <deque>
#include <functional>
#include <mutex>
#include <vector>

class RtspStreamSource;

struct RtspNalUnit {
    std::vector<uint8_t> data;
    uint64_t timestamp;
    bool is_idr;
};

struct RtspAccessUnit {
    std::deque<RtspNalUnit> nals;
};

struct RtspQueueStats {
    size_t queue_depth;
    size_t max_queue_size;
    size_t peak_queue_depth;
    size_t active_sources;
    uint64_t dropped;
    uint64_t skipped;
};

class RtspFrameQueue {
public:
    explicit RtspFrameQueue(size_t max_queue_size);
    ~RtspFrameQueue();

    void push_nal_unit(RtspNalUnit nal);
    void push_access_unit(std::deque<RtspNalUnit> nals);
    bool pop_nal_unit(RtspNalUnit& nal);
    void register_source(RtspStreamSource* source);
    void unregister_source(RtspStreamSource* source);
    void notify_active_sources();
    void clear();
    bool has_active_sources() const;
    RtspQueueStats stats() const;
    void set_overflow_callback(std::function<void()> callback);

private:
    void drop_oldest_access_unit_locked();

    std::deque<RtspAccessUnit> queue_;
    std::vector<RtspStreamSource*> active_sources_;
    mutable std::mutex mutex_;
    size_t max_queue_size_;
    size_t peak_queue_depth_;
    bool waiting_for_idr_after_drop_;
    uint64_t dropped_;
    uint64_t skipped_;
    std::function<void()> overflow_callback_;
    mutable std::chrono::steady_clock::time_point last_overflow_log_time_;
};

#endif
