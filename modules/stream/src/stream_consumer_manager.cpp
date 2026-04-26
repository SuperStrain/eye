#include "stream_consumer_manager.h"
#include "logger.h"

StreamConsumerManager& StreamConsumerManager::instance() {
    static StreamConsumerManager instance;
    return instance;
}

StreamConsumerManager::StreamConsumerManager() {
    distributors_[StreamType::VIDEO_MAIN] =
        std::unique_ptr<StreamDistributor>(new StreamDistributor(VencChannel::CHN0));
    distributors_[StreamType::VIDEO_SUB] =
        std::unique_ptr<StreamDistributor>(new StreamDistributor(VencChannel::CHN1));
    distributors_[StreamType::VIDEO_MJPEG] =
        std::unique_ptr<StreamDistributor>(new StreamDistributor(VencChannel::CHN2));
}

void StreamConsumerManager::set_fetcher(StreamType type, std::unique_ptr<IStreamProvider> fetcher) {
    fetchers_[type] = std::move(fetcher);
}

StreamDistributor& StreamConsumerManager::get_distributor(StreamType type) {
    auto it = distributors_.find(type);
    if (it == distributors_.end() || !it->second) {
        LOGGER_FATAL(STREAM, "No distributor for stream type %d", static_cast<int>(type));
        static StreamDistributor dummy(VencChannel::CHN0);
        return dummy;
    }
    return *it->second;
}

uint32_t StreamConsumerManager::register_consumer(StreamType type,
                                                  ConsumerCallback cb,
                                                  ConsumerConfig config) {
    auto it = distributors_.find(type);
    if (it == distributors_.end() || !it->second) {
        LOGGER_ERROR(STREAM, "Cannot register consumer for unknown stream type %d", static_cast<int>(type));
        return static_cast<uint32_t>(-1);
    }
    return it->second->add_consumer(cb, config);
}

void StreamConsumerManager::unregister_consumer(StreamType type,
                                                uint32_t consumer_id) {
    auto it = distributors_.find(type);
    if (it == distributors_.end() || !it->second) {
        LOGGER_ERROR(STREAM, "Cannot unregister consumer for unknown stream type %d", static_cast<int>(type));
        return;
    }
    it->second->remove_consumer(consumer_id);
}

void StreamConsumerManager::start_all() {
    for (auto& fetcher : fetchers_) {
        fetcher.second->start();
    }
}

void StreamConsumerManager::stop_all() {
    for (auto& fetcher : fetchers_) {
        fetcher.second->stop();
    }
}
