#include "stream_consumer_manager.h"

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
    return *distributors_[type];
}

uint32_t StreamConsumerManager::register_consumer(StreamType type,
                                                  ConsumerCallback cb,
                                                  ConsumerConfig config) {
    return distributors_[type]->add_consumer(cb, config);
}

void StreamConsumerManager::unregister_consumer(StreamType type,
                                                uint32_t consumer_id) {
    distributors_[type]->remove_consumer(consumer_id);
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
