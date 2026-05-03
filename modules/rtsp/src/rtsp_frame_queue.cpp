#include "rtsp_frame_queue.h"
#include "rtsp_stream_source.h"
#include "logger.h"

RtspFrameQueue::RtspFrameQueue(size_t max_queue_size)
    : max_queue_size_(max_queue_size) {
    if (max_queue_size_ == 0) {
        max_queue_size_ = 1;
    }
}

RtspFrameQueue::~RtspFrameQueue() {}

void RtspFrameQueue::push_nal_unit(RtspNalUnit nal) {
    std::lock_guard<std::mutex> lock(mutex_);
    while (queue_.size() >= max_queue_size_) {
        queue_.pop_front();
        std::chrono::steady_clock::time_point now =
            std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(
                now - last_overflow_log_time_).count() >= 5) {
            LOGGER_WARN(RTSP,
                "Frame queue overflow, dropping oldest NAL");
            last_overflow_log_time_ = now;
        }
    }
    queue_.push_back(std::move(nal));
}

bool RtspFrameQueue::pop_nal_unit(RtspNalUnit& nal) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
        return false;
    }
    nal = std::move(queue_.front());
    queue_.pop_front();
    return true;
}

void RtspFrameQueue::register_source(RtspStreamSource* source) {
    std::lock_guard<std::mutex> lock(mutex_);
    active_sources_.push_back(source);
}

void RtspFrameQueue::unregister_source(RtspStreamSource* source) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = active_sources_.begin(); it != active_sources_.end(); ++it) {
        if (*it == source) {
            active_sources_.erase(it);
            break;
        }
    }
}

void RtspFrameQueue::notify_active_sources() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto* source : active_sources_) {
        source->notify_frame_available();
    }
}

void RtspFrameQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.clear();
}

bool RtspFrameQueue::has_active_sources() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !active_sources_.empty();
}
