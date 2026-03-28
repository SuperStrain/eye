#include "stream_distributor.h"
#include <algorithm>

StreamDistributor::StreamDistributor(VencChannel chn) : channel_(chn) {}

StreamDistributor::~StreamDistributor() {
    std::lock_guard<std::mutex> lock(slots_mutex_);
    for (auto& slot : slots_) {
        slot->running = false;
        slot->cv.notify_all();
        if (slot->worker.joinable()) {
            slot->worker.join();
        }
    }
}

uint32_t StreamDistributor::add_consumer(ConsumerCallback cb, ConsumerConfig config) {
    auto slot = std::make_shared<ConsumerSlot>();
    slot->consumer_id = next_consumer_id_++;
    slot->callback = std::move(cb);
    slot->max_queue_size = config.max_queue_size;
    slot->config = config;

    slot->worker = std::thread(&StreamDistributor::worker_func, this, slot);

    std::lock_guard<std::mutex> lock(slots_mutex_);
    slots_.push_back(slot);
    return slot->consumer_id;
}

void StreamDistributor::remove_consumer(uint32_t id) {
    std::shared_ptr<ConsumerSlot> target;
    {
        std::lock_guard<std::mutex> lock(slots_mutex_);
        auto it = std::find_if(slots_.begin(), slots_.end(),
            [id](const std::shared_ptr<ConsumerSlot>& s) { return s->consumer_id == id; });
        if (it == slots_.end()) return;
        target = *it;
        slots_.erase(it);
    }

    target->running = false;
    target->cv.notify_all();
    if (target->worker.joinable()) {
        target->worker.join();
    }

    {
        std::lock_guard<std::mutex> lock(target->mutex);
        target->queue.clear();
    }
}

void StreamDistributor::push(StreamFramePtr frame) {
    std::vector<std::shared_ptr<ConsumerSlot>> snapshot;
    {
        std::lock_guard<std::mutex> lock(slots_mutex_);
        snapshot = slots_;
    }

    for (auto& slot : snapshot) {
        StreamFramePtr dropped;
        {
            std::lock_guard<std::mutex> q_lock(slot->mutex);
            if (slot->queue.size() >= slot->max_queue_size) {
                dropped = std::move(slot->queue.front());
                slot->queue.pop_front();
                slot->dropped++;
            }
            slot->queue.push_back(frame);
        }
        slot->cv.notify_one();
    }
}

void StreamDistributor::worker_func(std::shared_ptr<ConsumerSlot> slot) {
    if (slot->config.thread_priority > 0) {
        sched_param sched;
        sched.sched_priority = slot->config.thread_priority;
        pthread_setschedparam(pthread_self(), SCHED_RR, &sched);
    }

    while (slot->running) {
        StreamFramePtr frame;
        {
            std::unique_lock<std::mutex> lock(slot->mutex);
            slot->cv.wait(lock, [slot] {
                return !slot->queue.empty() || !slot->running;
            });
            if (!slot->running) break;
            frame = std::move(slot->queue.front());
            slot->queue.pop_front();
        }

        try {
            slot->callback(frame);
            slot->consumed++;
        } catch (const std::exception& e) {
        }
    }
}
