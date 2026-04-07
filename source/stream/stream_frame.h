#ifndef _STREAM_FRAME_H_
#define _STREAM_FRAME_H_

#include "stream_common.h"
#include <ot_common_venc.h>
#include <memory>
#include <vector>

class StreamFrame;
using StreamFramePtr = std::shared_ptr<StreamFrame>;

class StreamFrame {
public:
    StreamFrame(VencChannel chn, StreamType type, CodecType codec, const ot_venc_stream& src);
    ~StreamFrame();

    StreamFrame(const StreamFrame&) = delete;
    StreamFrame& operator=(const StreamFrame&) = delete;

    VencChannel channel() const { return channel_; }
    StreamType type() const { return type_; }
    CodecType codec_type() const { return codec_type_; }
    uint64_t timestamp() const { return timestamp_; }
    const ot_venc_stream& data() const { return stream_; }

private:
    VencChannel channel_;
    ot_venc_stream stream_;
    uint64_t timestamp_;
    StreamType type_;
    CodecType codec_type_;

    std::vector<ot_venc_pack> pack_array_;
    std::vector<std::vector<uint8_t>> pack_buffers_;
};

#endif
