// source/stream/stream_fetcher.h
#ifndef _STREAM_FETCHER_H_
#define _STREAM_FETCHER_H_

#include "stream_distributor.h"
#include "i_stream_provider.h"
#include <ot_common_venc.h>
#include <thread>
#include <atomic>

class StreamFetcher : public IStreamProvider {
public:
    StreamFetcher(VencChannel chn, StreamType type, CodecType codec,
                  StreamDistributor& distributor);
    ~StreamFetcher();

    int start() override;
    int stop() override;
    int fetchFrame(VencChannel chn, FrameData& frame) override;
    int releaseFrame(VencChannel chn) override;
    bool is_running() const { return running_; }

private:
    void run();

    VencChannel channel_;
    StreamType stream_type_;
    CodecType codec_type_;
    StreamDistributor& distributor_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    int fd_;
    ot_venc_stream current_stream_;
};

#endif
