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

    CodecType main_codec = CodecType::H265;
    CodecType sub_codec = CodecType::H264;

    fetchers_[StreamType::VIDEO_MAIN] =
        std::unique_ptr<StreamFetcher>(new StreamFetcher(
            VencChannel::CHN0, StreamType::VIDEO_MAIN, main_codec,
            *distributors_[StreamType::VIDEO_MAIN]));

    fetchers_[StreamType::VIDEO_SUB] =
        std::unique_ptr<StreamFetcher>(new StreamFetcher(
            VencChannel::CHN1, StreamType::VIDEO_SUB, sub_codec,
            *distributors_[StreamType::VIDEO_SUB]));

    fetchers_[StreamType::VIDEO_MJPEG] =
        std::unique_ptr<StreamFetcher>(new StreamFetcher(
            VencChannel::CHN2, StreamType::VIDEO_MJPEG, CodecType::MJPEG,
            *distributors_[StreamType::VIDEO_MJPEG]));
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
    for (auto&fetcher  : fetchers_) {
        fetcher.second->start();
    }
}

void StreamConsumerManager::stop_all() {
    for (auto& fetcher : fetchers_) {
        fetcher.second->stop();
    }
}
