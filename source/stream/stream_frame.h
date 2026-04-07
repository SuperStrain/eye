#ifndef _STREAM_FRAME_H_
#define _STREAM_FRAME_H_

#include "stream_common.h"
#include "common_types.h"
#include <memory>

class StreamFrame {
public:
    StreamFrame(VencChannel chn, StreamType type, CodecType codec, const FrameData& data);
    ~StreamFrame();
    StreamFrame(const StreamFrame&) = delete;
    StreamFrame& operator=(const StreamFrame&) = delete;
    VencChannel channel() const { return channel_; }
    StreamType type() const { return type_; }
    CodecType codec_type() const { return codec_type_; }
    uint64_t timestamp() const { return timestamp_; }
    bool is_idr() const;
    const FrameData& data() const { return frame_data_; }

private:
    VencChannel channel_;
    StreamType type_;
    CodecType codec_type_;
    uint64_t timestamp_;
    FrameData frame_data_;
};

using StreamFramePtr = std::shared_ptr<StreamFrame>;

#endif
