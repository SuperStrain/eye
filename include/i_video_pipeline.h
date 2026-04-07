#ifndef I_VIDEO_PIPELINE_H
#define I_VIDEO_PIPELINE_H

#include "common_types.h"

class IVideoPipeline {
public:
    virtual ~IVideoPipeline() = default;
    virtual int init() = 0;
    virtual int deinit() = 0;
    virtual int start() = 0;
    virtual int stop() = 0;
};

#endif
