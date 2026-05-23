#ifndef I_VIDEO_ENCODER_H
#define I_VIDEO_ENCODER_H

#include "common_types.h"
#include "stream_common.h"

class IVideoEncoder {
public:
    virtual ~IVideoEncoder() = default;
    virtual int createChannel(int chn, CodecType codec, Size resolution) = 0;
    virtual int destroyChannel(int chn) = 0;
    virtual int startChannel(int chn) = 0;
    virtual int stopChannel(int chn) = 0;
    virtual int requestIdr(int chn, bool instant) = 0;
};

#endif
