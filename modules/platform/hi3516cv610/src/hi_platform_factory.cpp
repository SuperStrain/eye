#include "platform_factory.h"

#include "hi_stream_fetcher.h"
#include "hi_video_pipeline.h"

IVideoPipeline& platform_video_pipeline() {
    return hiMppMedia::videoProcessHi::getInstance();
}

std::unique_ptr<IStreamProvider> create_stream_fetcher(
    VencChannel chn,
    StreamType type,
    CodecType codec,
    StreamDistributor& distributor) {
    return std::unique_ptr<IStreamProvider>(new StreamFetcher(chn, type, codec, distributor));
}
