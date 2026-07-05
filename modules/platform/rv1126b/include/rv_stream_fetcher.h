#ifndef RV_STREAM_FETCHER_H
#define RV_STREAM_FETCHER_H

#include "i_stream_provider.h"
#include "stream_distributor.h"
#include <atomic>
#include <thread>

class RvStreamFetcher : public IStreamProvider {
public:
    RvStreamFetcher(VencChannel chn, StreamType type, CodecType codec,
                    StreamDistributor& distributor);
    ~RvStreamFetcher();

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
    std::atomic<bool> running_;
    std::thread thread_;
};

#endif
