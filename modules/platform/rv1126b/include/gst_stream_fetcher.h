#ifndef GST_STREAM_FETCHER_H
#define GST_STREAM_FETCHER_H

#include "i_stream_provider.h"
#include "stream_distributor.h"
#include <atomic>
#include <thread>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

namespace rv1126bMedia {

class GstStreamFetcher : public IStreamProvider {
public:
    GstStreamFetcher(VencChannel chn, StreamType type, CodecType codec,
                     StreamDistributor& distributor);
    ~GstStreamFetcher();

    int start() override;
    int stop() override;
    int fetchFrame(VencChannel chn, FrameData& frame) override;
    int releaseFrame(VencChannel chn) override;

private:
    void run();

    VencChannel channel_;
    StreamType stream_type_;
    CodecType codec_type_;
    StreamDistributor& distributor_;
    GstAppSink* app_sink_;
    uint32_t seq_;
    std::atomic<bool> running_;
    std::thread thread_;
};

} // namespace rv1126bMedia

#endif
