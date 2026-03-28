#pragma once

#include <cstdint>
#include <functional>
#include <memory>

enum class VencChannel { CHN0 = 0, CHN1 = 1, CHN2 = 2 };

enum class StreamType {
    VIDEO_MAIN,
    VIDEO_SUB,
    VIDEO_MJPEG,
    AUDIO_AAC
};

enum class CodecType { H264, H265, MJPEG };

struct ConsumerConfig {
    size_t max_queue_size = 3;
    int thread_priority = 0;
};

class StreamFrame;
using StreamFramePtr = std::shared_ptr<StreamFrame>;
using ConsumerCallback = std::function<void(StreamFramePtr frame)>;
