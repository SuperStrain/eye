#include "platform_factory.h"

#ifdef USE_GSTREAMER
#include "gst_stream_fetcher.h"
#include "gst_video_pipeline.h"
#else
#include "rv_stream_fetcher.h"
#include "rv_video_pipeline.h"
#endif

IVideoPipeline& platform_video_pipeline() {
#ifdef USE_GSTREAMER
    return rv1126bMedia::GstVideoPipeline::getInstance();
#else
    return rv1126bMedia::RvVideoPipeline::getInstance();
#endif
}

std::unique_ptr<IStreamProvider> create_stream_fetcher(
    VencChannel chn,
    StreamType type,
    CodecType codec,
    StreamDistributor& distributor) {
#ifdef USE_GSTREAMER
    return std::unique_ptr<IStreamProvider>(
        new rv1126bMedia::GstStreamFetcher(chn, type, codec, distributor));
#else
    return std::unique_ptr<IStreamProvider>(
        new RvStreamFetcher(chn, type, codec, distributor));
#endif
}
