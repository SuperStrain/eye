#include "platform_factory.h"

#include "rv_stream_fetcher.h"
#include "rv_video_pipeline.h"

IVideoPipeline& platform_video_pipeline() {
    return rv1126bMedia::RvVideoPipeline::getInstance();
}

std::unique_ptr<IStreamProvider> create_stream_fetcher(
    VencChannel chn,
    StreamType type,
    CodecType codec,
    StreamDistributor& distributor) {
    return std::unique_ptr<IStreamProvider>(new RvStreamFetcher(chn, type, codec, distributor));
}
