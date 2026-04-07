#ifndef _RECORDER_CONSUMER_H_
#define _RECORDER_CONSUMER_H_

#include "stream_frame.h"
#include "stream_common.h"
#include <fstream>
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

struct WriteEntry {
    std::vector<uint8_t> data;
    bool is_idr;
};

class RecorderConsumer {
public:
    RecorderConsumer(const std::string& filepath, size_t write_queue_size = 30);
    ~RecorderConsumer();

    void on_frame(const StreamFrame& frame);

    uint64_t dropped() const { return dropped_; }
    uint64_t consumed() const { return consumed_; }

private:
    void write_loop();
    void try_drop_frame();

    static constexpr size_t kMaxWriteQueue = 30;
    std::deque<WriteEntry> write_queue_;
    size_t max_queue_size_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread writer_thread_;
    std::ofstream file_;
    std::atomic<bool> running_{true};
    std::atomic<uint64_t> dropped_{0};
    std::atomic<uint64_t> consumed_{0};
};

#endif
