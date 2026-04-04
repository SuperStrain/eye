#include "stream_distributor.h"
#include <algorithm>
#include <pthread.h>   // pthread_setschedparam, pthread_self
#include <sched.h>     // sched_param, SCHED_RR

StreamDistributor::StreamDistributor(VencChannel chn) : channel_(chn) {}

StreamDistributor::~StreamDistributor() {
    std::vector<std::shared_ptr<ConsumerSlot>> snapshot;
    {
        std::lock_guard<std::mutex> lock(slots_mutex_);
        snapshot = std::move(slots_);
    }

    for (auto& slot : snapshot) {
        // FIX 1: running 是 std::atomic<bool>，store 保证对 worker 线程可见
        slot->running.store(false, std::memory_order_release);
        slot->cv.notify_all();
        if (slot->worker.joinable()) {
            slot->worker.join();
        }
    }
}

uint32_t StreamDistributor::add_consumer(ConsumerCallback cb, ConsumerConfig config) {
    auto slot = std::make_shared<ConsumerSlot>();
    // FIX 3: next_consumer_id_ 是 std::atomic<uint32_t>，fetch_add 保证原子自增
    slot->consumer_id = next_consumer_id_.fetch_add(1, std::memory_order_relaxed);
    slot->callback = std::move(cb);
    slot->max_queue_size = config.max_queue_size;
    slot->config = config;
    // running 已在 ConsumerSlot 定义中初始化为 true，无需重复 store。

    // FIX 2 (revised): 持锁期间同时完成入队和启动线程，彻底消除竞态窗口。
    //   先入队后启动（锁已释放）仍有窗口：析构或 remove_consumer 可能在入队后、
    //   线程启动前执行，设 running=false 并跳过 join（此时尚未 joinable），
    //   随后线程启动后立即退出，ConsumerSlot 析构时 thread 仍 joinable → std::terminate。
    //   在同一把锁内完成两步，使析构与 add_consumer 对 slots_mutex_ 的竞争严格串行。
    {
        std::lock_guard<std::mutex> lock(slots_mutex_);
        slots_.push_back(slot);
        slot->worker = std::thread(&StreamDistributor::worker_func, this, slot);
    }

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

    // FIX 1: 原子写，对应 worker_func 中的原子读
    target->running.store(false, std::memory_order_release);
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

    // FIX 1: load 保证读到最新值
    while (slot->running.load(std::memory_order_acquire)) {
        StreamFramePtr frame;
        {
            std::unique_lock<std::mutex> lock(slot->mutex);
            slot->cv.wait(lock, [&slot] {
                // FIX 1: lambda 内同样使用原子读
                return !slot->queue.empty() || !slot->running.load(std::memory_order_acquire);
            });
            if (!slot->running.load(std::memory_order_acquire)) break;
            frame = std::move(slot->queue.front());
            slot->queue.pop_front();
        }

        try {
            slot->callback(*frame);
            slot->consumed++;
        } catch (const std::exception& e) {
        }
    }
}
