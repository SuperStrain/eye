#ifndef _STREAM_COMMON_H_
#define _STREAM_COMMON_H_

#include <cstdint>
#include <functional>
#include <memory>
#include "common_types.h"

struct ConsumerConfig {
    size_t max_queue_size = 3;
    int thread_priority = 0;
};

class StreamFrame;
using StreamFramePtr = std::shared_ptr<StreamFrame>;
using ConsumerCallback = std::function<void(const StreamFrame& frame)>;

#endif
