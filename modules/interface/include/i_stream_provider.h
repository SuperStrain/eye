#ifndef I_STREAM_PROVIDER_H
#define I_STREAM_PROVIDER_H

#include "common_types.h"
#include "stream_common.h"

class IStreamProvider {
public:
    virtual ~IStreamProvider() = default;
    virtual int start() = 0;
    virtual int stop() = 0;
    virtual int fetchFrame(VencChannel chn, FrameData& frame) = 0;
    virtual int releaseFrame(VencChannel chn) = 0;
};

#endif
