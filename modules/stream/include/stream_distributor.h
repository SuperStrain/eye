#ifndef STREAM_DISTRIBUTOR_H
#define STREAM_DISTRIBUTOR_H

#include "stream_frame.h"
#include "stream_common.h"
#include <cstdint>
#include <deque>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

struct ConsumerSlot {
    uint32_t consumer_id;
    std::deque<StreamFramePtr> queue;
    size_t max_queue_size;
    size_t peak_queue_depth{0};
    std::mutex mutex;
    std::condition_variable cv;
    std::thread worker;
    ConsumerCallback callback;
    ConsumerConfig config;
    std::atomic<bool> running{true};
    std::atomic<uint64_t> dropped{0};
    std::atomic<uint64_t> consumed{0};
};

struct ConsumerStats {
    size_t queue_depth;
    size_t max_queue_size;
    size_t peak_queue_depth;
    uint64_t dropped;
    uint64_t consumed;
};

class StreamDistributor {
public:
    StreamDistributor(VencChannel chn);
    ~StreamDistributor();

    VencChannel channel() const { return channel_; }

    uint32_t add_consumer(ConsumerCallback cb, ConsumerConfig config);
    void remove_consumer(uint32_t id);
    bool get_consumer_stats(uint32_t id, ConsumerStats& stats);

    void push(StreamFramePtr frame);

private:
    void worker_func(std::shared_ptr<ConsumerSlot> slot);

    VencChannel channel_;
    std::mutex slots_mutex_;    // ConsumerSlot
    std::vector<std::shared_ptr<ConsumerSlot>> slots_;
    std::atomic<uint32_t> next_consumer_id_{0};
};

#endif // STREAM_DISTRIBUTOR_H
