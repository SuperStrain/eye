#include "gst_stream_fetcher.h"

namespace rv1126bMedia {

GstStreamFetcher::GstStreamFetcher(VencChannel chn, StreamType type, CodecType codec,
                                   StreamDistributor& distributor)
    : channel_(chn), stream_type_(type), codec_type_(codec), distributor_(distributor),
      app_sink_(nullptr), seq_(0), running_(false) {}

GstStreamFetcher::~GstStreamFetcher() { stop(); }

int GstStreamFetcher::start() {
    if (running_) return 0;
    running_ = true;
    thread_ = std::thread(&GstStreamFetcher::run, this);
    return 0;
}

int GstStreamFetcher::stop() {
    if (!running_) return 0;
    running_ = false;
    if (thread_.joinable()) thread_.join();
    return 0;
}

int GstStreamFetcher::fetchFrame(VencChannel, FrameData&) { return -1; }
int GstStreamFetcher::releaseFrame(VencChannel) { return 0; }

void GstStreamFetcher::run() {
    while (running_) {
        // 骨架占位；Task 3 实现拉流。
    }
}

} // namespace rv1126bMedia
