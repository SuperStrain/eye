#include "recorder_consumer.h"
#include <algorithm>

RecorderConsumer::RecorderConsumer(const std::string& filepath, size_t write_queue_size)
    : max_queue_size_(write_queue_size) {
    file_.open(filepath, std::ios::binary);
    writer_thread_ = std::thread(&RecorderConsumer::write_loop, this);
}

RecorderConsumer::~RecorderConsumer() {
    running_ = false;
    cv_.notify_all();
    if (writer_thread_.joinable()) {
        writer_thread_.join();
    }
}

void RecorderConsumer::on_frame(StreamFramePtr frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (write_queue_.size() >= max_queue_size_) {
        try_drop_frame();
    }
    write_queue_.push_back(std::move(frame));
    cv_.notify_one();
}

void RecorderConsumer::write_loop() {
    while (running_) {
        StreamFramePtr frame;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] {
                return !write_queue_.empty() || !running_;
            });
            if (!running_) break;
            frame = std::move(write_queue_.front());
            write_queue_.pop_front();
        }

        const auto& stream = frame->data();
        for (uint32_t i = 0; i < stream.pack_cnt; ++i) {
            file_.write(reinterpret_cast<const char*>(stream.pack[i].addr),
                        stream.pack[i].len);
        }
        consumed_++;
    }
}

void RecorderConsumer::try_drop_frame() {
    for (auto it = write_queue_.begin(); it != write_queue_.end(); ++it) {
        if (!is_idr_frame(*it)) {
            write_queue_.erase(it);
            dropped_++;
            return;
        }
    }
    write_queue_.pop_front();
    dropped_++;
}

bool RecorderConsumer::is_idr_frame(const StreamFramePtr& frame) const {
    if (frame->codec_type() == CodecType::MJPEG) {
        return true;
    }

    const auto& stream = frame->data();
    for (uint32_t i = 0; i < stream.pack_cnt; ++i) {
        const auto& dt = stream.pack[i].data_type;

        if (frame->codec_type() == CodecType::H264) {
            if (dt.h264_type == OT_VENC_H264_NALU_IDR_SLICE) {
                return true;
            }
        } else if (frame->codec_type() == CodecType::H265) {
            if (dt.h265_type == OT_VENC_H265_NALU_IDR_SLICE) {
                return true;
            }
        }
    }
    return false;
}
