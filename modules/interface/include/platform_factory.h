#ifndef PLATFORM_FACTORY_H
#define PLATFORM_FACTORY_H

#include "common_types.h"
#include "i_stream_provider.h"
#include "i_video_pipeline.h"
#include <memory>

class StreamDistributor;

IVideoPipeline& platform_video_pipeline();

std::unique_ptr<IStreamProvider> create_stream_fetcher(
    VencChannel chn,
    StreamType type,
    CodecType codec,
    StreamDistributor& distributor);

#endif
