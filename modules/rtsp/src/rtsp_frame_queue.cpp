#include "rtsp_frame_queue.h"
#include "rtsp_stream_source.h"
#include "logger.h"

static bool access_unit_has_idr(const std::deque<RtspNalUnit>& nals) {
    for (const RtspNalUnit& nal : nals) {
        if (nal.is_idr) return true;
    }
    return false;
}

RtspFrameQueue::RtspFrameQueue(size_t max_queue_size)
    : max_queue_size_(max_queue_size),
      waiting_for_idr_after_drop_(false) {
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

    bool has_idr = access_unit_has_idr(nals);
    std::function<void()> overflow_callback;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= max_queue_size_) {
            drop_oldest_access_unit_locked();
            if (!has_idr) {
                overflow_callback = overflow_callback_;
            }
        }

        if (waiting_for_idr_after_drop_) {
            if (!has_idr) {
                nals.clear();
            } else {
                waiting_for_idr_after_drop_ = false;
            }
        }

        if (!nals.empty()) {
            RtspAccessUnit access_unit;
            access_unit.nals = std::move(nals);
            queue_.push_back(std::move(access_unit));
        }
    }

    if (overflow_callback) {
        overflow_callback();
    }
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

void RtspFrameQueue::drop_oldest_access_unit_locked() {
    bool dropped_idr = access_unit_has_idr(queue_.front().nals);
    queue_.clear();
    waiting_for_idr_after_drop_ = true;

    std::chrono::steady_clock::time_point now =
        std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(
            now - last_overflow_log_time_).count() >= 5) {
        if (dropped_idr) {
            LOGGER_WARN(RTSP,
                "Frame queue overflow, dropping IDR access unit, skipping frames until next IDR");
        } else {
            LOGGER_WARN(RTSP,
                "Frame queue overflow, dropping non-IDR access unit, reference chain broken, skipping P-frames until next IDR");
        }
        last_overflow_log_time_ = now;
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
    waiting_for_idr_after_drop_ = false;
}

bool RtspFrameQueue::has_active_sources() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !active_sources_.empty();
}

void RtspFrameQueue::set_overflow_callback(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    overflow_callback_ = std::move(callback);
}
