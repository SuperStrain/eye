#include "rtsp_frame_queue.h"
#include "rtsp_stream_source.h"
#include "logger.h"

RtspFrameQueue::RtspFrameQueue(size_t max_queue_size)
    : max_queue_size_(max_queue_size),
      reference_broken_(false) {
    if (max_queue_size_ == 0) {
        max_queue_size_ = 1;
    }
}

RtspFrameQueue::~RtspFrameQueue() {}

void RtspFrameQueue::push_nal_unit(RtspNalUnit nal) {
    std::deque<RtspNalUnit> nals;
    nals.push_back(std::move(nal));
    push_access_unit(std::move(nals));
}

void RtspFrameQueue::push_access_unit(std::deque<RtspNalUnit> nals) {
    if (nals.empty()) return;

    std::lock_guard<std::mutex> lock(mutex_);

    bool is_idr = false;
    for (size_t i = 0; i < nals.size(); ++i) {
        if (nals[i].is_idr) { is_idr = true; break; }
    }

    if (reference_broken_ && !is_idr) {
        return;
    }

    if (is_idr) {
        reference_broken_ = false;
    }

    while (queue_.size() >= max_queue_size_) {
        drop_oldest_access_unit_locked();
    }

    RtspAccessUnit access_unit;
    access_unit.nals = std::move(nals);
    queue_.push_back(std::move(access_unit));
}

bool RtspFrameQueue::pop_nal_unit(RtspNalUnit& nal) {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!queue_.empty() && queue_.front().nals.empty()) {
        queue_.pop_front();
    }
    if (queue_.empty()) {
        return false;
    }

    RtspAccessUnit& access_unit = queue_.front();
    nal = std::move(access_unit.nals.front());
    access_unit.nals.pop_front();
    if (access_unit.nals.empty()) {
        queue_.pop_front();
    }
    return true;
}

static bool access_unit_is_idr(const RtspAccessUnit& au) {
    for (size_t i = 0; i < au.nals.size(); ++i) {
        if (au.nals[i].is_idr) return true;
    }
    return false;
}

void RtspFrameQueue::drop_oldest_access_unit_locked() {
    for (auto it = queue_.begin(); it != queue_.end(); ++it) {
        if (!access_unit_is_idr(*it)) {
            reference_broken_ = true;
            queue_.erase(it);
            std::chrono::steady_clock::time_point now =
                std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(
                    now - last_overflow_log_time_).count() >= 5) {
                LOGGER_WARN(RTSP,
                    "Frame queue overflow, dropping non-IDR access unit, reference chain broken, skipping P-frames until next IDR");
                last_overflow_log_time_ = now;
            }
            return;
        }
    }

    if (!queue_.empty()) {
        reference_broken_ = true;
        queue_.pop_front();
        LOGGER_WARN(RTSP,
            "Frame queue overflow, forced to drop IDR access unit");
    }
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
    reference_broken_ = false;
}

bool RtspFrameQueue::has_active_sources() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !active_sources_.empty();
}
