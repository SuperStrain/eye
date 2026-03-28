#pragma once

#include "stream_fetcher.h"
#include "stream_distributor.h"
#include "stream_common.h"
#include <map>

class StreamConsumerManager {
public:
    static StreamConsumerManager& instance();

    uint32_t register_consumer(StreamType type, ConsumerCallback cb,
                                ConsumerConfig config = ConsumerConfig{});
    void unregister_consumer(StreamType type, uint32_t consumer_id);

    void start_all();
    void stop_all();

private:
    StreamConsumerManager();

    std::map<StreamType, std::unique_ptr<StreamDistributor>> distributors_;
    std::map<StreamType, std::unique_ptr<StreamFetcher>> fetchers_;
};
